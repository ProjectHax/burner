#pragma once

#include "../widgets/WindowChrome.hpp"

#include <QDialog>

class QLineEdit;
class QLabel;
class QPushButton;
class QProgressBar;
class QThread;

namespace Burner::Engine {
class ChecksumWorker;
}

namespace Burner::UI {

/// Standalone checksum calculation and verification dialog.
class ChecksumDialog : public ChromeDialog {
    Q_OBJECT

public:
    explicit ChecksumDialog(QWidget* parent = nullptr);
    ~ChecksumDialog() override;

private slots:
    void browseFile();
    void calculateChecksums();
    void verifyChecksum();
    void onChecksumReady(const QString& algorithm, const QString& hexDigest);
    void onChecksumFinished(bool success, const QString& message);
    void copyMd5();
    void copySha256();

private:
    void setupUi();
    void cleanupWorker();

    QLineEdit* m_filePathEdit{nullptr};
    QPushButton* m_browseButton{nullptr};
    QPushButton* m_calculateButton{nullptr};
    QProgressBar* m_progressBar{nullptr};
    QLabel* m_statusLabel{nullptr};

    QLineEdit* m_md5Result{nullptr};
    QLineEdit* m_sha256Result{nullptr};
    QPushButton* m_copyMd5Button{nullptr};
    QPushButton* m_copySha256Button{nullptr};

    QLineEdit* m_verifyInput{nullptr};
    QPushButton* m_verifyButton{nullptr};
    QLabel* m_verifyResult{nullptr};

    QThread* m_workerThread{nullptr};
    Engine::ChecksumWorker* m_worker{nullptr};

    QString m_cachedMd5;
    QString m_cachedSha256;
};

} // namespace Burner::UI
