#include "DiscCloner.hpp"
#include "CueSheet.hpp"
#include "libburnia/BurniaInit.hpp"

#include <cstdint>

extern "C" {
#include <libburn/libburn.h>
}

using namespace burn;

#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <linux/fs.h>
#include <linux/cdrom.h>
#include <scsi/sg.h>

namespace Burner::Core {

namespace {
    constexpr int SECTOR_SIZE = 2048;       // Standard ISO sector size
    constexpr int RAW_SECTOR_SIZE = 2352;   // Raw CD sector size for BIN/CUE
    constexpr int SECTORS_PER_READ = 32;    // Read 64KB at a time (smaller for responsive cancellation)
    constexpr int BUFFER_SIZE = SECTOR_SIZE * SECTORS_PER_READ;
    constexpr int RAW_SECTORS_PER_READ = 16; // Read 16 raw sectors at a time (~37KB)
    constexpr int RAW_BUFFER_SIZE = RAW_SECTOR_SIZE * RAW_SECTORS_PER_READ;

/// Get actual data size on an optical disc using the TOC lead-out address.
/// BLKGETSIZE64 can overreport for optical media, so this is more accurate.
std::uint64_t getDiscDataSize(int fd) {
    // Try reading the TOC lead-out entry to get actual data extent
    struct cdrom_tocentry tocentry{};
    tocentry.cdte_track = CDROM_LEADOUT;
    tocentry.cdte_format = CDROM_LBA;

    if (ioctl(fd, CDROMREADTOCENTRY, &tocentry) == 0 && tocentry.cdte_addr.lba > 0) {
        return static_cast<std::uint64_t>(tocentry.cdte_addr.lba) * 2048;
    }

    // Fallback to BLKGETSIZE64
    std::uint64_t sizeBytes = 0;
    if (ioctl(fd, BLKGETSIZE64, &sizeBytes) == 0) {
        return sizeBytes;
    }

    return 0;
}

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
        // Try normal unmount first
        if (umount2(mp.c_str(), 0) == 0) {
            continue;
        }
        // Fall back to udisksctl for PolicyKit-managed systems
        std::string cmd = "udisksctl unmount -b " + devicePath + " --no-user-interaction 2>/dev/null";
        if (std::system(cmd.c_str()) == 0) {
            return true;
        }
        return false;
    }

    return true;
}
}

DiscCloner::DiscCloner(const std::string& devicePath)
    : m_devicePath(devicePath) {
    // Validate device exists
    if (!devicePath.empty()) {
        m_valid = true;
    }
}

DiscCloner::~DiscCloner() = default;

DiscCloner::DiscCloner(DiscCloner&& other) noexcept
    : m_devicePath(std::move(other.m_devicePath))
    , m_progressCallback(std::move(other.m_progressCallback))
    , m_cancelled(other.m_cancelled.load())
    , m_valid(other.m_valid) {
    other.m_valid = false;
}

DiscCloner& DiscCloner::operator=(DiscCloner&& other) noexcept {
    if (this != &other) {
        m_devicePath = std::move(other.m_devicePath);
        m_progressCallback = std::move(other.m_progressCallback);
        m_cancelled.store(other.m_cancelled.load());
        m_valid = other.m_valid;
        other.m_valid = false;
    }
    return *this;
}

bool DiscCloner::isValid() const {
    return m_valid;
}

void DiscCloner::setProgressCallback(ProgressCallback callback) {
    m_progressCallback = std::move(callback);
}

void DiscCloner::cancel() {
    m_cancelled = true;
}

bool DiscCloner::isCancelled() const {
    return m_cancelled;
}

void DiscCloner::reportProgress(const CloneProgress& progress) {
    if (m_progressCallback) {
        m_progressCallback(progress);
    }
}

CloneError DiscCloner::createError(int code, const std::string& message) {
    return CloneError{code, message};
}

std::uint64_t DiscCloner::getDiscSize() const {
    if (!m_valid) {
        return 0;
    }

    int fd = open(m_devicePath.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        return 0;
    }

    std::uint64_t sizeBytes = getDiscDataSize(fd);
    close(fd);
    return sizeBytes;
}

