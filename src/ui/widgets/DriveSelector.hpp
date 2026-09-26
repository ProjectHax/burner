#pragma once

#include <QWidget>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>

namespace Burner::Models {
class DriveListModel;
}

namespace Burner::UI {

/// Widget for selecting an optical drive.
/// Shows drive name and media status.
class DriveSelector : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QString selectedDevice READ selectedDevice NOTIFY selectionChanged)

public:
    explicit DriveSelector(QWidget* parent = nullptr);

    /// Set the drive list model
    void setModel(Models::DriveListModel* model);

    /// Get currently selected device path
    [[nodiscard]] QString selectedDevice() const;

    /// Set selected device by path
    void setSelectedDevice(const QString& devicePath);

    /// Enable or disable all drive-related controls
    void setControlsEnabled(bool enabled);

signals:
    void selectionChanged(const QString& devicePath);
    void refreshRequested();
    void ejectRequested(const QString& devicePath);

private slots:
    void onCurrentIndexChanged(int index);
    void onRefreshClicked();
    void onEjectClicked();

private:
    void setupUi();

    QComboBox* m_combo{nullptr};
    QLabel* m_mediaLabel{nullptr};
    QPushButton* m_refreshButton{nullptr};
    QPushButton* m_ejectButton{nullptr};
    Models::DriveListModel* m_model{nullptr};
};

} // namespace Burner::UI
