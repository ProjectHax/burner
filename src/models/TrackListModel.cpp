#include "TrackListModel.hpp"
#include "../core/AudioEncoder.hpp"
#include <QFileInfo>
#include <QBrush>

namespace Burner::Models {

TrackListModel::TrackListModel(QObject* parent)
    : QAbstractListModel(parent) {
}

int TrackListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_tracks.size());
}

QVariant TrackListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= static_cast<int>(m_tracks.size())) {
        return {};
    }

    const auto& track = m_tracks[index.row()];

    switch (role) {
        case Qt::DisplayRole:
            return QString("%1. %2%3 (%4)")
                .arg(index.row() + 1)
                .arg(track.fileExists ? "" : "[!] ")
                .arg(track.title.isEmpty() ? QFileInfo(track.filePath).baseName() : track.title)
                .arg(formatDuration(track.durationSeconds));

        case Qt::ForegroundRole:
            if (!track.fileExists) {
                return QBrush(Qt::red);
            }
            return {};

        case Qt::ToolTipRole:
            if (!track.fileExists) {
                return tr("File not found: %1").arg(track.filePath);
            }
            return track.filePath;

        case FilePathRole:
            return track.filePath;

        case TitleRole:
            return track.title.isEmpty() ? QFileInfo(track.filePath).baseName() : track.title;

        case ArtistRole:
            return track.artist;

        case AlbumRole:
            return track.album;

        case DurationRole:
            return track.durationSeconds;

        case DurationStringRole:
            return formatDuration(track.durationSeconds);

        case FormatRole:
            return track.format;

        case SampleRateRole:
            return track.sampleRate;

        case BitRateRole:
            return track.bitRate;

        case SizeRole:
            return track.size;

        case TrackNumberRole:
            return index.row() + 1;

        case FileExistsRole:
            return track.fileExists;

        default:
            return {};
    }
}

QHash<int, QByteArray> TrackListModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[FilePathRole] = "filePath";
    roles[TitleRole] = "title";
    roles[ArtistRole] = "artist";
    roles[AlbumRole] = "album";
    roles[DurationRole] = "duration";
    roles[DurationStringRole] = "durationString";
    roles[FormatRole] = "format";
    roles[SampleRateRole] = "sampleRate";
    roles[BitRateRole] = "bitRate";
    roles[SizeRole] = "size";
    roles[TrackNumberRole] = "trackNumber";
    return roles;
}

Qt::ItemFlags TrackListModel::flags(const QModelIndex& index) const {
    Qt::ItemFlags defaultFlags = QAbstractListModel::flags(index);

    if (index.isValid()) {
        return defaultFlags | Qt::ItemIsDragEnabled;
    }
    return defaultFlags | Qt::ItemIsDropEnabled;
}

Qt::DropActions TrackListModel::supportedDropActions() const {
    return Qt::MoveAction;
}

bool TrackListModel::moveRows(const QModelIndex& sourceParent, int sourceRow, int count,
                               const QModelIndex& destinationParent, int destinationRow) {
    Q_UNUSED(sourceParent);
    Q_UNUSED(destinationParent);

    if (count != 1) {
        return false; // Only support moving single items
    }

    return moveTrack(sourceRow, destinationRow);
}

bool TrackListModel::addTrack(const QString& filePath) {
    QFileInfo fileInfo(filePath);
    bool exists = fileInfo.exists() && fileInfo.isReadable();

    // Check if it's a supported audio format
    QString ext = fileInfo.suffix().toLower();
    if (!Core::AudioEncoder::isFormatSupported(ext.toStdString())) {
        return false;
    }

    // Get audio metadata using FFmpeg
    auto audioInfo = Core::AudioEncoder::getAudioInfo(filePath.toStdString());

    int row = static_cast<int>(m_tracks.size());
    beginInsertRows(QModelIndex(), row, row);

    AudioTrack track;
    track.filePath = filePath;
    track.size = exists ? fileInfo.size() : 0;
    track.fileExists = exists;

    // Use metadata from FFmpeg if available
    if (!audioInfo.title.empty()) {
        track.title = QString::fromStdString(audioInfo.title);
    } else {
        track.title = fileInfo.baseName();
    }

    track.artist = QString::fromStdString(audioInfo.artist);
    track.album = QString::fromStdString(audioInfo.album);
    track.format = QString::fromStdString(audioInfo.codecName).toUpper();
    track.sampleRate = audioInfo.sampleRate;
    track.bitRate = audioInfo.bitRate;

    // Duration from FFmpeg (convert from ms to seconds)
    if (audioInfo.durationMs > 0) {
        track.durationSeconds = audioInfo.durationMs / 1000;
    } else {
        // Fallback estimate if FFmpeg couldn't determine duration
        if (ext == "mp3") {
            track.durationSeconds = static_cast<int>(track.size / (128 * 1024 / 8));
        } else if (ext == "wav") {
            track.durationSeconds = static_cast<int>(track.size / (44100 * 2 * 2));
        } else {
            track.durationSeconds = static_cast<int>(track.size / 20000);
        }
    }

    m_tracks.push_back(std::move(track));

    endInsertRows();

    updateTotals();
    emit trackAdded(row);
    return true;
}

