#pragma once

#include <QObject>
#include <QString>
#include <atomic>

namespace Burner::Engine {

/// Worker that computes file checksums in a background thread.
/// Should be moved to a QThread before use.
class ChecksumWorker : public QObject {
    Q_OBJECT

public:
    explicit ChecksumWorker(QObject* parent = nullptr);
    ~ChecksumWorker() override;

public slots:
    /// Calculate MD5 and SHA-256 for a file (single pass)
    void calculateChecksums(const QString& filePath);

    /// Request cancellation
    void cancel();

signals:
    /// Progress update
    void progressUpdated(double percent, quint64 bytesProcessed, quint64 totalBytes);

    /// Emitted for each completed algorithm
    void checksumReady(const QString& algorithm, const QString& hexDigest);

    /// All checksums complete
    void finished(bool success, const QString& message);

private:
    std::atomic<bool> m_cancelled{false};
};

} // namespace Burner::Engine
