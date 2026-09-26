#include "CdReader.hpp"

#include <fstream>
#include <cstring>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/cdrom.h>
#include <scsi/sg.h>

// For SHA-1 calculation (MusicBrainz disc ID)
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

namespace Burner::Core {

class CdReader::Impl {
public:
    CdToc cachedToc;
    bool tocRead{false};
    int deviceFd{-1};
};

CdReader::CdReader(const std::string& devicePath)
    : m_devicePath(devicePath)
    , m_impl(std::make_unique<Impl>()) {
}

CdReader::~CdReader() {
    if (m_impl && m_impl->deviceFd >= 0) {
        close(m_impl->deviceFd);
    }
}

CdReader::CdReader(CdReader&&) noexcept = default;
CdReader& CdReader::operator=(CdReader&&) noexcept = default;

bool CdReader::isValid() const {
    return !m_devicePath.empty();
}

bool CdReader::hasAudioDisc() const {
    if (!isValid()) {
        return false;
    }

    int fd = open(m_devicePath.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        return false;
    }

    // Check drive status
    int status = ioctl(fd, CDROM_DRIVE_STATUS, CDSL_CURRENT);
    if (status != CDS_DISC_OK) {
        close(fd);
        return false;
    }

    // Check disc type
    int discType = ioctl(fd, CDROM_DISC_STATUS, CDSL_CURRENT);
    close(fd);

    // CDS_AUDIO means audio CD, CDS_MIXED means mixed mode
    return discType == CDS_AUDIO || discType == CDS_MIXED;
}

std::optional<CdToc> CdReader::readToc() {
    if (!isValid()) {
        m_lastError = "Invalid device path";
        return std::nullopt;
    }

    if (m_impl->tocRead) {
        return m_impl->cachedToc;
    }

    int fd = open(m_devicePath.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        m_lastError = "Failed to open device: " + std::string(strerror(errno));
        return std::nullopt;
    }

    // Read TOC header to get number of tracks
    struct cdrom_tochdr tocHeader;
    if (ioctl(fd, CDROMREADTOCHDR, &tocHeader) < 0) {
        close(fd);
        m_lastError = "Failed to read TOC header: " + std::string(strerror(errno));
        return std::nullopt;
    }

    CdToc toc;
    int firstTrack = tocHeader.cdth_trk0;
    int lastTrack = tocHeader.cdth_trk1;

    // Read each track entry
    bool hasAudioTracks = false;

    for (int trackNum = firstTrack; trackNum <= lastTrack; ++trackNum) {
        struct cdrom_tocentry tocEntry;
        tocEntry.cdte_track = trackNum;
        tocEntry.cdte_format = CDROM_LBA;  // Use LBA addressing

        if (ioctl(fd, CDROMREADTOCENTRY, &tocEntry) < 0) {
            continue;  // Skip tracks we can't read
        }

        CdTrackInfo trackInfo;
        trackInfo.trackNumber = trackNum;
        trackInfo.startSector = tocEntry.cdte_addr.lba;
        trackInfo.isAudio = (tocEntry.cdte_ctrl & CDROM_DATA_TRACK) == 0;

        if (trackInfo.isAudio) {
            hasAudioTracks = true;
        }

        toc.tracks.push_back(trackInfo);
    }

    if (!hasAudioTracks) {
        close(fd);
        m_lastError = "No audio tracks on disc";
        return std::nullopt;
    }

    // Read lead-out entry (track 0xAA)
    struct cdrom_tocentry leadOutEntry;
    leadOutEntry.cdte_track = CDROM_LEADOUT;
    leadOutEntry.cdte_format = CDROM_LBA;

    if (ioctl(fd, CDROMREADTOCENTRY, &leadOutEntry) >= 0) {
        toc.leadOutSector = leadOutEntry.cdte_addr.lba;
    }

    close(fd);

    // Calculate sector counts and durations
    for (size_t i = 0; i < toc.tracks.size(); ++i) {
        int nextStart;
        if (i + 1 < toc.tracks.size()) {
            nextStart = toc.tracks[i + 1].startSector;
        } else {
            nextStart = toc.leadOutSector;
        }
        toc.tracks[i].sectorCount = nextStart - toc.tracks[i].startSector;
        toc.tracks[i].durationMs = (toc.tracks[i].sectorCount * 1000) / CdDaSpec::SECTORS_PER_SECOND;
    }

    toc.totalSectors = toc.leadOutSector;

    // Try to read CD-TEXT
    readCdText(toc);

    // Calculate disc ID
    toc.discId = calculateDiscId(toc);

    m_impl->cachedToc = toc;
    m_impl->tocRead = true;

    return toc;
}

bool CdReader::readCdText(CdToc& toc) {
    int fd = open(m_devicePath.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        return false;
    }

    // CD-TEXT is read using the READ TOC/PMA/ATIP command with format 5
    // This requires sending a raw SCSI command

    // Allocate buffer for CD-TEXT data (max ~32KB)
    constexpr size_t MAX_CDTEXT_SIZE = 32768;
    std::vector<unsigned char> buffer(MAX_CDTEXT_SIZE);

    // Build SCSI command (READ TOC/PMA/ATIP, format 5 = CD-TEXT)
    unsigned char cmd[10] = {0};
    cmd[0] = 0x43;                      // READ TOC/PMA/ATIP
    cmd[1] = 0x02;                      // MSF format
    cmd[2] = 5;                         // Format 5 = CD-TEXT
    cmd[7] = (MAX_CDTEXT_SIZE >> 8) & 0xFF;
    cmd[8] = MAX_CDTEXT_SIZE & 0xFF;

    struct sg_io_hdr sgio;
    unsigned char sense[32];
    memset(&sgio, 0, sizeof(sgio));
    memset(sense, 0, sizeof(sense));

    sgio.interface_id = 'S';
    sgio.cmd_len = 10;
    sgio.mx_sb_len = sizeof(sense);
    sgio.dxfer_direction = SG_DXFER_FROM_DEV;
    sgio.dxfer_len = MAX_CDTEXT_SIZE;
    sgio.dxferp = buffer.data();
    sgio.cmdp = cmd;
    sgio.sbp = sense;
    sgio.timeout = 30000;  // 30 seconds

    if (ioctl(fd, SG_IO, &sgio) < 0) {
        close(fd);
        return false;
    }

    close(fd);

    // Check for success
    if (sgio.status != 0 || sgio.host_status != 0 || sgio.driver_status != 0) {
        return false;
    }

    // Parse CD-TEXT data
    // The data starts with a 4-byte header: length (2 bytes), reserved (2 bytes)
    if (buffer[0] == 0 && buffer[1] == 0) {
        return false;  // No CD-TEXT
    }

    int dataLen = (buffer[0] << 8) | buffer[1];
    if (dataLen < 4) {
        return false;
    }

    // CD-TEXT packs start at offset 4
    // Each pack is 18 bytes: pack type, track, seq, char pos, block info, data[12], crc[2]
    // Pack types: 0x80 = title, 0x81 = performer, 0x82 = songwriter, etc.

    std::string albumTitle;
    std::string albumArtist;
    std::vector<std::string> trackTitles(toc.tracks.size());
    std::vector<std::string> trackArtists(toc.tracks.size());

    for (int offset = 4; offset + 18 <= 4 + dataLen; offset += 18) {
        unsigned char* pack = buffer.data() + offset;
        int packType = pack[0];
        int trackNum = pack[1];
        // Sequence and position info in pack[2] and pack[3]

        // Only process text packs (types 0x80-0x8F)
        if (packType < 0x80 || packType > 0x8F) {
            continue;
        }

        // Extract text (12 bytes of data starting at offset 4)
        std::string text;
        for (int i = 4; i < 16; ++i) {
            if (pack[i] == 0) break;
            text += static_cast<char>(pack[i]);
        }

        if (text.empty()) {
            continue;
        }

        if (packType == 0x80) {  // Title
            if (trackNum == 0) {
                albumTitle += text;
            } else if (trackNum <= static_cast<int>(trackTitles.size())) {
                trackTitles[trackNum - 1] += text;
            }
        } else if (packType == 0x81) {  // Performer
            if (trackNum == 0) {
                albumArtist += text;
            } else if (trackNum <= static_cast<int>(trackArtists.size())) {
                trackArtists[trackNum - 1] += text;
            }
        }
    }

    // Apply parsed data to TOC
    toc.albumTitle = albumTitle;
    toc.albumArtist = albumArtist;

    for (size_t i = 0; i < toc.tracks.size() && i < trackTitles.size(); ++i) {
        toc.tracks[i].title = trackTitles[i];
        toc.tracks[i].performer = trackArtists[i];
    }

    return !albumTitle.empty() || !albumArtist.empty();
}

std::string CdReader::calculateDiscId(const CdToc& toc) {
    if (toc.tracks.empty()) {
        return {};
    }

    // MusicBrainz disc ID calculation:
    // SHA-1 of ASCII hex string containing:
    // - First track number (2 hex digits)
    // - Last track number (2 hex digits)
    // - 100 frame offsets (8 hex digits each):
    //   - offset[0] = lead-out
    //   - offset[1] = track 1
    //   - offset[2] = track 2
    //   - ... offset[99] = track 99
    // Total: 2 + 2 + 800 = 804 characters
    //
    // Note: All offsets include the 150-sector lead-in adjustment

    // Build 804-character hex string
    std::string tocString;
    tocString.reserve(804);

    // Get actual first and last track numbers from TOC
    int firstTrack = toc.tracks.front().trackNumber;
    int lastTrack = toc.tracks.back().trackNumber;

    // First track number (2 hex digits)
    char buf[16];
    snprintf(buf, sizeof(buf), "%02X", firstTrack);
    tocString += buf;

    // Last track number (2 hex digits)
    snprintf(buf, sizeof(buf), "%02X", lastTrack);
    tocString += buf;

    // 100 frame offsets
    // offset[0] = lead-out
    int leadOutOffset = toc.leadOutSector + 150;
    snprintf(buf, sizeof(buf), "%08X", leadOutOffset);
    tocString += buf;

    // offset[1..99] = track 1..99
    for (int trackNum = 1; trackNum <= 99; ++trackNum) {
        int offset = 0;
        // Find track with this number
        for (const auto& track : toc.tracks) {
            if (track.trackNumber == trackNum) {
                offset = track.startSector + 150;  // Add 150 for lead-in
                break;
            }
        }
        snprintf(buf, sizeof(buf), "%08X", offset);
        tocString += buf;
    }

    // Calculate SHA-1 of ASCII hex string
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(tocString.c_str()), tocString.length(), hash);

