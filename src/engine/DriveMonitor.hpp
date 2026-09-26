#pragma once

#include <QObject>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <vector>
#include <atomic>
#include "../core/DriveInfo.hpp"

namespace Burner::Core {
class DriveManager;
}

namespace Burner::Engine {

/// Worker that scans drives in a background thread
class DriveScanWorker : public QObject {
    Q_OBJECT

public:
    explicit DriveScanWorker(Core::DriveManager& driveManager);

public slots:
    /// Quick scan - uses cached media info, doesn't spin up discs
    void scanQuick();
    /// Full scan - grabs drives to get media info (causes disc spin-up)
    void scanFull();
    void stop();
    void reset();

signals:
    void scanComplete(const std::vector<Core::DriveInfo>& drives);

private:
    Core::DriveManager& m_driveManager;
    std::atomic<bool> m_stopped{false};
};

/// Monitors for drive changes and media insertion/removal.
/// Scans drives in a background thread to avoid blocking the UI.
class DriveMonitor : public QObject {
    Q_OBJECT

public:
    /// Construct a DriveMonitor
    /// @param driveManager Reference to the drive manager
    /// @param parent Parent QObject
    explicit DriveMonitor(Core::DriveManager& driveManager, QObject* parent = nullptr);
    ~DriveMonitor() override;

    /// Start monitoring with the specified interval
    /// @param intervalMs Poll interval in milliseconds (default 3000)
    void start(int intervalMs = 3000);

    /// Stop monitoring
    void stop();

    /// Check if monitoring is active
    [[nodiscard]] bool isRunning() const;

    /// Get the current list of drives (thread-safe copy)
    [[nodiscard]] std::vector<Core::DriveInfo> currentDrives() const;

    /// Force an immediate quick refresh (uses cached media info)
    void refresh();

    /// Force a full refresh that grabs drives for media info (causes disc spin-up)
    void refreshFull();

    /// Wait for any in-progress scan to complete (with timeout)
    /// @param timeoutMs Maximum time to wait in milliseconds
    /// @return true if scan completed, false if timeout
    bool waitForScan(int timeoutMs = 2000);

signals:
    /// Emitted when the drive list changes
    void drivesChanged(const std::vector<Core::DriveInfo>& drives);

    /// Emitted when a scan completes (regardless of whether drives changed)
    void scanCompleted();

    /// Emitted when media is inserted into a drive
    void mediaInserted(const QString& devicePath, const Core::MediaInfo& media);

    /// Emitted when media is removed from a drive
    void mediaRemoved(const QString& devicePath);

private slots:
    void onScanComplete(const std::vector<Core::DriveInfo>& drives);
    void triggerScan();
    void triggerFullScan();

private:
    Core::DriveManager& m_driveManager;
    QTimer* m_timer;
    QThread* m_workerThread;
    DriveScanWorker* m_worker;

    mutable QMutex m_drivesMutex;
    std::vector<Core::DriveInfo> m_drives;
    std::atomic<bool> m_scanInProgress{false};
    std::atomic<bool> m_fullScanRequested{false};
};

} // namespace Burner::Engine
