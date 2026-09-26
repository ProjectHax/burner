#include "CdRipper.hpp"

#include <fstream>

namespace Burner::Core {

CdRipper::CdRipper(const std::string& devicePath)
    : m_devicePath(devicePath)
    , m_reader(std::make_unique<CdReader>(devicePath))
    , m_metadataProvider(std::make_unique<MetadataProvider>())
    , m_encoder(std::make_unique<AudioOutputEncoder>()) {

    m_metadataProvider->setUserAgent("Burner", BURNER_VERSION, "burner-app@example.com");
}

CdRipper::~CdRipper() = default;

CdRipper::CdRipper(CdRipper&& other) noexcept
    : m_devicePath(std::move(other.m_devicePath))
    , m_lastError(std::move(other.m_lastError))
    , m_cancelled(other.m_cancelled.load())
    , m_progressCallback(std::move(other.m_progressCallback))
    , m_reader(std::move(other.m_reader))
    , m_metadataProvider(std::move(other.m_metadataProvider))
    , m_encoder(std::move(other.m_encoder)) {
}

CdRipper& CdRipper::operator=(CdRipper&& other) noexcept {
    if (this != &other) {
        m_devicePath = std::move(other.m_devicePath);
        m_lastError = std::move(other.m_lastError);
        m_cancelled = other.m_cancelled.load();
        m_progressCallback = std::move(other.m_progressCallback);
        m_reader = std::move(other.m_reader);
        m_metadataProvider = std::move(other.m_metadataProvider);
        m_encoder = std::move(other.m_encoder);
    }
    return *this;
}

bool CdRipper::isValid() const {
    return m_reader && m_reader->isValid();
}

bool CdRipper::hasAudioDisc() const {
    return m_reader && m_reader->hasAudioDisc();
}

std::optional<CdToc> CdRipper::scanDisc() {
    if (!m_reader) {
        m_lastError = "Reader not initialized";
        return std::nullopt;
    }

    auto toc = m_reader->readToc();
    if (!toc) {
        m_lastError = m_reader->lastError();
    }
    return toc;
}

MusicBrainzResult CdRipper::lookupMetadata(const CdToc& toc) {
    return m_metadataProvider->lookupByDiscId(toc.discId);
}

std::optional<CoverArt> CdRipper::fetchCoverArt(const std::string& releaseId) {
    return m_metadataProvider->fetchCoverArt(releaseId);
}

void CdRipper::setProgressCallback(ProgressCallback callback) {
    m_progressCallback = std::move(callback);
}

void CdRipper::reportProgress(const RipProgress& progress) {
    if (m_progressCallback) {
        m_progressCallback(progress);
    }
}

RipResult CdRipper::rip(const RipConfig& config,
                         const AlbumMetadata* albumMeta) {
    m_cancelled = false;

    RipResult result;
    result.success = false;

    // Validate
    if (!m_reader || !m_reader->isValid()) {
        result.errorMessage = "Invalid device";
        return result;
    }

    // Read TOC
    auto tocOpt = m_reader->readToc();
    if (!tocOpt) {
        result.errorMessage = m_reader->lastError();
        return result;
    }

    const CdToc& toc = *tocOpt;

    // Determine which tracks to rip
    std::vector<const CdTrackInfo*> tracksToRip;
    for (const auto& track : toc.tracks) {
        if (!track.isAudio) {
            continue;
        }
        if (config.tracksToRip.empty() ||
            config.tracksToRip.count(track.trackNumber) > 0) {
            tracksToRip.push_back(&track);
        }
    }

    if (tracksToRip.empty()) {
        result.errorMessage = "No audio tracks to rip";
        return result;
    }

    // Create output directory
    std::error_code ec;
    std::filesystem::create_directories(config.outputDir, ec);
    if (ec) {
        result.errorMessage = "Failed to create output directory: " + ec.message();
        return result;
    }

    // Prepare metadata for tracks
    int totalTracks = static_cast<int>(tracksToRip.size());
    result.totalTracks = totalTracks;

    // Get cover art if we have metadata
    std::optional<CoverArt> coverArt;
    if (albumMeta && !albumMeta->releaseId.empty() && config.embedCoverArt) {
        RipProgress progress;
        progress.currentOperation = "Fetching cover art...";
        progress.totalTracks = totalTracks;
        reportProgress(progress);

        coverArt = m_metadataProvider->fetchCoverArt(albumMeta->releaseId);
    }

    // Save cover art to file if requested
    if (coverArt && config.saveCoverFile) {
        std::filesystem::path coverPath = config.outputDir / "cover.jpg";
        std::ofstream coverFile(coverPath, std::ios::binary);
        if (coverFile) {
            coverFile.write(reinterpret_cast<const char*>(coverArt->data.data()),
                            static_cast<std::streamsize>(coverArt->data.size()));
        }
    }

    // Rip each track
    int trackIndex = 0;
    for (const auto* trackInfo : tracksToRip) {
        if (m_cancelled) {
            result.errorMessage = "Operation cancelled";
            return result;
        }

        ++trackIndex;

        RipProgress progress;
        progress.currentTrack = trackIndex;
        progress.totalTracks = totalTracks;
        progress.overallPercent = (trackIndex - 1) * 100.0 / totalTracks;

        // Determine track metadata
        TrackNamingInfo namingInfo;
        if (albumMeta) {
            namingInfo.albumArtist = albumMeta->albumArtist;
            namingInfo.albumTitle = albumMeta->title;
            namingInfo.year = albumMeta->year;
            namingInfo.genre = albumMeta->genre;
            namingInfo.totalTracks = static_cast<int>(albumMeta->tracks.size());

            // Find matching track metadata
            for (const auto& tm : albumMeta->tracks) {
                if (tm.trackNumber == trackInfo->trackNumber) {
                    namingInfo.trackTitle = tm.title;
                    namingInfo.trackArtist = tm.artist;
                    break;
                }
            }
        }

        // Fall back to CD-TEXT or defaults
        if (namingInfo.albumTitle.empty()) {
            namingInfo.albumTitle = toc.albumTitle.empty() ? "Unknown Album" : toc.albumTitle;
        }
        if (namingInfo.albumArtist.empty()) {
            namingInfo.albumArtist = toc.albumArtist.empty() ? "Unknown Artist" : toc.albumArtist;
        }
        if (namingInfo.trackTitle.empty()) {
            namingInfo.trackTitle = trackInfo->title.empty() ?
                "Track " + std::to_string(trackInfo->trackNumber) : trackInfo->title;
        }
        if (namingInfo.trackArtist.empty()) {
            namingInfo.trackArtist = trackInfo->performer.empty() ?
                namingInfo.albumArtist : trackInfo->performer;
        }

        namingInfo.trackNumber = trackInfo->trackNumber;
        if (namingInfo.totalTracks == 0) {
            namingInfo.totalTracks = static_cast<int>(toc.tracks.size());
        }

        progress.currentTrackTitle = namingInfo.trackTitle;
        progress.currentOperation = "Reading track " + std::to_string(trackInfo->trackNumber) + "...";
        reportProgress(progress);

        // Generate output path
        std::string extension = AudioOutputEncoder::formatExtension(config.format);
        std::filesystem::path relativePath = config.pattern.generate(namingInfo, extension);
        std::filesystem::path outputPath = config.outputDir / relativePath;

        // Create subdirectories if needed
        std::filesystem::create_directories(outputPath.parent_path(), ec);

        // Extract audio
        std::vector<std::uint8_t> pcmData;
        {
            auto readProgress = [this, &progress, totalTracks, trackIndex](int sectorsRead, int totalSectors) {
                progress.trackPercent = (sectorsRead * 100.0) / totalSectors;
                progress.overallPercent = ((trackIndex - 1) + progress.trackPercent / 100.0) * 100.0 / totalTracks;
                reportProgress(progress);
            };

            pcmData = m_reader->extractTrackToBuffer(trackInfo->trackNumber, readProgress);
        }

        if (pcmData.empty()) {
            result.tracksFailed++;
            continue;
        }

        if (m_cancelled) {
            result.errorMessage = "Operation cancelled";
            return result;
        }

        // Encode
        progress.currentOperation = "Encoding to " + AudioOutputEncoder::formatName(config.format) + "...";
        reportProgress(progress);

        AudioMetadata metadata;
        metadata.title = namingInfo.trackTitle;
        metadata.artist = namingInfo.trackArtist;
        metadata.album = namingInfo.albumTitle;
        metadata.albumArtist = namingInfo.albumArtist;
        metadata.trackNumber = namingInfo.trackNumber;
        metadata.totalTracks = namingInfo.totalTracks;
        metadata.year = namingInfo.year;
        metadata.genre = namingInfo.genre;

        if (coverArt && config.embedCoverArt) {
            metadata.coverArt = coverArt->data;
            metadata.coverArtMimeType = coverArt->mimeType;
        }

        auto encodeProgress = [this, &progress, totalTracks, trackIndex](std::int64_t bytesEncoded, std::int64_t totalBytes) {
            progress.trackPercent = 50.0 + (bytesEncoded * 50.0) / totalBytes;
            progress.overallPercent = ((trackIndex - 1) + progress.trackPercent / 100.0) * 100.0 / totalTracks;
            progress.encodedBytes = static_cast<int>(bytesEncoded);
            progress.totalBytes = static_cast<int>(totalBytes);
            reportProgress(progress);
        };

        bool encodeSuccess = m_encoder->encode(pcmData, outputPath, config.format,
                                                config.quality, &metadata, encodeProgress);

        if (!encodeSuccess) {
            result.tracksFailed++;
            continue;
        }

        result.outputFiles.push_back(outputPath);
        result.tracksRipped++;
    }

    // Final progress
    RipProgress finalProgress;
    finalProgress.overallPercent = 100.0;
    finalProgress.currentTrack = totalTracks;
    finalProgress.totalTracks = totalTracks;
    finalProgress.currentOperation = "Complete";
    reportProgress(finalProgress);

    result.success = (result.tracksRipped > 0);
    if (result.tracksFailed > 0) {
        result.errorMessage = std::to_string(result.tracksFailed) + " track(s) failed to rip";
    }

    return result;
}

void CdRipper::cancel() {
    m_cancelled = true;
    if (m_reader) {
        m_reader->cancel();
    }
    if (m_encoder) {
        m_encoder->cancel();
    }
}

bool CdRipper::isCancelled() const {
    return m_cancelled;
}

} // namespace Burner::Core
