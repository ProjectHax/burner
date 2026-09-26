#include "BurnWorker.hpp"
#include "../core/DiscCloner.hpp"
#include "../core/VcdBuilder.hpp"
#include "../core/CueSheet.hpp"
#include "../core/libburnia/DriveHandle.hpp"

#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

namespace Burner::Engine {

bool BurnWorker::validateFile(const QString& filePath, QString& errorOut) {
    QFileInfo info(filePath);

    // Check if exists
    if (!info.exists()) {
        errorOut = tr("File not found: %1").arg(filePath);
        return false;
    }

    // For directories, just check they're accessible
    if (info.isDir()) {
        if (!info.isReadable()) {
            errorOut = tr("Directory not readable: %1").arg(filePath);
            return false;
        }
        return true;
    }

    // Check if readable
    if (!info.isReadable()) {
        errorOut = tr("File not readable: %1").arg(filePath);
        return false;
    }

    // Try to actually open the file to verify access
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        errorOut = tr("Cannot open file: %1\nReason: %2")
                       .arg(filePath)
                       .arg(file.errorString());
        return false;
    }

    // Try to read a small amount to verify the file is actually accessible
    if (info.size() > 0) {
        QByteArray testRead = file.read(1);
        if (testRead.isEmpty() && file.error() != QFile::NoError) {
            errorOut = tr("Cannot read file: %1\nReason: %2")
                           .arg(filePath)
                           .arg(file.errorString());
            file.close();
            return false;
        }
    }

    file.close();
    return true;
}

bool BurnWorker::validateFiles(const QStringList& files, QString& errorOut) {
    QStringList failedFiles;

    for (const QString& filePath : files) {
        QString fileError;
        if (!validateFile(filePath, fileError)) {
            failedFiles.append(fileError);
        }
    }

    if (!failedFiles.isEmpty()) {
        errorOut = tr("File validation failed:\n\n%1").arg(failedFiles.join("\n\n"));
        return false;
    }

    return true;
}

BurnWorker::BurnWorker(QObject* parent)
    : QObject(parent) {
    qRegisterMetaType<Core::BurnOptions>("Core::BurnOptions");
    qRegisterMetaType<Burner::Core::BurnOptions>("Burner::Core::BurnOptions");
    qRegisterMetaType<FilePathMappingList>("FilePathMappingList");
}

BurnWorker::~BurnWorker() = default;

void BurnWorker::burnDataDisc(const QString& devicePath,
                               const QStringList& filePaths,
                               const QString& volumeLabel,
                               const Core::BurnOptions& options) {
    m_cancelled = false;

    // Validate files before starting burn
    emit statusChanged(tr("Validating files..."));
    QString validationError;
    if (!validateFiles(filePaths, validationError)) {
        emit error(validationError);
        emit finished(false, tr("File validation failed"));
        return;
    }

    emit statusChanged(tr("Initializing burn engine..."));

    try {
        Core::BurnEngine engine(devicePath.toStdString());

        if (!engine.isValid()) {
            emit error(tr("Failed to initialize burn engine for device: %1").arg(devicePath));
            emit finished(false, tr("Engine initialization failed"));
            return;
        }

        setupCallbacks(engine);

        // Build image from files
        emit statusChanged(tr("Building disc image..."));

        Core::ImageBuilder builder;
        builder.setVolumeLabel(volumeLabel.isEmpty() ? "DATA_DISC" : volumeLabel.toStdString());

        for (const QString& path : filePaths) {
            QFileInfo info(path);
            if (info.isDir()) {
                builder.addDirectory(path.toStdString());
            } else {
                builder.addFile(path.toStdString());
            }

            if (m_cancelled) {
                emit statusChanged(tr("Cancelled"));
                emit finished(false, tr("Operation cancelled by user"));
                return;
            }
        }

        if (builder.fileCount() == 0) {
            emit error(tr("No files to burn"));
            emit finished(false, tr("No files added to disc"));
            return;
        }

        emit statusChanged(tr("Starting burn with %1 files (%2 bytes)...")
                          .arg(builder.fileCount())
                          .arg(builder.totalSize()));

        // Perform the burn
        auto result = engine.burnDataDisc(builder, options);

        if (result.hasError()) {
            emit error(QString::fromStdString(result.message));
            emit finished(false, QString::fromStdString(result.message));
        } else {
            emit finished(true, tr("Burn completed successfully"));
        }

    } catch (const std::exception& e) {
        emit error(QString::fromUtf8(e.what()));
        emit finished(false, QString::fromUtf8(e.what()));
    }
}

