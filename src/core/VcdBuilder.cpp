#include "VcdBuilder.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#include <fstream>
#include <algorithm>
#include <cstring>

namespace Burner::Core {

namespace {
    // RAII wrapper for AVFormatContext (input)
    struct FormatContextDeleter {
        void operator()(AVFormatContext* ctx) const {
            if (ctx) avformat_close_input(&ctx);
        }
    };
    using FormatContextPtr = std::unique_ptr<AVFormatContext, FormatContextDeleter>;

    // VCD/SVCD maximum durations
    constexpr int VCD_MAX_DURATION_SEC = 74 * 60;   // 74 minutes
    constexpr int SVCD_MAX_DURATION_SEC = 60 * 60;  // 60 minutes

    // Supported video extensions
    const std::vector<std::string> SUPPORTED_EXTENSIONS = {
        "mp4", "avi", "mkv", "mov", "wmv", "mpg", "mpeg", "m4v", "flv", "webm"
    };
}

VcdSpec VcdSpec::vcd(TvSystem system) {
    VcdSpec spec;
    spec.format = VcdFormat::VCD_2_0;
    spec.system = system;
    spec.videoCodec = "mpeg1video";
    spec.audioCodec = "mp2";
    spec.audioSampleRate = 44100;
    spec.audioBitrate = 224000;

    if (system == TvSystem::PAL) {
        spec.width = 352;
        spec.height = 288;
        spec.frameRate = 25.0;
        spec.videoBitrate = 1150000;
    } else {
        spec.width = 352;
        spec.height = 240;
        spec.frameRate = 29.97;
        spec.videoBitrate = 1150000;
    }

    return spec;
}

VcdSpec VcdSpec::svcd(TvSystem system) {
    VcdSpec spec;
    spec.format = VcdFormat::SVCD;
    spec.system = system;
    spec.videoCodec = "mpeg2video";
    spec.audioCodec = "mp2";
    spec.audioSampleRate = 44100;
    spec.audioBitrate = 384000;

    if (system == TvSystem::PAL) {
        spec.width = 480;
        spec.height = 576;
        spec.frameRate = 25.0;
        spec.videoBitrate = 2600000;
    } else {
        spec.width = 480;
        spec.height = 480;
        spec.frameRate = 29.97;
        spec.videoBitrate = 2600000;
    }

    return spec;
}

VcdBuilder::VcdBuilder() = default;
VcdBuilder::~VcdBuilder() = default;

VcdBuilder::VcdBuilder(VcdBuilder&&) noexcept = default;
VcdBuilder& VcdBuilder::operator=(VcdBuilder&&) noexcept = default;

void VcdBuilder::setFormat(VcdFormat format) {
    m_format = format;
}

void VcdBuilder::setTvSystem(TvSystem system) {
    m_system = system;
}

void VcdBuilder::setProgressCallback(ProgressCallback callback) {
    m_progressCallback = std::move(callback);
}

void VcdBuilder::reportProgress(const VcdProgress& progress) {
    if (m_progressCallback) {
        m_progressCallback(progress);
    }
}

bool VcdBuilder::isFormatSupported(const std::string& extension) {
    std::string ext = extension;
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return std::find(SUPPORTED_EXTENSIONS.begin(),
                     SUPPORTED_EXTENSIONS.end(), ext) != SUPPORTED_EXTENSIONS.end();
}

VideoInfo VcdBuilder::getVideoInfo(const std::filesystem::path& filePath) {
    VideoInfo info;
    info.filePath = filePath.string();

    AVFormatContext* fmtCtx = nullptr;
    if (avformat_open_input(&fmtCtx, filePath.c_str(), nullptr, nullptr) < 0) {
        return info;
    }
    FormatContextPtr ctx(fmtCtx);

    if (avformat_find_stream_info(fmtCtx, nullptr) < 0) {
        return info;
    }

    // Get duration
    if (fmtCtx->duration != AV_NOPTS_VALUE) {
        info.durationMs = static_cast<int>(fmtCtx->duration / (AV_TIME_BASE / 1000));
    }

    // Find video stream
    for (unsigned int i = 0; i < fmtCtx->nb_streams; ++i) {
        if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            AVCodecParameters* codecpar = fmtCtx->streams[i]->codecpar;
            info.width = codecpar->width;
            info.height = codecpar->height;

            // Get frame rate
            AVRational fr = fmtCtx->streams[i]->avg_frame_rate;
            if (fr.num > 0 && fr.den > 0) {
                info.frameRate = static_cast<double>(fr.num) / fr.den;
            }

            // Get codec name
            const AVCodecDescriptor* desc = avcodec_descriptor_get(codecpar->codec_id);
            if (desc) {
                info.codecName = desc->name;
            }
            break;
        }
    }

