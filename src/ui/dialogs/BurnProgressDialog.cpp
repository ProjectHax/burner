#include "BurnProgressDialog.hpp"
#include "../Dialogs.hpp"
#include "../widgets/BurnLog.hpp"

#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCloseEvent>
#include <QMessageBox>
#include <QSettings>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

namespace Burner::UI {

BurnProgressDialog::BurnProgressDialog(QWidget* parent)
    : ChromeDialog(parent, tr("Burning Disc")) {
    setModal(true);
    setupUi();
    setupSounds();
    loadSoundSettings();
}

BurnProgressDialog::~BurnProgressDialog() {
    saveSoundSettings();
}

void BurnProgressDialog::setMode(Mode mode) {
    m_mode = mode;

    switch (mode) {
        case BurnMode:
            m_bufferWidget->show();
            m_speedLabel->setText(tr("Speed: --"));
            break;
        case CloneMode:
            m_bufferWidget->hide();
            m_speedLabel->setText(tr("Read: --"));
            break;
        case BlankMode:
            m_bufferWidget->hide();
            m_speedLabel->hide();
            break;
        case IntegrityMode:
            m_bufferWidget->hide();
            m_speedLabel->setText(tr("Read: --"));
            break;
    }
}

void BurnProgressDialog::setProgress(double percent) {
    m_progressBar->setValue(static_cast<int>(percent));
    m_progressLabel->setText(QString("%1%").arg(percent, 0, 'f', 1));
}

void BurnProgressDialog::setStatus(const QString& status) {
    m_statusLabel->setText(status);

    // Add to log if status changed (avoid duplicate entries)
    if (status != m_lastLoggedStatus && !status.isEmpty()) {
        m_lastLoggedStatus = status;
        LogLevel level = determineLogLevel(status);
        m_burnLog->addEntry(level, status);
    }
}

void BurnProgressDialog::setBufferFill(int percent) {
    m_bufferBar->setValue(percent);
}

void BurnProgressDialog::setTimes(int elapsedSeconds, int remainingSeconds) {
    m_timeLabel->setText(tr("Elapsed: %1 | Remaining: %2")
        .arg(formatTime(elapsedSeconds))
        .arg(formatTime(remainingSeconds)));
}

void BurnProgressDialog::setWriteSpeed(int kbps) {
    if (m_mode == BlankMode) {
        return;  // No speed display for blank mode
    }

    QString prefix = (m_mode == CloneMode || m_mode == IntegrityMode) ? tr("Read: ") : tr("Speed: ");

    if (kbps <= 0) {
        m_speedLabel->setText(prefix + "--");
        return;
    }

    double multiplier = 0;
    QString mediaName;

    switch (m_speedMedia) {
        case CD:
            multiplier = kbps / 150.0;   // 1x CD = 150 KB/s
            mediaName = QStringLiteral("CD");
            break;
        case BluRay:
            multiplier = kbps / 4500.0;  // 1x BD = 4500 KB/s
            mediaName = QStringLiteral("BD");
            break;
        case DVD:
        default:
            multiplier = kbps / 1385.0;  // 1x DVD = 1385 KB/s
            mediaName = QStringLiteral("DVD");
            break;
    }

    m_speedLabel->setText(tr("%1%2x %3 (%4 KB/s)")
        .arg(prefix)
        .arg(multiplier, 0, 'f', 1)
        .arg(mediaName)
        .arg(kbps));
}

void BurnProgressDialog::setSpeedMedia(SpeedMedia media) {
    m_speedMedia = media;
}

void BurnProgressDialog::onComplete(bool success, const QString& message) {
    m_burning = false;
    m_progressBar->setValue(100);
    m_cancelButton->hide();
    m_closeButton->show();

    QString completeTitle, failedTitle, successText, failedText;

    switch (m_mode) {
        case CloneMode:
            completeTitle = tr("Clone Complete");
            failedTitle = tr("Clone Failed");
            successText = tr("Clone completed successfully!");
            failedText = tr("Clone failed: %1");
            break;
        case BlankMode:
            completeTitle = tr("Blank Complete");
            failedTitle = tr("Blank Failed");
            successText = tr("Disc blanked successfully!");
            failedText = tr("Blank failed: %1");
            break;
        case IntegrityMode:
            completeTitle = tr("Integrity Check Complete");
            failedTitle = tr("Integrity Check Failed");
            successText = tr("Integrity check passed!");
            failedText = tr("Integrity check failed: %1");
            break;
        case BurnMode:
        default:
            completeTitle = tr("Burn Complete");
            failedTitle = tr("Burn Failed");
            successText = tr("Burn completed successfully!");
            failedText = tr("Burn failed: %1");
            break;
    }

    if (success) {
        setWindowTitle(completeTitle);
        m_statusLabel->setText(successText);
        m_statusLabel->setStyleSheet("QLabel { color: #4caf50; font-weight: bold; }");
        m_burnLog->addEntry(LogLevel::Success, successText);
        playSuccessSound();
    } else {
        setWindowTitle(failedTitle);
        QString errorMsg = failedText.arg(message);
        m_statusLabel->setText(errorMsg);
        m_statusLabel->setStyleSheet("QLabel { color: #ef5350; font-weight: bold; }");
        m_burnLog->addEntry(LogLevel::Error, errorMsg);
        playErrorSound();
    }

    // Expand log to show final status
    m_burnLog->setExpanded(true);
}

void BurnProgressDialog::closeEvent(QCloseEvent* event) {
    if (m_burning) {
        event->ignore();
        reject();
    } else {
        event->accept();
    }
}

void BurnProgressDialog::reject() {
    if (m_burning) {
        QString title, message;

        switch (m_mode) {
            case CloneMode:
                title = tr("Cancel Clone?");
                message = tr("Are you sure you want to cancel the clone operation?");
                break;
            case BlankMode:
                title = tr("Cancel Blank?");
                message = tr("Are you sure you want to cancel the blank operation?\n"
                             "The disc may be left in an inconsistent state.");
                break;
            case IntegrityMode:
                title = tr("Cancel Integrity Check?");
                message = tr("Are you sure you want to cancel the integrity check?");
                break;
            case BurnMode:
            default:
                title = tr("Cancel Burn?");
                message = tr("Are you sure you want to cancel the burn operation?\n"
                             "The disc may be unusable.");
                break;
        }

        QMessageBox::StandardButton result = Dialogs::question(
            this, title, message,
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );

        if (result == QMessageBox::Yes) {
            emit cancelRequested();
            m_statusLabel->setText(tr("Cancelling..."));
            m_cancelButton->setEnabled(false);
        }
    } else {
        QDialog::reject();
    }
}

void BurnProgressDialog::setupUi() {
    auto* mainLayout = body();
    mainLayout->setSpacing(12);

    // Status label
    m_statusLabel = new QLabel(tr("Preparing to burn..."), this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    QFont statusFont = m_statusLabel->font();
    statusFont.setPointSize(statusFont.pointSize() + 1);
    m_statusLabel->setFont(statusFont);
    mainLayout->addWidget(m_statusLabel);

    // Main progress bar with label
    auto* progressLayout = new QHBoxLayout();
    m_progressBar = new QProgressBar(this);
    m_progressBar->setMinimum(0);
    m_progressBar->setMaximum(100);
    m_progressBar->setTextVisible(false);
    m_progressBar->setMinimumWidth(300);
    progressLayout->addWidget(m_progressBar, 1);

    m_progressLabel = new QLabel("0%", this);
    m_progressLabel->setMinimumWidth(50);
    m_progressLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    progressLayout->addWidget(m_progressLabel);
    mainLayout->addLayout(progressLayout);

    // Buffer bar (in container widget so it can be hidden for clone mode)
    m_bufferWidget = new QWidget(this);
    auto* bufferLayout = new QHBoxLayout(m_bufferWidget);
    bufferLayout->setContentsMargins(0, 0, 0, 0);
    m_bufferLabel = new QLabel(tr("Buffer:"), m_bufferWidget);
    bufferLayout->addWidget(m_bufferLabel);
    m_bufferBar = new QProgressBar(m_bufferWidget);
    m_bufferBar->setMinimum(0);
    m_bufferBar->setMaximum(100);
    m_bufferBar->setValue(0);
    m_bufferBar->setMaximumHeight(16);
    bufferLayout->addWidget(m_bufferBar, 1);
    mainLayout->addWidget(m_bufferWidget);

    // Time and speed labels
    auto* infoLayout = new QHBoxLayout();
    m_timeLabel = new QLabel(this);
    m_timeLabel->setAlignment(Qt::AlignLeft);
    infoLayout->addWidget(m_timeLabel, 1);

    m_speedLabel = new QLabel(tr("Speed: --"), this);
    m_speedLabel->setAlignment(Qt::AlignRight);
    infoLayout->addWidget(m_speedLabel);
    mainLayout->addLayout(infoLayout);

    // Burn log (collapsible)
    m_burnLog = new BurnLog(this);
    m_burnLog->addEntry(LogLevel::Info, tr("Burn operation initialized"));
    mainLayout->addWidget(m_burnLog);

    mainLayout->addStretch();

    // Buttons and sound option
    auto* buttonLayout = new QHBoxLayout();

    m_soundCheckbox = new QCheckBox(tr("Play sound on completion"), this);
    m_soundCheckbox->setChecked(m_soundEnabled);
    connect(m_soundCheckbox, &QCheckBox::toggled, this, [this](bool checked) {
        m_soundEnabled = checked;
    });
    buttonLayout->addWidget(m_soundCheckbox);

    buttonLayout->addStretch();

    m_cancelButton = new QPushButton(tr("Cancel"), this);
    connect(m_cancelButton, &QPushButton::clicked, this, &BurnProgressDialog::reject);
    buttonLayout->addWidget(m_cancelButton);

    m_closeButton = new QPushButton(tr("Close"), this);
    m_closeButton->hide();
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(m_closeButton);

    mainLayout->addLayout(buttonLayout);

    setMinimumSize(450, 280);

    // Resize dialog when log is expanded/collapsed
    connect(m_burnLog, &BurnLog::expandedChanged, this, [this](bool expanded) {
        if (expanded) {
            // Expand dialog to show log fully
            setMinimumHeight(500);
            resize(width(), 550);
        } else {
            // Shrink back to compact size
            setMinimumHeight(280);
            resize(width(), 300);
        }
    });
}

QString BurnProgressDialog::formatTime(int seconds) const {
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

LogLevel BurnProgressDialog::determineLogLevel(const QString& status) const {
    QString lower = status.toLower();

    if (lower.contains("prepar") || lower.contains("build") ||
        lower.contains("initial") || lower.contains("validat") ||
        lower.contains("convert"))
        return LogLevel::Prepare;
    if (lower.contains("blank") || lower.contains("eras"))
        return LogLevel::Blank;
    if (lower.contains("writ") || lower.contains("burn"))
        return LogLevel::Write;
    if (lower.contains("clos") || lower.contains("final") || lower.contains("lead"))
        return LogLevel::Close;
    if (lower.contains("verif"))
        return LogLevel::Verify;
    if (lower.contains("error") || lower.contains("fail"))
        return LogLevel::Error;
    if (lower.contains("complet") || lower.contains("success"))
        return LogLevel::Success;
    if (lower.contains("warn") || lower.contains("cancel"))
        return LogLevel::Warning;
    if (lower.contains("drive") || lower.contains("device") ||
        lower.contains("disc") || lower.contains("tray") ||
        lower.contains("open") || lower.contains("media"))
        return LogLevel::Drive;

    return LogLevel::Info;
}

void BurnProgressDialog::setupSounds() {
#ifdef HAVE_QT_MULTIMEDIA
    m_successSound = std::make_unique<QSoundEffect>(this);
    m_errorSound = std::make_unique<QSoundEffect>(this);

    // Try to find system sounds first
    QString successPath = findSystemSound("complete");
    QString errorPath = findSystemSound("dialog-error");

    if (!successPath.isEmpty()) {
        m_successSound->setSource(QUrl::fromLocalFile(successPath));
    } else {
        // Fallback to embedded sound (if available)
        m_successSound->setSource(QUrl("qrc:/sounds/success.wav"));
    }

    if (!errorPath.isEmpty()) {
        m_errorSound->setSource(QUrl::fromLocalFile(errorPath));
    } else {
        // Fallback to embedded sound (if available)
        m_errorSound->setSource(QUrl("qrc:/sounds/error.wav"));
    }

    m_successSound->setVolume(0.7);
    m_errorSound->setVolume(0.7);
#endif
}

void BurnProgressDialog::loadSoundSettings() {
    QSettings settings("Burner", "Burner");
    m_soundEnabled = settings.value("notifications/soundEnabled", true).toBool();
    if (m_soundCheckbox) {
        m_soundCheckbox->setChecked(m_soundEnabled);
    }
}

void BurnProgressDialog::saveSoundSettings() {
    QSettings settings("Burner", "Burner");
    settings.setValue("notifications/soundEnabled", m_soundEnabled);
}

void BurnProgressDialog::playSuccessSound() {
#ifdef HAVE_QT_MULTIMEDIA
    if (m_soundEnabled && m_successSound && m_successSound->status() == QSoundEffect::Ready) {
        m_successSound->play();
    }
#endif
}

void BurnProgressDialog::playErrorSound() {
#ifdef HAVE_QT_MULTIMEDIA
    if (m_soundEnabled && m_errorSound && m_errorSound->status() == QSoundEffect::Ready) {
        m_errorSound->play();
    }
#endif
}

QString BurnProgressDialog::findSystemSound(const QString& soundId) const {
    // Search for freedesktop sound theme sounds
    QStringList searchPaths = {
        "/usr/share/sounds/freedesktop/stereo",
        "/usr/share/sounds/gnome/default/alerts",
        "/usr/share/sounds"
    };

    // Add XDG data dirs
    QString xdgDataDirs = qEnvironmentVariable("XDG_DATA_DIRS", "/usr/local/share:/usr/share");
    for (const QString& dir : xdgDataDirs.split(':')) {
        searchPaths.prepend(dir + "/sounds/freedesktop/stereo");
    }

    // QSoundEffect only supports WAV - skip .oga/.ogg which it can't decode
    QStringList extensions = {".wav"};
    for (const QString& path : searchPaths) {
        for (const QString& ext : extensions) {
            QString fullPath = path + "/" + soundId + ext;
            if (QFile::exists(fullPath)) {
                return fullPath;
            }
        }
    }

    return QString();
}

} // namespace Burner::UI