    // Base64 encode the hash (with MusicBrainz-specific alphabet)
    BIO* bio = BIO_new(BIO_s_mem());
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_push(b64, bio);
    BIO_write(bio, hash, SHA_DIGEST_LENGTH);
    BIO_flush(bio);

    BUF_MEM* bufPtr;
    BIO_get_mem_ptr(bio, &bufPtr);

    std::string base64(bufPtr->data, bufPtr->length);
    BIO_free_all(bio);

    // Apply MusicBrainz alphabet substitutions: + -> .  / -> _  = -> -
    for (char& c : base64) {
        if (c == '+') c = '.';
        else if (c == '/') c = '_';
        else if (c == '=') c = '-';
    }

    return base64;
}

bool CdReader::extractTrack(int trackNumber,
                            const std::filesystem::path& outputPath,
                            ProgressCallback progress) {

    std::vector<std::uint8_t> data = extractTrackToBuffer(trackNumber, progress);
    if (data.empty()) {
        return false;
    }

    std::ofstream file(outputPath, std::ios::binary);
    if (!file) {
        m_lastError = "Failed to open output file: " + outputPath.string();
        return false;
    }

    file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return file.good();
}

std::vector<std::uint8_t> CdReader::extractTrackToBuffer(int trackNumber,
                                                          ProgressCallback progress) {
    m_cancelled = false;

    if (!isValid()) {
        m_lastError = "Invalid drive handle";
        return {};
    }

    // Ensure TOC is read
    auto tocOpt = readToc();
    if (!tocOpt) {
        return {};
    }

    const CdToc& toc = *tocOpt;

    // Find the track
    const CdTrackInfo* trackInfo = nullptr;
    for (const auto& t : toc.tracks) {
        if (t.trackNumber == trackNumber) {
            trackInfo = &t;
            break;
        }
    }

    if (!trackInfo) {
        m_lastError = "Track " + std::to_string(trackNumber) + " not found";
        return {};
    }

    if (!trackInfo->isAudio) {
        m_lastError = "Track " + std::to_string(trackNumber) + " is not an audio track";
        return {};
    }

    // Open device for raw reading
    int fd = open(m_devicePath.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        m_lastError = "Failed to open device: " + std::string(strerror(errno));
        return {};
    }

    // Allocate buffer for the entire track
    std::vector<std::uint8_t> buffer;
    size_t totalSize = static_cast<size_t>(trackInfo->sectorCount) * CdDaSpec::SECTOR_SIZE;
    buffer.resize(totalSize);

    // Read audio sectors using READ CD command (SCSI opcode 0xBE)
    int sectorsRead = 0;
    int startSector = trackInfo->startSector;
    int totalSectors = trackInfo->sectorCount;

    // Read in chunks for progress reporting and cancellation
    constexpr int CHUNK_SECTORS = 27;  // ~3 chunks per second of audio

    while (sectorsRead < totalSectors && !m_cancelled) {
        int sectorsToRead = std::min(CHUNK_SECTORS, totalSectors - sectorsRead);

        // Calculate buffer offset
        size_t offset = static_cast<size_t>(sectorsRead) * CdDaSpec::SECTOR_SIZE;

        // Build READ CD command
        unsigned char cmd[12] = {0};
        int lba = startSector + sectorsRead;

        cmd[0] = 0xBE;                          // READ CD
        cmd[1] = 0x04;                          // Expected sector type: CD-DA
        cmd[2] = (lba >> 24) & 0xFF;            // LBA (MSB)
        cmd[3] = (lba >> 16) & 0xFF;
        cmd[4] = (lba >> 8) & 0xFF;
        cmd[5] = lba & 0xFF;                    // LBA (LSB)
        cmd[6] = (sectorsToRead >> 16) & 0xFF;  // Transfer length (MSB)
        cmd[7] = (sectorsToRead >> 8) & 0xFF;
        cmd[8] = sectorsToRead & 0xFF;          // Transfer length (LSB)
        cmd[9] = 0x10;                          // User data only (2352 bytes per sector)

        struct sg_io_hdr sgio;
        unsigned char sense[32];
        memset(&sgio, 0, sizeof(sgio));
        memset(sense, 0, sizeof(sense));

        size_t xferLen = static_cast<size_t>(sectorsToRead) * CdDaSpec::SECTOR_SIZE;

        sgio.interface_id = 'S';
        sgio.cmd_len = 12;
        sgio.mx_sb_len = sizeof(sense);
        sgio.dxfer_direction = SG_DXFER_FROM_DEV;
        sgio.dxfer_len = static_cast<unsigned int>(xferLen);
        sgio.dxferp = buffer.data() + offset;
        sgio.cmdp = cmd;
        sgio.sbp = sense;
        sgio.timeout = 30000;

        if (ioctl(fd, SG_IO, &sgio) < 0) {
            close(fd);
            m_lastError = "SCSI read failed at sector " + std::to_string(lba) +
                          ": " + std::string(strerror(errno));
            return {};
        }

        if (sgio.status != 0) {
            close(fd);
            m_lastError = "SCSI read error at sector " + std::to_string(lba);
            return {};
        }

        sectorsRead += sectorsToRead;

        if (progress) {
            progress(sectorsRead, totalSectors);
        }
    }

    close(fd);

    if (m_cancelled) {
        m_lastError = "Operation cancelled";
        return {};
    }

    return buffer;
}

void CdReader::cancel() {
    m_cancelled = true;
}

bool CdReader::isCancelled() const {
    return m_cancelled;
}

} // namespace Burner::Core
