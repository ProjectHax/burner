#include "ChecksumWorker.hpp"
#include "../core/ChecksumCalculator.hpp"

#include <QFileInfo>

namespace Burner::Engine {

ChecksumWorker::ChecksumWorker(QObject* parent)
    : QObject(parent) {
}

ChecksumWorker::~ChecksumWorker() = default;

void ChecksumWorker::calculateChecksums(const QString& filePath) {
    m_cancelled.store(false);

    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isReadable()) {
        emit finished(false, tr("File not found or not readable: %1").arg(filePath));
        return;
    }

    Core::ChecksumCalculator calculator;

    auto progress = [this](std::uint64_t bytesProcessed, std::uint64_t totalBytes) {
        double percent = totalBytes > 0
            ? (static_cast<double>(bytesProcessed) / static_cast<double>(totalBytes)) * 100.0
            : 0.0;
        emit progressUpdated(percent, bytesProcessed, totalBytes);
    };

    auto results = calculator.calculateMultiple(
        filePath.toStdString(),
        {Core::HashAlgorithm::MD5, Core::HashAlgorithm::SHA256},
        progress);

    if (m_cancelled.load()) {
        emit finished(false, tr("Cancelled"));
        return;
    }

    bool allSuccess = true;
    for (const auto& result : results) {
        if (result.success) {
            emit checksumReady(
                QString::fromStdString(Core::ChecksumCalculator::algorithmName(result.algorithm)),
                QString::fromStdString(result.hexDigest));
        } else {
            allSuccess = false;
        }
    }

    if (allSuccess) {
        emit finished(true, tr("Checksums calculated successfully"));
    } else {
        QString error;
        for (const auto& r : results) {
            if (!r.success) {
                error = QString::fromStdString(r.error);
                break;
            }
        }
        emit finished(false, error);
    }
}

void ChecksumWorker::cancel() {
    m_cancelled.store(true);
}

} // namespace Burner::Engine