void BurnWorker::burnDataDiscWithStructure(const QString& devicePath,
                                            const FilePathMappingList& fileMappings,
                                            const QString& volumeLabel,
                                            const Core::BurnOptions& options) {
    m_cancelled = false;

    // Validate files before starting burn
    emit statusChanged(tr("Validating files..."));
    QStringList localPaths;
    for (const auto& mapping : fileMappings) {
        localPaths.append(mapping.first);
    }
    QString validationError;
    if (!validateFiles(localPaths, validationError)) {
        emit error(validationError);
        emit finished(false, tr("File validation failed"));
        return;
    }

    emit statusChanged(tr("Initializing burn engine..."));

    try {
        Core::BurnEngine engine(devicePath.toStdString());

        if (!engine.isValid()) {
            emit error(tr("Failed to initialize burn engine for device: %1").arg(devicePath));
            emit finished(false, tr("Engine initialization failed"));
            return;
        }

        setupCallbacks(engine);

        // Build image from files with correct directory structure
        emit statusChanged(tr("Building disc image..."));

        Core::ImageBuilder builder;
        builder.setVolumeLabel(volumeLabel.isEmpty() ? "DATA_DISC" : volumeLabel.toStdString());

        // Check if disc is appendable for multi-session support
        if (!options.closeDisc) {
            // When "leave open" is checked, we're in multi-session mode
            // Check if the disc already has sessions (appendable)
            // The BurnEngine's internal drive handle will be used for burning,
            // but we can use the device path for ISO import
            // Note: We only import previous session if disc is appendable,
            // which is handled internally by importPreviousSession
        }

        // Detect appendable disc and configure multi-session
        // We create a temporary DriveHandle to check disc status
        // (the BurnEngine will create its own for the actual burn)
        {
            Core::DriveHandle probe(devicePath.toStdString());
            auto mediaInfo = probe.getMediaInfo();
            if (mediaInfo && mediaInfo->status == Core::MediaStatus::Appendable) {
                int nwa = probe.getNextWritableAddress();
                if (nwa > 0) {
                    emit statusChanged(tr("Appendable disc detected - importing previous session..."));
                    builder.setMultiSessionDevice(devicePath.toStdString(), nwa);
                }
            }
            probe.release();
        }

        for (const auto& mapping : fileMappings) {
            const QString& localPath = mapping.first;
            const QString& isoPath = mapping.second;

            // Add file with its correct ISO path
            builder.addFile(localPath.toStdString(), isoPath.toStdString());

            if (m_cancelled) {
                emit statusChanged(tr("Cancelled"));
                emit finished(false, tr("Operation cancelled by user"));
                return;
            }
        }

        if (builder.fileCount() == 0) {
            emit error(tr("No files to burn"));
            emit finished(false, tr("No files added to disc"));
            return;
        }

        emit statusChanged(tr("Starting burn with %1 files (%2 bytes)...")
                          .arg(builder.fileCount())
                          .arg(builder.totalSize()));

        // Perform the burn
        auto result = engine.burnDataDisc(builder, options);

        if (result.hasError()) {
            emit error(QString::fromStdString(result.message));
            emit finished(false, QString::fromStdString(result.message));
        } else {
            emit finished(true, tr("Burn completed successfully"));
        }

    } catch (const std::exception& e) {
        emit error(QString::fromUtf8(e.what()));
        emit finished(false, QString::fromUtf8(e.what()));
    }
}