    // Get title from metadata
    AVDictionaryEntry* tag = av_dict_get(fmtCtx->metadata, "title", nullptr, 0);
    if (tag && tag->value) {
        info.title = tag->value;
    } else {
        info.title = filePath.stem().string();
    }

    return info;
}

bool VcdBuilder::addVideo(const std::filesystem::path& filePath) {
    if (!std::filesystem::exists(filePath)) {
        return false;
    }

    std::string ext = filePath.extension().string();
    if (!ext.empty() && ext[0] == '.') {
        ext = ext.substr(1);
    }

    if (!isFormatSupported(ext)) {
        return false;
    }

    VideoInfo info = getVideoInfo(filePath);
    if (info.durationMs <= 0) {
        return false;
    }

    m_videos.push_back(std::move(info));
    return true;
}

int VcdBuilder::totalDurationSeconds() const {
    int total = 0;
    for (const auto& video : m_videos) {
        total += video.durationMs / 1000;
    }
    return total;
}

bool VcdBuilder::fitsOnDisc() const {
    int maxDuration = (m_format == VcdFormat::SVCD) ?
                      SVCD_MAX_DURATION_SEC : VCD_MAX_DURATION_SEC;
    return totalDurationSeconds() <= maxDuration;
}

void VcdBuilder::clear() {
    m_videos.clear();
    m_encodedFiles.clear();
}

void VcdBuilder::cancel() {
    m_cancelled = true;
}

VcdError VcdBuilder::encodeVideos(const std::filesystem::path& outputDir) {
    m_cancelled = false;
    m_encodedFiles.clear();

    if (m_videos.empty()) {
        return VcdError{1, "No videos to encode"};
    }

    // Create output directory if needed
    std::error_code ec;
    std::filesystem::create_directories(outputDir, ec);
    if (ec) {
        return VcdError{2, "Failed to create output directory: " + ec.message()};
    }

    // Get target spec
    VcdSpec spec = (m_format == VcdFormat::SVCD) ?
                   VcdSpec::svcd(m_system) : VcdSpec::vcd(m_system);

    VcdProgress progress;
    progress.totalFiles = static_cast<int>(m_videos.size());

    for (int i = 0; i < static_cast<int>(m_videos.size()); ++i) {
        if (m_cancelled) {
            return VcdError{10, "Operation cancelled by user"};
        }

        const auto& video = m_videos[i];
        progress.currentFile = i + 1;
        progress.currentFileName = std::filesystem::path(video.filePath).filename().string();
        progress.currentOperation = "Encoding video " + std::to_string(i + 1) +
                                    " of " + std::to_string(m_videos.size());
        progress.percent = (i * 100.0) / m_videos.size();
        reportProgress(progress);

        // Generate output filename
        std::filesystem::path outputPath = outputDir /
            (std::to_string(i + 1) + "_video.mpg");

        auto result = encodeVideo(video, outputPath, spec);
        if (result.hasError()) {
            return result;
        }

        m_encodedFiles.push_back(outputPath);
    }

    progress.percent = 100.0;
    progress.currentOperation = "Encoding complete";
    reportProgress(progress);

    return VcdError{0, "Videos encoded successfully"};
}

