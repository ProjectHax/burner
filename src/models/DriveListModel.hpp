#pragma once

#include <QAbstractListModel>
#include <vector>
#include "../core/DriveInfo.hpp"

namespace Burner::Models {

/// Qt list model for optical drives.
/// Provides data for combo boxes and list views.
class DriveListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        DevicePathRole = Qt::UserRole + 1,
        VendorRole,
        ModelRole,
        DescriptionRole,
        MediaTypeRole,
        MediaStatusRole,
        CapacityRole,
        FreeSpaceRole,
        CanWriteCDRole,
        CanWriteDVDRole,
        CanWriteBDRole
    };

    explicit DriveListModel(QObject* parent = nullptr);

    // QAbstractListModel interface
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// Update the drive list
    void updateDrives(const std::vector<Core::DriveInfo>& drives);

    /// Get drive info at index
    [[nodiscard]] const Core::DriveInfo* driveAt(int index) const;

    /// Get device path at index
    [[nodiscard]] QString devicePathAt(int index) const;

    /// Find index of device path
    [[nodiscard]] int indexOf(const QString& devicePath) const;

private:
    std::vector<Core::DriveInfo> m_drives;
};

} // namespace Burner::Models