void BurnWorker::burnIsoFile(const QString& devicePath,
                              const QString& isoPath,
                              const Core::BurnOptions& options) {
    m_cancelled = false;

    // Validate ISO file before starting burn
    emit statusChanged(tr("Validating ISO file..."));
    QString validationError;
    if (!validateFile(isoPath, validationError)) {
        emit error(validationError);
        emit finished(false, tr("ISO file validation failed"));
        return;
    }

    emit statusChanged(tr("Initializing burn engine..."));

    try {
        Core::BurnEngine engine(devicePath.toStdString());

        if (!engine.isValid()) {
            emit error(tr("Failed to initialize burn engine for device: %1").arg(devicePath));
            emit finished(false, tr("Engine initialization failed"));
            return;
        }

        setupCallbacks(engine);

        emit statusChanged(tr("Burning ISO file..."));

        auto result = engine.burnIsoFile(isoPath.toStdString(), options);

        if (result.hasError()) {
            emit error(QString::fromStdString(result.message));
            emit finished(false, QString::fromStdString(result.message));
        } else {
            emit finished(true, tr("Burn completed successfully"));
        }

    } catch (const std::exception& e) {
        emit error(QString::fromUtf8(e.what()));
        emit finished(false, QString::fromUtf8(e.what()));
    }
}

void BurnWorker::burnCueImage(const QString& devicePath,
                               const QString& cuePath,
                               const Core::BurnOptions& options) {
    m_cancelled = false;

    // Validate CUE file
    emit statusChanged(tr("Validating CUE file..."));
    QString validationError;
    if (!validateFile(cuePath, validationError)) {
        emit error(validationError);
        emit finished(false, tr("CUE file validation failed"));
        return;
    }

    // Parse CUE sheet
    emit statusChanged(tr("Parsing CUE sheet..."));
    Core::CueSheet cueSheet;
    if (!cueSheet.parse(cuePath.toStdString())) {
        emit error(tr("Failed to parse CUE file: %1")
                  .arg(QString::fromStdString(cueSheet.lastError())));
        emit finished(false, tr("CUE parsing failed"));
        return;
    }

    if (!cueSheet.validate()) {
        emit error(tr("CUE validation failed: %1")
                  .arg(QString::fromStdString(cueSheet.lastError())));
        emit finished(false, tr("CUE validation failed"));
        return;
    }

    // Validate BIN file
    QString binPath = QString::fromStdString(cueSheet.binPath().string());
    if (!validateFile(binPath, validationError)) {
        emit error(validationError);
        emit finished(false, tr("BIN file validation failed"));
        return;
    }

    emit statusChanged(tr("Initializing burn engine..."));

    try {
        Core::BurnEngine engine(devicePath.toStdString());

        if (!engine.isValid()) {
            emit error(tr("Failed to initialize burn engine for device: %1").arg(devicePath));
            emit finished(false, tr("Engine initialization failed"));
            return;
        }

        setupCallbacks(engine);

        emit statusChanged(tr("Burning CUE image with %1 track(s)...")
                          .arg(cueSheet.trackCount()));

        auto result = engine.burnCueImage(cueSheet, options);

        if (result.hasError()) {
            emit error(QString::fromStdString(result.message));
            emit finished(false, QString::fromStdString(result.message));
        } else {
            emit finished(true, tr("CUE image burn completed successfully"));
        }

    } catch (const std::exception& e) {
        emit error(QString::fromUtf8(e.what()));
        emit finished(false, QString::fromUtf8(e.what()));
    }
}

void BurnWorker::burnAudioCD(const QString& devicePath,
                              const QStringList& audioFiles,
                              const Core::BurnOptions& options) {
    m_cancelled = false;

    if (audioFiles.isEmpty()) {
        emit error(tr("No audio files to burn"));
        emit finished(false, tr("No audio files provided"));
        return;
    }

    // Validate audio files before starting burn
    emit statusChanged(tr("Validating audio files..."));
    QString validationError;
    if (!validateFiles(audioFiles, validationError)) {
        emit error(validationError);
        emit finished(false, tr("Audio file validation failed"));
        return;
    }

    emit statusChanged(tr("Initializing burn engine..."));

    try {
        Core::BurnEngine engine(devicePath.toStdString());

        if (!engine.isValid()) {
            emit error(tr("Failed to initialize burn engine for device: %1").arg(devicePath));
            emit finished(false, tr("Engine initialization failed"));
            return;
        }

        setupCallbacks(engine);

        emit statusChanged(tr("Preparing audio CD with %1 tracks...").arg(audioFiles.size()));

        // Convert QStringList to vector<filesystem::path>
        std::vector<std::filesystem::path> paths;
        paths.reserve(static_cast<size_t>(audioFiles.size()));
        for (const QString& file : audioFiles) {
            paths.emplace_back(file.toStdString());
        }

        // Perform the burn
        auto result = engine.burnAudioCD(paths, options);

        if (result.hasError()) {
            emit error(QString::fromStdString(result.message));
            emit finished(false, QString::fromStdString(result.message));
        } else {
            emit finished(true, tr("Audio CD burn completed successfully"));
        }

    } catch (const std::exception& e) {
        emit error(QString::fromUtf8(e.what()));
        emit finished(false, QString::fromUtf8(e.what()));
    }
}