VcdError VcdBuilder::encodeVideo(const VideoInfo& video,
                                  const std::filesystem::path& outputPath,
                                  const VcdSpec& spec) {
    // Build ffmpeg command line args for encoding
    // We use the FFmpeg libraries to transcode

    AVFormatContext* inputCtx = nullptr;
    if (avformat_open_input(&inputCtx, video.filePath.c_str(), nullptr, nullptr) < 0) {
        return VcdError{3, "Failed to open input: " + video.filePath};
    }

    if (avformat_find_stream_info(inputCtx, nullptr) < 0) {
        avformat_close_input(&inputCtx);
        return VcdError{4, "Failed to find stream info"};
    }

    // Find video and audio streams
    int videoStreamIdx = -1;
    int audioStreamIdx = -1;
    for (unsigned int i = 0; i < inputCtx->nb_streams; ++i) {
        if (inputCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && videoStreamIdx < 0) {
            videoStreamIdx = static_cast<int>(i);
        } else if (inputCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audioStreamIdx < 0) {
            audioStreamIdx = static_cast<int>(i);
        }
    }

    if (videoStreamIdx < 0) {
        avformat_close_input(&inputCtx);
        return VcdError{5, "No video stream found in: " + video.filePath};
    }

    // Open video decoder
    AVCodecParameters* videoCodecPar = inputCtx->streams[videoStreamIdx]->codecpar;
    const AVCodec* videoDecoder = avcodec_find_decoder(videoCodecPar->codec_id);
    if (!videoDecoder) {
        avformat_close_input(&inputCtx);
        return VcdError{6, "Video decoder not found"};
    }

    AVCodecContext* videoDecCtx = avcodec_alloc_context3(videoDecoder);
    avcodec_parameters_to_context(videoDecCtx, videoCodecPar);
    if (avcodec_open2(videoDecCtx, videoDecoder, nullptr) < 0) {
        avcodec_free_context(&videoDecCtx);
        avformat_close_input(&inputCtx);
        return VcdError{7, "Failed to open video decoder"};
    }

    // Open output format
    AVFormatContext* outputCtx = nullptr;
    if (avformat_alloc_output_context2(&outputCtx, nullptr, "vcd", outputPath.c_str()) < 0) {
        // Fall back to MPEG format
        if (avformat_alloc_output_context2(&outputCtx, nullptr, "mpeg", outputPath.c_str()) < 0) {
            avcodec_free_context(&videoDecCtx);
            avformat_close_input(&inputCtx);
            return VcdError{8, "Failed to create output context"};
        }
    }

    // Find encoder
    const AVCodec* videoEncoder = avcodec_find_encoder_by_name(spec.videoCodec.c_str());
    if (!videoEncoder) {
        avformat_free_context(outputCtx);
        avcodec_free_context(&videoDecCtx);
        avformat_close_input(&inputCtx);
        return VcdError{9, "Video encoder not found: " + spec.videoCodec};
    }

    // Create output video stream
    AVStream* outVideoStream = avformat_new_stream(outputCtx, videoEncoder);
    if (!outVideoStream) {
        avformat_free_context(outputCtx);
        avcodec_free_context(&videoDecCtx);
        avformat_close_input(&inputCtx);
        return VcdError{10, "Failed to create output stream"};
    }

    // Configure encoder context
    AVCodecContext* videoEncCtx = avcodec_alloc_context3(videoEncoder);
    videoEncCtx->width = spec.width;
    videoEncCtx->height = spec.height;
    videoEncCtx->time_base = AVRational{1, static_cast<int>(spec.frameRate * 1000)};
    videoEncCtx->framerate = AVRational{static_cast<int>(spec.frameRate * 1000), 1000};
    videoEncCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    videoEncCtx->bit_rate = spec.videoBitrate;
    videoEncCtx->gop_size = 15;
    videoEncCtx->max_b_frames = 2;

    if (outputCtx->oformat->flags & AVFMT_GLOBALHEADER) {
        videoEncCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if (avcodec_open2(videoEncCtx, videoEncoder, nullptr) < 0) {
        avcodec_free_context(&videoEncCtx);
        avformat_free_context(outputCtx);
        avcodec_free_context(&videoDecCtx);
        avformat_close_input(&inputCtx);
        return VcdError{11, "Failed to open video encoder"};
    }

    avcodec_parameters_from_context(outVideoStream->codecpar, videoEncCtx);
    outVideoStream->time_base = videoEncCtx->time_base;

    // Open output file
    if (!(outputCtx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&outputCtx->pb, outputPath.c_str(), AVIO_FLAG_WRITE) < 0) {
            avcodec_free_context(&videoEncCtx);
            avformat_free_context(outputCtx);
            avcodec_free_context(&videoDecCtx);
            avformat_close_input(&inputCtx);
            return VcdError{12, "Failed to open output file"};
        }
    }

    // Write header
    if (avformat_write_header(outputCtx, nullptr) < 0) {
        avio_closep(&outputCtx->pb);
        avcodec_free_context(&videoEncCtx);
        avformat_free_context(outputCtx);
        avcodec_free_context(&videoDecCtx);
        avformat_close_input(&inputCtx);
        return VcdError{13, "Failed to write output header"};
    }

    // Create scaler for video resize
    SwsContext* swsCtx = sws_getContext(
        videoDecCtx->width, videoDecCtx->height, videoDecCtx->pix_fmt,
        spec.width, spec.height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );

    if (!swsCtx) {
        av_write_trailer(outputCtx);
        avio_closep(&outputCtx->pb);
        avcodec_free_context(&videoEncCtx);
        avformat_free_context(outputCtx);
        avcodec_free_context(&videoDecCtx);
        avformat_close_input(&inputCtx);
        return VcdError{14, "Failed to create scaler"};
    }

    // Allocate frames
    AVFrame* decFrame = av_frame_alloc();
    AVFrame* encFrame = av_frame_alloc();
    encFrame->format = AV_PIX_FMT_YUV420P;
    encFrame->width = spec.width;
    encFrame->height = spec.height;
    av_frame_get_buffer(encFrame, 0);

    AVPacket* packet = av_packet_alloc();
    AVPacket* outPacket = av_packet_alloc();

    int64_t pts = 0;

    // Read and transcode
    while (!m_cancelled && av_read_frame(inputCtx, packet) >= 0) {
        if (packet->stream_index == videoStreamIdx) {
            if (avcodec_send_packet(videoDecCtx, packet) >= 0) {
                while (avcodec_receive_frame(videoDecCtx, decFrame) >= 0) {
                    // Scale frame
                    sws_scale(swsCtx,
                              decFrame->data, decFrame->linesize, 0, videoDecCtx->height,
                              encFrame->data, encFrame->linesize);

                    encFrame->pts = pts++;

                    // Encode
                    if (avcodec_send_frame(videoEncCtx, encFrame) >= 0) {
                        while (avcodec_receive_packet(videoEncCtx, outPacket) >= 0) {
                            outPacket->stream_index = 0;
                            av_packet_rescale_ts(outPacket, videoEncCtx->time_base,
                                                 outVideoStream->time_base);
                            av_interleaved_write_frame(outputCtx, outPacket);
                        }
                    }
                }
            }
        }
        av_packet_unref(packet);
    }

    // Flush encoder
    avcodec_send_frame(videoEncCtx, nullptr);
    while (avcodec_receive_packet(videoEncCtx, outPacket) >= 0) {
        outPacket->stream_index = 0;
        av_packet_rescale_ts(outPacket, videoEncCtx->time_base, outVideoStream->time_base);
        av_interleaved_write_frame(outputCtx, outPacket);
    }

    // Cleanup
    av_packet_free(&outPacket);
    av_packet_free(&packet);
    av_frame_free(&encFrame);
    av_frame_free(&decFrame);
    sws_freeContext(swsCtx);
    av_write_trailer(outputCtx);
    avio_closep(&outputCtx->pb);
    avcodec_free_context(&videoEncCtx);
    avformat_free_context(outputCtx);
    avcodec_free_context(&videoDecCtx);
    avformat_close_input(&inputCtx);

    if (m_cancelled) {
        std::filesystem::remove(outputPath);
        return VcdError{15, "Encoding cancelled"};
    }

    return VcdError{0, "Video encoded successfully"};
}

VcdError VcdBuilder::buildImage(const std::filesystem::path& outputPath) {
    if (m_encodedFiles.empty()) {
        return VcdError{1, "No encoded files available. Call encodeVideos first."};
    }

    // Building a proper VCD/SVCD ISO requires vcdimager or similar tool
    // For now, we just indicate that encoded files are ready
    // A full implementation would use libvcd or vcdimager

    return VcdError{0, "Encoded files ready for burning"};
}

} // namespace Burner::Core
