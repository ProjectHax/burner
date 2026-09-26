#include "DriveManager.hpp"
#include "libburnia/DriveHandle.hpp"

#include <libburn/libburn.h>
#include <filesystem>
#include <algorithm>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <linux/cdrom.h>

using namespace burn;

namespace {

/// Check if a device is currently mounted
bool isDeviceMounted(const std::string& devicePath) {
    std::ifstream mounts("/proc/mounts");
    if (!mounts.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(mounts, line)) {
        std::istringstream iss(line);
        std::string device;
        iss >> device;

        // Check direct match
        if (device == devicePath) {
            return true;
        }

        // Also check if the device is a symlink that resolves to our device
        // e.g., /dev/cdrom -> /dev/sr0
        try {
            if (std::filesystem::is_symlink(device)) {
                auto resolved = std::filesystem::read_symlink(device);
                if (!resolved.is_absolute()) {
                    resolved = std::filesystem::path(device).parent_path() / resolved;
                }
                if (std::filesystem::canonical(resolved) == std::filesystem::canonical(devicePath)) {
                    return true;
                }
            }
        } catch (...) {
            // Ignore errors from symlink resolution
        }
    }

    return false;
}

/// Enumerate optical drives from /sys/block as a fallback
std::vector<std::string> enumerateOpticalDrivesFromSys() {
    std::vector<std::string> drives;

    try {
        for (const auto& entry : std::filesystem::directory_iterator("/sys/block")) {
            std::string name = entry.path().filename().string();

            // Check if this is an optical drive (sr*, scd*)
            if (name.substr(0, 2) == "sr" || name.substr(0, 3) == "scd") {
                std::string devicePath = "/dev/" + name;
                if (std::filesystem::exists(devicePath)) {
                    drives.push_back(devicePath);
                }
            }
        }
    } catch (...) {
        // Ignore errors
    }

    return drives;
}

/// Read drive vendor/model from /sys/block
bool readDriveInfoFromSys(const std::string& devicePath, std::string& vendor, std::string& model) {
    std::string name = std::filesystem::path(devicePath).filename().string();
    std::string sysPath = "/sys/block/" + name + "/device/";

    try {
        std::ifstream vendorFile(sysPath + "vendor");
        if (vendorFile.is_open()) {
            std::getline(vendorFile, vendor);
            // Trim whitespace
            while (!vendor.empty() && std::isspace(vendor.back())) {
                vendor.pop_back();
            }
        }

        std::ifstream modelFile(sysPath + "model");
        if (modelFile.is_open()) {
            std::getline(modelFile, model);
            // Trim whitespace
            while (!model.empty() && std::isspace(model.back())) {
                model.pop_back();
            }
        }

        return !vendor.empty() || !model.empty();
    } catch (...) {
        return false;
    }
}

/// Probe media info using kernel ioctls (no libburn grab required).
/// Fills in type, status, isAudioDisc, audioTracks, tracks, and capacityBytes.
void probeMediaWithIoctls(const std::string& devicePath, Burner::Core::MediaInfo& media) {
    using namespace Burner::Core;

    int fd = open(devicePath.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        return;
    }

    // Check disc status to determine if it's audio/data/mixed
    int discStatus = ioctl(fd, CDROM_DISC_STATUS);
    if (discStatus < 0 || discStatus == CDS_NO_DISC || discStatus == CDS_TRAY_OPEN) {
        media.status = MediaStatus::NoMedia;
        close(fd);
        return;
    }

    bool isCd = false;
    switch (discStatus) {
        case CDS_AUDIO:
            media.isAudioDisc = true;
            isCd = true;
            media.status = MediaStatus::Full;
            break;
        case CDS_MIXED:
            media.isAudioDisc = true;
            isCd = true;
            media.status = MediaStatus::Full;
            break;
        case CDS_DATA_1: // Mode 1 CD
        case CDS_DATA_2: // Mode 2 CD (XA)
            isCd = true;
            media.status = MediaStatus::Full;
            break;
        case CDS_NO_INFO:
        default:
            break;
    }

    // Read TOC to count tracks
    struct cdrom_tochdr tochdr{};
    if (ioctl(fd, CDROMREADTOCHDR, &tochdr) == 0) {
        int audioCount = 0;
        int dataCount = 0;
        for (int t = tochdr.cdth_trk0; t <= tochdr.cdth_trk1; ++t) {
            struct cdrom_tocentry entry{};
            entry.cdte_track = static_cast<__u8>(t);
            entry.cdte_format = CDROM_LBA;
            if (ioctl(fd, CDROMREADTOCENTRY, &entry) == 0) {
                if (entry.cdte_ctrl & 0x04) {
                    dataCount++;
                } else {
                    audioCount++;
                }
            }
        }
        media.audioTracks = audioCount;
        media.tracks = audioCount + dataCount;
        if (audioCount > 0) {
            media.isAudioDisc = true;
        }

        // Having a readable TOC means it's a CD
        isCd = true;
    }

    if (isCd && media.type == MediaType::None) {
        media.type = MediaType::CD_ROM;
    }

    // Get disc size for data discs
    std::uint64_t deviceSize = 0;
    if (ioctl(fd, BLKGETSIZE64, &deviceSize) == 0 && deviceSize > 0) {
        media.capacityBytes = deviceSize;
        if (media.status == MediaStatus::Unknown || media.status == MediaStatus::NoMedia) {
            media.status = MediaStatus::Full;
        }
        // If size > 900MB it's likely DVD or BD, not CD
        if (!isCd && media.type == MediaType::None) {
            if (deviceSize > 5000000000ULL) {
                media.type = MediaType::BD_ROM;
            } else if (deviceSize > 900000000ULL) {
                media.type = MediaType::DVD_ROM;
            } else {
                media.type = MediaType::CD_ROM;
            }
        }
    }

    close(fd);
}

} // anonymous namespace

