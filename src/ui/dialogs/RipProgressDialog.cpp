#include "RipProgressDialog.hpp"

#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCloseEvent>

namespace Burner::UI {

RipProgressDialog::RipProgressDialog(QWidget* parent)
    : ChromeDialog(parent, tr("Ripping CD")) {
    setModal(true);
    setMinimumWidth(400);
    setupUi();
}

RipProgressDialog::~RipProgressDialog() = default;

void RipProgressDialog::setupUi() {
    auto* layout = body();
    layout->setSpacing(12);

    // Status
    m_statusLabel = new QLabel(tr("Starting..."), this);
    layout->addWidget(m_statusLabel);

    // Overall progress
    layout->addWidget(new QLabel(tr("Overall Progress:"), this));
    m_overallBar = new QProgressBar(this);
    m_overallBar->setRange(0, 100);
    m_overallBar->setValue(0);
    layout->addWidget(m_overallBar);

    // Track progress
    m_trackLabel = new QLabel(tr("Track 0/0"), this);
    layout->addWidget(m_trackLabel);
    m_trackBar = new QProgressBar(this);
    m_trackBar->setRange(0, 100);
    m_trackBar->setValue(0);
    layout->addWidget(m_trackBar);

    layout->addStretch();

    // Buttons
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_cancelButton = new QPushButton(tr("Cancel"), this);
    connect(m_cancelButton, &QPushButton::clicked, this, [this]() {
        m_cancelButton->setEnabled(false);
        m_cancelButton->setText(tr("Cancelling..."));
        emit cancelRequested();
    });
    buttonLayout->addWidget(m_cancelButton);

    m_closeButton = new QPushButton(tr("Close"), this);
    m_closeButton->setVisible(false);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(m_closeButton);

    layout->addLayout(buttonLayout);
}

void RipProgressDialog::setProgress(double percent) {
    m_overallBar->setValue(static_cast<int>(percent));
}

void RipProgressDialog::setTrackProgress(int currentTrack, int totalTracks, double trackPercent) {
    m_trackLabel->setText(tr("Track %1/%2").arg(currentTrack).arg(totalTracks));
    m_trackBar->setValue(static_cast<int>(trackPercent));
}

void RipProgressDialog::setStatus(const QString& status) {
    m_statusLabel->setText(status);
}

void RipProgressDialog::onComplete(bool success, const QString& message) {
    m_ripping = false;

    m_overallBar->setValue(100);
    m_trackBar->setValue(100);

    if (success) {
        m_statusLabel->setText(tr("Complete!"));
    } else {
        m_statusLabel->setText(tr("Failed: %1").arg(message));
    }

    m_cancelButton->setVisible(false);
    m_closeButton->setVisible(true);
    m_closeButton->setDefault(true);
    m_closeButton->setFocus();
}

void RipProgressDialog::closeEvent(QCloseEvent* event) {
    if (m_ripping) {
        emit cancelRequested();
        event->ignore();
    } else {
        event->accept();
    }
}

void RipProgressDialog::reject() {
    if (m_ripping) {
        emit cancelRequested();
    } else {
        QDialog::reject();
    }
}

} // namespace Burner::UI
