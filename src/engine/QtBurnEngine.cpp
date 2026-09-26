#include "QtBurnEngine.hpp"
#include "BurnWorker.hpp"
#include "../core/libburnia/BurniaInit.hpp"
#include "../core/DriveManager.hpp"

#include <QThread>
#include <QMetaType>

namespace Burner::Engine {

class QtBurnEngine::Impl {
public:
    std::unique_ptr<Core::BurniaInit> init;
    std::unique_ptr<Core::DriveManager> driveManager;
    QString initError;
    bool burning{false};
    double progress{0.0};
    QString status;

    QThread* workerThread{nullptr};
    BurnWorker* worker{nullptr};
};

QtBurnEngine::QtBurnEngine(QObject* parent)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>()) {

    // Register metatypes for signal/slot
    qRegisterMetaType<Core::BurnOptions>("Core::BurnOptions");
    qRegisterMetaType<Burner::Core::BurnOptions>("Burner::Core::BurnOptions");
    qRegisterMetaType<quint64>("quint64");
    qRegisterMetaType<FilePathMappingList>("FilePathMappingList");

    try {
        m_impl->init = std::make_unique<Core::BurniaInit>();
        m_impl->driveManager = std::make_unique<Core::DriveManager>(*m_impl->init);
        m_impl->status = tr("Ready");
    } catch (const std::exception& e) {
        m_impl->initError = QString::fromUtf8(e.what());
        m_impl->status = tr("Initialization failed");
    }
}

QtBurnEngine::~QtBurnEngine() {
    cleanupWorker();
}

bool QtBurnEngine::isBurning() const {
    return m_impl->burning;
}

double QtBurnEngine::progress() const {
    return m_impl->progress;
}

QString QtBurnEngine::status() const {
    return m_impl->status;
}

Core::DriveManager& QtBurnEngine::driveManager() {
    if (!m_impl->driveManager) {
        throw std::runtime_error("Engine not initialized");
    }
    return *m_impl->driveManager;
}

bool QtBurnEngine::isInitialized() const {
    return m_impl->init && m_impl->init->isInitialized();
}

QString QtBurnEngine::initError() const {
    return m_impl->initError;
}

void QtBurnEngine::burnDataDisc(const QString& device,
                                 const QStringList& files,
                                 const QString& volumeLabel,
                                 const Core::BurnOptions& options) {
    if (m_impl->burning) {
        emit burnError(tr("A burn operation is already in progress"));
        return;
    }

    if (!isInitialized()) {
        emit burnError(tr("Engine not initialized: %1").arg(m_impl->initError));
        return;
    }

    if (files.isEmpty()) {
        emit burnError(tr("No files to burn"));
        return;
    }

    startWorker(device);

    // Invoke burn operation in worker thread
    QMetaObject::invokeMethod(m_impl->worker, "burnDataDisc",
                               Qt::QueuedConnection,
                               Q_ARG(QString, device),
                               Q_ARG(QStringList, files),
                               Q_ARG(QString, volumeLabel),
                               Q_ARG(Core::BurnOptions, options));
}

void QtBurnEngine::burnDataDiscWithStructure(const QString& device,
                                              const FilePathMappingList& fileMappings,
                                              const QString& volumeLabel,
                                              const Core::BurnOptions& options) {
    if (m_impl->burning) {
        emit burnError(tr("A burn operation is already in progress"));
        return;
    }

    if (!isInitialized()) {
        emit burnError(tr("Engine not initialized: %1").arg(m_impl->initError));
        return;
    }

    if (fileMappings.isEmpty()) {
        emit burnError(tr("No files to burn"));
        return;
    }

    startWorker(device);

    // Invoke burn operation in worker thread
    QMetaObject::invokeMethod(m_impl->worker, "burnDataDiscWithStructure",
                               Qt::QueuedConnection,
                               Q_ARG(QString, device),
                               Q_ARG(FilePathMappingList, fileMappings),
                               Q_ARG(QString, volumeLabel),
                               Q_ARG(Core::BurnOptions, options));
}