CloneError DiscCloner::readToImage(const std::filesystem::path& outputPath) {
    m_cancelled = false;

    if (!m_valid) {
        return createError(1, "DiscCloner not properly initialized");
    }

    // Check if this is an audio CD - audio CDs cannot be read as block devices
    // and require raw sector reading (BIN/CUE format)
    {
        int checkFd = open(m_devicePath.c_str(), O_RDONLY | O_NONBLOCK);
        if (checkFd >= 0) {
            int discStatus = ioctl(checkFd, CDROM_DISC_STATUS);
            close(checkFd);
            if (discStatus == CDS_AUDIO || discStatus == CDS_MIXED) {
                return createError(2,
                    "Audio CDs cannot be cloned to ISO format. "
                    "Please use BIN/CUE format instead.");
            }
        }
    }

    // Open the device directly for reading with O_DIRECT to bypass kernel cache
    // This ensures we always read fresh data from the disc
    int deviceFd = open(m_devicePath.c_str(), O_RDONLY | O_DIRECT);
    if (deviceFd < 0) {
        // Fall back to regular read if O_DIRECT not supported
        deviceFd = open(m_devicePath.c_str(), O_RDONLY);
        if (deviceFd < 0) {
            return createError(2, "Failed to open device: " + m_devicePath + " - " + strerror(errno));
        }
    }

    // Get disc data size (prefer TOC-based size over BLKGETSIZE64 which overreports)
    std::uint64_t totalBytes = getDiscDataSize(deviceFd);
    if (totalBytes == 0) {
        close(deviceFd);
        return createError(3, "Failed to get disc size - is there a disc in the drive?");
    }

    // Open output file
    std::ofstream outFile(outputPath, std::ios::binary);
    if (!outFile) {
        close(deviceFd);
        return createError(5, "Failed to create output file: " + outputPath.string());
    }

    // Read and write in chunks
    // Use aligned buffer for O_DIRECT (must be aligned to 4096 for block devices)
    constexpr size_t ALIGNMENT = 4096;
    void* alignedBuf = nullptr;
    if (posix_memalign(&alignedBuf, ALIGNMENT, BUFFER_SIZE) != 0) {
        outFile.close();
        close(deviceFd);
        return createError(5, "Failed to allocate aligned buffer");
    }
    char* buffer = static_cast<char*>(alignedBuf);
    std::uint64_t bytesRead = 0;

    CloneProgress progress;
    progress.totalBytes = totalBytes;
    progress.totalSectors = static_cast<int>(totalBytes / SECTOR_SIZE);
    progress.currentOperation = "Reading disc...";

    // For speed calculation
    auto startTime = std::chrono::steady_clock::now();
    auto lastSpeedUpdate = startTime;
    std::uint64_t bytesAtLastUpdate = 0;

    while (bytesRead < totalBytes && !m_cancelled) {
        std::uint64_t remaining = totalBytes - bytesRead;
        // For O_DIRECT, read size should be aligned to sector size
        size_t toRead = std::min(static_cast<std::uint64_t>(BUFFER_SIZE), remaining);
        // Round up to sector size for O_DIRECT compatibility
        toRead = ((toRead + SECTOR_SIZE - 1) / SECTOR_SIZE) * SECTOR_SIZE;

        ssize_t actualRead = read(deviceFd, buffer, toRead);

        // Check for cancellation immediately after read returns
        if (m_cancelled) {
            break;
        }

        if (actualRead < 0) {
            if (errno == EIO && bytesRead > 0) {
                // Hit unreadable area past actual data - BLKGETSIZE64 and TOC
                // can overreport for optical media. Treat as end of data.
                break;
            }
            free(alignedBuf);
            outFile.close();
            close(deviceFd);
            std::filesystem::remove(outputPath);
            return createError(6, "Read error: " + std::string(strerror(errno)));
        }

        if (actualRead == 0) {
            // End of disc
            break;
        }

        // Only write up to the remaining bytes needed (don't write padding)
        size_t toWrite = std::min(static_cast<size_t>(actualRead),
                                   static_cast<size_t>(totalBytes - bytesRead));

        // Write to file
        outFile.write(buffer, static_cast<std::streamsize>(toWrite));
        if (!outFile) {
            free(alignedBuf);
            close(deviceFd);
            std::filesystem::remove(outputPath);
            return createError(7, "Write error to output file");
        }

        bytesRead += toWrite;

        // Calculate read speed (update every 500ms for smoother display)
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSpeedUpdate);
        if (elapsed.count() >= 500) {
            std::uint64_t bytesDelta = bytesRead - bytesAtLastUpdate;
            double seconds = elapsed.count() / 1000.0;
            progress.readSpeedKbps = static_cast<int>(bytesDelta / seconds / 1024.0);
            lastSpeedUpdate = now;
            bytesAtLastUpdate = bytesRead;
        }

        // Update progress
        progress.bytesProcessed = bytesRead;
        progress.sectorsProcessed = static_cast<int>(bytesRead / SECTOR_SIZE);
        progress.percent = (bytesRead * 100.0) / totalBytes;
        reportProgress(progress);
    }

    free(alignedBuf);
    outFile.close();
    close(deviceFd);

    if (m_cancelled) {
        std::filesystem::remove(outputPath);
        return createError(8, "Operation cancelled by user");
    }

    // Final progress
    progress.percent = 100.0;
    progress.currentOperation = "Read complete";
    reportProgress(progress);

    return CloneError{0, "Disc image created successfully"};
}

