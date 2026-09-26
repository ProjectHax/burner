#include "AudioEncoder.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

#include "FFmpegCompat.hpp"

#include <fstream>
#include <cstring>

namespace Burner::Core {

// RAII wrapper for AVFormatContext
class AVFormatContextGuard {
public:
    AVFormatContextGuard() = default;
    ~AVFormatContextGuard() {
        if (ctx) {
            avformat_close_input(&ctx);
        }
    }
    AVFormatContext* ctx{nullptr};
};

// RAII wrapper for AVCodecContext
class AVCodecContextGuard {
public:
    AVCodecContextGuard() = default;
    ~AVCodecContextGuard() {
        if (ctx) {
            avcodec_free_context(&ctx);
        }
    }
    AVCodecContext* ctx{nullptr};
};

// RAII wrapper for SwrContext
class SwrContextGuard {
public:
    SwrContextGuard() = default;
    ~SwrContextGuard() {
        if (ctx) {
            swr_free(&ctx);
        }
    }
    SwrContext* ctx{nullptr};
};

// RAII wrapper for AVPacket
class AVPacketGuard {
public:
    AVPacketGuard() : pkt(av_packet_alloc()) {}
    ~AVPacketGuard() {
        if (pkt) {
            av_packet_free(&pkt);
        }
    }
    AVPacket* pkt{nullptr};
};

// RAII wrapper for AVFrame
class AVFrameGuard {
public:
    AVFrameGuard() : frame(av_frame_alloc()) {}
    ~AVFrameGuard() {
        if (frame) {
            av_frame_free(&frame);
        }
    }
    AVFrame* frame{nullptr};
};

class AudioEncoder::Impl {
public:
    // Nothing needed for now
};

AudioEncoder::AudioEncoder()
    : m_impl(std::make_unique<Impl>()) {
}

AudioEncoder::~AudioEncoder() = default;

AudioEncoder::AudioEncoder(AudioEncoder&&) noexcept = default;
AudioEncoder& AudioEncoder::operator=(AudioEncoder&&) noexcept = default;

bool AudioEncoder::isAvailable() {
    // Check if we can access FFmpeg functions
    return avcodec_version() > 0;
}

std::vector<std::string> AudioEncoder::supportedFormats() {
    return {
        "mp3", "flac", "ogg", "wav", "aac", "m4a", "wma",
        "aiff", "ape", "opus", "ac3", "dts"
    };
}

bool AudioEncoder::isFormatSupported(const std::string& extension) {
    auto formats = supportedFormats();
    std::string ext = extension;
    // Remove leading dot if present
    if (!ext.empty() && ext[0] == '.') {
        ext = ext.substr(1);
    }
    // Convert to lowercase
    for (char& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    for (const auto& fmt : formats) {
        if (fmt == ext) {
            return true;
        }
    }
    return false;
}

AudioInfo AudioEncoder::getAudioInfo(const std::filesystem::path& filePath) {
    AudioInfo info;
    info.filePath = filePath.string();

    AVFormatContextGuard formatCtx;

    // Open input file
    if (avformat_open_input(&formatCtx.ctx, filePath.c_str(), nullptr, nullptr) < 0) {
        return info;
    }

    // Get stream info
    if (avformat_find_stream_info(formatCtx.ctx, nullptr) < 0) {
        return info;
    }

    // Find audio stream
    int audioStream = -1;
    for (unsigned int i = 0; i < formatCtx.ctx->nb_streams; ++i) {
        if (formatCtx.ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStream = static_cast<int>(i);
            break;
        }
    }

    if (audioStream < 0) {
        return info;
    }

    AVStream* stream = formatCtx.ctx->streams[audioStream];
    AVCodecParameters* codecpar = stream->codecpar;

    // Get codec info
    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    if (codec) {
        info.codecName = codec->name;
    }

    // Get audio parameters
    info.sampleRate = codecpar->sample_rate;
    info.channels = FFmpegCompat::channelCount(codecpar);
    info.bitsPerSample = codecpar->bits_per_raw_sample > 0
        ? codecpar->bits_per_raw_sample
        : av_get_bytes_per_sample(static_cast<AVSampleFormat>(codecpar->format)) * 8;
    info.bitRate = static_cast<int>(codecpar->bit_rate);

    // Get duration
    if (formatCtx.ctx->duration != AV_NOPTS_VALUE) {
        info.durationMs = static_cast<int>(formatCtx.ctx->duration / 1000);
    } else if (stream->duration != AV_NOPTS_VALUE) {
        info.durationMs = static_cast<int>(
            stream->duration * av_q2d(stream->time_base) * 1000);
    }

    // Get file size
    info.fileSize = std::filesystem::file_size(filePath);

    // Get metadata
    AVDictionaryEntry* tag = nullptr;

    tag = av_dict_get(formatCtx.ctx->metadata, "title", nullptr, 0);
    if (tag) info.title = tag->value;

    tag = av_dict_get(formatCtx.ctx->metadata, "artist", nullptr, 0);
    if (tag) info.artist = tag->value;

    tag = av_dict_get(formatCtx.ctx->metadata, "album", nullptr, 0);
    if (tag) info.album = tag->value;

    tag = av_dict_get(formatCtx.ctx->metadata, "track", nullptr, 0);
    if (tag) {
        try {
            info.trackNumber = std::stoi(tag->value);
        } catch (...) {}
    }

    return info;
}

bool AudioEncoder::decodeToCdda(const std::filesystem::path& inputPath,
                                 const std::filesystem::path& outputPath,
                                 ProgressCallback progress) {
    m_lastError.clear();

    // Get audio info for progress calculation
    auto audioInfo = getAudioInfo(inputPath);
    std::int64_t expectedSize = estimateCddaSize(audioInfo.durationMs);

    // Open input
    AVFormatContextGuard formatCtx;
    if (avformat_open_input(&formatCtx.ctx, inputPath.c_str(), nullptr, nullptr) < 0) {
        m_lastError = "Failed to open input file";
        return false;
    }

    if (avformat_find_stream_info(formatCtx.ctx, nullptr) < 0) {
        m_lastError = "Failed to find stream info";
        return false;
    }

    // Find audio stream
    int audioStream = -1;
    for (unsigned int i = 0; i < formatCtx.ctx->nb_streams; ++i) {
        if (formatCtx.ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStream = static_cast<int>(i);
            break;
        }
    }

    if (audioStream < 0) {
        m_lastError = "No audio stream found";
        return false;
    }

    AVCodecParameters* codecpar = formatCtx.ctx->streams[audioStream]->codecpar;

    // Find decoder
    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) {
        m_lastError = "Unsupported audio codec";
        return false;
    }

    // Create decoder context
    AVCodecContextGuard codecCtx;
    codecCtx.ctx = avcodec_alloc_context3(codec);
    if (!codecCtx.ctx) {
        m_lastError = "Failed to allocate codec context";
        return false;
    }

    if (avcodec_parameters_to_context(codecCtx.ctx, codecpar) < 0) {
        m_lastError = "Failed to copy codec parameters";
        return false;
    }

    if (avcodec_open2(codecCtx.ctx, codec, nullptr) < 0) {
        m_lastError = "Failed to open codec";
        return false;
    }

    // Create resampler to convert to CD-DA format
    SwrContextGuard swrCtx;

    int ret = FFmpegCompat::allocResampler(&swrCtx.ctx,
        FFmpegCompat::defaultLayout(CdDaSpec::CHANNELS), AV_SAMPLE_FMT_S16, CdDaSpec::SAMPLE_RATE,
        FFmpegCompat::layoutOf(codecCtx.ctx), codecCtx.ctx->sample_fmt, codecCtx.ctx->sample_rate);

    if (ret < 0 || !swrCtx.ctx) {
        m_lastError = "Failed to create resampler";
        return false;
    }

    if (swr_init(swrCtx.ctx) < 0) {
        m_lastError = "Failed to initialize resampler";
        return false;
    }

    // Open output file
    std::ofstream outFile(outputPath, std::ios::binary);
    if (!outFile) {
        m_lastError = "Failed to open output file";
        return false;
    }

    // Allocate packet and frame
    AVPacketGuard packet;
    AVFrameGuard frame;

    if (!packet.pkt || !frame.frame) {
        m_lastError = "Failed to allocate packet/frame";
        return false;
    }

    // Decode and resample
    std::int64_t totalWritten = 0;
    std::vector<uint8_t> outBuffer(CdDaSpec::SAMPLE_RATE * CdDaSpec::BYTES_PER_FRAME); // 1 second buffer

    while (av_read_frame(formatCtx.ctx, packet.pkt) >= 0) {
        if (packet.pkt->stream_index == audioStream) {
            ret = avcodec_send_packet(codecCtx.ctx, packet.pkt);
            if (ret < 0) {
                av_packet_unref(packet.pkt);
                continue;
            }

            while (ret >= 0) {
                ret = avcodec_receive_frame(codecCtx.ctx, frame.frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                }
                if (ret < 0) {
                    m_lastError = "Error during decoding";
                    return false;
                }

                // Calculate output samples
                int outSamples = av_rescale_rnd(
                    swr_get_delay(swrCtx.ctx, codecCtx.ctx->sample_rate) + frame.frame->nb_samples,
                    CdDaSpec::SAMPLE_RATE, codecCtx.ctx->sample_rate, AV_ROUND_UP);

                // Ensure buffer is large enough
                size_t requiredSize = static_cast<size_t>(outSamples) * CdDaSpec::BYTES_PER_FRAME;
                if (outBuffer.size() < requiredSize) {
                    outBuffer.resize(requiredSize);
                }

                uint8_t* outPtr = outBuffer.data();
                int converted = swr_convert(swrCtx.ctx,
                    &outPtr, outSamples,
                    const_cast<const uint8_t**>(frame.frame->data), frame.frame->nb_samples);

                if (converted > 0) {
                    size_t bytesToWrite = static_cast<size_t>(converted) * CdDaSpec::BYTES_PER_FRAME;
                    outFile.write(reinterpret_cast<char*>(outBuffer.data()),
                                  static_cast<std::streamsize>(bytesToWrite));
                    totalWritten += static_cast<std::int64_t>(bytesToWrite);

                    if (progress && expectedSize > 0) {
                        progress(totalWritten, expectedSize);
                    }
                }

                av_frame_unref(frame.frame);
            }
        }
        av_packet_unref(packet.pkt);
    }

    // Flush decoder
    avcodec_send_packet(codecCtx.ctx, nullptr);
    while (avcodec_receive_frame(codecCtx.ctx, frame.frame) >= 0) {
        int outSamples = av_rescale_rnd(
            swr_get_delay(swrCtx.ctx, codecCtx.ctx->sample_rate) + frame.frame->nb_samples,
            CdDaSpec::SAMPLE_RATE, codecCtx.ctx->sample_rate, AV_ROUND_UP);

        size_t requiredSize = static_cast<size_t>(outSamples) * CdDaSpec::BYTES_PER_FRAME;
        if (outBuffer.size() < requiredSize) {
            outBuffer.resize(requiredSize);
        }

        uint8_t* outPtr = outBuffer.data();
        int converted = swr_convert(swrCtx.ctx,
            &outPtr, outSamples,
            const_cast<const uint8_t**>(frame.frame->data), frame.frame->nb_samples);

        if (converted > 0) {
            size_t bytesToWrite = static_cast<size_t>(converted) * CdDaSpec::BYTES_PER_FRAME;
            outFile.write(reinterpret_cast<char*>(outBuffer.data()),
                          static_cast<std::streamsize>(bytesToWrite));
            totalWritten += static_cast<std::int64_t>(bytesToWrite);
        }
        av_frame_unref(frame.frame);
    }

    // Flush resampler
    int remaining;
    do {
        uint8_t* outPtr = outBuffer.data();
        remaining = swr_convert(swrCtx.ctx, &outPtr,
            static_cast<int>(outBuffer.size() / CdDaSpec::BYTES_PER_FRAME),
            nullptr, 0);
        if (remaining > 0) {
            size_t bytesToWrite = static_cast<size_t>(remaining) * CdDaSpec::BYTES_PER_FRAME;
            outFile.write(reinterpret_cast<char*>(outBuffer.data()),
                          static_cast<std::streamsize>(bytesToWrite));
            totalWritten += static_cast<std::int64_t>(bytesToWrite);
        }
    } while (remaining > 0);

    outFile.close();
    return true;
}

std::vector<std::uint8_t> AudioEncoder::decodeToCddaBuffer(
    const std::filesystem::path& inputPath,
    ProgressCallback progress) {

    std::vector<std::uint8_t> result;
    m_lastError.clear();

    // Get audio info for buffer sizing
    auto audioInfo = getAudioInfo(inputPath);
    std::int64_t expectedSize = estimateCddaSize(audioInfo.durationMs);
    result.reserve(static_cast<size_t>(expectedSize));

    // Open input
    AVFormatContextGuard formatCtx;
    if (avformat_open_input(&formatCtx.ctx, inputPath.c_str(), nullptr, nullptr) < 0) {
        m_lastError = "Failed to open input file";
        return result;
    }

    if (avformat_find_stream_info(formatCtx.ctx, nullptr) < 0) {
        m_lastError = "Failed to find stream info";
        return result;
    }

    // Find audio stream
    int audioStream = -1;
    for (unsigned int i = 0; i < formatCtx.ctx->nb_streams; ++i) {
        if (formatCtx.ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStream = static_cast<int>(i);
            break;
        }
    }

    if (audioStream < 0) {
        m_lastError = "No audio stream found";
        return result;
    }

    AVCodecParameters* codecpar = formatCtx.ctx->streams[audioStream]->codecpar;

    // Find decoder
    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) {
        m_lastError = "Unsupported audio codec";
        return result;
    }