void BurnWorker::blankDisc(const QString& devicePath, bool fullBlank) {
    m_cancelled = false;

    emit statusChanged(tr("Initializing..."));

    try {
        Core::BurnEngine engine(devicePath.toStdString());

        if (!engine.isValid()) {
            emit error(tr("Failed to initialize engine for device: %1").arg(devicePath));
            emit finished(false, tr("Engine initialization failed"));
            return;
        }

        setupCallbacks(engine);

        emit statusChanged(fullBlank ? tr("Performing full blank...") : tr("Performing quick blank..."));

        auto result = engine.blankDisc(fullBlank);

        if (result.hasError()) {
            emit error(QString::fromStdString(result.message));
            emit finished(false, QString::fromStdString(result.message));
        } else {
            emit finished(true, tr("Blank completed successfully"));
        }

    } catch (const std::exception& e) {
        emit error(QString::fromUtf8(e.what()));
        emit finished(false, QString::fromUtf8(e.what()));
    }
}

void BurnWorker::checkIntegrity(const QString& devicePath, bool stopOnFirstError) {
    m_cancelled = false;

    emit statusChanged(tr("Initializing integrity check..."));

    try {
        // Note: We don't check isValid() here because checkIntegrity() doesn't
        // need libburn's DriveHandle - it opens the device directly for reading.
        // This allows integrity checks to work even when libburn can't grab the drive.
        Core::BurnEngine engine(devicePath.toStdString());

        setupCallbacks(engine);

        emit statusChanged(tr("Checking disc integrity..."));

        // Set active engine so cancel() can reach it
        m_activeEngine = &engine;
        auto result = engine.checkIntegrity(stopOnFirstError);
        m_activeEngine = nullptr;

        // Check for cancellation first - don't report bad sectors if cancelled
        if (m_cancelled) {
            emit finished(false, tr("Integrity check cancelled"));
        } else if (result.success) {
            emit finished(true, tr("Integrity check passed: %1").arg(QString::fromStdString(result.message)));
        } else if (result.badSectors > 0) {
            QString msg = tr("Integrity check found %1 bad sector(s)").arg(result.badSectors);
            emit error(msg);
            emit finished(false, msg);
        } else {
            // Other issue
            emit finished(false, QString::fromStdString(result.message));
        }

    } catch (const std::exception& e) {
        m_activeEngine = nullptr;
        emit error(QString::fromUtf8(e.what()));
        emit finished(false, QString::fromUtf8(e.what()));
    }
}

void BurnWorker::cancel() {
    m_cancelled = true;

    // Also cancel active cloner if running
    if (m_activeCloner) {
        m_activeCloner->cancel();
    }

    // Also cancel active engine if running (e.g., integrity check)
    if (m_activeEngine) {
        m_activeEngine->cancel();
    }
}