void QtBurnEngine::burnAudioCD(const QString& device,
                                const QStringList& audioFiles,
                                const Core::BurnOptions& options) {
    if (m_impl->burning) {
        emit burnError(tr("A burn operation is already in progress"));
        return;
    }

    if (!isInitialized()) {
        emit burnError(tr("Engine not initialized: %1").arg(m_impl->initError));
        return;
    }

    if (audioFiles.isEmpty()) {
        emit burnError(tr("No audio files to burn"));
        return;
    }

    startWorker(device);

    // Invoke burn operation in worker thread
    QMetaObject::invokeMethod(m_impl->worker, "burnAudioCD",
                               Qt::QueuedConnection,
                               Q_ARG(QString, device),
                               Q_ARG(QStringList, audioFiles),
                               Q_ARG(Core::BurnOptions, options));
}

void QtBurnEngine::burnIsoFile(const QString& device,
                                const QString& isoPath,
                                const Core::BurnOptions& options) {
    if (m_impl->burning) {
        emit burnError(tr("A burn operation is already in progress"));
        return;
    }

    if (!isInitialized()) {
        emit burnError(tr("Engine not initialized: %1").arg(m_impl->initError));
        return;
    }

    startWorker(device);

    // Invoke burn operation in worker thread
    QMetaObject::invokeMethod(m_impl->worker, "burnIsoFile",
                               Qt::QueuedConnection,
                               Q_ARG(QString, device),
                               Q_ARG(QString, isoPath),
                               Q_ARG(Core::BurnOptions, options));
}

void QtBurnEngine::burnCueImage(const QString& device,
                                 const QString& cuePath,
                                 const Core::BurnOptions& options) {
    if (m_impl->burning) {
        emit burnError(tr("A burn operation is already in progress"));
        return;
    }

    if (!isInitialized()) {
        emit burnError(tr("Engine not initialized: %1").arg(m_impl->initError));
        return;
    }

    startWorker(device);

    QMetaObject::invokeMethod(m_impl->worker, "burnCueImage",
                               Qt::QueuedConnection,
                               Q_ARG(QString, device),
                               Q_ARG(QString, cuePath),
                               Q_ARG(Core::BurnOptions, options));
}

void QtBurnEngine::blankDisc(const QString& device, bool fullBlank) {
    if (m_impl->burning) {
        emit burnError(tr("An operation is already in progress"));
        return;
    }

    if (!isInitialized()) {
        emit burnError(tr("Engine not initialized: %1").arg(m_impl->initError));
        return;
    }

    startWorker(device);

    QMetaObject::invokeMethod(m_impl->worker, "blankDisc",
                               Qt::QueuedConnection,
                               Q_ARG(QString, device),
                               Q_ARG(bool, fullBlank));
}

void QtBurnEngine::checkIntegrity(const QString& device, bool stopOnFirstError) {
    if (m_impl->burning) {
        emit burnError(tr("An operation is already in progress"));
        return;
    }

    if (!isInitialized()) {
        emit burnError(tr("Engine not initialized: %1").arg(m_impl->initError));
        return;
    }

    startWorker(device);

    QMetaObject::invokeMethod(m_impl->worker, "checkIntegrity",
                               Qt::QueuedConnection,
                               Q_ARG(QString, device),
                               Q_ARG(bool, stopOnFirstError));
}

void QtBurnEngine::cloneToImage(const QString& device, const QString& outputPath) {
    if (m_impl->burning) {
        emit burnError(tr("An operation is already in progress"));
        return;
    }

    if (!isInitialized()) {
        emit burnError(tr("Engine not initialized: %1").arg(m_impl->initError));
        return;
    }

    if (outputPath.isEmpty()) {
        emit burnError(tr("No output path specified"));
        return;
    }

    startWorker(device);

    QMetaObject::invokeMethod(m_impl->worker, "cloneToImage",
                               Qt::QueuedConnection,
                               Q_ARG(QString, device),
                               Q_ARG(QString, outputPath));
}