    // Create decoder context
    AVCodecContextGuard codecCtx;
    codecCtx.ctx = avcodec_alloc_context3(codec);
    if (!codecCtx.ctx) {
        m_lastError = "Failed to allocate codec context";
        return result;
    }

    if (avcodec_parameters_to_context(codecCtx.ctx, codecpar) < 0) {
        m_lastError = "Failed to copy codec parameters";
        return result;
    }

    if (avcodec_open2(codecCtx.ctx, codec, nullptr) < 0) {
        m_lastError = "Failed to open codec";
        return result;
    }

    // Create resampler
    SwrContextGuard swrCtx;

    int ret = FFmpegCompat::allocResampler(&swrCtx.ctx,
        FFmpegCompat::defaultLayout(CdDaSpec::CHANNELS), AV_SAMPLE_FMT_S16, CdDaSpec::SAMPLE_RATE,
        FFmpegCompat::layoutOf(codecCtx.ctx), codecCtx.ctx->sample_fmt, codecCtx.ctx->sample_rate);

    if (ret < 0 || !swrCtx.ctx || swr_init(swrCtx.ctx) < 0) {
        m_lastError = "Failed to create resampler";
        return result;
    }

    // Allocate packet and frame
    AVPacketGuard packet;
    AVFrameGuard frame;

