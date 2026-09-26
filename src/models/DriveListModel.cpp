#include "DriveListModel.hpp"

namespace Burner::Models {

DriveListModel::DriveListModel(QObject* parent)
    : QAbstractListModel(parent) {
}

int DriveListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_drives.size());
}

QVariant DriveListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= static_cast<int>(m_drives.size())) {
        return {};
    }

    const auto& drive = m_drives[index.row()];

    switch (role) {
        case Qt::DisplayRole:
        case DescriptionRole:
            return QString::fromStdString(drive.description());

        case DevicePathRole:
            return QString::fromStdString(drive.devicePath);

        case VendorRole:
            return QString::fromStdString(drive.vendor);

        case ModelRole:
            return QString::fromStdString(drive.model);

        case MediaTypeRole:
            return static_cast<int>(drive.media.type);

        case MediaStatusRole:
            return static_cast<int>(drive.media.status);

        case CapacityRole:
            return static_cast<qint64>(drive.media.capacityBytes);

        case FreeSpaceRole:
            return static_cast<qint64>(drive.media.freeBytes);

        case CanWriteCDRole:
            return drive.canWriteCD;

        case CanWriteDVDRole:
            return drive.canWriteDVD;

        case CanWriteBDRole:
            return drive.canWriteBD;

        default:
            return {};
    }
}

QHash<int, QByteArray> DriveListModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[DevicePathRole] = "devicePath";
    roles[VendorRole] = "vendor";
    roles[ModelRole] = "model";
    roles[DescriptionRole] = "description";
    roles[MediaTypeRole] = "mediaType";
    roles[MediaStatusRole] = "mediaStatus";
    roles[CapacityRole] = "capacity";
    roles[FreeSpaceRole] = "freeSpace";
    roles[CanWriteCDRole] = "canWriteCD";
    roles[CanWriteDVDRole] = "canWriteDVD";
    roles[CanWriteBDRole] = "canWriteBD";
    return roles;
}

void DriveListModel::updateDrives(const std::vector<Core::DriveInfo>& drives) {
    beginResetModel();
    m_drives = drives;
    endResetModel();
}

const Core::DriveInfo* DriveListModel::driveAt(int index) const {
    if (index < 0 || index >= static_cast<int>(m_drives.size())) {
        return nullptr;
    }
    return &m_drives[index];
}

QString DriveListModel::devicePathAt(int index) const {
    if (auto* drive = driveAt(index)) {
        return QString::fromStdString(drive->devicePath);
    }
    return {};
}

int DriveListModel::indexOf(const QString& devicePath) const {
    std::string path = devicePath.toStdString();
    for (size_t i = 0; i < m_drives.size(); ++i) {
        if (m_drives[i].devicePath == path) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace Burner::Models
