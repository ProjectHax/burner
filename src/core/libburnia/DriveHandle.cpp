#include "DriveHandle.hpp"

#include <libburn/libburn.h>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <climits>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <sys/mount.h>

using namespace burn;

namespace Burner::Core {

namespace {

/// Try to unmount a device if it's mounted (auto-mount by desktop environments
/// prevents libburn from obtaining exclusive access to the drive)
bool tryUnmountDevice(const std::string& devicePath) {
    std::ifstream mounts("/proc/mounts");
    if (!mounts.is_open()) {
        return true;
    }

    std::vector<std::string> mountPoints;
    std::string line;
    while (std::getline(mounts, line)) {
        std::istringstream iss(line);
        std::string device, mountPoint;
        iss >> device >> mountPoint;
        if (device == devicePath) {
            mountPoints.push_back(mountPoint);
        }
    }
    mounts.close();

    if (mountPoints.empty()) {
        return true;
    }

    for (const auto& mp : mountPoints) {
        if (umount2(mp.c_str(), 0) == 0) {
            continue;
        }
        std::string cmd = "udisksctl unmount -b " + devicePath + " --no-user-interaction 2>/dev/null";
        if (std::system(cmd.c_str()) == 0) {
            return true;
        }
        return false;
    }

    return true;
}

} // anonymous namespace

namespace {

MediaType profileToMediaType(int profileNumber) {
    switch (profileNumber) {
        case 0x08: return MediaType::CD_ROM;
        case 0x09: return MediaType::CD_R;
        case 0x0A: return MediaType::CD_RW;
        case 0x10: return MediaType::DVD_ROM;
        case 0x11: return MediaType::DVD_R;
        case 0x12: return MediaType::DVD_RAM;
        case 0x13: return MediaType::DVD_RW;
        case 0x14: return MediaType::DVD_RW;
        case 0x15: return MediaType::DVD_R_DL;
        case 0x1A: return MediaType::DVD_Plus_RW;
        case 0x1B: return MediaType::DVD_Plus_R;
        case 0x2B: return MediaType::DVD_Plus_R_DL;
        case 0x40: return MediaType::BD_ROM;
        case 0x41: return MediaType::BD_R;
        case 0x42: return MediaType::BD_R;
        case 0x43: return MediaType::BD_RE;
        default: return MediaType::Unknown;
    }
}

MediaStatus discStatusToMediaStatus(burn_disc_status status) {
    switch (status) {
        case BURN_DISC_UNREADY: return MediaStatus::Unknown;
        case BURN_DISC_BLANK: return MediaStatus::Blank;
        case BURN_DISC_EMPTY: return MediaStatus::NoMedia;
        case BURN_DISC_APPENDABLE: return MediaStatus::Appendable;
        case BURN_DISC_FULL: return MediaStatus::Full;
        case BURN_DISC_UNGRABBED: return MediaStatus::Unknown;
        case BURN_DISC_UNSUITABLE: return MediaStatus::Unknown;
        default: return MediaStatus::Unknown;
    }
}

} // anonymous namespace

DriveHandle::DriveHandle(const std::string& devicePath)
    : m_devicePath(devicePath) {

    // Convert filesystem path to libburn address
    // Note: libburn API requires non-const char* for path
    char pathBuf[PATH_MAX];
    std::strncpy(pathBuf, devicePath.c_str(), PATH_MAX - 1);
    pathBuf[PATH_MAX - 1] = '\0';

    char adr[BURN_DRIVE_ADR_LEN];
    int ret = burn_drive_convert_fs_adr(pathBuf, adr);
    if (ret <= 0) {
        throw std::runtime_error("Invalid device path: " + devicePath);
    }

    fprintf(stderr, "DriveHandle: Attempting to grab drive %s (adr=%s)\n", devicePath.c_str(), adr);

    // Unmount device if auto-mounted by desktop environment
    if (!tryUnmountDevice(devicePath)) {
        fprintf(stderr, "DriveHandle: Device %s is mounted and could not be unmounted\n", devicePath.c_str());
        throw std::runtime_error("Failed to access drive (device is mounted): " + devicePath);
    }

    // Scan for the specific drive
    burn_drive_info* driveList = nullptr;
    unsigned int driveCount = 0;

    // Use scan_and_grab for efficiency
    ret = burn_drive_scan_and_grab(&driveList, adr, 0);
    if (ret <= 0 || driveList == nullptr) {
        fprintf(stderr, "DriveHandle: burn_drive_scan_and_grab failed with ret=%d\n", ret);
        throw std::runtime_error("Failed to access drive: " + devicePath);
    }

    fprintf(stderr, "DriveHandle: Successfully grabbed drive\n");

    m_driveInfo = driveList;
    m_drive = driveList->drive;
    m_grabbed = true;
}

DriveHandle::~DriveHandle() {
    cleanup();
}

DriveHandle::DriveHandle(DriveHandle&& other) noexcept
    : m_devicePath(std::move(other.m_devicePath))
    , m_drive(other.m_drive)
    , m_driveInfo(other.m_driveInfo)
    , m_grabbed(other.m_grabbed) {
    other.m_drive = nullptr;
    other.m_driveInfo = nullptr;
    other.m_grabbed = false;
}

DriveHandle& DriveHandle::operator=(DriveHandle&& other) noexcept {
    if (this != &other) {
        cleanup();

        m_devicePath = std::move(other.m_devicePath);
        m_drive = other.m_drive;
        m_driveInfo = other.m_driveInfo;
        m_grabbed = other.m_grabbed;

        other.m_drive = nullptr;
        other.m_driveInfo = nullptr;
        other.m_grabbed = false;
    }
    return *this;
}

void DriveHandle::cleanup() {
    if (m_drive && m_grabbed) {
        burn_drive_release(m_drive, 0);
        m_grabbed = false;
    }
    if (m_driveInfo) {
        burn_drive_info_free(m_driveInfo);
        m_driveInfo = nullptr;
    }
    m_drive = nullptr;
}

bool DriveHandle::grab() {
    if (!m_drive || m_grabbed) {
        return m_grabbed;
    }

    int ret = burn_drive_grab(m_drive, 0);
    if (ret == 1) {
        m_grabbed = true;
        return true;
    }
    return false;
}

void DriveHandle::release(bool eject) {
    if (m_drive && m_grabbed) {
        burn_drive_release(m_drive, eject ? 1 : 0);
        m_grabbed = false;
    }
}

std::string DriveHandle::vendor() const {
    if (!m_driveInfo) return {};
    return std::string(m_driveInfo->vendor);
}

std::string DriveHandle::product() const {
    if (!m_driveInfo) return {};
    return std::string(m_driveInfo->product);
}

std::string DriveHandle::revision() const {
    if (!m_driveInfo) return {};
    return std::string(m_driveInfo->revision);
}

std::vector<int> DriveHandle::writeSpeeds() const {
    std::vector<int> speeds;
    if (!m_drive) return speeds;

    // Use burn_drive_get_speedlist to get available speeds
    burn_speed_descriptor* speedList = nullptr;
    int ret = burn_drive_get_speedlist(m_drive, &speedList);

    if (ret > 0 && speedList) {
        for (burn_speed_descriptor* curr = speedList; curr != nullptr; curr = curr->next) {
            if (curr->write_speed > 0) {
                // Avoid duplicates
                bool found = false;
                for (int s : speeds) {
                    if (s == curr->write_speed) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    speeds.push_back(curr->write_speed);
                }
            }
        }
        burn_drive_free_speedlist(&speedList);
    }

    // Sort speeds in ascending order
    std::sort(speeds.begin(), speeds.end());

    return speeds;
}

bool DriveHandle::canWriteCD() const {
    if (!m_driveInfo) return false;
    // Check for CD-R or CD-RW write capability
    return m_driveInfo->write_cdr || m_driveInfo->write_cdrw;
}

bool DriveHandle::canWriteDVD() const {
    if (!m_driveInfo) return false;
    return m_driveInfo->write_dvdr || m_driveInfo->write_dvdram;
}

bool DriveHandle::canWriteBD() const {
    if (!m_driveInfo) return false;
    // BD write capability indicated by profile support
    // This is a simplified check
    return false; // Would need to check specific BD profiles
}

std::optional<MediaInfo> DriveHandle::getMediaInfo() const {
    if (!m_drive || !m_grabbed) {
        return std::nullopt;
    }

    MediaInfo info;

    // Get disc status
    burn_disc_status status = burn_disc_get_status(m_drive);
    info.status = discStatusToMediaStatus(status);

    if (info.status == MediaStatus::NoMedia) {
        return info;
    }

    // Get media profile
    int profileNumber = 0;
    char profileName[80] = {0};
    int ret = burn_disc_get_profile(m_drive, &profileNumber, profileName);
    if (ret > 0) {
        info.type = profileToMediaType(profileNumber);
        info.profileName = profileName;
    }

    // Get capacity information
    off_t freeBytes = 0;
    int nwa = 0; // Next writable address
    ret = burn_disc_get_msc1(m_drive, &nwa);

    // Get available space
    ret = burn_disc_available_space(m_drive, nullptr);
    if (ret > 0) {
        info.freeBytes = static_cast<std::uint64_t>(ret) * 2048; // Convert sectors to bytes
    }

    // Check if rewritable
    info.rewritable = (info.type == MediaType::CD_RW ||
                       info.type == MediaType::DVD_RW ||
                       info.type == MediaType::DVD_Plus_RW ||
                       info.type == MediaType::DVD_RAM ||
                       info.type == MediaType::BD_RE);

    // Get session and track count
    burn_disc* disc = burn_drive_get_disc(m_drive);
    if (disc) {
        int numSessions = 0;
        burn_session** sessions = burn_disc_get_sessions(disc, &numSessions);
        info.sessions = numSessions;

        // Count tracks across all sessions
        int totalTracks = 0;
        for (int i = 0; i < numSessions; ++i) {
            int numTracks = 0;
            burn_session_get_tracks(sessions[i], &numTracks);
            totalTracks += numTracks;
        }
        info.tracks = totalTracks;

        burn_disc_free(disc);
    }

    return info;
}

int DriveHandle::getNextWritableAddress() const {
    if (!m_drive || !m_grabbed) return -1;

    int nwa = 0;
    int ret = burn_disc_get_msc1(m_drive, &nwa);
    if (ret <= 0) return -1;
    return nwa;
}

bool DriveHandle::eject() {
    if (!m_drive) return false;

    if (m_grabbed) {
        burn_drive_release(m_drive, 1); // Release with eject
        m_grabbed = false;
        return true;
    }

    return burn_drive_grab(m_drive, 0) == 1 &&
           (burn_drive_release(m_drive, 1), true);
}

bool DriveHandle::load() {
    if (!m_drive) return false;

    // burn_drive_load doesn't exist in all versions
    // Use burn_drive_grab which will wait for tray to close
    if (!m_grabbed) {
        return grab();
    }
    return true;
}

} // namespace Burner::Core
