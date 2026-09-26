#pragma once

#include "libburnia/IsoImageHandle.hpp"
#include "libburnia/DriveHandle.hpp"
#include "ImageBuilder.hpp"
#include "CueSheet.hpp"
#include <filesystem>
#include <functional>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <chrono>

// Forward declarations for libburn types (in burn:: namespace when compiled as C++)
namespace burn {
struct burn_write_opts;
}

using burn::burn_write_opts;

namespace Burner::Core {

/// Burn operation progress information
struct BurnProgress {
    double percent{0.0};                  ///< 0.0 - 100.0
    std::uint64_t bytesWritten{0};
    std::uint64_t totalBytes{0};
    int currentTrack{0};
    int totalTracks{1};
    int currentSession{0};
    int bufferFillPercent{0};             ///< FIFO buffer fill level
    int writeSpeedKBps{0};                ///< Current write speed in KB/s
    std::chrono::seconds elapsed{0};
    std::chrono::seconds remaining{0};    ///< Estimated remaining time
    std::string currentOperation;         ///< Description of current operation
};

/// Burn status enumeration
enum class BurnStatus {
    Idle,
    Preparing,
    Blanking,
    Writing,
    Closing,
    Verifying,
    Completed,
    Failed,
    Cancelled
};

/// Burn options configuration
struct BurnOptions {
    int speed{0};                  ///< Write speed in KB/s (0 = max)
    bool simulate{false};          ///< Dry run - don't actually burn
    bool verify{false};            ///< Verify after burning
    bool ejectAfter{true};         ///< Eject disc when done
    bool closeDisc{true};          ///< Finalize the disc
    bool burnProof{true};          ///< Buffer underrun protection
    bool blankBeforeBurn{false};   ///< Blank rewritable media first
};

/// Error information for failed burns
struct BurnError {
    int code{0};
    std::string message;
    std::string details;

    [[nodiscard]] bool hasError() const { return code != 0; }
};

/// Result of disc integrity check
struct IntegrityResult {
    bool success{false};              ///< Overall success (no errors found)
    std::uint64_t sectorsChecked{0};  ///< Number of sectors checked
    std::uint64_t badSectors{0};      ///< Number of unreadable sectors
    std::vector<std::uint64_t> badSectorList;  ///< List of bad sector numbers
    std::string message;              ///< Result message
};

/// Core disc burning engine.
/// Wraps libburn operations for burning data to optical media.
class BurnEngine {
public:
    /// Progress callback function type
    using ProgressCallback = std::function<void(const BurnProgress&)>;

    /// Status callback function type
    using StatusCallback = std::function<void(BurnStatus, const std::string&)>;

    /// Construct a BurnEngine for the specified device
    /// @param devicePath Path to the optical drive (e.g., /dev/sr0)
    explicit BurnEngine(const std::string& devicePath);

    ~BurnEngine();

    // Non-copyable
    BurnEngine(const BurnEngine&) = delete;
    BurnEngine& operator=(const BurnEngine&) = delete;

    // Movable
    BurnEngine(BurnEngine&&) noexcept;
    BurnEngine& operator=(BurnEngine&&) noexcept;

    /// Check if engine is valid
    [[nodiscard]] bool isValid() const;

    /// Get the device path
    [[nodiscard]] const std::string& devicePath() const;

    // Callbacks

    /// Set progress callback
    void setProgressCallback(ProgressCallback callback);

    /// Set status callback
    void setStatusCallback(StatusCallback callback);

    // Burn operations

    /// Burn a data disc from an ImageBuilder
    /// @param builder The ImageBuilder with files to burn
    /// @param options Burn options
    /// @return BurnError (check hasError())
    BurnError burnDataDisc(const ImageBuilder& builder,
                           const BurnOptions& options = {});

    /// Burn a data disc from an IsoImageHandle
    /// @param image The ISO image to burn
    /// @param options Burn options
    /// @param nwa Next writable address for multi-session (0 = not multi-session)
    /// @return BurnError (check hasError())
    BurnError burnDataDisc(IsoImageHandle& image,
                           const BurnOptions& options = {},
                           int nwa = 0);

    /// Burn an ISO file to disc
    /// @param isoPath Path to the ISO file
    /// @param options Burn options
    /// @return BurnError (check hasError())
    BurnError burnIsoFile(const std::filesystem::path& isoPath,
                          const BurnOptions& options = {});

    /// Burn a CUE/BIN disc image
    /// @param cueSheet Parsed CUE sheet with track layout
    /// @param options Burn options
    /// @return BurnError (check hasError())
    BurnError burnCueImage(const CueSheet& cueSheet,
                           const BurnOptions& options = {});

    /// Burn raw data from a burn_source
    /// @param source The burn source (takes ownership)
    /// @param sizeInBytes Size of data to burn
    /// @param options Burn options
    /// @return BurnError (check hasError())
    BurnError burnFromSource(burn_source* source,
                              std::uint64_t sizeInBytes,
                              const BurnOptions& options = {});

    /// Burn an audio CD from a list of audio files
    /// @param audioFiles Paths to audio files (will be converted to CD-DA)
    /// @param options Burn options
    /// @return BurnError (check hasError())
    BurnError burnAudioCD(const std::vector<std::filesystem::path>& audioFiles,
                          const BurnOptions& options = {});

    // Disc operations

    /// Blank a rewritable disc
    /// @param full Full blank (slow) or quick blank
    /// @return BurnError
    BurnError blankDisc(bool full = false);

    /// Format a DVD+RW or BD-RE disc
    /// @return BurnError
    BurnError formatDisc();

    /// Check disc integrity by reading all sectors
    /// @param stopOnFirstError Stop as soon as a read error is found
    /// @return IntegrityResult with sector statistics
    IntegrityResult checkIntegrity(bool stopOnFirstError = false);

    // Control

    /// Request cancellation of current operation
    void cancel();

    /// Check if cancellation was requested
    [[nodiscard]] bool isCancelled() const;

    /// Get last error
    [[nodiscard]] const BurnError& lastError() const { return m_lastError; }

    /// Get current status
    [[nodiscard]] BurnStatus status() const { return m_status; }

private:
    // Internal implementation
    void reportProgress(const BurnProgress& progress);
    void reportStatus(BurnStatus status, const std::string& message);
    void pollProgress(burn_drive* drive, burn_source* fifoSource);
    void checkMessages();
    BurnError createError(int code, const std::string& message);

    // Configure write options
    void configureWriteOpts(burn_write_opts* opts, const BurnOptions& options);

    // Verify burned data by reading it back
    BurnError verifyBurnedData(std::uint64_t expectedSize);

    std::string m_devicePath;
    std::unique_ptr<DriveHandle> m_drive;

    ProgressCallback m_progressCallback;
    StatusCallback m_statusCallback;

    std::atomic<bool> m_cancelled{false};
    std::atomic<BurnStatus> m_status{BurnStatus::Idle};
    BurnError m_lastError;

    std::chrono::steady_clock::time_point m_startTime;
};

} // namespace Burner::Core