void QtBurnEngine::cloneToBinCue(const QString& device, const QString& outputPath) {
    if (m_impl->burning) {
        emit burnError(tr("An operation is already in progress"));
        return;
    }

    if (!isInitialized()) {
        emit burnError(tr("Engine not initialized: %1").arg(m_impl->initError));
        return;
    }

    if (outputPath.isEmpty()) {
        emit burnError(tr("No output path specified"));
        return;
    }

    startWorker(device);

    QMetaObject::invokeMethod(m_impl->worker, "cloneToBinCue",
                               Qt::QueuedConnection,
                               Q_ARG(QString, device),
                               Q_ARG(QString, outputPath));
}

void QtBurnEngine::cloneFromImage(const QString& device, const QString& imagePath,
                                   bool verify, bool ejectAfter) {
    if (m_impl->burning) {
        emit burnError(tr("An operation is already in progress"));
        return;
    }

    if (!isInitialized()) {
        emit burnError(tr("Engine not initialized: %1").arg(m_impl->initError));
        return;
    }

    if (imagePath.isEmpty()) {
        emit burnError(tr("No image file specified"));
        return;
    }

    startWorker(device);

    QMetaObject::invokeMethod(m_impl->worker, "cloneFromImage",
                               Qt::QueuedConnection,
                               Q_ARG(QString, device),
                               Q_ARG(QString, imagePath),
                               Q_ARG(bool, verify),
                               Q_ARG(bool, ejectAfter));
}

void QtBurnEngine::burnVideoCd(const QString& device,
                                const QStringList& videoFiles,
                                const QString& vcdType,
                                const Core::BurnOptions& options) {
    if (m_impl->burning) {
        emit burnError(tr("An operation is already in progress"));
        return;
    }

    if (!isInitialized()) {
        emit burnError(tr("Engine not initialized: %1").arg(m_impl->initError));
        return;
    }

    if (videoFiles.isEmpty()) {
        emit burnError(tr("No video files to burn"));
        return;
    }

    startWorker(device);

    QMetaObject::invokeMethod(m_impl->worker, "burnVideoCd",
                               Qt::QueuedConnection,
                               Q_ARG(QString, device),
                               Q_ARG(QStringList, videoFiles),
                               Q_ARG(QString, vcdType),
                               Q_ARG(Core::BurnOptions, options));
}

void QtBurnEngine::createIsoFile(const QString& outputPath,
                                  const QStringList& files,
                                  const QString& volumeLabel) {
    if (m_impl->burning) {
        emit burnError(tr("An operation is already in progress"));
        return;
    }

    // ISO creation doesn't require libburn initialization, but we check anyway
    // to ensure the engine is in a good state
    if (!isInitialized()) {
        emit burnError(tr("Engine not initialized: %1").arg(m_impl->initError));
        return;
    }

    if (files.isEmpty()) {
        emit burnError(tr("No files to add to ISO"));
        return;
    }

    if (outputPath.isEmpty()) {
        emit burnError(tr("No output path specified"));
        return;
    }

    // Use empty device since we're not burning to a drive
    startWorker(QString());

    QMetaObject::invokeMethod(m_impl->worker, "createIsoFile",
                               Qt::QueuedConnection,
                               Q_ARG(QString, outputPath),
                               Q_ARG(QStringList, files),
                               Q_ARG(QString, volumeLabel));
}

void QtBurnEngine::createIsoFileWithStructure(const QString& outputPath,
                                               const FilePathMappingList& fileMappings,
                                               const QString& volumeLabel) {
    if (m_impl->burning) {
        emit burnError(tr("An operation is already in progress"));
        return;
    }

    if (!isInitialized()) {
        emit burnError(tr("Engine not initialized: %1").arg(m_impl->initError));
        return;
    }

    if (fileMappings.isEmpty()) {
        emit burnError(tr("No files to add to ISO"));
        return;
    }

    if (outputPath.isEmpty()) {
        emit burnError(tr("No output path specified"));
        return;
    }

    // Use empty device since we're not burning to a drive
    startWorker(QString());

    QMetaObject::invokeMethod(m_impl->worker, "createIsoFileWithStructure",
                               Qt::QueuedConnection,
                               Q_ARG(QString, outputPath),
                               Q_ARG(FilePathMappingList, fileMappings),
                               Q_ARG(QString, volumeLabel));
}