void BurnWorker::setupCallbacks(Core::BurnEngine& engine) {
    engine.setProgressCallback([this](const Core::BurnProgress& progress) {
        if (m_cancelled) {
            return;
        }

        emit progressUpdated(
            progress.percent,
            progress.bytesWritten,
            progress.totalBytes,
            progress.bufferFillPercent,
            progress.writeSpeedKBps
        );

        emit progressDetail(
            QString::fromStdString(progress.currentOperation),
            static_cast<int>(progress.elapsed.count()),
            static_cast<int>(progress.remaining.count())
        );
    });

    engine.setStatusCallback([this](Core::BurnStatus status, const std::string& message) {
        if (m_cancelled && status != Core::BurnStatus::Cancelled) {
            return;
        }

        QString statusStr;
        switch (status) {
            case Core::BurnStatus::Idle:
                statusStr = tr("Idle");
                break;
            case Core::BurnStatus::Preparing:
                statusStr = tr("Preparing");
                break;
            case Core::BurnStatus::Blanking:
                statusStr = tr("Blanking disc");
                break;
            case Core::BurnStatus::Writing:
                statusStr = tr("Writing");
                break;
            case Core::BurnStatus::Closing:
                statusStr = tr("Closing disc");
                break;
            case Core::BurnStatus::Verifying:
                statusStr = tr("Verifying");
                break;
            case Core::BurnStatus::Completed:
                statusStr = tr("Completed");
                break;
            case Core::BurnStatus::Failed:
                statusStr = tr("Failed");
                break;
            case Core::BurnStatus::Cancelled:
                statusStr = tr("Cancelled");
                break;
        }

        if (!message.empty()) {
            statusStr += ": " + QString::fromStdString(message);
        }

        emit statusChanged(statusStr);
    });
}

void BurnWorker::cloneToImage(const QString& devicePath, const QString& outputPath) {
    m_cancelled = false;

    emit statusChanged(tr("Initializing disc cloner..."));

    try {
        Core::DiscCloner cloner(devicePath.toStdString());

        if (!cloner.isValid()) {
            emit error(tr("Failed to initialize cloner for device: %1").arg(devicePath));
            emit finished(false, tr("Cloner initialization failed"));
            return;
        }

        cloner.setProgressCallback([this, &cloner](const Core::CloneProgress& progress) {
            if (m_cancelled) {
                // Propagate cancellation to the cloner
                cloner.cancel();
                return;
            }

            emit progressUpdated(
                progress.percent,
                progress.bytesProcessed,
                progress.totalBytes,
                0,  // No buffer for clone operations
                progress.readSpeedKbps
            );

            emit progressDetail(
                QString::fromStdString(progress.currentOperation),
                0, 0
            );
        });

        emit statusChanged(tr("Reading disc to image..."));

        // Set active cloner so cancel() can reach it directly
        m_activeCloner = &cloner;
        auto result = cloner.readToImage(outputPath.toStdString());
        m_activeCloner = nullptr;

        if (result.hasError()) {
            emit error(QString::fromStdString(result.message));
            emit finished(false, QString::fromStdString(result.message));
        } else {
            emit finished(true, tr("Disc image created successfully"));
        }

    } catch (const std::exception& e) {
        m_activeCloner = nullptr;
        emit error(QString::fromUtf8(e.what()));
        emit finished(false, QString::fromUtf8(e.what()));
    }
}

void BurnWorker::cloneToBinCue(const QString& devicePath, const QString& outputPath) {
    m_cancelled = false;

    emit statusChanged(tr("Initializing disc cloner..."));

    try {
        Core::DiscCloner cloner(devicePath.toStdString());

        if (!cloner.isValid()) {
            emit error(tr("Failed to initialize cloner for device: %1").arg(devicePath));
            emit finished(false, tr("Cloner initialization failed"));
            return;
        }

        cloner.setProgressCallback([this, &cloner](const Core::CloneProgress& progress) {
            if (m_cancelled) {
                cloner.cancel();
                return;
            }

            emit progressUpdated(
                progress.percent,
                progress.bytesProcessed,
                progress.totalBytes,
                0,
                progress.readSpeedKbps
            );

            emit progressDetail(
                QString::fromStdString(progress.currentOperation),
                0, 0
            );
        });

        emit statusChanged(tr("Reading disc to BIN/CUE image..."));

        m_activeCloner = &cloner;
        auto result = cloner.readToBinCue(outputPath.toStdString());
        m_activeCloner = nullptr;

        if (result.hasError()) {
            emit error(QString::fromStdString(result.message));
            emit finished(false, QString::fromStdString(result.message));
        } else {
            emit finished(true, tr("BIN/CUE image created successfully"));
        }

    } catch (const std::exception& e) {
        m_activeCloner = nullptr;
        emit error(QString::fromUtf8(e.what()));
        emit finished(false, QString::fromUtf8(e.what()));
    }
}

