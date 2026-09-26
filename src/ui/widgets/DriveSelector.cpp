#include "DriveSelector.hpp"
#include "../../models/DriveListModel.hpp"

namespace Burner::UI {

DriveSelector::DriveSelector(QWidget* parent)
    : QWidget(parent) {
    setupUi();
}

void DriveSelector::setupUi() {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Drive label
    auto* label = new QLabel(tr("Drive:"), this);
    layout->addWidget(label);

    // Drive combo box
    m_combo = new QComboBox(this);
    m_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_combo->setMinimumWidth(200);
    layout->addWidget(m_combo);

    // Media status label
    m_mediaLabel = new QLabel(this);
    m_mediaLabel->setMinimumWidth(100);
    layout->addWidget(m_mediaLabel);

    // Refresh button
    m_refreshButton = new QPushButton(tr("Refresh"), this);
    m_refreshButton->setToolTip(tr("Scan for drives"));
    layout->addWidget(m_refreshButton);

    // Eject button
    m_ejectButton = new QPushButton(tr("Eject"), this);
    m_ejectButton->setToolTip(tr("Eject disc"));
    layout->addWidget(m_ejectButton);

    // Connections
    connect(m_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DriveSelector::onCurrentIndexChanged);
    connect(m_refreshButton, &QPushButton::clicked,
            this, &DriveSelector::onRefreshClicked);
    connect(m_ejectButton, &QPushButton::clicked,
            this, &DriveSelector::onEjectClicked);
}

void DriveSelector::setModel(Models::DriveListModel* model) {
    m_model = model;
    m_combo->setModel(model);

    if (model && model->rowCount() > 0) {
        m_combo->setCurrentIndex(0);
    }
}

QString DriveSelector::selectedDevice() const {
    if (!m_model || m_combo->currentIndex() < 0) {
        return {};
    }
    return m_model->devicePathAt(m_combo->currentIndex());
}

void DriveSelector::setSelectedDevice(const QString& devicePath) {
    if (!m_model) {
        return;
    }

    int index = m_model->indexOf(devicePath);
    if (index >= 0) {
        m_combo->setCurrentIndex(index);
    }
}

void DriveSelector::onCurrentIndexChanged(int index) {
    if (!m_model || index < 0) {
        m_mediaLabel->clear();
        m_ejectButton->setEnabled(false);
        return;
    }

    const auto* drive = m_model->driveAt(index);
    if (!drive) {
        m_mediaLabel->clear();
        m_ejectButton->setEnabled(false);
        return;
    }

    // Update media status label
    QString statusText;
    switch (drive->media.status) {
        case Core::MediaStatus::NoMedia:
            statusText = tr("No disc");
            break;
        case Core::MediaStatus::Blank:
            statusText = tr("Blank %1").arg(QString::fromStdString(drive->media.profileName));
            break;
        case Core::MediaStatus::Appendable:
            statusText = tr("Appendable");
            break;
        case Core::MediaStatus::Full:
            statusText = tr("Full");
            break;
        case Core::MediaStatus::Busy:
            statusText = tr("Mounted (unmount to burn)");
            break;
        default:
            statusText = QString::fromStdString(drive->media.profileName);
            break;
    }

    m_mediaLabel->setText(statusText);
    m_ejectButton->setEnabled(true);

    emit selectionChanged(QString::fromStdString(drive->devicePath));
}

void DriveSelector::onRefreshClicked() {
    emit refreshRequested();
}

void DriveSelector::onEjectClicked() {
    QString device = selectedDevice();
    if (!device.isEmpty()) {
        emit ejectRequested(device);
    }
}

void DriveSelector::setControlsEnabled(bool enabled) {
    m_combo->setEnabled(enabled);
    m_refreshButton->setEnabled(enabled);
    // Eject button depends on having a valid selection
    m_ejectButton->setEnabled(enabled && m_combo->currentIndex() >= 0);
}

} // namespace Burner::UI
