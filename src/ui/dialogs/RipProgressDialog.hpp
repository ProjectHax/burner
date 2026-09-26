#pragma once

#include "../widgets/WindowChrome.hpp"

#include <QDialog>

class QProgressBar;
class QLabel;
class QPushButton;

namespace Burner::UI {

/// Modal dialog showing CD ripping progress.
class RipProgressDialog : public ChromeDialog {
    Q_OBJECT

public:
    explicit RipProgressDialog(QWidget* parent = nullptr);
    ~RipProgressDialog() override;

public slots:
    /// Update overall progress (0.0 - 100.0)
    void setProgress(double percent);

    /// Update track progress
    void setTrackProgress(int currentTrack, int totalTracks, double trackPercent);

    /// Set current status message
    void setStatus(const QString& status);

    /// Called when rip completes
    void onComplete(bool success, const QString& message);

signals:
    void cancelRequested();

protected:
    void closeEvent(QCloseEvent* event) override;
    void reject() override;

private:
    void setupUi();

    QProgressBar* m_overallBar{nullptr};
    QProgressBar* m_trackBar{nullptr};
    QLabel* m_statusLabel{nullptr};
    QLabel* m_trackLabel{nullptr};
    QPushButton* m_cancelButton{nullptr};
    QPushButton* m_closeButton{nullptr};
    bool m_ripping{true};
};

} // namespace Burner::UI