    if (!packet.pkt || !frame.frame) {
        m_lastError = "Failed to allocate packet/frame";
        return result;
    }

    // Decode and resample
    std::vector<uint8_t> tempBuffer(CdDaSpec::SAMPLE_RATE * CdDaSpec::BYTES_PER_FRAME);

    while (av_read_frame(formatCtx.ctx, packet.pkt) >= 0) {
        if (packet.pkt->stream_index == audioStream) {
            ret = avcodec_send_packet(codecCtx.ctx, packet.pkt);
            if (ret < 0) {
                av_packet_unref(packet.pkt);
                continue;
            }

            while (ret >= 0) {
                ret = avcodec_receive_frame(codecCtx.ctx, frame.frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                }
                if (ret < 0) {
                    m_lastError = "Error during decoding";
                    return result;
                }

                int outSamples = av_rescale_rnd(
                    swr_get_delay(swrCtx.ctx, codecCtx.ctx->sample_rate) + frame.frame->nb_samples,
                    CdDaSpec::SAMPLE_RATE, codecCtx.ctx->sample_rate, AV_ROUND_UP);

                size_t requiredSize = static_cast<size_t>(outSamples) * CdDaSpec::BYTES_PER_FRAME;
                if (tempBuffer.size() < requiredSize) {
                    tempBuffer.resize(requiredSize);
                }

                uint8_t* outPtr = tempBuffer.data();
                int converted = swr_convert(swrCtx.ctx,
                    &outPtr, outSamples,
                    const_cast<const uint8_t**>(frame.frame->data), frame.frame->nb_samples);

                if (converted > 0) {
                    size_t bytesToAdd = static_cast<size_t>(converted) * CdDaSpec::BYTES_PER_FRAME;
                    result.insert(result.end(), tempBuffer.begin(), tempBuffer.begin() + bytesToAdd);

                    if (progress && expectedSize > 0) {
                        progress(static_cast<std::int64_t>(result.size()), expectedSize);
                    }
                }

                av_frame_unref(frame.frame);
            }
        }
        av_packet_unref(packet.pkt);
    }

