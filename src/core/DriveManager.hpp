#pragma once

#include "DriveInfo.hpp"
#include "libburnia/BurniaInit.hpp"
#include <vector>
#include <optional>
#include <memory>
#include <functional>

namespace Burner::Core {

/// Manages optical drive discovery and operations.
/// Wraps libburn drive scanning functionality.
class DriveManager {
public:
    /// Callback for drive change notifications
    using DriveChangeCallback = std::function<void(const std::vector<DriveInfo>&)>;

    /// Construct a DriveManager
    /// @param init Reference to initialized BurniaInit (must outlive DriveManager)
    explicit DriveManager(BurniaInit& init);

    ~DriveManager();

    // Non-copyable
    DriveManager(const DriveManager&) = delete;
    DriveManager& operator=(const DriveManager&) = delete;

    /// Scan for available optical drives
    /// @param grabForMediaInfo If true, grab drives to get media info (causes disc spin-up).
    ///                         If false, only enumerate drives without media details.
    /// @return Vector of DriveInfo for each discovered drive
    [[nodiscard]] std::vector<DriveInfo> scanDrives(bool grabForMediaInfo = true);

    /// Get information about a specific drive
    /// @param devicePath Path to the device (e.g., /dev/sr0)
    /// @return DriveInfo if found, nullopt otherwise
    [[nodiscard]] std::optional<DriveInfo> getDrive(const std::string& devicePath);

    /// Get cached list of drives from last scan
    [[nodiscard]] const std::vector<DriveInfo>& cachedDrives() const { return m_cachedDrives; }

    /// Open/eject the tray for a drive
    /// @param devicePath Path to the device
    /// @return true if successful
    bool ejectDrive(const std::string& devicePath);

    /// Close the tray for a drive
    /// @param devicePath Path to the device
    /// @return true if successful
    bool loadDrive(const std::string& devicePath);

private:
    BurniaInit& m_init;
    std::vector<DriveInfo> m_cachedDrives;
};

} // namespace Burner::Core
