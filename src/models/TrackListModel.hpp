#pragma once

#include <QAbstractListModel>
#include <vector>

namespace Burner::Models {

/// Information about an audio track
struct AudioTrack {
    QString filePath;
    QString title;
    QString artist;
    QString album;
    int durationSeconds{0};
    QString format;        ///< e.g., "MP3", "FLAC", "WAV"
    int sampleRate{0};
    int bitRate{0};
    qint64 size{0};
    bool fileExists{true}; ///< Whether the file is accessible
};

/// Qt list model for audio CD tracks.
/// Supports reordering via drag-and-drop.
class TrackListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int totalDuration READ totalDurationSeconds NOTIFY totalDurationChanged)
    Q_PROPERTY(bool fitsOnCD READ fitsOnCD NOTIFY capacityStatusChanged)

public:
    enum Roles {
        FilePathRole = Qt::UserRole + 1,
        TitleRole,
        ArtistRole,
        AlbumRole,
        DurationRole,
        DurationStringRole,
        FormatRole,
        SampleRateRole,
        BitRateRole,
        SizeRole,
        TrackNumberRole,
        FileExistsRole
    };

    static constexpr int MAX_CD_DURATION_SECONDS = 80 * 60; // 80 minutes

    explicit TrackListModel(QObject* parent = nullptr);

    // QAbstractListModel interface
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    // Drag-drop reordering
    Qt::DropActions supportedDropActions() const override;
    bool moveRows(const QModelIndex& sourceParent, int sourceRow, int count,
                  const QModelIndex& destinationParent, int destinationRow) override;

    /// Add a track from an audio file
    bool addTrack(const QString& filePath);

    /// Add multiple tracks
    bool addTracks(const QStringList& filePaths);

    /// Remove track at index
    bool removeTrack(int index);

    /// Move track from one position to another
    bool moveTrack(int from, int to);

    /// Clear all tracks
    void clear();

    /// Get all track file paths in order
    [[nodiscard]] QStringList trackPaths() const;

    /// Get total duration in seconds
    [[nodiscard]] int totalDurationSeconds() const;

    /// Get total duration as formatted string
    [[nodiscard]] QString totalDurationString() const;

    /// Check if tracks fit on a standard CD (80 min)
    [[nodiscard]] bool fitsOnCD() const;

    /// Get track count
    [[nodiscard]] int trackCount() const { return static_cast<int>(m_tracks.size()); }

    /// Refresh file existence status for all tracks
    void refreshFileExistence();

    /// Check if all files exist
    [[nodiscard]] bool allFilesExist() const;

signals:
    void totalDurationChanged(int seconds);
    void capacityStatusChanged(bool fitsOnCD);
    void trackAdded(int index);
    void trackRemoved(int index);

private:
    QString formatDuration(int seconds) const;
    void updateTotals();

    std::vector<AudioTrack> m_tracks;
    int m_totalDuration{0};
};

} // namespace Burner::Models
