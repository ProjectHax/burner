#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <optional>
#include <filesystem>

namespace Burner::Core {

/// Information about a single audio track on a CD
struct CdTrackInfo {
    int trackNumber{0};
    int startSector{0};
    int sectorCount{0};
    int durationMs{0};
    bool isAudio{true};
    std::string title;      ///< From CD-TEXT
    std::string performer;  ///< From CD-TEXT
};

/// Table of contents for an audio CD
struct CdToc {
    std::vector<CdTrackInfo> tracks;
    std::string discId;         ///< MusicBrainz disc ID (SHA-1 based)
    std::string albumTitle;     ///< From CD-TEXT
    std::string albumArtist;    ///< From CD-TEXT
    int totalSectors{0};
    int leadOutSector{0};
};

/// CD-DA audio specifications
struct CdDaSpec {
    static constexpr int SAMPLE_RATE = 44100;
    static constexpr int CHANNELS = 2;
    static constexpr int BITS_PER_SAMPLE = 16;
    static constexpr int BYTES_PER_SAMPLE = 2;
    static constexpr int BYTES_PER_FRAME = CHANNELS * BYTES_PER_SAMPLE;
    static constexpr int BYTES_PER_SECOND = SAMPLE_RATE * BYTES_PER_FRAME;
    static constexpr int SECTOR_SIZE = 2352;              ///< Bytes per CD sector
    static constexpr int SECTORS_PER_SECOND = 75;         ///< CD frame rate
    static constexpr int SAMPLES_PER_SECTOR = 588;        ///< Samples per sector
};

/// Reader for audio CDs using libburn.
/// Handles TOC reading, CD-TEXT extraction, and raw audio extraction.
class CdReader {
public:
    /// Progress callback: (sectorsRead, totalSectors)
    using ProgressCallback = std::function<void(int, int)>;

    /// Construct a CdReader for the specified device
    /// @param devicePath Path to the optical drive (e.g., /dev/sr0)
    explicit CdReader(const std::string& devicePath);
    ~CdReader();

    // Non-copyable
    CdReader(const CdReader&) = delete;
    CdReader& operator=(const CdReader&) = delete;

    // Movable
    CdReader(CdReader&&) noexcept;
    CdReader& operator=(CdReader&&) noexcept;

    /// Check if reader is valid and disc is an audio CD
    [[nodiscard]] bool isValid() const;

    /// Check if there's an audio CD in the drive
    [[nodiscard]] bool hasAudioDisc() const;

    /// Read the table of contents
    /// @return CdToc with track info, or nullopt if no audio CD
    [[nodiscard]] std::optional<CdToc> readToc();

    /// Read CD-TEXT data and update TOC with titles/performers
    /// @param toc TOC to update with CD-TEXT data
    /// @return true if CD-TEXT was found and read
    bool readCdText(CdToc& toc);

    /// Calculate MusicBrainz disc ID from TOC
    /// @param toc Table of contents
    /// @return Base64-encoded disc ID
    [[nodiscard]] static std::string calculateDiscId(const CdToc& toc);

    /// Extract raw audio from a track to file
    /// @param trackNumber 1-based track number
    /// @param outputPath Path to output file (raw PCM, 44.1kHz 16-bit stereo LE)
    /// @param progress Optional progress callback
    /// @return true if successful
    bool extractTrack(int trackNumber,
                      const std::filesystem::path& outputPath,
                      ProgressCallback progress = nullptr);

    /// Extract raw audio from a track to buffer
    /// @param trackNumber 1-based track number
    /// @param progress Optional progress callback
    /// @return Raw PCM data, or empty vector on failure
    [[nodiscard]] std::vector<std::uint8_t> extractTrackToBuffer(
        int trackNumber,
        ProgressCallback progress = nullptr);

    /// Request cancellation of current extraction
    void cancel();

    /// Check if cancellation was requested
    [[nodiscard]] bool isCancelled() const;

    /// Get last error message
    [[nodiscard]] const std::string& lastError() const { return m_lastError; }

    /// Get device path
    [[nodiscard]] const std::string& devicePath() const { return m_devicePath; }

private:
    std::string m_devicePath;
    std::string m_lastError;
    bool m_cancelled{false};

    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Burner::Core
