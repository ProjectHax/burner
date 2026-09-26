#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QPair>
#include <QList>
#include <memory>

#include "../core/BurnEngine.hpp"

// Type alias for file path mappings (localPath -> isoPath)
using FilePathMappingList = QList<QPair<QString, QString>>;

namespace Burner::Core {
class BurniaInit;
class DriveManager;
struct DriveInfo;
}

class QThread;

namespace Burner::Engine {

class BurnWorker;

/// Qt wrapper for the core burning engine.
/// Provides signal/slot interface for UI integration.
class QtBurnEngine : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool burning READ isBurning NOTIFY burningChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit QtBurnEngine(QObject* parent = nullptr);
    ~QtBurnEngine() override;

    /// Check if a burn operation is in progress
    [[nodiscard]] bool isBurning() const;

    /// Get current burn progress (0.0 - 100.0)
    [[nodiscard]] double progress() const;

    /// Get current status message
    [[nodiscard]] QString status() const;

    /// Get the drive manager for drive operations
    [[nodiscard]] Core::DriveManager& driveManager();

    /// Check if the engine is properly initialized
    [[nodiscard]] bool isInitialized() const;

    /// Get initialization error message (if any)
    [[nodiscard]] QString initError() const;

public slots:
    /// Burn a data disc with the specified files (legacy - puts all at root)
    void burnDataDisc(const QString& device,
                      const QStringList& files,
                      const QString& volumeLabel,
                      const Core::BurnOptions& options = {});

    /// Burn a data disc with file mappings (preserves directory structure)
    /// @param fileMappings List of (localPath, isoPath) pairs
    void burnDataDiscWithStructure(const QString& device,
                                   const FilePathMappingList& fileMappings,
                                   const QString& volumeLabel,
                                   const Core::BurnOptions& options = {});

    /// Burn an audio CD from audio files
    void burnAudioCD(const QString& device,
                     const QStringList& audioFiles,
                     const Core::BurnOptions& options = {});

    /// Burn an ISO file to disc
    void burnIsoFile(const QString& device,
                     const QString& isoPath,
                     const Core::BurnOptions& options = {});

    /// Burn a CUE/BIN disc image
    void burnCueImage(const QString& device,
                      const QString& cuePath,
                      const Core::BurnOptions& options = {});

    /// Blank a rewritable disc
    void blankDisc(const QString& device, bool fullBlank = false);

    /// Check disc integrity by reading all sectors
    void checkIntegrity(const QString& device, bool stopOnFirstError = false);

    /// Clone disc to image file
    void cloneToImage(const QString& device, const QString& outputPath);

    /// Clone disc to BIN/CUE image pair
    void cloneToBinCue(const QString& device, const QString& outputPath);

    /// Clone image file to disc
    void cloneFromImage(const QString& device, const QString& imagePath,
                        bool verify = false, bool ejectAfter = true);

    /// Burn a Video CD from video files
    void burnVideoCd(const QString& device,
                     const QStringList& videoFiles,
                     const QString& vcdType,
                     const Core::BurnOptions& options = {});

    /// Create an ISO file from files (no disc/drive required) - legacy, no structure
    void createIsoFile(const QString& outputPath,
                       const QStringList& files,
                       const QString& volumeLabel);

    /// Create an ISO file with file mappings (preserves directory structure)
    void createIsoFileWithStructure(const QString& outputPath,
                                    const FilePathMappingList& fileMappings,
                                    const QString& volumeLabel);

    /// Cancel the current burn operation
    void cancel();

    /// Eject the specified drive
    void ejectDrive(const QString& device);

signals:
    void burningChanged(bool burning);
    void progressChanged(double percent);
    void progressDetail(quint64 bytesWritten, quint64 totalBytes, int bufferFill, int writeSpeedKBps);
    void timeUpdate(int elapsedSeconds, int remainingSeconds);
    void statusChanged(const QString& status);
    void burnComplete(bool success, const QString& message);
    void burnError(const QString& error);
    void drivesChanged();

private slots:
    void onWorkerFinished(bool success, const QString& message);
    void onWorkerProgress(double percent, quint64 bytesWritten,
                          quint64 totalBytes, int bufferFill, int writeSpeedKBps = 0);
    void onWorkerStatus(const QString& status);
    void onWorkerError(const QString& error);
    void cleanupWorker();

private:
    void startWorker(const QString& device);

    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Burner::Engine
