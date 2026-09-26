#pragma once

// Portability layer for FFmpeg's channel layout API.
//
// FFmpeg 5.1 replaced the channel_layout/channels fields with AVChannelLayout
// (ch_layout) and swr_alloc_set_opts() with swr_alloc_set_opts2(); FFmpeg 7.0
// removed the old API. Older distributions (e.g. Ubuntu 22.04 with FFmpeg 4.4)
// only have the old one.

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libswresample/swresample.h>
}

#define BURNER_FFMPEG_HAS_CH_LAYOUT (LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 24, 100))

namespace Burner::Core::FFmpegCompat {

#if BURNER_FFMPEG_HAS_CH_LAYOUT
using ChannelLayout = AVChannelLayout;
#else
/// Stand-in for AVChannelLayout on FFmpeg < 5.1
struct ChannelLayout {
    uint64_t mask;
    int nb_channels;
};
#endif

/// Default layout for a channel count (e.g. 2 -> stereo)
inline ChannelLayout defaultLayout(int channels) {
#if BURNER_FFMPEG_HAS_CH_LAYOUT
    ChannelLayout layout;
    av_channel_layout_default(&layout, channels);
    return layout;
#else
    return {static_cast<uint64_t>(av_get_default_channel_layout(channels)), channels};
#endif
}

/// Channel layout of a codec context
inline ChannelLayout layoutOf(const AVCodecContext* ctx) {
#if BURNER_FFMPEG_HAS_CH_LAYOUT
    return ctx->ch_layout;
#else
    // Decoders may report only a channel count without a layout mask
    if (ctx->channel_layout) {
        return {ctx->channel_layout, ctx->channels};
    }
    return defaultLayout(ctx->channels);
#endif
}

/// Number of channels in a stream's codec parameters
inline int channelCount(const AVCodecParameters* par) {
#if BURNER_FFMPEG_HAS_CH_LAYOUT
    return par->ch_layout.nb_channels;
#else
    return par->channels;
#endif
}

inline void setLayout(AVCodecContext* ctx, const ChannelLayout& layout) {
#if BURNER_FFMPEG_HAS_CH_LAYOUT
    av_channel_layout_copy(&ctx->ch_layout, &layout);
#else
    ctx->channel_layout = layout.mask;
    ctx->channels = layout.nb_channels;
#endif
}

inline void setLayout(AVFrame* frame, const ChannelLayout& layout) {
#if BURNER_FFMPEG_HAS_CH_LAYOUT
    av_channel_layout_copy(&frame->ch_layout, &layout);
#else
    frame->channel_layout = layout.mask;
    frame->channels = layout.nb_channels;
#endif
}

/// Allocate and configure a resampler (see swr_alloc_set_opts2)
inline int allocResampler(SwrContext** swr,
                          const ChannelLayout& outLayout, AVSampleFormat outFormat, int outRate,
                          const ChannelLayout& inLayout, AVSampleFormat inFormat, int inRate) {
#if BURNER_FFMPEG_HAS_CH_LAYOUT
    // FFmpeg 5.x takes non-const layout pointers (const since 6.0)
    ChannelLayout out = outLayout;
    ChannelLayout in = inLayout;
    return swr_alloc_set_opts2(swr, &out, outFormat, outRate,
                               &in, inFormat, inRate, 0, nullptr);
#else
    *swr = swr_alloc_set_opts(*swr, static_cast<int64_t>(outLayout.mask), outFormat, outRate,
                              static_cast<int64_t>(inLayout.mask), inFormat, inRate, 0, nullptr);
    return *swr ? 0 : AVERROR(ENOMEM);
#endif
}

} // namespace Burner::Core::FFmpegCompat
