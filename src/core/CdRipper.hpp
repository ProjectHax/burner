#pragma once

#include "CdReader.hpp"
#include "AudioOutputEncoder.hpp"
#include "MetadataProvider.hpp"
#include "FileNamingPattern.hpp"

#include <filesystem>
#include <functional>
#include <atomic>
#include <memory>
#include <set>

namespace Burner::Core {

/// Configuration for a rip operation
struct RipConfig {
    std::string devicePath;
    std::filesystem::path outputDir;
    AudioFormat format{AudioFormat::FLAC};
    AudioQuality quality;
    FileNamingPattern pattern;
    std::set<int> tracksToRip;       ///< Track numbers to rip (empty = all)
    bool embedCoverArt{true};
    bool saveCoverFile{true};
    bool ejectAfterRip{false};
    bool lookupMetadata{true};       ///< Query MusicBrainz
};

/// Progress information for a rip operation
struct RipProgress {
    double overallPercent{0.0};
    int currentTrack{0};
    int totalTracks{0};
    double trackPercent{0.0};
    std::string currentOperation;
    std::string currentTrackTitle;
    int encodedBytes{0};
    int totalBytes{0};
};

/// Result of a rip operation
struct RipResult {
    bool success{false};
    std::string errorMessage;
    std::vector<std::filesystem::path> outputFiles;
    int tracksRipped{0};
    int tracksFailed{0};
    int totalTracks{0};
};

/// High-level CD ripping orchestrator.
/// Combines CdReader, MetadataProvider, and AudioOutputEncoder.
class CdRipper {
public:
    /// Progress callback
    using ProgressCallback = std::function<void(const RipProgress&)>;

    /// Construct a CdRipper for the specified device
    /// @param devicePath Path to the optical drive
    explicit CdRipper(const std::string& devicePath);
    ~CdRipper();

    // Non-copyable
    CdRipper(const CdRipper&) = delete;
    CdRipper& operator=(const CdRipper&) = delete;

    // Movable
    CdRipper(CdRipper&&) noexcept;
    CdRipper& operator=(CdRipper&&) noexcept;

    /// Check if ripper is valid
    [[nodiscard]] bool isValid() const;

    /// Check if there's an audio CD in the drive
    [[nodiscard]] bool hasAudioDisc() const;

    /// Scan the disc and return TOC with metadata
    /// @return CdToc with track info and any available metadata
    [[nodiscard]] std::optional<CdToc> scanDisc();

    /// Look up metadata from MusicBrainz
    /// @param toc TOC with disc ID
    /// @return MusicBrainzResult with album matches
    [[nodiscard]] MusicBrainzResult lookupMetadata(const CdToc& toc);

    /// Fetch cover art for a release
    /// @param releaseId MusicBrainz release MBID
    /// @return CoverArt if available
    [[nodiscard]] std::optional<CoverArt> fetchCoverArt(const std::string& releaseId);

    /// Set progress callback
    void setProgressCallback(ProgressCallback callback);

    /// Rip the disc according to configuration
    /// @param config Rip configuration
    /// @param albumMeta Optional metadata to use (from MusicBrainz)
    /// @return RipResult with outcome
    RipResult rip(const RipConfig& config,
                  const AlbumMetadata* albumMeta = nullptr);

    /// Request cancellation
    void cancel();

    /// Check if cancelled
    [[nodiscard]] bool isCancelled() const;

    /// Get last error message
    [[nodiscard]] const std::string& lastError() const { return m_lastError; }

    /// Get the CD reader
    [[nodiscard]] CdReader& reader() { return *m_reader; }

    /// Get the metadata provider
    [[nodiscard]] MetadataProvider& metadataProvider() { return *m_metadataProvider; }

private:
    void reportProgress(const RipProgress& progress);

    std::string m_devicePath;
    std::string m_lastError;
    std::atomic<bool> m_cancelled{false};
    ProgressCallback m_progressCallback;

    std::unique_ptr<CdReader> m_reader;
    std::unique_ptr<MetadataProvider> m_metadataProvider;
    std::unique_ptr<AudioOutputEncoder> m_encoder;
};

} // namespace Burner::Core
