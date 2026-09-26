#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <atomic>
#include <memory>
#include <set>

#include "../core/CdRipper.hpp"
#include "../core/AudioOutputEncoder.hpp"

namespace Burner::Engine {

/// Worker object that performs CD rip operations in a separate thread.
/// Should be moved to a QThread before use.
class RipWorker : public QObject {
    Q_OBJECT

public:
    explicit RipWorker(QObject* parent = nullptr);
    ~RipWorker() override;

public slots:
    /// Scan disc for tracks and metadata
    void scanDisc(const QString& devicePath);

    /// Look up metadata from MusicBrainz
    void lookupMetadata(const QString& devicePath, const QString& discId);

    /// Fetch cover art for a release
    void fetchCoverArt(const QString& releaseId);

    /// Rip selected tracks
    void ripTracks(const QString& devicePath,
                   const QString& outputDir,
                   Burner::Core::AudioFormat format,
                   int quality,
                   const QString& pattern,
                   const QList<int>& trackNumbers,
                   bool embedCoverArt,
                   bool saveCoverFile);

    /// Rip with full metadata
    void ripTracksWithMetadata(const QString& devicePath,
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
                                const QStringList& trackArtists);

    /// Request cancellation
    void cancel();

signals:
    /// Emitted when disc scan completes
    void scanComplete(bool success,
                      const QString& discId,
                      int trackCount,
                      const QList<int>& trackNumbers,
                      const QList<int>& trackDurations,
                      const QStringList& trackTitles,
                      const QStringList& trackArtists,
                      const QString& albumTitle,
                      const QString& albumArtist);

    /// Emitted when metadata lookup completes
    void metadataComplete(bool success,
                          const QString& albumTitle,
                          const QString& albumArtist,
                          int year,
                          const QString& genre,
                          const QString& releaseId,
                          const QStringList& trackTitles,
                          const QStringList& trackArtists);

    /// Emitted with multiple metadata matches
    void metadataMatches(const QStringList& titles,
                          const QStringList& artists,
                          const QStringList& releaseIds);

    /// Emitted when cover art is fetched
    void coverArtFetched(bool success, const QByteArray& imageData, const QString& mimeType);

    /// Emitted during rip progress
    void progressUpdated(double overallPercent,
                          int currentTrack,
                          int totalTracks,
                          double trackPercent,
                          const QString& operation);

    /// Emitted when rip completes
    void ripComplete(bool success,
                      int tracksRipped,
                      int tracksFailed,
                      const QString& message);

    /// Emitted on error
    void error(const QString& errorMessage);

private:
    std::atomic<bool> m_cancelled{false};
    std::unique_ptr<Burner::Core::CdRipper> m_activeRipper;
};

} // namespace Burner::Engine
