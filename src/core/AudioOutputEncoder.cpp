#include "AudioOutputEncoder.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
}

#include "FFmpegCompat.hpp"

#include <fstream>
#include <cstring>
#include <algorithm>

namespace {

// Base64 encoding for OGG METADATA_BLOCK_PICTURE
std::string base64Encode(const std::vector<std::uint8_t>& data) {
    static constexpr char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string result;
    result.reserve(((data.size() + 2) / 3) * 4);

    size_t i = 0;
    while (i + 2 < data.size()) {
        uint32_t n = (static_cast<uint32_t>(data[i]) << 16) |
                     (static_cast<uint32_t>(data[i + 1]) << 8) |
                     static_cast<uint32_t>(data[i + 2]);
        result += table[(n >> 18) & 0x3F];
        result += table[(n >> 12) & 0x3F];
        result += table[(n >> 6) & 0x3F];
        result += table[n & 0x3F];
        i += 3;
    }

    if (i + 1 == data.size()) {
        uint32_t n = static_cast<uint32_t>(data[i]) << 16;
        result += table[(n >> 18) & 0x3F];
        result += table[(n >> 12) & 0x3F];
        result += '=';
        result += '=';
    } else if (i + 2 == data.size()) {
        uint32_t n = (static_cast<uint32_t>(data[i]) << 16) |
                     (static_cast<uint32_t>(data[i + 1]) << 8);
        result += table[(n >> 18) & 0x3F];
        result += table[(n >> 12) & 0x3F];
        result += table[(n >> 6) & 0x3F];
        result += '=';
    }

    return result;
}

// Write a 32-bit big-endian value to a buffer
void writeBE32(std::vector<std::uint8_t>& buf, std::uint32_t val) {
    buf.push_back(static_cast<std::uint8_t>((val >> 24) & 0xFF));
    buf.push_back(static_cast<std::uint8_t>((val >> 16) & 0xFF));
    buf.push_back(static_cast<std::uint8_t>((val >> 8) & 0xFF));
    buf.push_back(static_cast<std::uint8_t>(val & 0xFF));
}

// Create FLAC-style METADATA_BLOCK_PICTURE for OGG Vorbis comments
// Format: https://xiph.org/flac/format.html#metadata_block_picture
std::string createMetadataBlockPicture(const std::vector<std::uint8_t>& imageData,
                                       const std::string& mimeType) {
    std::vector<std::uint8_t> block;
    block.reserve(32 + mimeType.size() + imageData.size());

    // Picture type: 3 = front cover
    writeBE32(block, 3);

    // MIME type length and data
    writeBE32(block, static_cast<std::uint32_t>(mimeType.size()));
    for (char c : mimeType) {
        block.push_back(static_cast<std::uint8_t>(c));
    }

    // Description (empty)
    writeBE32(block, 0);

    // Width, height, color depth, indexed colors (all 0 = unknown)
    writeBE32(block, 0);
    writeBE32(block, 0);
    writeBE32(block, 0);
    writeBE32(block, 0);

    // Picture data length and data
    writeBE32(block, static_cast<std::uint32_t>(imageData.size()));
    block.insert(block.end(), imageData.begin(), imageData.end());

    return base64Encode(block);
}

// Get codec ID for cover art based on MIME type
AVCodecID coverArtCodecId(const std::string& mimeType) {
    if (mimeType == "image/png") {
        return AV_CODEC_ID_PNG;
    }
    return AV_CODEC_ID_MJPEG;
}

// Read 16-bit big-endian value
std::uint16_t readBE16(const std::uint8_t* data) {
    return (static_cast<std::uint16_t>(data[0]) << 8) |
           static_cast<std::uint16_t>(data[1]);
}

// Read 32-bit big-endian value
std::uint32_t readBE32(const std::uint8_t* data) {
    return (static_cast<std::uint32_t>(data[0]) << 24) |
           (static_cast<std::uint32_t>(data[1]) << 16) |
           (static_cast<std::uint32_t>(data[2]) << 8) |
           static_cast<std::uint32_t>(data[3]);
}

// Extract dimensions from JPEG data
bool getJpegDimensions(const std::vector<std::uint8_t>& data, int& width, int& height) {
    if (data.size() < 2 || data[0] != 0xFF || data[1] != 0xD8) {
        return false;  // Not a JPEG
    }

    size_t i = 2;
    while (i + 4 < data.size()) {
        if (data[i] != 0xFF) {
            return false;
        }

        std::uint8_t marker = data[i + 1];

        // SOF0, SOF1, SOF2 (baseline, extended, progressive)
        if (marker == 0xC0 || marker == 0xC1 || marker == 0xC2) {
            if (i + 9 < data.size()) {
                height = readBE16(&data[i + 5]);
                width = readBE16(&data[i + 7]);
                return true;
            }
            return false;
        }

        // Skip other markers
        if (marker == 0xD8 || marker == 0xD9) {
            i += 2;  // SOI, EOI have no length
        } else if (marker >= 0xD0 && marker <= 0xD7) {
            i += 2;  // RST markers have no length
        } else {
            // Read segment length
            std::uint16_t len = readBE16(&data[i + 2]);
            i += 2 + len;
        }
    }
    return false;
}

// Extract dimensions from PNG data
bool getPngDimensions(const std::vector<std::uint8_t>& data, int& width, int& height) {
    // PNG signature + IHDR
    if (data.size() < 24) {
        return false;
    }

    // Check PNG signature
    static const std::uint8_t pngSig[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    if (std::memcmp(data.data(), pngSig, 8) != 0) {
        return false;
    }

    // IHDR chunk: length(4) + "IHDR"(4) + width(4) + height(4)
    width = static_cast<int>(readBE32(&data[16]));
    height = static_cast<int>(readBE32(&data[20]));
    return true;
}

// Get image dimensions from cover art data
bool getImageDimensions(const std::vector<std::uint8_t>& data,
                        const std::string& mimeType,
                        int& width, int& height) {
    if (mimeType == "image/png") {
        return getPngDimensions(data, width, height);
    }
    return getJpegDimensions(data, width, height);
}

// Check if format supports attached picture streams
bool formatSupportsAttachedPic(Burner::Core::AudioFormat format) {
    return format == Burner::Core::AudioFormat::FLAC ||
           format == Burner::Core::AudioFormat::MP3 ||
           format == Burner::Core::AudioFormat::AAC;
}

} // anonymous namespace

namespace Burner::Core {

class AudioOutputEncoder::Impl {
public:
    // Nothing needed for stateless encoder
};

AudioOutputEncoder::AudioOutputEncoder()
    : m_impl(std::make_unique<Impl>()) {
}

AudioOutputEncoder::~AudioOutputEncoder() = default;

AudioOutputEncoder::AudioOutputEncoder(AudioOutputEncoder&&) noexcept = default;
AudioOutputEncoder& AudioOutputEncoder::operator=(AudioOutputEncoder&&) noexcept = default;

bool AudioOutputEncoder::isAvailable() {
    // Check if FFmpeg encoders are available
    return avcodec_find_encoder(AV_CODEC_ID_FLAC) != nullptr;
}

bool AudioOutputEncoder::isFormatAvailable(AudioFormat format) {
    AVCodecID codecId;
    switch (format) {
        case AudioFormat::FLAC:
            codecId = AV_CODEC_ID_FLAC;
            break;
        case AudioFormat::MP3:
            codecId = AV_CODEC_ID_MP3;
            break;
        case AudioFormat::OGG:
            codecId = AV_CODEC_ID_VORBIS;
            break;
        case AudioFormat::AAC:
            codecId = AV_CODEC_ID_AAC;
            break;
        case AudioFormat::WAV:
            return true;  // WAV is always available (raw PCM)
        default:
            return false;
    }
    return avcodec_find_encoder(codecId) != nullptr;
}

std::string AudioOutputEncoder::formatExtension(AudioFormat format) {
    switch (format) {
        case AudioFormat::FLAC: return ".flac";
        case AudioFormat::MP3:  return ".mp3";
        case AudioFormat::OGG:  return ".ogg";
        case AudioFormat::AAC:  return ".m4a";
        case AudioFormat::WAV:  return ".wav";
        default: return ".raw";
    }
}

std::string AudioOutputEncoder::formatName(AudioFormat format) {
    switch (format) {
        case AudioFormat::FLAC: return "FLAC";
        case AudioFormat::MP3:  return "MP3";
        case AudioFormat::OGG:  return "OGG Vorbis";
        case AudioFormat::AAC:  return "AAC";
        case AudioFormat::WAV:  return "WAV";
        default: return "Unknown";
    }
}

bool AudioOutputEncoder::encode(const std::vector<std::uint8_t>& inputData,
                                 const std::filesystem::path& outputPath,
                                 AudioFormat format,
                                 const AudioQuality& quality,
                                 const AudioMetadata* metadata,
                                 ProgressCallback progress) {
    m_cancelled = false;

    if (inputData.empty()) {
        m_lastError = "Input data is empty";
        return false;
    }

    // Input specs: 44.1kHz, 16-bit stereo, little-endian (CD-DA)
    constexpr int SAMPLE_RATE = 44100;
    constexpr int CHANNELS = 2;
    constexpr int BITS_PER_SAMPLE = 16;
    constexpr int BYTES_PER_SAMPLE = BITS_PER_SAMPLE / 8;
    constexpr int BYTES_PER_FRAME = CHANNELS * BYTES_PER_SAMPLE;

    // Determine output format
    const char* formatName = nullptr;
    AVCodecID codecId = AV_CODEC_ID_NONE;

    switch (format) {
        case AudioFormat::FLAC:
            formatName = "flac";
            codecId = AV_CODEC_ID_FLAC;
            break;
        case AudioFormat::MP3:
            formatName = "mp3";
            codecId = AV_CODEC_ID_MP3;
            break;
        case AudioFormat::OGG:
            formatName = "ogg";
            codecId = AV_CODEC_ID_VORBIS;
            break;
        case AudioFormat::AAC:
            formatName = "ipod";  // M4A container
            codecId = AV_CODEC_ID_AAC;
            break;
        case AudioFormat::WAV:
            formatName = "wav";
            codecId = AV_CODEC_ID_PCM_S16LE;
            break;
    }

    // Find encoder
    const AVCodec* codec = avcodec_find_encoder(codecId);
    if (!codec) {
        m_lastError = "Encoder not found for format: " + AudioOutputEncoder::formatName(format);
        return false;
    }

    // Allocate format context
    AVFormatContext* formatCtx = nullptr;
    int ret = avformat_alloc_output_context2(&formatCtx, nullptr, formatName,
                                              outputPath.string().c_str());
    if (ret < 0 || !formatCtx) {
        m_lastError = "Failed to allocate output context";
        return false;
    }

    // Create stream
    AVStream* stream = avformat_new_stream(formatCtx, nullptr);
    if (!stream) {
        avformat_free_context(formatCtx);
        m_lastError = "Failed to create output stream";
        return false;
    }

    // Allocate codec context
    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
    if (!codecCtx) {
        avformat_free_context(formatCtx);
        m_lastError = "Failed to allocate codec context";
        return false;
    }

    // Configure codec
    codecCtx->sample_rate = SAMPLE_RATE;
    codecCtx->sample_fmt = AV_SAMPLE_FMT_S16;
    const auto stereoLayout = FFmpegCompat::defaultLayout(2);
    FFmpegCompat::setLayout(codecCtx, stereoLayout);

    // Format-specific settings
    switch (format) {
        case AudioFormat::FLAC:
            codecCtx->compression_level = quality.compression;
            break;
        case AudioFormat::MP3:
            if (quality.vbrQuality >= 0) {
                // VBR mode
                codecCtx->flags |= AV_CODEC_FLAG_QSCALE;
                codecCtx->global_quality = quality.vbrQuality * FF_QP2LAMBDA;
            } else {
                codecCtx->bit_rate = quality.bitrate * 1000;
            }
            break;
        case AudioFormat::OGG:
            // Vorbis uses FLTP sample format
            codecCtx->sample_fmt = AV_SAMPLE_FMT_FLTP;
            if (quality.vbrQuality >= 0) {
                codecCtx->global_quality = quality.vbrQuality * FF_QP2LAMBDA;
            } else {
                codecCtx->bit_rate = quality.bitrate * 1000;
            }
            break;
        case AudioFormat::AAC:
            codecCtx->sample_fmt = AV_SAMPLE_FMT_FLTP;
            codecCtx->bit_rate = quality.bitrate * 1000;
            break;
        case AudioFormat::WAV:
            // No extra config needed
            break;
    }

    // Some formats require certain flags
    if (formatCtx->oformat->flags & AVFMT_GLOBALHEADER) {
        codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    // Open encoder
    ret = avcodec_open2(codecCtx, codec, nullptr);
    if (ret < 0) {
        avcodec_free_context(&codecCtx);
        avformat_free_context(formatCtx);
        m_lastError = "Failed to open encoder";
        return false;
    }

    // Copy codec parameters to stream
    ret = avcodec_parameters_from_context(stream->codecpar, codecCtx);
    if (ret < 0) {
        avcodec_free_context(&codecCtx);
        avformat_free_context(formatCtx);
        m_lastError = "Failed to copy codec parameters";
        return false;
    }

    stream->time_base = {1, SAMPLE_RATE};

    // Add metadata
    if (metadata) {
        if (!metadata->title.empty()) {
            av_dict_set(&formatCtx->metadata, "title", metadata->title.c_str(), 0);
        }
        if (!metadata->artist.empty()) {
            av_dict_set(&formatCtx->metadata, "artist", metadata->artist.c_str(), 0);
        }
        if (!metadata->album.empty()) {
            av_dict_set(&formatCtx->metadata, "album", metadata->album.c_str(), 0);
        }
        if (!metadata->albumArtist.empty()) {
            av_dict_set(&formatCtx->metadata, "album_artist", metadata->albumArtist.c_str(), 0);
        }
        if (metadata->trackNumber > 0) {
            std::string trackStr = std::to_string(metadata->trackNumber);
            if (metadata->totalTracks > 0) {
                trackStr += "/" + std::to_string(metadata->totalTracks);
            }
            av_dict_set(&formatCtx->metadata, "track", trackStr.c_str(), 0);
        }
        if (metadata->year > 0) {
            av_dict_set(&formatCtx->metadata, "date", std::to_string(metadata->year).c_str(), 0);
        }
        if (!metadata->genre.empty()) {
            av_dict_set(&formatCtx->metadata, "genre", metadata->genre.c_str(), 0);
        }
        if (metadata->discNumber > 0) {
            std::string discStr = std::to_string(metadata->discNumber);
            if (metadata->totalDiscs > 0) {
                discStr += "/" + std::to_string(metadata->totalDiscs);
            }
            av_dict_set(&formatCtx->metadata, "disc", discStr.c_str(), 0);
        }
        if (!metadata->comment.empty()) {
            av_dict_set(&formatCtx->metadata, "comment", metadata->comment.c_str(), 0);
        }

        // Handle cover art for OGG (uses METADATA_BLOCK_PICTURE in Vorbis comments)
        if (format == AudioFormat::OGG && !metadata->coverArt.empty()) {
            std::string mimeType = metadata->coverArtMimeType;
            if (mimeType.empty()) {
                mimeType = "image/jpeg";
            }
            std::string pictureData = createMetadataBlockPicture(metadata->coverArt, mimeType);
            av_dict_set(&formatCtx->metadata, "METADATA_BLOCK_PICTURE", pictureData.c_str(), 0);
        }
    }

    // Create cover art stream for formats that support attached pictures
    AVStream* coverStream = nullptr;
    if (metadata && !metadata->coverArt.empty() && formatSupportsAttachedPic(format)) {
        coverStream = avformat_new_stream(formatCtx, nullptr);
        if (coverStream) {
            std::string mimeType = metadata->coverArtMimeType;
            if (mimeType.empty()) {
                mimeType = "image/jpeg";
            }
            AVCodecID coverCodecId = coverArtCodecId(mimeType);

            int coverWidth = 0, coverHeight = 0;
            getImageDimensions(metadata->coverArt, mimeType, coverWidth, coverHeight);

            coverStream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
            coverStream->codecpar->codec_id = coverCodecId;
            coverStream->codecpar->width = coverWidth;
            coverStream->codecpar->height = coverHeight;
            coverStream->codecpar->format = AV_PIX_FMT_YUVJ420P;
            coverStream->disposition = AV_DISPOSITION_ATTACHED_PIC;

            // For MP3 (ID3v2), we need to set the attached pic data directly
            if (format == AudioFormat::MP3) {
                AVPacket* coverPkt = av_packet_alloc();
                if (coverPkt) {
                    av_new_packet(coverPkt, static_cast<int>(metadata->coverArt.size()));
                    std::memcpy(coverPkt->data, metadata->coverArt.data(), metadata->coverArt.size());
                    coverPkt->flags |= AV_PKT_FLAG_KEY;
                    coverPkt->stream_index = coverStream->index;
                    coverStream->attached_pic = *coverPkt;
                    coverStream->attached_pic.stream_index = coverStream->index;
                    av_packet_free(&coverPkt);
                }
            }
        }
    }

    // Open output file
    if (!(formatCtx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&formatCtx->pb, outputPath.string().c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            avcodec_free_context(&codecCtx);
            avformat_free_context(formatCtx);
            m_lastError = "Failed to open output file";
            return false;
        }
    }

    // Write header
    ret = avformat_write_header(formatCtx, nullptr);
    if (ret < 0) {
        avio_closep(&formatCtx->pb);
        avcodec_free_context(&codecCtx);
        avformat_free_context(formatCtx);
        m_lastError = "Failed to write header";
        return false;
    }

    // Write cover art packet for FLAC and AAC (MP3 uses attached_pic set earlier)
    if (coverStream && metadata && !metadata->coverArt.empty() &&
        (format == AudioFormat::FLAC || format == AudioFormat::AAC)) {
        AVPacket* coverPkt = av_packet_alloc();
        if (coverPkt) {
            av_new_packet(coverPkt, static_cast<int>(metadata->coverArt.size()));
            std::memcpy(coverPkt->data, metadata->coverArt.data(), metadata->coverArt.size());
            coverPkt->flags |= AV_PKT_FLAG_KEY;
            coverPkt->stream_index = coverStream->index;
            coverPkt->pts = 0;
            coverPkt->dts = 0;

            // Write cover art frame - failure is non-fatal
            av_write_frame(formatCtx, coverPkt);
            av_packet_free(&coverPkt);
        }
    }

    // Set up resampler if needed (for formats requiring float input)
    SwrContext* swr = nullptr;
    if (codecCtx->sample_fmt != AV_SAMPLE_FMT_S16) {
        ret = FFmpegCompat::allocResampler(&swr,
                                           FFmpegCompat::layoutOf(codecCtx), codecCtx->sample_fmt, SAMPLE_RATE,
                                           stereoLayout, AV_SAMPLE_FMT_S16, SAMPLE_RATE);
        if (ret < 0 || swr_init(swr) < 0) {
            if (swr) swr_free(&swr);
            av_write_trailer(formatCtx);
            avio_closep(&formatCtx->pb);
            avcodec_free_context(&codecCtx);
            avformat_free_context(formatCtx);
            m_lastError = "Failed to initialize resampler";
            return false;
        }
    }

    // Allocate frame
    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        if (swr) swr_free(&swr);
        av_write_trailer(formatCtx);
        avio_closep(&formatCtx->pb);
        avcodec_free_context(&codecCtx);
        avformat_free_context(formatCtx);
        m_lastError = "Failed to allocate frame";
        return false;
    }

    frame->format = codecCtx->sample_fmt;
    FFmpegCompat::setLayout(frame, FFmpegCompat::layoutOf(codecCtx));
    frame->sample_rate = SAMPLE_RATE;

    // Determine frame size
    int frameSize = codecCtx->frame_size;
    if (frameSize <= 0) {
        frameSize = 1024;  // Default for PCM
    }
    frame->nb_samples = frameSize;

    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        av_frame_free(&frame);
        if (swr) swr_free(&swr);
        av_write_trailer(formatCtx);
        avio_closep(&formatCtx->pb);
        avcodec_free_context(&codecCtx);
        avformat_free_context(formatCtx);
        m_lastError = "Failed to allocate frame buffer";
        return false;
    }

