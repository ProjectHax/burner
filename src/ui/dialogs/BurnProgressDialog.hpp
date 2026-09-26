#pragma once

#include "../widgets/WindowChrome.hpp"

#include <QDialog>
#include <memory>

#ifdef HAVE_QT_MULTIMEDIA
#include <QSoundEffect>
#endif

class QProgressBar;
class QLabel;
class QPushButton;
class QCheckBox;

namespace Burner::UI {

class BurnLog;
enum class LogLevel;

/// Modal dialog showing burn progress.
class BurnProgressDialog : public ChromeDialog {
    Q_OBJECT

public:
    enum Mode {
        BurnMode,       ///< Burning disc - show buffer, write speed
        CloneMode,      ///< Cloning disc - show read speed only
        BlankMode,      ///< Blanking disc - no buffer, no speed
        IntegrityMode   ///< Checking integrity - show read speed
    };

    enum SpeedMedia { CD, DVD, BluRay };

    explicit BurnProgressDialog(QWidget* parent = nullptr);
    ~BurnProgressDialog() override;

    /// Set the dialog mode (call before showing)
    void setMode(Mode mode);

public slots:
    /// Update progress (0.0 - 100.0)
    void setProgress(double percent);

    /// Set current status message
    void setStatus(const QString& status);

    /// Set buffer fill level (0-100)
    void setBufferFill(int percent);

    /// Set elapsed and remaining time
    void setTimes(int elapsedSeconds, int remainingSeconds);

    /// Set current write speed
    void setWriteSpeed(int kbps);

    /// Set media type for correct speed multiplier display (CD/DVD/BD)
    void setSpeedMedia(SpeedMedia media);

    /// Called when burn completes
    void onComplete(bool success, const QString& message);

signals:
    void cancelRequested();

protected:
    void closeEvent(QCloseEvent* event) override;
    void reject() override;

private:
    void setupUi();
    void setupSounds();
    void loadSoundSettings();
    void saveSoundSettings();
    QString formatTime(int seconds) const;
    LogLevel determineLogLevel(const QString& status) const;
    void playSuccessSound();
    void playErrorSound();
    QString findSystemSound(const QString& soundId) const;

    QProgressBar* m_progressBar{nullptr};
    QProgressBar* m_bufferBar{nullptr};
    QLabel* m_bufferLabel{nullptr};
    QLabel* m_statusLabel{nullptr};
    QLabel* m_progressLabel{nullptr};
    QLabel* m_timeLabel{nullptr};
    QLabel* m_speedLabel{nullptr};
    QPushButton* m_cancelButton{nullptr};
    QPushButton* m_closeButton{nullptr};
    QWidget* m_bufferWidget{nullptr};
    bool m_burning{true};
    Mode m_mode{BurnMode};
    SpeedMedia m_speedMedia{DVD};

    // Burn log
    BurnLog* m_burnLog{nullptr};
    QString m_lastLoggedStatus;

    // Sound notifications
    QCheckBox* m_soundCheckbox{nullptr};
    bool m_soundEnabled{true};
#ifdef HAVE_QT_MULTIMEDIA
    std::unique_ptr<QSoundEffect> m_successSound;
    std::unique_ptr<QSoundEffect> m_errorSound;
#endif
};

} // namespace Burner::UI