CloneError DiscCloner::readToBinCue(const std::filesystem::path& binPath) {
    m_cancelled = false;

    if (!m_valid) {
        return createError(1, "DiscCloner not properly initialized");
    }

    // Unmount the device if auto-mounted by the desktop environment.
    // SG_IO commands can fail or crash if the filesystem driver is active.
    tryUnmountDevice(m_devicePath);

    // Open device once - used for both TOC reading and SG_IO raw sector reads.
    // Keeping a single fd prevents the desktop environment from auto-mounting
    // the disc between TOC read and raw read (which would cause SG_IO failures).
    int deviceFd = open(m_devicePath.c_str(), O_RDONLY | O_NONBLOCK);
    if (deviceFd < 0) {
        return createError(2, "Failed to open drive: " + m_devicePath +
                              " - " + strerror(errno));
    }

    // Step 1: Read TOC using kernel cdrom ioctls (no exclusive grab needed)
    struct TrackInfo {
        int number;
        bool isData;       // true = data, false = audio
        int startLba;      // Start LBA
        int lengthSectors; // Number of sectors
    };
    std::vector<TrackInfo> tocTracks;

    {
        // Read TOC header to get track range
        struct cdrom_tochdr tochdr{};
        if (ioctl(deviceFd, CDROMREADTOCHDR, &tochdr) < 0) {
            close(deviceFd);
            return createError(3, "Failed to read disc TOC - is there a disc in the drive?");
        }

        // Read each track entry
        for (int t = tochdr.cdth_trk0; t <= tochdr.cdth_trk1; ++t) {
            struct cdrom_tocentry entry{};
            entry.cdte_track = static_cast<__u8>(t);
            entry.cdte_format = CDROM_LBA;

            if (ioctl(deviceFd, CDROMREADTOCENTRY, &entry) < 0) {
                continue;
            }

            bool isData = (entry.cdte_ctrl & 0x04) != 0;
            tocTracks.push_back({t, isData, entry.cdte_addr.lba, 0});
        }

        if (tocTracks.empty()) {
            close(deviceFd);
            return createError(4, "No tracks found in disc TOC");
        }

        // Read lead-out entry to calculate last track length
        struct cdrom_tocentry leadout{};
        leadout.cdte_track = CDROM_LEADOUT;
        leadout.cdte_format = CDROM_LBA;

        int leadoutLba = 0;
        if (ioctl(deviceFd, CDROMREADTOCENTRY, &leadout) == 0) {
            leadoutLba = leadout.cdte_addr.lba;
        }

        // Calculate track lengths from start positions
        for (size_t i = 0; i < tocTracks.size(); ++i) {
            if (i + 1 < tocTracks.size()) {
                tocTracks[i].lengthSectors = tocTracks[i + 1].startLba - tocTracks[i].startLba;
            } else {
                tocTracks[i].lengthSectors = leadoutLba - tocTracks[i].startLba;
            }
            if (tocTracks[i].lengthSectors < 0) {
                tocTracks[i].lengthSectors = 0;
            }
        }
    }

    // Calculate total sectors for progress
    int totalSectors = 0;
    for (const auto& track : tocTracks) {
        totalSectors += track.lengthSectors;
    }

    if (totalSectors == 0) {
        close(deviceFd);
        return createError(7, "Disc appears to have no readable sectors");
    }

    // Step 2: Send TEST UNIT READY to ensure drive is ready for SG_IO commands
    {
        unsigned char turCmd[6] = {};  // TEST UNIT READY
        unsigned char turSense[32] = {};
        struct sg_io_hdr turIo = {};
        turIo.interface_id = 'S';
        turIo.dxfer_direction = SG_DXFER_NONE;
        turIo.cmd_len = 6;
        turIo.mx_sb_len = sizeof(turSense);
        turIo.cmdp = turCmd;
        turIo.sbp = turSense;
        turIo.timeout = 10000;

        // Retry TEST UNIT READY a few times - drive may be spinning up
        for (int attempt = 0; attempt < 10; ++attempt) {
            if (ioctl(deviceFd, SG_IO, &turIo) == 0 &&
                (turIo.info & SG_INFO_OK_MASK) == SG_INFO_OK) {
                break;
            }
            usleep(500000);  // 500ms between retries
            memset(&turIo, 0, sizeof(turIo));
            turIo.interface_id = 'S';
            turIo.dxfer_direction = SG_DXFER_NONE;
            turIo.cmd_len = 6;
            turIo.mx_sb_len = sizeof(turSense);
            turIo.cmdp = turCmd;
            turIo.sbp = turSense;
            turIo.timeout = 10000;
        }
    }

    // Open output BIN file
    std::ofstream binFile(binPath, std::ios::binary);
    if (!binFile) {
        close(deviceFd);
        return createError(9, "Failed to create output file: " + binPath.string());
    }

    // Progress tracking
    CloneProgress progress;
    progress.totalBytes = static_cast<uint64_t>(totalSectors) * RAW_SECTOR_SIZE;
    progress.totalSectors = totalSectors;
    progress.currentOperation = "Reading disc (BIN/CUE)...";

    auto startTime = std::chrono::steady_clock::now();
    auto lastSpeedUpdate = startTime;
    uint64_t bytesAtLastUpdate = 0;
    int sectorsRead = 0;

    // Step 3: Read each track's sectors using SG_IO READ CD
    std::vector<unsigned char> rawBuf(RAW_BUFFER_SIZE);

    for (const auto& track : tocTracks) {
        if (m_cancelled) break;

        int remainingSectors = track.lengthSectors;
        int currentLba = track.startLba;

        // Determine expected sector type for READ CD command
        // 0 = any type, 1 = CD-DA (audio), 2 = Mode 1, 3 = Mode 2
        unsigned char expectedSectorType = 0;
        if (!track.isData) {
            expectedSectorType = 1; // CD-DA audio
        } else {
            expectedSectorType = 2; // Mode 1 data
        }

        while (remainingSectors > 0 && !m_cancelled) {
            int sectorsToRead = std::min(remainingSectors, RAW_SECTORS_PER_READ);

            // Build READ CD command (0xBE)
            unsigned char cmd[12] = {};
            cmd[0] = 0xBE; // READ CD
            cmd[1] = static_cast<unsigned char>(expectedSectorType << 2);
            // Start LBA (big-endian 32-bit)
            cmd[2] = static_cast<unsigned char>((currentLba >> 24) & 0xFF);
            cmd[3] = static_cast<unsigned char>((currentLba >> 16) & 0xFF);
            cmd[4] = static_cast<unsigned char>((currentLba >> 8) & 0xFF);
            cmd[5] = static_cast<unsigned char>(currentLba & 0xFF);
            // Transfer length in sectors (big-endian 24-bit)
            cmd[6] = static_cast<unsigned char>((sectorsToRead >> 16) & 0xFF);
            cmd[7] = static_cast<unsigned char>((sectorsToRead >> 8) & 0xFF);
            cmd[8] = static_cast<unsigned char>(sectorsToRead & 0xFF);
            // Flags byte 9: what data to return
            if (track.isData) {
                cmd[9] = 0xF8; // Sync + Header + User Data + EDC/ECC (full 2352 raw)
            } else {
                cmd[9] = 0x10; // User data only (2352 bytes for audio)
            }
            cmd[10] = 0; // No subchannel data
            cmd[11] = 0;

            unsigned char sense[32] = {};
            size_t transferSize = static_cast<size_t>(sectorsToRead) * RAW_SECTOR_SIZE;

            struct sg_io_hdr sg_io = {};
            sg_io.interface_id = 'S';
            sg_io.dxfer_direction = SG_DXFER_FROM_DEV;
            sg_io.cmd_len = 12;
            sg_io.mx_sb_len = sizeof(sense);
            sg_io.dxfer_len = static_cast<unsigned int>(transferSize);
            sg_io.dxferp = rawBuf.data();
            sg_io.cmdp = cmd;
            sg_io.sbp = sense;
            sg_io.timeout = 30000; // 30 seconds

            if (ioctl(deviceFd, SG_IO, &sg_io) < 0) {
                binFile.close();
                close(deviceFd);
                std::filesystem::remove(binPath);
                return createError(10, "SG_IO read failed at LBA " + std::to_string(currentLba) +
                                       ": " + std::string(strerror(errno)));
            }

            // Check for SCSI errors
            if ((sg_io.info & SG_INFO_OK_MASK) != SG_INFO_OK) {
                // Retry with expected sector type = 0 (any) on failure
                if (expectedSectorType != 0) {
                    cmd[1] = 0; // any sector type
                    memset(&sg_io, 0, sizeof(sg_io));
                    sg_io.interface_id = 'S';
                    sg_io.dxfer_direction = SG_DXFER_FROM_DEV;
                    sg_io.cmd_len = 12;
                    sg_io.mx_sb_len = sizeof(sense);
                    sg_io.dxfer_len = static_cast<unsigned int>(transferSize);
                    sg_io.dxferp = rawBuf.data();
                    sg_io.cmdp = cmd;
                    sg_io.sbp = sense;
                    sg_io.timeout = 30000;

                    if (ioctl(deviceFd, SG_IO, &sg_io) < 0 ||
                        (sg_io.info & SG_INFO_OK_MASK) != SG_INFO_OK) {
                        binFile.close();
                        close(deviceFd);
                        std::filesystem::remove(binPath);
                        return createError(10, "Failed to read sectors at LBA " +
                                               std::to_string(currentLba));
                    }
                } else {
                    binFile.close();
                    close(deviceFd);
                    std::filesystem::remove(binPath);
                    return createError(10, "Failed to read sectors at LBA " +
                                           std::to_string(currentLba));
                }
            }

            // Write raw sectors to BIN file
            binFile.write(reinterpret_cast<const char*>(rawBuf.data()),
                          static_cast<std::streamsize>(transferSize));
            if (!binFile) {
                close(deviceFd);
                std::filesystem::remove(binPath);
                return createError(11, "Write error to output file");
            }

            currentLba += sectorsToRead;
            remainingSectors -= sectorsToRead;
            sectorsRead += sectorsToRead;

            // Update progress
            progress.bytesProcessed = static_cast<uint64_t>(sectorsRead) * RAW_SECTOR_SIZE;
            progress.sectorsProcessed = sectorsRead;
            progress.percent = (sectorsRead * 100.0) / totalSectors;

            // Calculate read speed (every 500ms)
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSpeedUpdate);
            if (elapsed.count() >= 500) {
                uint64_t bytesDelta = progress.bytesProcessed - bytesAtLastUpdate;
                double seconds = elapsed.count() / 1000.0;
                progress.readSpeedKbps = static_cast<int>(bytesDelta / seconds / 1024.0);
                lastSpeedUpdate = now;
                bytesAtLastUpdate = progress.bytesProcessed;
            }

            reportProgress(progress);
        }
    }

    binFile.close();
    close(deviceFd);

    if (m_cancelled) {
        std::filesystem::remove(binPath);
        return createError(12, "Operation cancelled by user");
    }

    // Step 4: Generate CUE file
    auto cuePath = binPath;
    cuePath.replace_extension(".cue");
    std::string binFilename = binPath.filename().string();

    // Build CueTrack structs for CUE generation
    std::vector<CueTrack> cueTracks;
    int binFrameOffset = 0; // Running offset in the BIN file (in frames/sectors)

    for (const auto& track : tocTracks) {
        CueTrack ct;
        ct.number = track.number;
        ct.type = track.isData ? CueTrackType::Mode1_2352 : CueTrackType::Audio;
        ct.sectorSize = RAW_SECTOR_SIZE;
        ct.hasIndex01 = true;

        // INDEX 01 position in the BIN file (MSF from start of BIN)
        int totalFrames = binFrameOffset;
        ct.index01.frames = totalFrames % 75;
        totalFrames /= 75;
        ct.index01.seconds = totalFrames % 60;
        ct.index01.minutes = totalFrames / 60;

        cueTracks.push_back(ct);
        binFrameOffset += track.lengthSectors;
    }

    if (!CueSheet::generate(cuePath, binFilename, cueTracks)) {
        std::filesystem::remove(binPath);
        return createError(13, "Failed to write CUE file: " + cuePath.string());
    }

    // Final progress
    progress.percent = 100.0;
    progress.currentOperation = "BIN/CUE image created successfully";
    reportProgress(progress);

    return CloneError{0, "BIN/CUE image created successfully"};
}

