#include "CdTrackModel.hpp"

namespace Burner::Models {

CdTrackModel::CdTrackModel(QObject* parent)
    : QAbstractTableModel(parent) {
}

int CdTrackModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_tracks.size());
}

int CdTrackModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return ColumnCount;
}

QVariant CdTrackModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= static_cast<int>(m_tracks.size())) {
        return {};
    }

    const auto& track = m_tracks[index.row()];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case ColumnNumber:
                return track.trackNumber;
            case ColumnTitle:
                return track.title.isEmpty() ?
                    tr("Track %1").arg(track.trackNumber) : track.title;
            case ColumnArtist:
                return track.artist;
            case ColumnDuration:
                return formatDuration(track.durationMs);
            default:
                return {};
        }
    }

    if (role == Qt::CheckStateRole && index.column() == ColumnCheck) {
        return track.selected ? Qt::Checked : Qt::Unchecked;
    }

    if (role == Qt::TextAlignmentRole) {
        if (index.column() == ColumnNumber || index.column() == ColumnDuration) {
            return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
        }
    }

    // Custom roles
    switch (role) {
        case TrackNumberRole:
            return track.trackNumber;
        case TitleRole:
            return track.title;
        case ArtistRole:
            return track.artist;
        case DurationMsRole:
            return track.durationMs;
        case DurationStringRole:
            return formatDuration(track.durationMs);
        case SelectedRole:
            return track.selected;
        default:
            return {};
    }
}

bool CdTrackModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || index.row() >= static_cast<int>(m_tracks.size())) {
        return false;
    }

    auto& track = m_tracks[index.row()];

    if (role == Qt::CheckStateRole && index.column() == ColumnCheck) {
        track.selected = (value.toInt() == Qt::Checked);
        emit dataChanged(index, index, {Qt::CheckStateRole, SelectedRole});
        emit selectionChanged();
        return true;
    }

    if (role == Qt::EditRole) {
        switch (index.column()) {
            case ColumnTitle:
                track.title = value.toString();
                emit dataChanged(index, index, {Qt::DisplayRole, TitleRole});
                return true;
            case ColumnArtist:
                track.artist = value.toString();
                emit dataChanged(index, index, {Qt::DisplayRole, ArtistRole});
                return true;
            default:
                break;
        }
    }

    return false;
}

QVariant CdTrackModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }

    switch (section) {
        case ColumnCheck:
            return QString();  // Checkbox column has no header
        case ColumnNumber:
            return tr("#");
        case ColumnTitle:
            return tr("Title");
        case ColumnArtist:
            return tr("Artist");
        case ColumnDuration:
            return tr("Duration");
        default:
            return {};
    }
}

Qt::ItemFlags CdTrackModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;

    if (index.column() == ColumnCheck) {
        flags |= Qt::ItemIsUserCheckable;
    }

    if (index.column() == ColumnTitle || index.column() == ColumnArtist) {
        flags |= Qt::ItemIsEditable;
    }

    return flags;
}

QHash<int, QByteArray> CdTrackModel::roleNames() const {
    auto roles = QAbstractTableModel::roleNames();
    roles[TrackNumberRole] = "trackNumber";
    roles[TitleRole] = "title";
    roles[ArtistRole] = "artist";
    roles[DurationMsRole] = "durationMs";
    roles[DurationStringRole] = "durationString";
    roles[SelectedRole] = "selected";
    return roles;
}

void CdTrackModel::setTracks(const QList<int>& trackNumbers,
                              const QList<int>& durations,
                              const QStringList& titles,
                              const QStringList& artists) {
    beginResetModel();
    m_tracks.clear();

    int count = trackNumbers.size();
    m_tracks.reserve(count);

    for (int i = 0; i < count; ++i) {
        CdTrack track;
        track.trackNumber = trackNumbers[i];
        track.durationMs = i < durations.size() ? durations[i] : 0;
        track.title = i < titles.size() ? titles[i] : QString();
        track.artist = i < artists.size() ? artists[i] : QString();
        track.selected = true;
        m_tracks.push_back(track);
    }

    endResetModel();
    emit tracksChanged();
    emit selectionChanged();
}