void QtBurnEngine::cancel() {
    if (!m_impl->burning || !m_impl->worker) {
        return;
    }

    m_impl->status = tr("Cancelling...");
    emit statusChanged(m_impl->status);

    // Use DirectConnection since m_cancelled is atomic and we need
    // immediate effect (QueuedConnection won't work if worker is blocked in I/O)
    QMetaObject::invokeMethod(m_impl->worker, "cancel",
                               Qt::DirectConnection);
}

void QtBurnEngine::ejectDrive(const QString& device) {
    if (!m_impl->driveManager) {
        return;
    }

    bool success = m_impl->driveManager->ejectDrive(device.toStdString());
    if (success) {
        m_impl->status = tr("Ejected %1").arg(device);
    } else {
        m_impl->status = tr("Failed to eject %1").arg(device);
    }
    emit statusChanged(m_impl->status);
    emit drivesChanged();
}

void QtBurnEngine::startWorker(const QString& device) {
    // Clean up any existing worker
    cleanupWorker();

    // Create new worker thread
    m_impl->workerThread = new QThread(this);
    m_impl->worker = new BurnWorker(); // No parent - will be moved to thread

    m_impl->worker->moveToThread(m_impl->workerThread);

    // Connect worker signals
    connect(m_impl->workerThread, &QThread::finished,
            m_impl->worker, &QObject::deleteLater);

    connect(m_impl->worker, &BurnWorker::progressUpdated,
            this, &QtBurnEngine::onWorkerProgress);

    connect(m_impl->worker, &BurnWorker::progressDetail,
            this, [this](const QString& operation, int elapsed, int remaining) {
                emit timeUpdate(elapsed, remaining);
            });

    connect(m_impl->worker, &BurnWorker::statusChanged,
            this, &QtBurnEngine::onWorkerStatus);

    connect(m_impl->worker, &BurnWorker::finished,
            this, &QtBurnEngine::onWorkerFinished);

    connect(m_impl->worker, &BurnWorker::error,
            this, &QtBurnEngine::onWorkerError);

    // Start the thread
    m_impl->workerThread->start();

    m_impl->burning = true;
    m_impl->progress = 0.0;
    emit burningChanged(true);
    emit progressChanged(0.0);
}

void QtBurnEngine::cleanupWorker() {
    if (m_impl->workerThread) {
        m_impl->workerThread->quit();
        m_impl->workerThread->wait(5000); // Wait up to 5 seconds

        if (m_impl->workerThread->isRunning()) {
            m_impl->workerThread->terminate();
            m_impl->workerThread->wait();
        }

        delete m_impl->workerThread;
        m_impl->workerThread = nullptr;
        m_impl->worker = nullptr; // Deleted by deleteLater
    }
}

void QtBurnEngine::onWorkerFinished(bool success, const QString& message) {
    m_impl->burning = false;
    m_impl->status = message;

    emit burningChanged(false);
    emit statusChanged(message);
    emit burnComplete(success, message);

    // Clean up worker after a short delay
    QMetaObject::invokeMethod(this, "cleanupWorker", Qt::QueuedConnection);
}

void QtBurnEngine::onWorkerProgress(double percent, quint64 bytesWritten,
                                     quint64 totalBytes, int bufferFill, int writeSpeedKBps) {
    m_impl->progress = percent;
    emit progressChanged(percent);
    emit progressDetail(bytesWritten, totalBytes, bufferFill, writeSpeedKBps);
}

void QtBurnEngine::onWorkerStatus(const QString& status) {
    m_impl->status = status;
    emit statusChanged(status);
}

void QtBurnEngine::onWorkerError(const QString& error) {
    emit burnError(error);
}

} // namespace Burner::Engine
