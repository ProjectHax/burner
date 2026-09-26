#pragma once

#include <QWidget>

class QLineEdit;
class QLabel;
class QPushButton;
class QGroupBox;
class QProgressBar;
class QDragEnterEvent;
class QDropEvent;
class QThread;

namespace Burner::Engine {
class ChecksumWorker;
}

namespace Burner::UI {

/// Page for burning ISO image files to disc.
class IsoWriterPage : public QWidget {
    Q_OBJECT

public:
    explicit IsoWriterPage(QWidget* parent = nullptr);
    ~IsoWriterPage() override;

    /// Get the selected ISO file path
    [[nodiscard]] QString isoPath() const;

    /// Get the ISO file size in bytes
    [[nodiscard]] qint64 isoSize() const { return m_isoSize; }

    /// Check if a valid ISO is selected
    [[nodiscard]] bool hasValidIso() const { return m_isValidIso; }

    /// Check if a CUE file is selected (vs plain ISO/BIN)
    [[nodiscard]] bool isCueImage() const { return m_isCueImage; }

public slots:
    void browseIso();
    void setIsoPath(const QString& path);
    void clearSelection();

signals:
    void isoSelected(const QString& path);
    void isoCleared();

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void setupUi();
    void updateIsoInfo(const QString& path);
    void updateCueInfo(const QString& path);
    bool validateIsoFile(const QString& path);
    QString readIsoVolumeLabel(const QString& path);
    QString formatSize(qint64 bytes) const;

    // Checksum
    void startChecksumCalculation(const QString& path);
    void cleanupChecksumWorker();
    void onChecksumReady(const QString& algorithm, const QString& hexDigest);
    void onChecksumFinished(bool success, const QString& message);
    void verifyChecksum();

    QLineEdit* m_pathEdit{nullptr};
    QPushButton* m_browseButton{nullptr};
    QPushButton* m_clearButton{nullptr};
    QGroupBox* m_infoGroup{nullptr};
    QLabel* m_volumeLabel{nullptr};
    QLabel* m_sizeLabel{nullptr};
    QLabel* m_typeLabel{nullptr};
    QLabel* m_trackListLabel{nullptr};
    QLabel* m_discRecommendation{nullptr};
    QLabel* m_dropHint{nullptr};
    qint64 m_isoSize{0};
    bool m_isValidIso{false};
    bool m_isCueImage{false};

    // Checksum widgets
    QLabel* m_md5Label{nullptr};
    QLabel* m_sha256Label{nullptr};
    QProgressBar* m_checksumProgress{nullptr};
    QLineEdit* m_verifyInput{nullptr};
    QPushButton* m_verifyButton{nullptr};
    QLabel* m_verifyResult{nullptr};

    // Checksum worker
    QThread* m_checksumThread{nullptr};
    Engine::ChecksumWorker* m_checksumWorker{nullptr};
    QString m_cachedMd5;
    QString m_cachedSha256;
    QString m_checksumFilePath;
};

} // namespace Burner::UI