void BurnWorker::cloneFromImage(const QString& devicePath, const QString& imagePath,
                                 bool verify, bool ejectAfter) {
    m_cancelled = false;

    // Validate image file before starting write
    emit statusChanged(tr("Validating image file..."));
    QString validationError;
    if (!validateFile(imagePath, validationError)) {
        emit error(validationError);
        emit finished(false, tr("Image file validation failed"));
        return;
    }

    emit statusChanged(tr("Initializing disc cloner..."));

    try {
        Core::DiscCloner cloner(devicePath.toStdString());

        if (!cloner.isValid()) {
            emit error(tr("Failed to initialize cloner for device: %1").arg(devicePath));
            emit finished(false, tr("Cloner initialization failed"));
            return;
        }

        cloner.setProgressCallback([this](const Core::CloneProgress& progress) {
            if (m_cancelled) {
                return;
            }

            emit progressUpdated(
                progress.percent,
                progress.bytesProcessed,
                progress.totalBytes,
                0,
                0
            );

            emit progressDetail(
                QString::fromStdString(progress.currentOperation),
                0, 0
            );
        });

        emit statusChanged(tr("Writing image to disc..."));

        auto result = cloner.writeFromImage(imagePath.toStdString(), verify, ejectAfter);

        if (result.hasError()) {
            emit error(QString::fromStdString(result.message));
            emit finished(false, QString::fromStdString(result.message));
        } else {
            emit finished(true, tr("Image written to disc successfully"));
        }

    } catch (const std::exception& e) {
        emit error(QString::fromUtf8(e.what()));
        emit finished(false, QString::fromUtf8(e.what()));
    }
}

void BurnWorker::burnVideoCd(const QString& devicePath,
                              const QStringList& videoFiles,
                              const QString& vcdType,
                              const Core::BurnOptions& options) {
    m_cancelled = false;

    if (videoFiles.isEmpty()) {
        emit error(tr("No video files to burn"));
        emit finished(false, tr("No video files provided"));
        return;
    }

    // Validate video files before starting burn
    emit statusChanged(tr("Validating video files..."));
    QString validationError;
    if (!validateFiles(videoFiles, validationError)) {
        emit error(validationError);
        emit finished(false, tr("Video file validation failed"));
        return;
    }

    emit statusChanged(tr("Initializing Video CD builder..."));

    try {
        Core::VcdBuilder builder;

        // Set format based on type
        if (vcdType.contains("SVCD", Qt::CaseInsensitive)) {
            builder.setFormat(Core::VcdFormat::SVCD);
        } else {
            builder.setFormat(Core::VcdFormat::VCD_2_0);
        }

        // Default to PAL for now (could be made configurable)
        builder.setTvSystem(Core::TvSystem::PAL);

        builder.setProgressCallback([this](const Core::VcdProgress& progress) {
            if (m_cancelled) {
                return;
            }

            emit progressUpdated(progress.percent, 0, 0, 0, 0);
            emit statusChanged(QString::fromStdString(progress.currentOperation));
        });

        // Add videos
        emit statusChanged(tr("Adding videos..."));
        for (const QString& file : videoFiles) {
            if (!builder.addVideo(file.toStdString())) {
                emit error(tr("Failed to add video: %1").arg(file));
                emit finished(false, tr("Failed to add video"));
                return;
            }

            if (m_cancelled) {
                emit statusChanged(tr("Cancelled"));
                emit finished(false, tr("Operation cancelled by user"));
                return;
            }
        }

        if (builder.videoCount() == 0) {
            emit error(tr("No videos to burn"));
            emit finished(false, tr("No videos added"));
            return;
        }

        if (!builder.fitsOnDisc()) {
            emit error(tr("Total video duration exceeds disc capacity"));
            emit finished(false, tr("Videos too long for disc"));
            return;
        }

        // Create temporary directory for encoded files
        QTemporaryDir tempDir;
        if (!tempDir.isValid()) {
            emit error(tr("Failed to create temporary directory"));
            emit finished(false, tr("Temporary directory creation failed"));
            return;
        }

        emit statusChanged(tr("Encoding videos to %1 format...").arg(vcdType));

        // Encode videos
        auto encodeResult = builder.encodeVideos(tempDir.path().toStdString());
        if (encodeResult.hasError()) {
            emit error(QString::fromStdString(encodeResult.message));
            emit finished(false, QString::fromStdString(encodeResult.message));
            return;
        }

        // Get encoded files and burn them
        const auto& encodedFiles = builder.encodedFiles();
        if (encodedFiles.empty()) {
            emit error(tr("No encoded files available"));
            emit finished(false, tr("Encoding produced no output"));
            return;
        }

        emit statusChanged(tr("Burning Video CD..."));

        // For a proper VCD, we would need vcdimager to create the proper structure
        // For now, we'll burn the encoded MPEG files as data
        // This is a simplified approach - a full implementation would create proper VCD/SVCD structure

        Core::BurnEngine engine(devicePath.toStdString());
        if (!engine.isValid()) {
            emit error(tr("Failed to initialize burn engine"));
            emit finished(false, tr("Engine initialization failed"));
            return;
        }

        setupCallbacks(engine);

        Core::ImageBuilder imageBuilder;
        imageBuilder.setVolumeLabel(vcdType.toStdString());

        // Add encoded videos to the image
        // The directory structure will be created automatically by addFile
        for (size_t i = 0; i < encodedFiles.size(); ++i) {
            // For VCD, files go in MPEGAV directory with specific names
            imageBuilder.addFile(encodedFiles[i].string());
        }

        auto burnResult = engine.burnDataDisc(imageBuilder, options);

        if (burnResult.hasError()) {
            emit error(QString::fromStdString(burnResult.message));
            emit finished(false, QString::fromStdString(burnResult.message));
        } else {
            emit finished(true, tr("Video CD burn completed successfully"));
        }

    } catch (const std::exception& e) {
        emit error(QString::fromUtf8(e.what()));
        emit finished(false, QString::fromUtf8(e.what()));
    }
}

