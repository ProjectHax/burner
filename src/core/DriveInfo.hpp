#pragma once

#include "libburnia/DriveHandle.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace Burner::Core {

/// Complete information about an optical drive
struct DriveInfo {
    std::string devicePath;      ///< e.g., /dev/sr0
    std::string vendor;          ///< e.g., "ASUS"
    std::string model;           ///< e.g., "DRW-24B1ST"
    std::string revision;        ///< Firmware revision
    std::vector<int> writeSpeeds;///< Available write speeds in KB/s
    bool canWriteCD{false};
    bool canWriteDVD{false};
    bool canWriteBD{false};
    MediaInfo media;             ///< Information about inserted media

    /// Get a display name for the drive
    [[nodiscard]] std::string displayName() const {
        std::string name = vendor;
        if (!name.empty() && !model.empty()) {
            name += " ";
        }
        name += model;
        if (name.empty()) {
            name = devicePath;
        }
        return name;
    }

    /// Get a description including media info
    [[nodiscard]] std::string description() const {
        std::string desc = displayName() + " (" + devicePath + ")";
        if (media.status != MediaStatus::NoMedia && media.status != MediaStatus::Unknown) {
            desc += " - " + media.profileName;
        }
        return desc;
    }
};

} // namespace Burner::Core
