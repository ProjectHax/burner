#include "DriveMonitor.hpp"
#include "../core/DriveManager.hpp"

#include <QDebug>
#include <QMutexLocker>
#include <QCoreApplication>

namespace Burner::Engine {

// DriveScanWorker implementation

DriveScanWorker::DriveScanWorker(Core::DriveManager& driveManager)
    : m_driveManager(driveManager) {
}

void DriveScanWorker::scanQuick() {
    if (m_stopped) {
        return;
    }

    auto drives = m_driveManager.scanDrives(false); // Quick scan without grab

    if (!m_stopped) {
        emit scanComplete(drives);
    }
}

void DriveScanWorker::scanFull() {
    if (m_stopped) {
        return;
    }

    auto drives = m_driveManager.scanDrives(true); // Full scan with grab

    if (!m_stopped) {
        emit scanComplete(drives);
    }
}

void DriveScanWorker::stop() {
    m_stopped = true;
}

void DriveScanWorker::reset() {
    m_stopped = false;
}

// DriveMonitor implementation

DriveMonitor::DriveMonitor(Core::DriveManager& driveManager, QObject* parent)
    : QObject(parent)
    , m_driveManager(driveManager)
    , m_timer(new QTimer(this))
    , m_workerThread(new QThread(this))
    , m_worker(new DriveScanWorker(driveManager)) {

    // Register metatypes for queued connections
    qRegisterMetaType<std::vector<Core::DriveInfo>>("std::vector<Core::DriveInfo>");
    qRegisterMetaType<Core::DriveInfo>("Core::DriveInfo");
    qRegisterMetaType<Core::MediaInfo>("Core::MediaInfo");

    // Move worker to background thread
    m_worker->moveToThread(m_workerThread);

    // Connect signals
    connect(m_timer, &QTimer::timeout, this, &DriveMonitor::triggerScan);
    connect(m_worker, &DriveScanWorker::scanComplete,
            this, &DriveMonitor::onScanComplete, Qt::QueuedConnection);
    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);

    // Start worker thread
    m_workerThread->start();
}

DriveMonitor::~DriveMonitor() {
    stop();

    m_worker->stop();
    m_workerThread->quit();
    m_workerThread->wait(3000);

    if (m_workerThread->isRunning()) {
        m_workerThread->terminate();
        m_workerThread->wait();
    }
}

void DriveMonitor::start(int intervalMs) {
    if (!m_timer->isActive()) {
        // Reset the worker to accept scans again
        QMetaObject::invokeMethod(m_worker, "reset", Qt::QueuedConnection);
        // Trigger immediate full scan to get initial media info
        triggerFullScan();
        m_timer->start(intervalMs);
    }
}

void DriveMonitor::stop() {
    m_timer->stop();
    m_worker->stop(); // Tell worker to stop accepting new scans
}

bool DriveMonitor::isRunning() const {
    return m_timer->isActive();
}

std::vector<Core::DriveInfo> DriveMonitor::currentDrives() const {
    QMutexLocker locker(&m_drivesMutex);
    return m_drives;
}

void DriveMonitor::refresh() {
    triggerScan();
}

void DriveMonitor::refreshFull() {
    triggerFullScan();
}

bool DriveMonitor::waitForScan(int timeoutMs) {
    int elapsed = 0;
    const int sleepInterval = 50;

    while (m_scanInProgress && elapsed < timeoutMs) {
        // Process events to allow the scanComplete signal to be delivered
        QCoreApplication::processEvents(QEventLoop::AllEvents, sleepInterval);
        QThread::msleep(sleepInterval);
        elapsed += sleepInterval;
    }

    return !m_scanInProgress;
}

void DriveMonitor::triggerScan() {
    // Don't queue multiple scans
    if (m_scanInProgress.exchange(true)) {
        return;
    }

    // Use quick scan for periodic checks (doesn't spin up disc)
    QMetaObject::invokeMethod(m_worker, "scanQuick", Qt::QueuedConnection);
}

void DriveMonitor::triggerFullScan() {
    // Don't queue multiple scans
    if (m_scanInProgress.exchange(true)) {
        return;
    }

    m_fullScanRequested = true;
    // Full scan grabs drives to get media info
    QMetaObject::invokeMethod(m_worker, "scanFull", Qt::QueuedConnection);
}

void DriveMonitor::onScanComplete(const std::vector<Core::DriveInfo>& newDrives) {
    m_scanInProgress = false;

    std::vector<Core::DriveInfo> oldDrives;
    {
        QMutexLocker locker(&m_drivesMutex);
        oldDrives = m_drives;
    }

    // Check for changes
    bool changed = (newDrives.size() != oldDrives.size());

    if (!changed) {
        for (size_t i = 0; i < newDrives.size(); ++i) {
            if (newDrives[i].devicePath != oldDrives[i].devicePath ||
                newDrives[i].media.status != oldDrives[i].media.status ||
                newDrives[i].media.freeBytes != oldDrives[i].media.freeBytes) {
                changed = true;
                break;
            }
        }
    }

    // Check for media changes
    for (const auto& newDrive : newDrives) {
        auto oldIt = std::find_if(oldDrives.begin(), oldDrives.end(),
            [&newDrive](const Core::DriveInfo& d) {
                return d.devicePath == newDrive.devicePath;
            });

        if (oldIt != oldDrives.end()) {
            // Drive existed before
            bool hadMedia = (oldIt->media.status != Core::MediaStatus::NoMedia &&
                            oldIt->media.status != Core::MediaStatus::Unknown);
            bool hasMedia = (newDrive.media.status != Core::MediaStatus::NoMedia &&
                            newDrive.media.status != Core::MediaStatus::Unknown);

            if (!hadMedia && hasMedia) {
                emit mediaInserted(QString::fromStdString(newDrive.devicePath),
                                   newDrive.media);
            } else if (hadMedia && !hasMedia) {
                emit mediaRemoved(QString::fromStdString(newDrive.devicePath));
            }
        } else {
            // Drive is newly discovered - emit mediaInserted if it has media
            bool hasMedia = (newDrive.media.status != Core::MediaStatus::NoMedia &&
                            newDrive.media.status != Core::MediaStatus::Unknown);
            if (hasMedia) {
                emit mediaInserted(QString::fromStdString(newDrive.devicePath),
                                   newDrive.media);
            }
        }
    }

    // Update stored drives
    {
        QMutexLocker locker(&m_drivesMutex);
        m_drives = newDrives;
    }

    if (changed) {
        emit drivesChanged(newDrives);
    }

    // Always emit scanCompleted so UI can re-enable controls
    emit scanCompleted();
}

} // namespace Burner::Engine