void BurnWorker::createIsoFile(const QString& outputPath,
                                const QStringList& filePaths,
                                const QString& volumeLabel) {
    m_cancelled = false;

    if (filePaths.isEmpty()) {
        emit error(tr("No files to add to ISO"));
        emit finished(false, tr("No files provided"));
        return;
    }

    // Validate files before starting
    emit statusChanged(tr("Validating files..."));
    QString validationError;
    if (!validateFiles(filePaths, validationError)) {
        emit error(validationError);
        emit finished(false, tr("File validation failed"));
        return;
    }

    try {
        emit statusChanged(tr("Building ISO image..."));

        Core::ImageBuilder builder;
        builder.setVolumeLabel(volumeLabel.isEmpty() ? "DATA_DISC" : volumeLabel.toStdString());

        int totalFiles = filePaths.size();
        int processedFiles = 0;

        for (const QString& path : filePaths) {
            if (m_cancelled) {
                emit statusChanged(tr("Cancelled"));
                emit finished(false, tr("Operation cancelled by user"));
                return;
            }

            QFileInfo info(path);
            if (info.isDir()) {
                builder.addDirectory(path.toStdString());
            } else {
                builder.addFile(path.toStdString());
            }

            ++processedFiles;
            double percent = 50.0 * processedFiles / totalFiles; // First 50% is adding files
            emit progressUpdated(percent, 0, 0, 0, 0);
        }

        if (builder.fileCount() == 0) {
            emit error(tr("No files to add to ISO"));
            emit finished(false, tr("No files added"));
            return;
        }

        emit statusChanged(tr("Writing ISO file (%1 bytes)...").arg(builder.totalSize()));

        // Save to file with progress callbacks
        // First callback is for the build phase (adding files to ISO structure)
        // Second callback is for the write phase (writing bytes to file)
        bool success = builder.saveToFile(
            outputPath.toStdString(),
            // Build progress callback
            [this](int processed, int total, const std::string& filename) {
                if (m_cancelled) {
                    return;
                }
                // First 10% is building the image structure
                double percent = 10.0 * processed / total;
                emit progressUpdated(percent, 0, 0, 0, 0);
                emit statusChanged(tr("Adding: %1").arg(QString::fromStdString(filename)));
            },
            // Write progress callback
            [this](std::uint64_t written, std::uint64_t total) {
                if (m_cancelled) {
                    return;
                }
                // Remaining 90% is writing the ISO data
                double percent = 10.0 + (90.0 * written / total);
                emit progressUpdated(percent, written, total, 0, 0);
                emit statusChanged(tr("Writing ISO: %1 / %2 MB")
                    .arg(written / (1024 * 1024))
                    .arg(total / (1024 * 1024)));
            }
        );

        if (success) {
            emit progressUpdated(100.0, 0, 0, 0, 0);
            emit finished(true, tr("ISO file created successfully: %1").arg(outputPath));
        } else {
            emit error(tr("Failed to write ISO file: %1").arg(outputPath));
            emit finished(false, tr("ISO file creation failed"));
        }

    } catch (const std::exception& e) {
        emit error(QString::fromUtf8(e.what()));
        emit finished(false, QString::fromUtf8(e.what()));
    }
}

