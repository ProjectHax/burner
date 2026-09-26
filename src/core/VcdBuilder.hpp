#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>
#include <cstdint>

namespace Burner::Core {

/// VCD/SVCD format types
enum class VcdFormat {
    VCD_1_1,    ///< Video CD 1.1
    VCD_2_0,    ///< Video CD 2.0 (most common)
    SVCD        ///< Super Video CD
};

/// TV system standard
enum class TvSystem {
    PAL,        ///< 25fps, 576 lines
    NTSC        ///< 29.97fps, 480 lines
};

/// VCD video specifications
struct VcdSpec {
    VcdFormat format;
    TvSystem system;
    int width;
    int height;
    double frameRate;
    int videoBitrate;       // bps
    int audioBitrate;       // bps
    int audioSampleRate;
    std::string videoCodec;
    std::string audioCodec;

    static VcdSpec vcd(TvSystem system);
    static VcdSpec svcd(TvSystem system);
};

/// Information about a video to be encoded
struct VideoInfo {
    std::string filePath;
    std::string title;
    int durationMs{0};
    int width{0};
    int height{0};
    double frameRate{0.0};
    std::string codecName;
};

/// Progress information for VCD building
struct VcdProgress {
    double percent{0.0};
    int currentFile{0};
    int totalFiles{0};
    std::string currentOperation;
    std::string currentFileName;
};

/// Error information for VCD operations
struct VcdError {
    int code{0};
    std::string message;

    [[nodiscard]] bool hasError() const { return code != 0; }
};

/// VCD/SVCD builder using FFmpeg for video encoding.
class VcdBuilder {
public:
    /// Progress callback type
    using ProgressCallback = std::function<void(const VcdProgress&)>;

    VcdBuilder();
    ~VcdBuilder();

    // Non-copyable
    VcdBuilder(const VcdBuilder&) = delete;
    VcdBuilder& operator=(const VcdBuilder&) = delete;

    // Movable
    VcdBuilder(VcdBuilder&&) noexcept;
    VcdBuilder& operator=(VcdBuilder&&) noexcept;

    /// Set VCD format
    void setFormat(VcdFormat format);

    /// Set TV system
    void setTvSystem(TvSystem system);

    /// Get current format
    [[nodiscard]] VcdFormat format() const { return m_format; }

    /// Get current TV system
    [[nodiscard]] TvSystem tvSystem() const { return m_system; }

    /// Set progress callback
    void setProgressCallback(ProgressCallback callback);

    /// Get video file info
    static VideoInfo getVideoInfo(const std::filesystem::path& filePath);

    /// Check if a video format is supported
    static bool isFormatSupported(const std::string& extension);

    /// Add a video file to be encoded
    bool addVideo(const std::filesystem::path& filePath);

    /// Get number of videos added
    [[nodiscard]] int videoCount() const { return static_cast<int>(m_videos.size()); }

    /// Get estimated total duration in seconds
    [[nodiscard]] int totalDurationSeconds() const;

    /// Check if videos fit on disc (74 min VCD, 60 min SVCD)
    [[nodiscard]] bool fitsOnDisc() const;

    /// Clear all videos
    void clear();

    /// Encode all videos to VCD/SVCD MPEG format
    /// @param outputDir Directory to write encoded files
    /// @return VcdError (check hasError())
    VcdError encodeVideos(const std::filesystem::path& outputDir);

    /// Get paths to encoded MPEG files after encoding
    [[nodiscard]] const std::vector<std::filesystem::path>& encodedFiles() const {
        return m_encodedFiles;
    }

    /// Build a VCD/SVCD ISO image
    /// @param outputPath Path for the ISO file
    /// @return VcdError (check hasError())
    VcdError buildImage(const std::filesystem::path& outputPath);

    /// Request cancellation
    void cancel();

    /// Check if cancelled
    [[nodiscard]] bool isCancelled() const { return m_cancelled; }

private:
    VcdError encodeVideo(const VideoInfo& video,
                         const std::filesystem::path& outputPath,
                         const VcdSpec& spec);
    void reportProgress(const VcdProgress& progress);

    VcdFormat m_format{VcdFormat::VCD_2_0};
    TvSystem m_system{TvSystem::PAL};
    ProgressCallback m_progressCallback;

    std::vector<VideoInfo> m_videos;
    std::vector<std::filesystem::path> m_encodedFiles;
    bool m_cancelled{false};
};

} // namespace Burner::Core