    // Flush decoder and resampler (similar to file version)
    avcodec_send_packet(codecCtx.ctx, nullptr);
    while (avcodec_receive_frame(codecCtx.ctx, frame.frame) >= 0) {
        int outSamples = av_rescale_rnd(
            swr_get_delay(swrCtx.ctx, codecCtx.ctx->sample_rate) + frame.frame->nb_samples,
            CdDaSpec::SAMPLE_RATE, codecCtx.ctx->sample_rate, AV_ROUND_UP);

        size_t requiredSize = static_cast<size_t>(outSamples) * CdDaSpec::BYTES_PER_FRAME;
        if (tempBuffer.size() < requiredSize) {
            tempBuffer.resize(requiredSize);
        }

        uint8_t* outPtr = tempBuffer.data();
        int converted = swr_convert(swrCtx.ctx,
            &outPtr, outSamples,
            const_cast<const uint8_t**>(frame.frame->data), frame.frame->nb_samples);

        if (converted > 0) {
            size_t bytesToAdd = static_cast<size_t>(converted) * CdDaSpec::BYTES_PER_FRAME;
            result.insert(result.end(), tempBuffer.begin(), tempBuffer.begin() + bytesToAdd);
        }
        av_frame_unref(frame.frame);
    }

    // Flush resampler
    int remaining;
    do {
        uint8_t* outPtr = tempBuffer.data();
        remaining = swr_convert(swrCtx.ctx, &outPtr,
            static_cast<int>(tempBuffer.size() / CdDaSpec::BYTES_PER_FRAME),
            nullptr, 0);
        if (remaining > 0) {
            size_t bytesToAdd = static_cast<size_t>(remaining) * CdDaSpec::BYTES_PER_FRAME;
            result.insert(result.end(), tempBuffer.begin(), tempBuffer.begin() + bytesToAdd);
        }
    } while (remaining > 0);

    return result;
}

std::int64_t AudioEncoder::estimateCddaSize(int durationMs) {
    // CD-DA: 44100 Hz * 2 channels * 2 bytes/sample
    return static_cast<std::int64_t>(durationMs) * CdDaSpec::BYTES_PER_SECOND / 1000;
}

} // namespace Burner::Core
