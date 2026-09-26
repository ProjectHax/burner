#include "RipWorker.hpp"

#include "../core/CdRipper.hpp"
#include "../core/CdReader.hpp"
#include "../core/MetadataProvider.hpp"
#include "../core/FileNamingPattern.hpp"

#include <QDebug>

namespace Burner::Engine {

RipWorker::RipWorker(QObject* parent)
    : QObject(parent) {
    qRegisterMetaType<Burner::Core::AudioFormat>("Burner::Core::AudioFormat");
}

RipWorker::~RipWorker() = default;

void RipWorker::scanDisc(const QString& devicePath) {
    m_cancelled = false;

    Burner::Core::CdReader reader(devicePath.toStdString());

    if (!reader.isValid()) {
        emit error(tr("Failed to access drive: %1").arg(devicePath));
        emit scanComplete(false, QString(), 0, {}, {}, {}, {}, QString(), QString());
        return;
    }

    if (!reader.hasAudioDisc()) {
        emit error(tr("No audio CD detected in drive"));
        emit scanComplete(false, QString(), 0, {}, {}, {}, {}, QString(), QString());
        return;
    }

    auto tocOpt = reader.readToc();
    if (!tocOpt) {
        emit error(tr("Failed to read disc: %1").arg(QString::fromStdString(reader.lastError())));
        emit scanComplete(false, QString(), 0, {}, {}, {}, {}, QString(), QString());
        return;
    }

    const auto& toc = *tocOpt;

    QList<int> trackNumbers;
    QList<int> trackDurations;
    QStringList trackTitles;
    QStringList trackArtists;

    for (const auto& track : toc.tracks) {
        if (track.isAudio) {
            trackNumbers.append(track.trackNumber);
            trackDurations.append(track.durationMs);
            trackTitles.append(QString::fromStdString(track.title));
            trackArtists.append(QString::fromStdString(track.performer));
        }
    }

    emit scanComplete(true,
                       QString::fromStdString(toc.discId),
                       trackNumbers.size(),
                       trackNumbers,
                       trackDurations,
                       trackTitles,
                       trackArtists,
                       QString::fromStdString(toc.albumTitle),
                       QString::fromStdString(toc.albumArtist));
}

void RipWorker::lookupMetadata(const QString& devicePath, const QString& discId) {
    Q_UNUSED(devicePath);
    m_cancelled = false;

    Burner::Core::MetadataProvider provider;
    provider.setUserAgent("Burner", BURNER_VERSION, "burner-app@example.com");

    auto result = provider.lookupByDiscId(discId.toStdString());

    if (!result.errorMessage.empty()) {
        emit error(QString::fromStdString(result.errorMessage));
        emit metadataComplete(false, QString(), QString(), 0, QString(), QString(), {}, {});
        return;
    }

    if (result.matches.empty()) {
        emit metadataComplete(false, QString(), QString(), 0, QString(), QString(), {}, {});
        return;
    }

    // If multiple matches, emit the list for user selection
    if (result.matches.size() > 1) {
        QStringList titles;
        QStringList artists;
        QStringList releaseIds;

        for (const auto& match : result.matches) {
            titles.append(QString::fromStdString(match.title));
            artists.append(QString::fromStdString(match.artist));
            releaseIds.append(QString::fromStdString(match.releaseId));
        }

        emit metadataMatches(titles, artists, releaseIds);
    }

    // Use first match as default
    const auto& album = result.matches.front();

    QStringList trackTitles;
    QStringList trackArtists;

    for (const auto& track : album.tracks) {
        trackTitles.append(QString::fromStdString(track.title));
        trackArtists.append(QString::fromStdString(track.artist));
    }

    emit metadataComplete(true,
                           QString::fromStdString(album.title),
                           QString::fromStdString(album.artist),
                           album.year,
                           QString::fromStdString(album.genre),
                           QString::fromStdString(album.releaseId),
                           trackTitles,
                           trackArtists);
}

void RipWorker::fetchCoverArt(const QString& releaseId) {
    m_cancelled = false;

    Burner::Core::MetadataProvider provider;
    provider.setUserAgent("Burner", BURNER_VERSION, "burner-app@example.com");

    auto coverOpt = provider.fetchCoverArt(releaseId.toStdString());

    if (!coverOpt) {
        emit coverArtFetched(false, QByteArray(), QString());
        return;
    }

    QByteArray imageData(reinterpret_cast<const char*>(coverOpt->data.data()),
                          static_cast<int>(coverOpt->data.size()));

    emit coverArtFetched(true, imageData, QString::fromStdString(coverOpt->mimeType));
}

void RipWorker::ripTracks(const QString& devicePath,
                           const QString& outputDir,
                           Burner::Core::AudioFormat format,
                           int quality,
                           const QString& pattern,
                           const QList<int>& trackNumbers,
                           bool embedCoverArt,
                           bool saveCoverFile) {
    ripTracksWithMetadata(devicePath, outputDir, format, quality, pattern,
                           trackNumbers, embedCoverArt, saveCoverFile,
                           QString(), QString(), 0, QString(), QString(),
                           {}, {});
}

void RipWorker::ripTracksWithMetadata(const QString& devicePath,
                                        const QString& outputDir,
                                        Burner::Core::AudioFormat format,
                                        int quality,
                                        const QString& pattern,
                                        const QList<int>& trackNumbers,
                                        bool embedCoverArt,
                                        bool saveCoverFile,
                                        const QString& albumTitle,
                                        const QString& albumArtist,
                                        int year,
                                        const QString& genre,
                                        const QString& releaseId,
                                        const QStringList& trackTitles,
                                        const QStringList& trackArtists) {
    m_cancelled = false;

    m_activeRipper = std::make_unique<Burner::Core::CdRipper>(devicePath.toStdString());

    if (!m_activeRipper->isValid()) {
        emit error(tr("Failed to access drive"));
        emit ripComplete(false, 0, 0, tr("Failed to access drive"));
        m_activeRipper.reset();
        return;
    }

    // Set up progress callback
    m_activeRipper->setProgressCallback([this](const Burner::Core::RipProgress& progress) {
        emit progressUpdated(progress.overallPercent,
                              progress.currentTrack,
                              progress.totalTracks,
                              progress.trackPercent,
                              QString::fromStdString(progress.currentOperation));
    });

    // Build configuration
    Burner::Core::RipConfig config;
    config.devicePath = devicePath.toStdString();
    config.outputDir = outputDir.toStdString();
    config.format = format;

    // Set quality based on format
    switch (format) {
        case Burner::Core::AudioFormat::FLAC:
            config.quality.compression = quality;
            break;
        case Burner::Core::AudioFormat::MP3:
        case Burner::Core::AudioFormat::AAC:
            config.quality.bitrate = quality;
            break;
        case Burner::Core::AudioFormat::OGG:
            config.quality.vbrQuality = quality;
            break;
        case Burner::Core::AudioFormat::WAV:
            break;
    }

    config.pattern = Burner::Core::FileNamingPattern(pattern.toStdString());
    config.embedCoverArt = embedCoverArt;
    config.saveCoverFile = saveCoverFile;

    for (int trackNum : trackNumbers) {
        config.tracksToRip.insert(trackNum);
    }

    // Build album metadata if provided
    std::unique_ptr<Burner::Core::AlbumMetadata> albumMeta;
    if (!albumTitle.isEmpty() || !albumArtist.isEmpty()) {
        albumMeta = std::make_unique<Burner::Core::AlbumMetadata>();
        albumMeta->title = albumTitle.toStdString();
        albumMeta->artist = albumArtist.toStdString();
        albumMeta->albumArtist = albumArtist.toStdString();
        albumMeta->year = year;
        albumMeta->genre = genre.toStdString();
        albumMeta->releaseId = releaseId.toStdString();

        for (int i = 0; i < trackTitles.size(); ++i) {
            Burner::Core::TrackMeta track;
            track.trackNumber = i + 1;
            track.title = trackTitles[i].toStdString();
            if (i < trackArtists.size()) {
                track.artist = trackArtists[i].toStdString();
            }
            albumMeta->tracks.push_back(track);
        }
    }

    // Perform rip
    auto result = m_activeRipper->rip(config, albumMeta.get());

    if (m_cancelled) {
        emit ripComplete(false, result.tracksRipped, result.tracksFailed, tr("Operation cancelled"));
    } else if (result.success) {
        emit ripComplete(true, result.tracksRipped, result.tracksFailed,
                          tr("Successfully ripped %1 track(s)").arg(result.tracksRipped));
    } else {
        emit ripComplete(false, result.tracksRipped, result.tracksFailed,
                          QString::fromStdString(result.errorMessage));
    }

    m_activeRipper.reset();
}

void RipWorker::cancel() {
    m_cancelled = true;
    if (m_activeRipper) {
        m_activeRipper->cancel();
    }
}

} // namespace Burner::Engine
