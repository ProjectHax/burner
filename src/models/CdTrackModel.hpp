#pragma once

#include <QAbstractTableModel>
#include <vector>

namespace Burner::Models {

/// Information about a CD audio track
struct CdTrack {
    int trackNumber{0};
    QString title;
    QString artist;
    int durationMs{0};
    bool selected{true};      ///< Whether to include in rip
};

/// Qt table model for CD audio tracks with selection checkboxes.
class CdTrackModel : public QAbstractTableModel {
    Q_OBJECT
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
    Q_PROPERTY(int totalDuration READ totalDurationMs NOTIFY tracksChanged)
    Q_PROPERTY(int selectedDuration READ selectedDurationMs NOTIFY selectionChanged)

public:
    enum Columns {
        ColumnCheck = 0,
        ColumnNumber,
        ColumnTitle,
        ColumnArtist,
        ColumnDuration,
        ColumnCount
    };

    enum Roles {
        TrackNumberRole = Qt::UserRole + 1,
        TitleRole,
        ArtistRole,
        DurationMsRole,
        DurationStringRole,
        SelectedRole
    };

    explicit CdTrackModel(QObject* parent = nullptr);

    // QAbstractTableModel interface
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// Set tracks from scan results
    void setTracks(const QList<int>& trackNumbers,
                   const QList<int>& durations,
                   const QStringList& titles,
                   const QStringList& artists);

    /// Update metadata (from MusicBrainz lookup)
    void updateMetadata(const QStringList& titles, const QStringList& artists);

    /// Update a single track's metadata
    void updateTrackMetadata(int trackNumber, const QString& title, const QString& artist);

    /// Get list of selected track numbers
    [[nodiscard]] QList<int> selectedTrackNumbers() const;

    /// Get list of selected track titles
    [[nodiscard]] QStringList selectedTrackTitles() const;

    /// Get list of selected track artists
    [[nodiscard]] QStringList selectedTrackArtists() const;

    /// Get all track titles (for saving to metadata)
    [[nodiscard]] QStringList allTrackTitles() const;

    /// Get all track artists
    [[nodiscard]] QStringList allTrackArtists() const;

    /// Get count of selected tracks
    [[nodiscard]] int selectedCount() const;

    /// Get total duration of all tracks in milliseconds
    [[nodiscard]] int totalDurationMs() const;

    /// Get total duration of selected tracks in milliseconds
    [[nodiscard]] int selectedDurationMs() const;

    /// Get total duration as formatted string (MM:SS)
    [[nodiscard]] QString totalDurationString() const;

    /// Get selected duration as formatted string
    [[nodiscard]] QString selectedDurationString() const;

    /// Get track count
    [[nodiscard]] int trackCount() const { return static_cast<int>(m_tracks.size()); }

    /// Clear all tracks
    void clear();

public slots:
    /// Select all tracks
    void selectAll();

    /// Deselect all tracks
    void deselectAll();

    /// Toggle selection for a track
    void toggleSelection(int trackNumber);

signals:
    void tracksChanged();
    void selectionChanged();

private:
    static QString formatDuration(int ms);

    std::vector<CdTrack> m_tracks;
};

} // namespace Burner::Models