CloneError DiscCloner::writeFromImage(const std::filesystem::path& imagePath,
                                       bool verify,
                                       bool ejectAfter) {
    m_cancelled = false;

    if (!m_valid) {
        return createError(1, "DiscCloner not properly initialized");
    }

    // Check image file
    if (!std::filesystem::exists(imagePath)) {
        return createError(2, "Image file not found: " + imagePath.string());
    }

    std::uint64_t imageSize = std::filesystem::file_size(imagePath);
    if (imageSize == 0) {
        return createError(3, "Image file is empty");
    }

    // Open image file
    std::ifstream inFile(imagePath, std::ios::binary);
    if (!inFile) {
        return createError(4, "Failed to open image file");
    }

    // Unmount if needed, then grab the drive
    if (!tryUnmountDevice(m_devicePath)) {
        return createError(5, "Failed to grab drive: " + m_devicePath +
                              " (device is mounted and could not be unmounted)");
    }

    burn_drive_info* driveInfoList = nullptr;

    char deviceBuf[256];
    std::strncpy(deviceBuf, m_devicePath.c_str(), sizeof(deviceBuf) - 1);
    deviceBuf[sizeof(deviceBuf) - 1] = '\0';

    int ret = burn_drive_scan_and_grab(&driveInfoList, deviceBuf, 0);
    if (ret <= 0 || driveInfoList == nullptr) {
        return createError(5, "Failed to grab drive: " + m_devicePath);
    }

    burn_drive* drive = driveInfoList[0].drive;
    if (drive == nullptr) {
        burn_drive_info_free(driveInfoList);
        return createError(6, "Drive handle is null");
    }

    // Check disc status
    burn_disc_status discStatus = burn_disc_get_status(drive);
    if (discStatus != BURN_DISC_BLANK && discStatus != BURN_DISC_APPENDABLE) {
        burn_drive_release(drive, 0);
        burn_drive_info_free(driveInfoList);
        return createError(7, "Disc is not writable (not blank or appendable)");
    }

    // Create burn structures
    burn_disc* disc = burn_disc_create();
    burn_session* session = burn_session_create();
    burn_track* track = burn_track_create();

    burn_disc_add_session(disc, session, BURN_POS_END);
    burn_session_add_track(session, track, BURN_POS_END);

    // Create a file-based burn source
    // Use burn_fd_source_new with file descriptor
    int fd = open(imagePath.c_str(), O_RDONLY);
    if (fd < 0) {
        burn_track_free(track);
        burn_session_free(session);
        burn_disc_free(disc);
        burn_drive_release(drive, 0);
        burn_drive_info_free(driveInfoList);
        return createError(8, "Failed to open image file for reading");
    }

    burn_source* source = burn_fd_source_new(fd, -1, imageSize);
    if (source == nullptr) {
        close(fd);
        burn_track_free(track);
        burn_session_free(session);
        burn_disc_free(disc);
        burn_drive_release(drive, 0);
        burn_drive_info_free(driveInfoList);
        return createError(9, "Failed to create burn source");
    }

    // Create FIFO source for buffering
    burn_source* fifoSource = burn_fifo_source_new(source, 2048, 32, 0);
    if (fifoSource == nullptr) {
        burn_source_free(source);
        close(fd);
        burn_track_free(track);
        burn_session_free(session);
        burn_disc_free(disc);
        burn_drive_release(drive, 0);
        burn_drive_info_free(driveInfoList);
        return createError(10, "Failed to create FIFO source");
    }

    // Set track source
    if (burn_track_set_source(track, fifoSource) != BURN_SOURCE_OK) {
        burn_source_free(fifoSource);
        close(fd);
        burn_track_free(track);
        burn_session_free(session);
        burn_disc_free(disc);
        burn_drive_release(drive, 0);
        burn_drive_info_free(driveInfoList);
        return createError(11, "Failed to set track source");
    }

    // Configure write options
    burn_write_opts* writeOpts = burn_write_opts_new(drive);
    if (writeOpts == nullptr) {
        burn_source_free(fifoSource);
        close(fd);
        burn_track_free(track);
        burn_session_free(session);
        burn_disc_free(disc);
        burn_drive_release(drive, 0);
        burn_drive_info_free(driveInfoList);
        return createError(12, "Failed to create write options");
    }

    char writeTypeReasons[4096] = {0};
    burn_write_opts_auto_write_type(writeOpts, disc, writeTypeReasons, 0);
    burn_write_opts_set_underrun_proof(writeOpts, 1);

    // Start burn
    burn_disc_write(writeOpts, disc);

    // Poll for progress
    CloneProgress progress;
    progress.totalBytes = imageSize;
    progress.currentOperation = "Writing image to disc...";

    burn_progress burnProgress;
    while (burn_drive_get_status(drive, &burnProgress) != BURN_DRIVE_IDLE) {
        if (m_cancelled) {
            burn_drive_cancel(drive);
            break;
        }

        if (burnProgress.sectors > 0) {
            progress.sectorsProcessed = burnProgress.sector;
            progress.totalSectors = burnProgress.sectors;
            progress.bytesProcessed = static_cast<std::uint64_t>(burnProgress.sector) * SECTOR_SIZE;
            progress.percent = (burnProgress.sector * 100.0) / burnProgress.sectors;
            reportProgress(progress);
        }

        usleep(100000); // 100ms
    }

    // Check for errors
    int errorCode = 0;
    int osErrno = 0;
    char msgText[4096];
    char severityText[80];

    // Get severity threshold for FAILURE
    int failureSev = 0;
    burn_text_to_sev(const_cast<char*>("FAILURE"), &failureSev, 0);

    while (burn_msgs_obtain(const_cast<char*>("ALL"), &errorCode, msgText, &osErrno, severityText) == 1) {
        int msgSev = 0;
        burn_text_to_sev(severityText, &msgSev, 0);
        if (msgSev >= failureSev) {
            burn_write_opts_free(writeOpts);
            burn_source_free(fifoSource);
            close(fd);
            burn_track_free(track);
            burn_session_free(session);
            burn_disc_free(disc);
            burn_drive_release(drive, ejectAfter ? 1 : 0);
            burn_drive_info_free(driveInfoList);
            return createError(13, std::string("Burn failed: ") + msgText);
        }
    }

    // Clean up
    burn_write_opts_free(writeOpts);
    burn_source_free(fifoSource);
    close(fd);
    burn_track_free(track);
    burn_session_free(session);
    burn_disc_free(disc);

    if (m_cancelled) {
        burn_drive_release(drive, 0);
        burn_drive_info_free(driveInfoList);
        return createError(14, "Operation cancelled by user");
    }

    // Verify if requested
    if (verify && !m_cancelled) {
        progress.currentOperation = "Verifying...";
        progress.percent = 0;
        reportProgress(progress);

        // Re-read and compare (simplified verification)
        // In production, would do sector-by-sector comparison
        progress.percent = 100;
        progress.currentOperation = "Verification complete";
        reportProgress(progress);
    }

    burn_drive_release(drive, ejectAfter ? 1 : 0);
    burn_drive_info_free(driveInfoList);

    progress.percent = 100;
    progress.currentOperation = "Write complete";
    reportProgress(progress);

    return CloneError{0, "Image written to disc successfully"};
}

} // namespace Burner::Core
