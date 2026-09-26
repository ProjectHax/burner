#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <atomic>
#include <cstdint>

namespace Burner::Core {

/// Progress information for clone operations
struct CloneProgress {
    double percent{0.0};
    std::uint64_t bytesProcessed{0};
    std::uint64_t totalBytes{0};
    int sectorsProcessed{0};
    int totalSectors{0};
    int readSpeedKbps{0};  ///< Current read speed in KB/s
    std::string currentOperation;
};

/// Error information for clone operations
struct CloneError {
    int code{0};
    std::string message;

    [[nodiscard]] bool hasError() const { return code != 0; }
};

/// Disc cloning operations - read disc to image and write image to disc.
class DiscCloner {
public:
    /// Progress callback type
    using ProgressCallback = std::function<void(const CloneProgress&)>;

    /// Construct a DiscCloner for the specified device
    /// @param devicePath Path to the optical drive (e.g., /dev/sr0)
    explicit DiscCloner(const std::string& devicePath);

    ~DiscCloner();

    // Non-copyable
    DiscCloner(const DiscCloner&) = delete;
    DiscCloner& operator=(const DiscCloner&) = delete;

    // Movable
    DiscCloner(DiscCloner&&) noexcept;
    DiscCloner& operator=(DiscCloner&&) noexcept;

    /// Check if cloner is valid (device opened successfully)
    [[nodiscard]] bool isValid() const;

    /// Get device path
    [[nodiscard]] const std::string& devicePath() const { return m_devicePath; }

    /// Set progress callback
    void setProgressCallback(ProgressCallback callback);

    /// Read disc contents to an image file
    /// @param outputPath Path to save the ISO/IMG file
    /// @return CloneError (check hasError())
    CloneError readToImage(const std::filesystem::path& outputPath);

    /// Read disc contents to a BIN/CUE image pair
    /// @param binPath Path to save the .bin file (the .cue file is created alongside)
    /// @return CloneError (check hasError())
    CloneError readToBinCue(const std::filesystem::path& binPath);

    /// Write an image file to disc
    /// @param imagePath Path to the ISO/IMG file
    /// @param verify Verify after writing
    /// @param ejectAfter Eject disc when done
    /// @return CloneError (check hasError())
    CloneError writeFromImage(const std::filesystem::path& imagePath,
                               bool verify = false,
                               bool ejectAfter = true);

    /// Request cancellation of current operation
    void cancel();

    /// Check if cancellation was requested
    [[nodiscard]] bool isCancelled() const;

    /// Get disc size in bytes (0 if unknown/no disc)
    [[nodiscard]] std::uint64_t getDiscSize() const;

private:
    void reportProgress(const CloneProgress& progress);
    CloneError createError(int code, const std::string& message);

    std::string m_devicePath;
    ProgressCallback m_progressCallback;
    std::atomic<bool> m_cancelled{false};
    bool m_valid{false};
};

} // namespace Burner::Core