bool TrackListModel::addTracks(const QStringList& filePaths) {
    bool success = false;
    for (const QString& path : filePaths) {
        if (addTrack(path)) {
            success = true;
        }
    }
    return success;
}

bool TrackListModel::removeTrack(int index) {
    if (index < 0 || index >= static_cast<int>(m_tracks.size())) {
        return false;
    }

    beginRemoveRows(QModelIndex(), index, index);
    m_tracks.erase(m_tracks.begin() + index);
    endRemoveRows();

    updateTotals();
    emit trackRemoved(index);
    return true;
}

bool TrackListModel::moveTrack(int from, int to) {
    if (from < 0 || from >= static_cast<int>(m_tracks.size()) ||
        to < 0 || to > static_cast<int>(m_tracks.size())) {
        return false;
    }

    if (from == to || from == to - 1) {
        return true; // No-op
    }

    int destRow = (to > from) ? to + 1 : to;

    beginMoveRows(QModelIndex(), from, from, QModelIndex(), destRow);

    if (to > from) {
        std::rotate(m_tracks.begin() + from, m_tracks.begin() + from + 1, m_tracks.begin() + to + 1);
    } else {
        std::rotate(m_tracks.begin() + to, m_tracks.begin() + from, m_tracks.begin() + from + 1);
    }

    endMoveRows();
    return true;
}

void TrackListModel::clear() {
    beginResetModel();
    m_tracks.clear();
    m_totalDuration = 0;
    endResetModel();
    emit totalDurationChanged(0);
    emit capacityStatusChanged(true);
}

QStringList TrackListModel::trackPaths() const {
    QStringList paths;
    paths.reserve(static_cast<int>(m_tracks.size()));
    for (const auto& track : m_tracks) {
        paths.append(track.filePath);
    }
    return paths;
}

int TrackListModel::totalDurationSeconds() const {
    return m_totalDuration;
}

QString TrackListModel::totalDurationString() const {
    return formatDuration(m_totalDuration);
}

bool TrackListModel::fitsOnCD() const {
    return m_totalDuration <= MAX_CD_DURATION_SECONDS;
}

QString TrackListModel::formatDuration(int seconds) const {
    int hours = seconds / 3600;
    int mins = (seconds % 3600) / 60;
    int secs = seconds % 60;

    if (hours > 0) {
        return QString("%1:%2:%3")
            .arg(hours)
            .arg(mins, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0'));
    }
    return QString("%1:%2")
        .arg(mins)
        .arg(secs, 2, 10, QChar('0'));
}

void TrackListModel::updateTotals() {
    int total = 0;
    for (const auto& track : m_tracks) {
        total += track.durationSeconds;
    }

    bool wasFitting = fitsOnCD();

    if (m_totalDuration != total) {
        m_totalDuration = total;
        emit totalDurationChanged(total);
    }

    if (wasFitting != fitsOnCD()) {
        emit capacityStatusChanged(fitsOnCD());
    }
}

void TrackListModel::refreshFileExistence() {
    for (size_t i = 0; i < m_tracks.size(); ++i) {
        QFileInfo info(m_tracks[i].filePath);
        m_tracks[i].fileExists = info.exists() && info.isReadable();
    }
    emit dataChanged(index(0), index(static_cast<int>(m_tracks.size()) - 1));
}

bool TrackListModel::allFilesExist() const {
    for (const auto& track : m_tracks) {
        if (!track.fileExists) {
            return false;
        }
    }
    return true;
}

} // namespace Burner::Models
