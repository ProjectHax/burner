#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <memory>

#include "../core/AudioOutputEncoder.hpp"

class QThread;

namespace Burner::Engine {

class RipWorker;

/// Qt wrapper for the CD ripping engine.
/// Provides signal/slot interface for UI integration.
class QtRipEngine : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool ripping READ isRipping NOTIFY rippingChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit QtRipEngine(QObject* parent = nullptr);
    ~QtRipEngine() override;

    /// Check if a rip operation is in progress
    [[nodiscard]] bool isRipping() const;

    /// Get current rip progress (0.0 - 100.0)
    [[nodiscard]] double progress() const;

    /// Get current status message
    [[nodiscard]] QString status() const;

    /// Check if the engine is properly initialized
    [[nodiscard]] bool isInitialized() const;

public slots:
    /// Scan disc for tracks and metadata
    void scanDisc(const QString& device);

    /// Look up metadata from MusicBrainz
    void lookupMetadata(const QString& device, const QString& discId);

    /// Fetch cover art for a release
    void fetchCoverArt(const QString& releaseId);

    /// Rip selected tracks with metadata
    void ripTracks(const QString& device,
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

    /// Cancel the current operation
    void cancel();

signals:
    void rippingChanged(bool ripping);
    void progressChanged(double percent);
    void statusChanged(const QString& status);

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

    /// Emitted with multiple metadata matches for user selection
    void metadataMatches(const QStringList& titles,
                          const QStringList& artists,
                          const QStringList& releaseIds);

    /// Emitted when cover art is fetched
    void coverArtFetched(bool success, const QByteArray& imageData, const QString& mimeType);

    /// Emitted during rip progress
    void ripProgress(double overallPercent,
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
    void ripError(const QString& error);

private slots:
    void onWorkerProgress(double overallPercent, int currentTrack, int totalTracks,
                           double trackPercent, const QString& operation);
    void onWorkerComplete(bool success, int tracksRipped, int tracksFailed,
                           const QString& message);
    void cleanupWorker();

private:
    void startWorker();

    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Burner::Engine
