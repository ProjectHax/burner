#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QPair>
#include <QList>
#include <memory>
#include <atomic>

#include "../core/BurnEngine.hpp"
#include "../core/ImageBuilder.hpp"
#include "../core/DiscCloner.hpp"

// Type alias for file path mappings (localPath -> isoPath)
// Defined before namespace to be available in Q_DECLARE_METATYPE
using FilePathMappingList = QList<QPair<QString, QString>>;

namespace Burner::Engine {

/// Worker object that performs burn operations in a separate thread.
/// Should be moved to a QThread before use.
class BurnWorker : public QObject {
    Q_OBJECT

public:
    explicit BurnWorker(QObject* parent = nullptr);
    ~BurnWorker() override;

public slots:
    /// Burn a data disc with files from specified paths (legacy - puts all at root)
    void burnDataDisc(const QString& devicePath,
                      const QStringList& filePaths,
                      const QString& volumeLabel,
                      const Core::BurnOptions& options);

    /// Burn a data disc with file mappings (preserves directory structure)
    /// @param fileMappings List of (localPath, isoPath) pairs
    void burnDataDiscWithStructure(const QString& devicePath,
                                   const FilePathMappingList& fileMappings,
                                   const QString& volumeLabel,
                                   const Core::BurnOptions& options);

    /// Burn an ISO file to disc
    void burnIsoFile(const QString& devicePath,
                     const QString& isoPath,
                     const Core::BurnOptions& options);

    /// Burn a CUE/BIN disc image
    void burnCueImage(const QString& devicePath,
                      const QString& cuePath,
                      const Core::BurnOptions& options);

    /// Burn an audio CD from audio files
    void burnAudioCD(const QString& devicePath,
                     const QStringList& audioFiles,
                     const Core::BurnOptions& options);

    /// Blank a rewritable disc
    void blankDisc(const QString& devicePath, bool fullBlank);

    /// Check disc integrity by reading all sectors
    void checkIntegrity(const QString& devicePath, bool stopOnFirstError);

    /// Request cancellation
    void cancel();

    /// Clone disc to image file
    void cloneToImage(const QString& devicePath, const QString& outputPath);

    /// Clone disc to BIN/CUE image pair
    void cloneToBinCue(const QString& devicePath, const QString& outputPath);

    /// Clone image file to disc
    void cloneFromImage(const QString& devicePath, const QString& imagePath,
                        bool verify, bool ejectAfter);

    /// Burn a Video CD
    void burnVideoCd(const QString& devicePath,
                     const QStringList& videoFiles,
                     const QString& vcdType,
                     const Core::BurnOptions& options);

    /// Create an ISO file from files (no disc required) - legacy, no structure
    void createIsoFile(const QString& outputPath,
                       const QStringList& filePaths,
                       const QString& volumeLabel);

    /// Create an ISO file with file mappings (preserves directory structure)
    void createIsoFileWithStructure(const QString& outputPath,
                                    const FilePathMappingList& fileMappings,
                                    const QString& volumeLabel);

signals:
    /// Emitted when progress is updated
    void progressUpdated(double percent,
                         quint64 bytesWritten,
                         quint64 totalBytes,
                         int bufferFill,
                         int writeSpeedKBps);

    /// Emitted with detailed progress information
    void progressDetail(const QString& operation,
                        int elapsedSeconds,
                        int remainingSeconds);

    /// Emitted when status changes
    void statusChanged(const QString& status);

    /// Emitted when burn completes successfully
    void finished(bool success, const QString& message);

    /// Emitted on error
    void error(const QString& errorMessage);

private:
    void setupCallbacks(Core::BurnEngine& engine);

    /// Validate that all files exist and are readable
    /// @param files List of file paths to validate
    /// @param errorOut Output parameter for error message
    /// @return true if all files are valid, false otherwise
    bool validateFiles(const QStringList& files, QString& errorOut);

    /// Validate a single file
    /// @param filePath Path to validate
    /// @param errorOut Output parameter for error message
    /// @return true if file is valid, false otherwise
    bool validateFile(const QString& filePath, QString& errorOut);

    std::atomic<bool> m_cancelled{false};
    Core::DiscCloner* m_activeCloner{nullptr};  // For cancellation during clone
    Core::BurnEngine* m_activeEngine{nullptr};  // For cancellation during integrity check
};

} // namespace Burner::Engine

// Register types for signal/slot
Q_DECLARE_METATYPE(Burner::Core::BurnOptions)
Q_DECLARE_METATYPE(FilePathMappingList)