namespace Burner::Core {

DriveManager::DriveManager(BurniaInit& init)
    : m_init(init) {
    if (!m_init.isInitialized()) {
        throw std::runtime_error("BurniaInit must be initialized before creating DriveManager");
    }
}

DriveManager::~DriveManager() = default;

std::vector<DriveInfo> DriveManager::scanDrives(bool grabForMediaInfo) {
    std::vector<DriveInfo> drives;

    // FIRST: Enumerate drives from /sys/block (instant, always works)
    auto sysDrives = enumerateOpticalDrivesFromSys();

    // Build initial drive list from sysfs
    for (const auto& devicePath : sysDrives) {
        DriveInfo driveInfo;
        driveInfo.devicePath = devicePath;
        readDriveInfoFromSys(devicePath, driveInfo.vendor, driveInfo.model);

        // Check if mounted
        bool isMounted = isDeviceMounted(devicePath);
        if (isMounted) {
            driveInfo.media.status = MediaStatus::Busy;
            driveInfo.media.profileName = "Mounted";
        }

        // Use cached info if available
        auto cachedIt = std::find_if(m_cachedDrives.begin(), m_cachedDrives.end(),
            [&devicePath](const DriveInfo& cached) {
                return cached.devicePath == devicePath;
            });
        if (cachedIt != m_cachedDrives.end()) {
            // Preserve cached write capabilities and speeds
            driveInfo.writeSpeeds = cachedIt->writeSpeeds;
            driveInfo.canWriteCD = cachedIt->canWriteCD;
            driveInfo.canWriteDVD = cachedIt->canWriteDVD;
            driveInfo.canWriteBD = cachedIt->canWriteBD;
            // Preserve cached media info only if not mounted AND cache wasn't
            // from a mounted state (Busy is a placeholder, not real media info)
            if (!isMounted && cachedIt->media.status != MediaStatus::Busy) {
                driveInfo.media = cachedIt->media;
            }
        }

        drives.push_back(std::move(driveInfo));
    }

    // If not requesting full media info, return sysfs + cached info
    if (!grabForMediaInfo) {
        m_cachedDrives = drives;
        return drives;
    }

    // Probe media type via ioctls for any drives missing media info.
    // This always works (no exclusive access needed) and provides a baseline
    // even if libburn scan/grab fails below.
    for (auto& driveInfo : drives) {
        if (driveInfo.media.type == MediaType::None &&
            driveInfo.media.status != MediaStatus::Busy) {
            probeMediaWithIoctls(driveInfo.devicePath, driveInfo.media);
        }
    }

    // SECOND: Enhance each drive with libburn info using synchronous per-drive
    // scan_and_grab. We avoid the async burn_drive_scan() because its internal
    // thread can outlive our timeout and crash (SIGBUS) when other operations
    // try to use the drive concurrently.
    for (auto& driveIt : drives) {
        // Skip mounted drives
        if (driveIt.media.status == MediaStatus::Busy) {
            fprintf(stderr, "DriveManager: Drive %s is mounted, skipping media probe\n",
                    driveIt.devicePath.c_str());
            continue;
        }

        // Convert device path to libburn address format
        char pathBuf[BURN_DRIVE_ADR_LEN];
        char libburnAdr[BURN_DRIVE_ADR_LEN];
        std::strncpy(pathBuf, driveIt.devicePath.c_str(), sizeof(pathBuf) - 1);
        pathBuf[sizeof(pathBuf) - 1] = '\0';
        int addrRet = burn_drive_convert_fs_adr(pathBuf, libburnAdr);
        if (addrRet <= 0) {
            // Can't convert - use raw path
            std::strncpy(libburnAdr, driveIt.devicePath.c_str(), sizeof(libburnAdr) - 1);
            libburnAdr[sizeof(libburnAdr) - 1] = '\0';
        }

        // Synchronous scan + grab for this specific drive
        burn_drive_info* driveInfoList = nullptr;
        int ret = burn_drive_scan_and_grab(&driveInfoList, libburnAdr, 0);
        if (ret <= 0 || driveInfoList == nullptr) {
            fprintf(stderr, "DriveManager: Failed to grab drive %s, using ioctl info\n",
                    driveIt.devicePath.c_str());
            continue;
        }

        burn_drive_info& info = driveInfoList[0];
        burn_drive* drive = info.drive;

        // Update drive capabilities
        if (driveIt.vendor.empty()) driveIt.vendor = info.vendor;
        if (driveIt.model.empty()) driveIt.model = info.product;
        driveIt.revision = info.revision;
        driveIt.canWriteCD = info.write_cdr || info.write_cdrw;
        driveIt.canWriteDVD = info.write_dvdr || info.write_dvdram;

        // Get write speeds
        burn_speed_descriptor* speedDescList = nullptr;
        int speedRet = burn_drive_get_speedlist(drive, &speedDescList);
        if (speedRet > 0 && speedDescList) {
            driveIt.writeSpeeds.clear();
            for (burn_speed_descriptor* curr = speedDescList; curr != nullptr; curr = curr->next) {
                if (curr->write_speed > 0) {
                    bool found = false;
                    for (int s : driveIt.writeSpeeds) {
                        if (s == curr->write_speed) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        driveIt.writeSpeeds.push_back(curr->write_speed);
                    }
                }
            }
            burn_drive_free_speedlist(&speedDescList);
            std::sort(driveIt.writeSpeeds.begin(), driveIt.writeSpeeds.end());
        }

        // Get media info from grabbed drive
        burn_disc_status status = burn_disc_get_status(drive);
        fprintf(stderr, "DriveManager: Drive %s disc status=%d\n",
                driveIt.devicePath.c_str(), status);

        if (status != BURN_DISC_EMPTY) {
            int profileNumber = 0;
            char profileName[80] = {0};
            if (burn_disc_get_profile(drive, &profileNumber, profileName) > 0) {
                driveIt.media.profileName = profileName;
                fprintf(stderr, "DriveManager: Profile=%d (%s)\n", profileNumber, profileName);
            }

            // Determine media status
            switch (status) {
                case BURN_DISC_BLANK:
                    driveIt.media.status = MediaStatus::Blank;
                    break;
                case BURN_DISC_APPENDABLE:
                    driveIt.media.status = MediaStatus::Appendable;
                    break;
                case BURN_DISC_FULL:
                    driveIt.media.status = MediaStatus::Full;
                    break;
                default:
                    driveIt.media.status = MediaStatus::Unknown;
                    break;
            }

            // Check for audio tracks using TOC
            burn_disc* disc = burn_drive_get_disc(drive);
            if (disc) {
                int numSessions = 0;
                burn_session** sessions = burn_disc_get_sessions(disc, &numSessions);

                int audioTrackCount = 0;
                int dataTrackCount = 0;

                for (int s = 0; s < numSessions && sessions; ++s) {
                    int numTracks = 0;
                    burn_track** tracks = burn_session_get_tracks(sessions[s], &numTracks);

                    for (int t = 0; t < numTracks && tracks; ++t) {
                        struct burn_toc_entry tocEntry;
                        burn_track_get_entry(tracks[t], &tocEntry);

                        // Control bit 2: 0 = audio, 1 = data
                        bool isDataTrack = (tocEntry.control & 0x04) != 0;

                        if (isDataTrack) {
                            dataTrackCount++;
                        } else {
                            audioTrackCount++;
                        }
                    }
                }

                driveIt.media.audioTracks = audioTrackCount;
                driveIt.media.tracks = audioTrackCount + dataTrackCount;
                driveIt.media.isAudioDisc = (audioTrackCount > 0);
                driveIt.media.sessions = numSessions;

                fprintf(stderr, "DriveManager: TOC scan - %d audio tracks, %d data tracks, %d sessions\n",
                        audioTrackCount, dataTrackCount, numSessions);

                burn_disc_free(disc);
            }

            // Get capacity
            off_t capacity = burn_disc_available_space(drive, nullptr);
            fprintf(stderr, "DriveManager: burn_disc_available_space=%ld (%ld MB)\n",
                    static_cast<long>(capacity), static_cast<long>(capacity / (1024*1024)));

            // Fallback capacity based on profile
            off_t expectedCapacity = 0;
            switch (profileNumber) {
                case 0x09: // CD-R
                case 0x0A: // CD-RW
                    expectedCapacity = 737280000LL; // 700 MB
                    break;
                case 0x11: // DVD-R
                case 0x13: // DVD-RW restricted
                case 0x14: // DVD-RW sequential
                case 0x1A: // DVD+RW
                case 0x1B: // DVD+R
                    expectedCapacity = 4707319808LL; // 4.7 GB
                    break;
                case 0x15: // DVD-R DL
                case 0x2B: // DVD+R DL
                    expectedCapacity = 8543666176LL; // 8.5 GB
                    break;
                case 0x41: // BD-R SRM
                case 0x42: // BD-R RRM
                case 0x43: // BD-RE
                    expectedCapacity = 25025314816LL; // 25 GB
                    break;
            }

            // Set total capacity based on disc profile
            if (expectedCapacity > 0) {
                driveIt.media.capacityBytes = static_cast<std::uint64_t>(expectedCapacity);
            }

            if (status == BURN_DISC_BLANK && expectedCapacity > 0 &&
                capacity < expectedCapacity / 2) {
                fprintf(stderr, "DriveManager: Using expected capacity %ld for blank disc\n",
                        static_cast<long>(expectedCapacity));
                capacity = expectedCapacity;
            }

            if (capacity > 0) {
                driveIt.media.freeBytes = static_cast<std::uint64_t>(capacity);
            }

            // For full discs, try to get actual used size via ioctl
            if (driveIt.media.status == MediaStatus::Full ||
                driveIt.media.freeBytes == 0) {
                int fd = open(driveIt.devicePath.c_str(), O_RDONLY | O_NONBLOCK);
                if (fd >= 0) {
                    std::uint64_t deviceSize = 0;
                    if (ioctl(fd, BLKGETSIZE64, &deviceSize) == 0 && deviceSize > 0) {
                        driveIt.media.capacityBytes = deviceSize;
                        fprintf(stderr, "DriveManager: Got disc size via ioctl: %lu bytes (%lu MB)\n",
                                deviceSize, deviceSize / (1024*1024));
                    }
                    close(fd);
                }
            }
        } else {
            driveIt.media.status = MediaStatus::NoMedia;
            fprintf(stderr, "DriveManager: No media in drive\n");
        }

        burn_drive_release(drive, 0);
        burn_drive_info_free(driveInfoList);
    }

    m_cachedDrives = drives;
    return drives;
}

std::optional<DriveInfo> DriveManager::getDrive(const std::string& devicePath) {
    auto drives = scanDrives();

    auto it = std::find_if(drives.begin(), drives.end(),
        [&devicePath](const DriveInfo& info) {
            return info.devicePath == devicePath;
        });

    if (it != drives.end()) {
        return *it;
    }

    return std::nullopt;
}

bool DriveManager::ejectDrive(const std::string& devicePath) {
    try {
        DriveHandle handle(devicePath);
        return handle.eject();
    } catch (...) {
        return false;
    }
}

bool DriveManager::loadDrive(const std::string& devicePath) {
    try {
        DriveHandle handle(devicePath);
        return handle.load();
    } catch (...) {
        return false;
    }
}

} // namespace Burner::Core