void CdTrackModel::updateMetadata(const QStringList& titles, const QStringList& artists) {
    for (size_t i = 0; i < m_tracks.size(); ++i) {
        if (i < static_cast<size_t>(titles.size())) {
            m_tracks[i].title = titles[static_cast<int>(i)];
        }
        if (i < static_cast<size_t>(artists.size())) {
            m_tracks[i].artist = artists[static_cast<int>(i)];
        }
    }

    if (!m_tracks.empty()) {
        emit dataChanged(index(0, ColumnTitle),
                          index(static_cast<int>(m_tracks.size()) - 1, ColumnArtist),
                          {Qt::DisplayRole, TitleRole, ArtistRole});
    }
}

void CdTrackModel::updateTrackMetadata(int trackNumber, const QString& title, const QString& artist) {
    for (size_t i = 0; i < m_tracks.size(); ++i) {
        if (m_tracks[i].trackNumber == trackNumber) {
            m_tracks[i].title = title;
            m_tracks[i].artist = artist;
            emit dataChanged(index(static_cast<int>(i), ColumnTitle),
                              index(static_cast<int>(i), ColumnArtist),
                              {Qt::DisplayRole, TitleRole, ArtistRole});
            return;
        }
    }
}

QList<int> CdTrackModel::selectedTrackNumbers() const {
    QList<int> result;
    for (const auto& track : m_tracks) {
        if (track.selected) {
            result.append(track.trackNumber);
        }
    }
    return result;
}

QStringList CdTrackModel::selectedTrackTitles() const {
    QStringList result;
    for (const auto& track : m_tracks) {
        if (track.selected) {
            result.append(track.title);
        }
    }
    return result;
}

QStringList CdTrackModel::selectedTrackArtists() const {
    QStringList result;
    for (const auto& track : m_tracks) {
        if (track.selected) {
            result.append(track.artist);
        }
    }
    return result;
}

QStringList CdTrackModel::allTrackTitles() const {
    QStringList result;
    for (const auto& track : m_tracks) {
        result.append(track.title);
    }
    return result;
}

QStringList CdTrackModel::allTrackArtists() const {
    QStringList result;
    for (const auto& track : m_tracks) {
        result.append(track.artist);
    }
    return result;
}

int CdTrackModel::selectedCount() const {
    int count = 0;
    for (const auto& track : m_tracks) {
        if (track.selected) {
            ++count;
        }
    }
    return count;
}

int CdTrackModel::totalDurationMs() const {
    int total = 0;
    for (const auto& track : m_tracks) {
        total += track.durationMs;
    }
    return total;
}

int CdTrackModel::selectedDurationMs() const {
    int total = 0;
    for (const auto& track : m_tracks) {
        if (track.selected) {
            total += track.durationMs;
        }
    }
    return total;
}

QString CdTrackModel::totalDurationString() const {
    return formatDuration(totalDurationMs());
}

QString CdTrackModel::selectedDurationString() const {
    return formatDuration(selectedDurationMs());
}

void CdTrackModel::clear() {
    beginResetModel();
    m_tracks.clear();
    endResetModel();
    emit tracksChanged();
    emit selectionChanged();
}

void CdTrackModel::selectAll() {
    for (auto& track : m_tracks) {
        track.selected = true;
    }
    if (!m_tracks.empty()) {
        emit dataChanged(index(0, ColumnCheck),
                          index(static_cast<int>(m_tracks.size()) - 1, ColumnCheck),
                          {Qt::CheckStateRole, SelectedRole});
    }
    emit selectionChanged();
}

void CdTrackModel::deselectAll() {
    for (auto& track : m_tracks) {
        track.selected = false;
    }
    if (!m_tracks.empty()) {
        emit dataChanged(index(0, ColumnCheck),
                          index(static_cast<int>(m_tracks.size()) - 1, ColumnCheck),
                          {Qt::CheckStateRole, SelectedRole});
    }
    emit selectionChanged();
}

void CdTrackModel::toggleSelection(int trackNumber) {
    for (size_t i = 0; i < m_tracks.size(); ++i) {
        if (m_tracks[i].trackNumber == trackNumber) {
            m_tracks[i].selected = !m_tracks[i].selected;
            emit dataChanged(index(static_cast<int>(i), ColumnCheck),
                              index(static_cast<int>(i), ColumnCheck),
                              {Qt::CheckStateRole, SelectedRole});
            emit selectionChanged();
            return;
        }
    }
}

QString CdTrackModel::formatDuration(int ms) {
    int totalSeconds = ms / 1000;
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;
    return QStringLiteral("%1:%2").arg(minutes).arg(seconds, 2, 10, QChar('0'));
}

} // namespace Burner::Models
