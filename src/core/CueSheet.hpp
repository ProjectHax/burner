#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <cstdint>

namespace Burner::Core {

/// MSF (Minutes, Seconds, Frames) time position on a disc
/// 75 frames per second
struct MsfTime {
    int minutes{0};
    int seconds{0};
    int frames{0};

    /// Convert to total frames (75 frames/sec)
    [[nodiscard]] int totalFrames() const {
        return minutes * 60 * 75 + seconds * 75 + frames;
    }

    /// Convert to byte offset for a given sector size
    [[nodiscard]] std::uint64_t toByteOffset(int sectorSize) const {
        return static_cast<std::uint64_t>(totalFrames()) * sectorSize;
    }
};

/// Track type from CUE sheet
enum class CueTrackType {
    Audio,        // AUDIO - CD-DA, 2352 bytes/sector
    Mode1_2048,   // MODE1/2048 - cooked data, 2048 bytes/sector
    Mode1_2352,   // MODE1/2352 - raw data with headers/ECC, 2352 bytes/sector
    Mode2_2336,   // MODE2/2336
    Mode2_2352,   // MODE2/2352 - raw mode2
    Unknown
};

/// A single track within the CUE sheet
struct CueTrack {
    int number{0};                      ///< Track number (1-based)
    CueTrackType type{CueTrackType::Unknown};
    int sectorSize{0};                  ///< Logical bytes per sector for this track type
    std::string title;                  ///< CD-TEXT title
    std::string performer;              ///< CD-TEXT performer

    MsfTime index00;                    ///< INDEX 00 - pregap start (optional)
    MsfTime index01;                    ///< INDEX 01 - track data start (required)
    bool hasIndex00{false};
    bool hasIndex01{false};

    MsfTime pregap;                     ///< PREGAP directive (silent pregap)
    bool hasPregap{false};

    /// Calculated fields (filled after parsing)
    std::uint64_t dataStartByte{0};     ///< Byte offset in BIN where track data begins
    std::uint64_t dataLengthBytes{0};   ///< Length of track data in bytes
};

/// File reference from CUE sheet
struct CueFile {
    std::filesystem::path path;         ///< Resolved absolute path to the data file
    std::string fileType;               ///< BINARY, MOTOROLA, AIFF, WAVE, MP3
};

/// Parsed CUE sheet representation
class CueSheet {
public:
    CueSheet() = default;
    ~CueSheet() = default;

    CueSheet(const CueSheet&) = delete;
    CueSheet& operator=(const CueSheet&) = delete;
    CueSheet(CueSheet&&) noexcept = default;
    CueSheet& operator=(CueSheet&&) noexcept = default;

    /// Parse a CUE file
    /// @param cuePath Path to the .cue file
    /// @return true if parsing succeeded
    bool parse(const std::filesystem::path& cuePath);

    /// Validate that all referenced files exist and sizes are consistent
    /// @return true if valid
    [[nodiscard]] bool validate() const;

    [[nodiscard]] const std::vector<CueTrack>& tracks() const { return m_tracks; }
    [[nodiscard]] int trackCount() const { return static_cast<int>(m_tracks.size()); }
    [[nodiscard]] const CueFile& file() const { return m_file; }
    [[nodiscard]] const std::filesystem::path& binPath() const { return m_file.path; }
    [[nodiscard]] std::uint64_t totalDataSize() const;
    [[nodiscard]] const std::string& title() const { return m_title; }
    [[nodiscard]] const std::string& performer() const { return m_performer; }

    /// Check if this is a single data track image (no temp files needed)
    [[nodiscard]] bool isSingleDataTrack() const;

    /// Check if all tracks are audio
    [[nodiscard]] bool isAudioDisc() const;

    /// Check if disc has both audio and data tracks
    [[nodiscard]] bool isMixedMode() const;

    /// Sector size used in the BIN file (2352 for BINARY)
    [[nodiscard]] int binSectorSize() const { return m_binSectorSize; }

    [[nodiscard]] const std::string& lastError() const { return m_lastError; }

    /// Generate a CUE file from track information
    /// @param cuePath Output path for the .cue file
    /// @param binFilename Filename of the BIN file (just the name, not full path)
    /// @param tracks Track information to write
    /// @return true if file was written successfully
    static bool generate(const std::filesystem::path& cuePath,
                         const std::string& binFilename,
                         const std::vector<CueTrack>& tracks);

    /// Get CUE track type string (e.g., "AUDIO", "MODE1/2352")
    static std::string trackTypeString(CueTrackType type);

    /// Format MSF time as "MM:SS:FF"
    static std::string formatMsf(const MsfTime& msf);

private:
    static CueTrackType parseTrackType(const std::string& typeStr);
    static int sectorSizeForType(CueTrackType type);
    static MsfTime parseMsf(const std::string& msf);
    static std::string parseQuotedString(const std::string& line, std::size_t startPos);
    static std::string trim(const std::string& s);
    static std::string toUpper(const std::string& s);
    void calculateTrackOffsets(std::uint64_t binFileSize);

    CueFile m_file;
    std::vector<CueTrack> m_tracks;
    std::string m_title;
    std::string m_performer;
    mutable std::string m_lastError;
    int m_binSectorSize{2352};  // For BINARY files, always 2352
};

} // namespace Burner::Core
