#pragma once

#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <optional>

// Forward declarations for libburn types (in burn:: namespace when compiled as C++)
namespace burn {
struct burn_drive;
struct burn_drive_info;
}

using burn::burn_drive;
using burn::burn_drive_info;

namespace Burner::Core {

/// Media type enumeration
enum class MediaType {
    None,
    Unknown,
    CD_ROM,
    CD_R,
    CD_RW,
    DVD_ROM,
    DVD_R,
    DVD_RW,
    DVD_R_DL,
    DVD_Plus_R,
    DVD_Plus_RW,
    DVD_Plus_R_DL,
    DVD_RAM,
    BD_ROM,
    BD_R,
    BD_RE
};

/// Media status enumeration
enum class MediaStatus {
    Unknown,
    NoMedia,
    Blank,
    Appendable,
    Full,
    Unformatted,
    Busy          ///< Drive is mounted or otherwise busy
};

/// Information about inserted media
struct MediaInfo {
    MediaType type{MediaType::None};
    MediaStatus status{MediaStatus::Unknown};
    std::uint64_t capacityBytes{0};
    std::uint64_t freeBytes{0};
    int sessions{0};
    int tracks{0};
    int audioTracks{0};       ///< Number of audio tracks (for audio CDs)
    bool rewritable{false};
    bool isAudioDisc{false};  ///< True if disc contains audio tracks
    std::string profileName;
};

/// RAII wrapper for libburn drive handle.
/// Manages exclusive drive access and provides drive information.
class DriveHandle {
public:
    /// Construct a DriveHandle for the specified device
    /// @param devicePath Path to the device (e.g., /dev/sr0)
    explicit DriveHandle(const std::string& devicePath);

    /// Release the drive and clean up
    ~DriveHandle();

    // Non-copyable
    DriveHandle(const DriveHandle&) = delete;
    DriveHandle& operator=(const DriveHandle&) = delete;

    // Movable
    DriveHandle(DriveHandle&& other) noexcept;
    DriveHandle& operator=(DriveHandle&& other) noexcept;

    /// Check if the handle is valid
    [[nodiscard]] bool isValid() const noexcept { return m_drive != nullptr; }

    /// Get the raw libburn drive pointer (for low-level operations)
    [[nodiscard]] burn_drive* raw() const noexcept { return m_drive; }

    /// Get the device path
    [[nodiscard]] const std::string& devicePath() const noexcept { return m_devicePath; }

    /// Grab exclusive access to the drive
    /// @return true if successful
    bool grab();

    /// Release exclusive access to the drive
    /// @param eject Whether to eject the tray after release
    void release(bool eject = false);

    /// Check if we have exclusive access
    [[nodiscard]] bool isGrabbed() const noexcept { return m_grabbed; }

    /// Get drive vendor string
    [[nodiscard]] std::string vendor() const;

    /// Get drive product/model string
    [[nodiscard]] std::string product() const;

    /// Get drive revision string
    [[nodiscard]] std::string revision() const;

    /// Get available write speeds (in KB/s)
    [[nodiscard]] std::vector<int> writeSpeeds() const;

    /// Check if drive can write CDs
    [[nodiscard]] bool canWriteCD() const;

    /// Check if drive can write DVDs
    [[nodiscard]] bool canWriteDVD() const;

    /// Check if drive can write Blu-rays
    [[nodiscard]] bool canWriteBD() const;

    /// Get information about inserted media
    [[nodiscard]] std::optional<MediaInfo> getMediaInfo() const;

    /// Get the next writable address (for multi-session)
    /// @return NWA in sectors, or -1 if not available
    [[nodiscard]] int getNextWritableAddress() const;

    /// Eject the tray
    bool eject();

    /// Load/close the tray
    bool load();

private:
    void cleanup();

    std::string m_devicePath;
    burn_drive* m_drive{nullptr};
    burn_drive_info* m_driveInfo{nullptr};
    bool m_grabbed{false};
};

} // namespace Burner::Core