void BurnWorker::createIsoFileWithStructure(const QString& outputPath,
                                             const FilePathMappingList& fileMappings,
                                             const QString& volumeLabel) {
    m_cancelled = false;

    if (fileMappings.isEmpty()) {
        emit error(tr("No files to add to ISO"));
        emit finished(false, tr("No files provided"));
        return;
    }

    // Validate files before starting
    emit statusChanged(tr("Validating files..."));
    QStringList localPaths;
    for (const auto& mapping : fileMappings) {
        localPaths.append(mapping.first);
    }
    QString validationError;
    if (!validateFiles(localPaths, validationError)) {
        emit error(validationError);
        emit finished(false, tr("File validation failed"));
        return;
    }

    try {
        emit statusChanged(tr("Building ISO image..."));

        Core::ImageBuilder builder;
        builder.setVolumeLabel(volumeLabel.isEmpty() ? "DATA_DISC" : volumeLabel.toStdString());

        int totalFiles = fileMappings.size();
        int processedFiles = 0;

        for (const auto& mapping : fileMappings) {
            if (m_cancelled) {
                emit statusChanged(tr("Cancelled"));
                emit finished(false, tr("Operation cancelled by user"));
                return;
            }

            const QString& localPath = mapping.first;
            const QString& isoPath = mapping.second;

            // Add file with its correct ISO path
            builder.addFile(localPath.toStdString(), isoPath.toStdString());

            ++processedFiles;
            double percent = 50.0 * processedFiles / totalFiles;
            emit progressUpdated(percent, 0, 0, 0, 0);
        }

        if (builder.fileCount() == 0) {
            emit error(tr("No files to add to ISO"));
            emit finished(false, tr("No files added"));
            return;
        }

        emit statusChanged(tr("Writing ISO file (%1 bytes)...").arg(builder.totalSize()));

        // Save to file with progress callbacks
        bool success = builder.saveToFile(
            outputPath.toStdString(),
            // Build progress callback
            [this](int processed, int total, const std::string& filename) {
                if (m_cancelled) {
                    return;
                }
                double percent = 10.0 * processed / total;
                emit progressUpdated(percent, 0, 0, 0, 0);
                emit statusChanged(tr("Adding: %1").arg(QString::fromStdString(filename)));
            },
            // Write progress callback
            [this](std::uint64_t written, std::uint64_t total) {
                if (m_cancelled) {
                    return;
                }
                double percent = 10.0 + (90.0 * written / total);
                emit progressUpdated(percent, written, total, 0, 0);
                emit statusChanged(tr("Writing ISO: %1 / %2 MB")
                    .arg(written / (1024 * 1024))
                    .arg(total / (1024 * 1024)));
            }
        );

        if (success) {
            emit progressUpdated(100.0, 0, 0, 0, 0);
            emit finished(true, tr("ISO file created successfully: %1").arg(outputPath));
        } else {
            emit error(tr("Failed to write ISO file: %1").arg(outputPath));
            emit finished(false, tr("ISO file creation failed"));
        }

    } catch (const std::exception& e) {
        emit error(QString::fromUtf8(e.what()));
        emit finished(false, QString::fromUtf8(e.what()));
    }
}

} // namespace Burner::Engine