    // Allocate packet
    AVPacket* pkt = av_packet_alloc();
    if (!pkt) {
        av_frame_free(&frame);
        if (swr) swr_free(&swr);
        av_write_trailer(formatCtx);
        avio_closep(&formatCtx->pb);
        avcodec_free_context(&codecCtx);
        avformat_free_context(formatCtx);
        m_lastError = "Failed to allocate packet";
        return false;
    }

    // Encode audio data
    std::int64_t samplesEncoded = 0;
    std::int64_t pts = 0;

    const std::uint8_t* inputPtr = inputData.data();
    std::int64_t inputRemaining = static_cast<std::int64_t>(inputData.size());

    while (inputRemaining > 0 && !m_cancelled) {
        ret = av_frame_make_writable(frame);
        if (ret < 0) {
            break;
        }

        // Calculate samples for this frame
        int samplesToEncode = std::min(static_cast<std::int64_t>(frameSize),
                                        inputRemaining / BYTES_PER_FRAME);
        if (samplesToEncode <= 0) {
            break;
        }

        frame->nb_samples = samplesToEncode;
        frame->pts = pts;

        // Copy or convert samples
        if (swr) {
            // Convert S16 to codec's format
            const uint8_t* srcData[1] = { inputPtr };
            ret = swr_convert(swr,
                              frame->data, samplesToEncode,
                              srcData, samplesToEncode);
            if (ret < 0) {
                break;
            }
        } else {
            // Direct copy for S16 output
            std::memcpy(frame->data[0], inputPtr,
                        static_cast<size_t>(samplesToEncode) * BYTES_PER_FRAME);
        }

        // Send frame to encoder
        ret = avcodec_send_frame(codecCtx, frame);
        if (ret < 0) {
            break;
        }

        // Receive encoded packets
        while (ret >= 0) {
            ret = avcodec_receive_packet(codecCtx, pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            }
            if (ret < 0) {
                goto cleanup;
            }

            pkt->stream_index = stream->index;
            av_packet_rescale_ts(pkt, codecCtx->time_base, stream->time_base);

            ret = av_interleaved_write_frame(formatCtx, pkt);
            if (ret < 0) {
                goto cleanup;
            }
        }

        inputPtr += static_cast<size_t>(samplesToEncode) * BYTES_PER_FRAME;
        inputRemaining -= static_cast<std::int64_t>(samplesToEncode) * BYTES_PER_FRAME;
        pts += samplesToEncode;
        samplesEncoded += samplesToEncode;

        if (progress) {
            progress(samplesEncoded * BYTES_PER_FRAME, static_cast<std::int64_t>(inputData.size()));
        }
    }

    // Flush encoder
    if (!m_cancelled) {
        avcodec_send_frame(codecCtx, nullptr);
        while (true) {
            ret = avcodec_receive_packet(codecCtx, pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            }
            if (ret < 0) {
                break;
            }

            pkt->stream_index = stream->index;
            av_packet_rescale_ts(pkt, codecCtx->time_base, stream->time_base);
            av_interleaved_write_frame(formatCtx, pkt);
        }
    }

cleanup:
    // Write trailer
    if (!m_cancelled) {
        av_write_trailer(formatCtx);
    }

    // Clean up
    av_packet_free(&pkt);
    av_frame_free(&frame);
    if (swr) swr_free(&swr);
    avio_closep(&formatCtx->pb);
    avcodec_free_context(&codecCtx);
    avformat_free_context(formatCtx);

    if (m_cancelled) {
        std::filesystem::remove(outputPath);
        m_lastError = "Operation cancelled";
        return false;
    }

    return true;
}

bool AudioOutputEncoder::encodeFile(const std::filesystem::path& inputPath,
                                     const std::filesystem::path& outputPath,
                                     AudioFormat format,
                                     const AudioQuality& quality,
                                     const AudioMetadata* metadata,
                                     ProgressCallback progress) {
    // Read input file
    std::ifstream file(inputPath, std::ios::binary | std::ios::ate);
    if (!file) {
        m_lastError = "Failed to open input file: " + inputPath.string();
        return false;
    }

    auto fileSize = file.tellg();
    file.seekg(0);

    std::vector<std::uint8_t> data(static_cast<size_t>(fileSize));
    file.read(reinterpret_cast<char*>(data.data()), fileSize);

    if (!file) {
        m_lastError = "Failed to read input file";
        return false;
    }

    return encode(data, outputPath, format, quality, metadata, progress);
}

void AudioOutputEncoder::cancel() {
    m_cancelled = true;
}

} // namespace Burner::Core
