#include "BurnEngine.hpp"
#include "AudioEncoder.hpp"

#include <libburn/libburn.h>
#include <cstring>
#include <thread>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

using namespace burn;

namespace Burner::Core {

BurnEngine::BurnEngine(const std::string& devicePath)
    : m_devicePath(devicePath) {
    try {
        m_drive = std::make_unique<DriveHandle>(devicePath);
    } catch (const std::exception& e) {
        m_lastError = createError(-1, std::string("Failed to open drive: ") + e.what());
    }
}

BurnEngine::~BurnEngine() = default;

// Move operations - manually implemented because std::atomic is not movable
BurnEngine::BurnEngine(BurnEngine&& other) noexcept
    : m_devicePath(std::move(other.m_devicePath))
    , m_drive(std::move(other.m_drive))
    , m_progressCallback(std::move(other.m_progressCallback))
    , m_statusCallback(std::move(other.m_statusCallback))
    , m_cancelled(other.m_cancelled.load())
    , m_status(other.m_status.load())
    , m_lastError(std::move(other.m_lastError))
    , m_startTime(other.m_startTime) {
}

BurnEngine& BurnEngine::operator=(BurnEngine&& other) noexcept {
    if (this != &other) {
        m_devicePath = std::move(other.m_devicePath);
        m_drive = std::move(other.m_drive);
        m_progressCallback = std::move(other.m_progressCallback);
        m_statusCallback = std::move(other.m_statusCallback);
        m_cancelled.store(other.m_cancelled.load());
        m_status.store(other.m_status.load());
        m_lastError = std::move(other.m_lastError);
        m_startTime = other.m_startTime;
    }
    return *this;
}

bool BurnEngine::isValid() const {
    return m_drive && m_drive->isValid();
}

const std::string& BurnEngine::devicePath() const {
    return m_devicePath;
}

void BurnEngine::setProgressCallback(ProgressCallback callback) {
    m_progressCallback = std::move(callback);
}

void BurnEngine::setStatusCallback(StatusCallback callback) {
    m_statusCallback = std::move(callback);
}

BurnError BurnEngine::burnDataDisc(const ImageBuilder& builder,
                                    const BurnOptions& options) {
    // Build the ISO image
    reportStatus(BurnStatus::Preparing, "Building ISO image...");

    int totalFiles = builder.fileCount();
    int processed = 0;

    auto image = builder.build([this, &processed, totalFiles](int done, int total, const std::string& file) {
        processed = done;
        BurnProgress progress;
        progress.percent = (totalFiles > 0) ? (done * 100.0 / totalFiles) : 0;
        progress.currentOperation = "Adding: " + file;
        reportProgress(progress);
    });

    // Pass NWA for multi-session
    int nwa = builder.isMultiSession() ? builder.multiSessionNwa() : 0;
    return burnDataDisc(image, options, nwa);
}

BurnError BurnEngine::burnDataDisc(IsoImageHandle& image,
                                    const BurnOptions& options,
                                    int nwa) {
    if (!isValid()) {
        return createError(-1, "Drive not available");
    }

    if (m_cancelled) {
        return createError(-2, "Operation cancelled");
    }

    reportStatus(BurnStatus::Preparing, "Preparing burn source...");

    // Create burn source from ISO image (with NWA for multi-session)
    burn_source* isoSource = image.createBurnSource(nwa);
    if (!isoSource) {
        return createError(-3, "Failed to create burn source from ISO image");
    }

    std::uint64_t size = image.estimatedSize();
    return burnFromSource(isoSource, size, options);
}

BurnError BurnEngine::burnIsoFile(const std::filesystem::path& isoPath,
                                   const BurnOptions& options) {
    if (!isValid()) {
        return createError(-1, "Drive not available");
    }

    if (!std::filesystem::exists(isoPath)) {
        return createError(-4, "ISO file not found: " + isoPath.string());
    }

    reportStatus(BurnStatus::Preparing, "Opening ISO file...");

    // Get file size
    std::uint64_t fileSize = std::filesystem::file_size(isoPath);

    // Create burn source from file
    burn_source* fileSource = burn_file_source_new(isoPath.c_str(), nullptr);
    if (!fileSource) {
        return createError(-5, "Failed to open ISO file");
    }

    return burnFromSource(fileSource, fileSize, options);
}

BurnError BurnEngine::burnCueImage(const CueSheet& cueSheet,
                                    const BurnOptions& options) {
    if (!isValid()) {
        return createError(-1, "Drive not available");
    }

    if (cueSheet.trackCount() == 0) {
        return createError(-30, "CUE sheet has no tracks");
    }

    const auto& binPath = cueSheet.binPath();
    if (!std::filesystem::exists(binPath)) {
        return createError(-31, "BIN file not found: " + binPath.string());
    }

    // Single data track optimization: burn directly like an ISO file
    if (cueSheet.isSingleDataTrack()) {
        reportStatus(BurnStatus::Preparing, "Opening BIN file...");

        const auto& track = cueSheet.tracks()[0];
        std::uint64_t fileSize = std::filesystem::file_size(binPath);

        burn_source* fileSource = burn_file_source_new(binPath.c_str(), nullptr);
        if (!fileSource) {
            return createError(-32, "Failed to open BIN file");
        }

        // For MODE1/2352 (raw sectors), the effective data size after stripping
        // headers is smaller, but burnFromSource handles the raw source.
        // We use burnFromSource for MODE1/2048, and manual track setup for MODE1/2352.
        if (track.type == CueTrackType::Mode1_2048) {
            return burnFromSource(fileSource, fileSize, options);
        }

        // MODE1/2352 or MODE2: need custom track setup with offset/tail
        m_cancelled = false;
        m_lastError = BurnError{};
        m_startTime = std::chrono::steady_clock::now();

        burn_drive* drive = m_drive->raw();

        // Check disc status
        reportStatus(BurnStatus::Preparing, "Checking disc...");
        burn_disc_status discStatus = burn_disc_get_status(drive);

        if (discStatus == BURN_DISC_EMPTY) {
            burn_source_free(fileSource);
            return createError(-6, "No disc inserted");
        }

        if (options.blankBeforeBurn &&
            (discStatus == BURN_DISC_FULL || discStatus == BURN_DISC_APPENDABLE)) {
            int profileNum = 0;
            burn_disc_get_profile(drive, &profileNum, nullptr);
            bool isRewritable = (profileNum == 0x0A || profileNum == 0x13 ||
                                profileNum == 0x14 || profileNum == 0x1A ||
                                profileNum == 0x43);
            if (isRewritable) {
                auto blankResult = blankDisc(false);
                if (blankResult.hasError()) {
                    burn_source_free(fileSource);
                    return blankResult;
                }
            }
        }

        discStatus = burn_disc_get_status(drive);
        if (discStatus != BURN_DISC_BLANK && discStatus != BURN_DISC_APPENDABLE) {
            burn_source_free(fileSource);
            return createError(-8, "Disc is not writable");
        }

        // Create disc structure with raw sector handling
        burn_disc* disc = burn_disc_create();
        burn_session* session = burn_session_create();
        burn_track* burnTrack = burn_track_create();

        if (track.type == CueTrackType::Mode1_2352) {
            // Raw MODE1: 16-byte sync/header + 2048 data + 288 ECC/EDC = 2352
            burn_track_define_data(burnTrack, 16, 288, 1, BURN_MODE1);
        } else if (track.type == CueTrackType::Mode2_2352) {
            burn_track_define_data(burnTrack, 16, 280, 1, BURN_MODE1);
        } else {
            // MODE2/2336 or other
            burn_track_define_data(burnTrack, 0, 0, 1, BURN_MODE1);
        }

        burn_track_set_source(burnTrack, fileSource);
        burn_session_add_track(session, burnTrack, BURN_POS_END);
        burn_disc_add_session(disc, session, BURN_POS_END);

        // Configure write options
        burn_write_opts* writeOpts = burn_write_opts_new(drive);
        configureWriteOpts(writeOpts, options);

        char reasons[BURN_REASONS_LEN];
        enum burn_write_types writeType = burn_write_opts_auto_write_type(
            writeOpts, disc, reasons, 0);

        if (writeType == BURN_WRITE_NONE) {
            burn_write_opts_free(writeOpts);
            burn_track_free(burnTrack);
            burn_session_free(session);
            burn_disc_free(disc);
            return createError(-10, std::string("No suitable write mode: ") + reasons);
        }

        burn_write_opts_set_write_type(writeOpts, writeType, BURN_BLOCK_MODE1);

        // Start burning
        reportStatus(BurnStatus::Writing, "Writing BIN/CUE image...");
        burn_disc_write(writeOpts, disc);

        // Poll for progress
        {
            BurnProgress progress;
            progress.totalBytes = fileSize;

            while (true) {
                if (m_cancelled) {
                    burn_drive_cancel(drive);
                    reportStatus(BurnStatus::Cancelled, "Burn cancelled by user");
                    break;
                }

                burn_progress burnProgress;
                memset(&burnProgress, 0, sizeof(burnProgress));
                burn_drive_status driveStatus = burn_drive_get_status(drive, &burnProgress);

                if (driveStatus == BURN_DRIVE_IDLE) break;

                if (burnProgress.sectors > 0 && burnProgress.sector > 0) {
                    progress.percent = std::min(100.0 * burnProgress.sector / burnProgress.sectors, 99.0);
                }
                progress.bytesWritten = static_cast<std::uint64_t>(burnProgress.sector) * 2048;
                progress.currentTrack = burnProgress.track;
                progress.totalTracks = burnProgress.tracks;
                progress.currentSession = burnProgress.session;

                progress.bufferFillPercent = burnProgress.buffer_capacity > 0
                    ? (100 * burnProgress.buffer_available / burnProgress.buffer_capacity)
                    : 0;

                auto now = std::chrono::steady_clock::now();
                progress.elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_startTime);

                if (progress.elapsed.count() > 0 && progress.bytesWritten > 0) {
                    progress.writeSpeedKBps = static_cast<int>(
                        progress.bytesWritten / 1024 / progress.elapsed.count());
                }

                if (progress.percent > 0 && progress.percent < 100) {
                    double totalTime = progress.elapsed.count() / (progress.percent / 100.0);
                    progress.remaining = std::chrono::seconds(
                        static_cast<int>(totalTime - progress.elapsed.count()));
                }

                progress.currentOperation = "Writing...";
                reportProgress(progress);
                checkMessages();
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
        }

        // Clean up
        burn_write_opts_free(writeOpts);
        burn_track_free(burnTrack);
        burn_session_free(session);
        burn_disc_free(disc);

        if (m_cancelled) {
            burn_drive_cancel(drive);
            m_lastError = createError(-2, "Operation cancelled");
            return m_lastError;
        }

        checkMessages();
        if (m_lastError.hasError()) {
            burn_drive_cancel(drive);
            reportStatus(BurnStatus::Failed, m_lastError.message);
            return m_lastError;
        }

        BurnProgress finalProgress;
        finalProgress.percent = 100.0;
        finalProgress.currentOperation = "Finalizing...";
        reportProgress(finalProgress);

        if (options.verify && !options.simulate) {
            reportStatus(BurnStatus::Verifying, "Verifying burned data...");
            auto verifyError = verifyBurnedData(fileSize);
            if (verifyError.hasError()) {
                reportStatus(BurnStatus::Failed, "Verification failed: " + verifyError.message);
                return verifyError;
            }
        }

        if (options.ejectAfter && !options.simulate) {
            m_drive->release(true);
        }

        reportStatus(BurnStatus::Completed,
            options.simulate ? "Simulation completed successfully"
                             : "Burn completed successfully");
        return BurnError{};
    }

    // Multi-track path: extract each track to a temp file
    m_cancelled = false;
    m_lastError = BurnError{};
    m_startTime = std::chrono::steady_clock::now();

    burn_drive* drive = m_drive->raw();

    reportStatus(BurnStatus::Preparing, "Checking disc...");
    burn_disc_status discStatus = burn_disc_get_status(drive);

    if (discStatus == BURN_DISC_EMPTY) {
        return createError(-6, "No disc inserted");
    }

    if (discStatus != BURN_DISC_BLANK) {
        return createError(-13, "Multi-track CUE images require a blank disc");
    }

    // Open BIN file for reading
    std::ifstream binFile(binPath, std::ios::binary);
    if (!binFile) {
        return createError(-33, "Failed to open BIN file: " + binPath.string());
    }

    // Create disc structure
    burn_disc* disc = burn_disc_create();
    burn_session* session = burn_session_create();

    std::uint64_t totalBytes = 0;
    std::vector<std::filesystem::path> tempFiles;

    reportStatus(BurnStatus::Preparing, "Extracting tracks from BIN file...");

    for (int i = 0; i < cueSheet.trackCount(); ++i) {
        if (m_cancelled) {
            burn_session_free(session);
            burn_disc_free(disc);
            for (const auto& tf : tempFiles) std::filesystem::remove(tf);
            return createError(-2, "Operation cancelled");
        }

        const auto& cueTrack = cueSheet.tracks()[static_cast<std::size_t>(i)];

        reportStatus(BurnStatus::Preparing,
            "Extracting track " + std::to_string(i + 1) + "/" +
            std::to_string(cueSheet.trackCount()));

        // Extract track data from BIN to temp file
        std::filesystem::path tempPath = std::filesystem::temp_directory_path() /
            ("burner_cue_track_" + std::to_string(i) + ".raw");
        tempFiles.push_back(tempPath);

        std::ofstream tempFile(tempPath, std::ios::binary);
        if (!tempFile) {
            burn_session_free(session);
            burn_disc_free(disc);
            for (const auto& tf : tempFiles) std::filesystem::remove(tf);
            return createError(-34, "Failed to create temporary file");
        }

        binFile.seekg(static_cast<std::streamoff>(cueTrack.dataStartByte));

        // Copy track data in chunks
        constexpr std::size_t CHUNK_SIZE = 64 * 1024;  // 64KB
        std::vector<char> buffer(CHUNK_SIZE);
        std::uint64_t remaining = cueTrack.dataLengthBytes;

        while (remaining > 0) {
            auto toRead = static_cast<std::streamsize>(
                std::min(remaining, static_cast<std::uint64_t>(CHUNK_SIZE)));
            binFile.read(buffer.data(), toRead);
            auto bytesRead = binFile.gcount();
            if (bytesRead <= 0) break;
            tempFile.write(buffer.data(), bytesRead);
            remaining -= static_cast<std::uint64_t>(bytesRead);
        }
        tempFile.close();

        // Create libburn track
        burn_track* burnTrack = burn_track_create();

        switch (cueTrack.type) {
            case CueTrackType::Audio:
                burn_track_define_data(burnTrack, 0, 0, 0, BURN_AUDIO);
                break;
            case CueTrackType::Mode1_2352:
                burn_track_define_data(burnTrack, 16, 288, 1, BURN_MODE1);
                break;
            case CueTrackType::Mode1_2048:
                burn_track_define_data(burnTrack, 0, 0, 1, BURN_MODE1);
                break;
            case CueTrackType::Mode2_2352:
                burn_track_define_data(burnTrack, 16, 280, 1, BURN_MODE1);
                break;
            case CueTrackType::Mode2_2336:
                burn_track_define_data(burnTrack, 0, 0, 1, BURN_MODE1);
                break;
            default:
                burn_track_define_data(burnTrack, 0, 0, 1, BURN_MODE1);
                break;
        }

        burn_source* trackSource = burn_file_source_new(tempPath.c_str(), nullptr);
        if (!trackSource) {
            burn_track_free(burnTrack);
            burn_session_free(session);
            burn_disc_free(disc);
            for (const auto& tf : tempFiles) std::filesystem::remove(tf);
            return createError(-35, "Failed to create track source for track " +
                              std::to_string(i + 1));
        }

        burn_track_set_source(burnTrack, trackSource);
        burn_session_add_track(session, burnTrack, BURN_POS_END);
        totalBytes += cueTrack.dataLengthBytes;
    }

    binFile.close();
    burn_disc_add_session(disc, session, BURN_POS_END);

    // Configure write options
    burn_write_opts* writeOpts = burn_write_opts_new(drive);
    configureWriteOpts(writeOpts, options);

    // For multi-track, prefer SAO/DAO
    char reasons[BURN_REASONS_LEN];
    enum burn_write_types writeType = burn_write_opts_auto_write_type(
        writeOpts, disc, reasons, 0);

    if (writeType == BURN_WRITE_NONE) {
        burn_write_opts_free(writeOpts);
        burn_session_free(session);
        burn_disc_free(disc);
        for (const auto& tf : tempFiles) std::filesystem::remove(tf);
        return createError(-10, std::string("No suitable write mode for CUE image: ") + reasons);
    }

    // Use RAW0 block type for proper multi-track handling (same as audio CD)
    burn_write_opts_set_write_type(writeOpts, writeType, BURN_BLOCK_RAW0);

    // Start burning
    reportStatus(BurnStatus::Writing, "Writing CUE image...");
    burn_disc_write(writeOpts, disc);

    // Poll for progress
    BurnProgress progress;
    progress.totalBytes = totalBytes;
    progress.totalTracks = cueSheet.trackCount();

    while (true) {
        if (m_cancelled) {
            burn_drive_cancel(drive);
            reportStatus(BurnStatus::Cancelled, "Burn cancelled by user");
            break;
        }

        burn_progress burnProgress;
        burn_drive_status driveStatus = burn_drive_get_status(drive, &burnProgress);

        if (driveStatus == BURN_DRIVE_IDLE) break;

        if (burnProgress.sectors > 0) {
            progress.percent = std::min(100.0 * burnProgress.sector / burnProgress.sectors, 99.0);
        }
        progress.bytesWritten = static_cast<std::uint64_t>(burnProgress.sector) * 2352;
        progress.currentTrack = burnProgress.track;
        progress.currentSession = burnProgress.session;

        progress.bufferFillPercent = burnProgress.buffer_capacity > 0
            ? (100 * burnProgress.buffer_available / burnProgress.buffer_capacity)
            : 0;

        auto now = std::chrono::steady_clock::now();
        progress.elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_startTime);

        if (progress.elapsed.count() > 0 && progress.bytesWritten > 0) {
            progress.writeSpeedKBps = static_cast<int>(
                progress.bytesWritten / 1024 / progress.elapsed.count());
        }

        if (progress.percent > 0 && progress.percent < 100) {
            double totalTime = progress.elapsed.count() / (progress.percent / 100.0);
            progress.remaining = std::chrono::seconds(
                static_cast<int>(totalTime - progress.elapsed.count()));
        }

        progress.currentOperation = "Writing track " + std::to_string(burnProgress.track + 1);
        reportProgress(progress);
        checkMessages();
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    // Clean up
    burn_write_opts_free(writeOpts);
    burn_session_free(session);
    burn_disc_free(disc);

    // Remove temp files
    for (const auto& tf : tempFiles) {
        std::filesystem::remove(tf);
    }

    if (m_cancelled) {
        m_lastError = createError(-2, "Operation cancelled");
        return m_lastError;
    }

    checkMessages();
    if (m_lastError.hasError()) {
        burn_drive_cancel(drive);
        reportStatus(BurnStatus::Failed, m_lastError.message);
        return m_lastError;
    }

    progress.percent = 100.0;
    progress.currentOperation = "Finalizing...";
    reportProgress(progress);

    if (options.verify && !options.simulate) {
        reportStatus(BurnStatus::Verifying, "Verifying burned data...");
        auto verifyError = verifyBurnedData(totalBytes);
        if (verifyError.hasError()) {
            reportStatus(BurnStatus::Failed, "Verification failed: " + verifyError.message);
            return verifyError;
        }
    }

    if (options.ejectAfter && !options.simulate) {
        m_drive->release(true);
    }

    reportStatus(BurnStatus::Completed,
        options.simulate ? "Simulation completed successfully"
                         : "CUE image burn completed successfully");
    return BurnError{};
}

BurnError BurnEngine::burnAudioCD(const std::vector<std::filesystem::path>& audioFiles,
                                   const BurnOptions& options) {
    if (!isValid()) {
        return createError(-1, "Drive not available");
    }

    if (audioFiles.empty()) {
        return createError(-12, "No audio files provided");
    }

    m_cancelled = false;
    m_startTime = std::chrono::steady_clock::now();

    burn_drive* drive = m_drive->raw();

    // Check disc status
    reportStatus(BurnStatus::Preparing, "Checking disc...");

    burn_disc_status discStatus = burn_disc_get_status(drive);

    if (discStatus == BURN_DISC_EMPTY) {
        return createError(-6, "No disc inserted");
    }

    if (discStatus != BURN_DISC_BLANK) {
        return createError(-13, "Audio CD requires a blank disc");
    }

    // Create disc structure
    burn_disc* disc = burn_disc_create();
    burn_session* session = burn_session_create();

    // Calculate total size for progress
    std::uint64_t totalBytes = 0;
    std::vector<AudioInfo> trackInfos;
    trackInfos.reserve(audioFiles.size());

    for (const auto& file : audioFiles) {
        auto info = AudioEncoder::getAudioInfo(file);
        trackInfos.push_back(info);
        totalBytes += static_cast<std::uint64_t>(AudioEncoder::estimateCddaSize(info.durationMs));
    }

    std::uint64_t processedBytes = 0;
    int trackNum = 0;

    // Process each track
    for (size_t i = 0; i < audioFiles.size(); ++i) {
        if (m_cancelled) {
            burn_session_free(session);
            burn_disc_free(disc);
            return createError(-2, "Operation cancelled");
        }

        const auto& audioFile = audioFiles[i];
        const auto& info = trackInfos[i];

        reportStatus(BurnStatus::Preparing,
            "Converting track " + std::to_string(i + 1) + "/" +
            std::to_string(audioFiles.size()) + ": " + audioFile.filename().string());

        // Decode audio to CD-DA format
        AudioEncoder encoder;
        auto pcmData = encoder.decodeToCddaBuffer(audioFile,
            [this, &processedBytes, totalBytes](std::int64_t current, std::int64_t total) {
                BurnProgress progress;
                progress.percent = 100.0 * (processedBytes + static_cast<std::uint64_t>(current)) / totalBytes;
                progress.currentOperation = "Converting audio...";
                reportProgress(progress);
            });

        if (pcmData.empty()) {
            burn_session_free(session);
            burn_disc_free(disc);
            return createError(-14, "Failed to decode audio file: " + audioFile.string() +
                              " - " + encoder.lastError());
        }

        // Create track for this audio
        burn_track* track = burn_track_create();

        // Set track mode for audio (CD-DA)
        // BURN_AUDIO = 0 for CD-DA audio
        burn_track_define_data(track, 0, 0, 0, BURN_AUDIO);

        // Create burn source from PCM data
        // We need to keep the data alive, so we use a file-based approach
        // Create a temporary file with the PCM data
        std::filesystem::path tempPath = std::filesystem::temp_directory_path() /
            ("burner_track_" + std::to_string(i) + ".raw");

        std::ofstream tempFile(tempPath, std::ios::binary);
        if (!tempFile) {
            burn_track_free(track);
            burn_session_free(session);
            burn_disc_free(disc);
            return createError(-15, "Failed to create temporary file");
        }
        tempFile.write(reinterpret_cast<const char*>(pcmData.data()),
                       static_cast<std::streamsize>(pcmData.size()));
        tempFile.close();

        // Pad to CD frame boundary (2352 bytes)
        std::uint64_t paddedSize = pcmData.size();
        if (paddedSize % CdDaSpec::CD_FRAME_SIZE != 0) {
            paddedSize = ((paddedSize / CdDaSpec::CD_FRAME_SIZE) + 1) * CdDaSpec::CD_FRAME_SIZE;
        }

        burn_source* trackSource = burn_file_source_new(tempPath.c_str(), nullptr);
        if (!trackSource) {
            burn_track_free(track);
            burn_session_free(session);
            burn_disc_free(disc);
            std::filesystem::remove(tempPath);
            return createError(-16, "Failed to create track source");
        }

        burn_track_set_source(track, trackSource);
        burn_session_add_track(session, track, BURN_POS_END);

        processedBytes += pcmData.size();
        ++trackNum;
    }

    burn_disc_add_session(disc, session, BURN_POS_END);

    // Configure write options
    burn_write_opts* writeOpts = burn_write_opts_new(drive);
    configureWriteOpts(writeOpts, options);

    // For audio CDs, we need SAO (Session-At-Once) or DAO (Disc-At-Once)
    char reasons[BURN_REASONS_LEN];
    enum burn_write_types writeType = burn_write_opts_auto_write_type(writeOpts, disc, reasons, 0);

    if (writeType == BURN_WRITE_NONE) {
        burn_write_opts_free(writeOpts);
        burn_session_free(session);
        burn_disc_free(disc);
        return createError(-10, std::string("No suitable write mode for audio CD: ") + reasons);
    }

    burn_write_opts_set_write_type(writeOpts, writeType, BURN_BLOCK_RAW0);

    // Start burning
    reportStatus(BurnStatus::Writing, "Writing audio CD...");

    burn_disc_write(writeOpts, disc);

    // Poll for progress
    BurnProgress progress;
    progress.totalBytes = totalBytes;
    progress.totalTracks = trackNum;

    while (true) {
        if (m_cancelled) {
            burn_drive_cancel(drive);
            reportStatus(BurnStatus::Cancelled, "Burn cancelled by user");
            break;
        }

        burn_progress burnProgress;
        burn_drive_status driveStatus = burn_drive_get_status(drive, &burnProgress);

        if (driveStatus == BURN_DRIVE_IDLE) {
            break;
        }

        if (driveStatus == BURN_DRIVE_WRITING) {
            if (burnProgress.sectors > 0) {
                progress.percent = 100.0 * burnProgress.sector / burnProgress.sectors;
            }
            progress.bytesWritten = static_cast<std::uint64_t>(burnProgress.sector) * CdDaSpec::CD_FRAME_SIZE;
            progress.currentTrack = burnProgress.track;
            progress.currentSession = burnProgress.session;

            auto now = std::chrono::steady_clock::now();
            progress.elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_startTime);

            if (progress.percent > 0) {
                double totalTime = progress.elapsed.count() / (progress.percent / 100.0);
                progress.remaining = std::chrono::seconds(
                    static_cast<int>(totalTime - progress.elapsed.count()));
            }

            progress.currentOperation = "Writing track " + std::to_string(burnProgress.track + 1);
            reportProgress(progress);
        }

        checkMessages();
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    // Clean up
    burn_write_opts_free(writeOpts);
    burn_session_free(session);
    burn_disc_free(disc);

    // Remove temporary files
    for (size_t i = 0; i < audioFiles.size(); ++i) {
        std::filesystem::path tempPath = std::filesystem::temp_directory_path() /
            ("burner_track_" + std::to_string(i) + ".raw");
        std::filesystem::remove(tempPath);
    }

    if (m_cancelled) {
        m_lastError = createError(-2, "Operation cancelled");
        return m_lastError;
    }

    checkMessages();

    // Verify if requested
    if (options.verify) {
        reportStatus(BurnStatus::Verifying, "Verifying audio CD...");
        BurnError verifyError = verifyBurnedData(totalBytes);
        if (verifyError.hasError()) {
            reportStatus(BurnStatus::Failed, "Verification failed: " + verifyError.message);
            return verifyError;
        }
    }

    // Eject if requested
    if (options.ejectAfter) {
        m_drive->release(true);
    }

    reportStatus(BurnStatus::Completed, "Audio CD burn completed successfully");
    return BurnError{};
}

BurnError BurnEngine::burnFromSource(burn_source* source,
                                       std::uint64_t sizeInBytes,
                                       const BurnOptions& options) {
    fprintf(stderr, "burnFromSource: Starting, size=%lu, simulate=%d\n",
            sizeInBytes, options.simulate);

    if (!isValid()) {
        if (source) burn_source_free(source);
        return createError(-1, "Drive not available");
    }

    // Get the actual size from the burn_source if available
    // This is more accurate than the passed size for libisofs sources
    if (source && source->get_size) {
        off_t actualSize = source->get_size(source);
        if (actualSize > 0) {
            fprintf(stderr, "burnFromSource: Actual source size from get_size=%ld\n",
                    static_cast<long>(actualSize));
            sizeInBytes = static_cast<std::uint64_t>(actualSize);
        }
    }

    m_cancelled = false;
    m_lastError = BurnError{}; // Clear any previous error
    m_startTime = std::chrono::steady_clock::now();

    fprintf(stderr, "burnFromSource: Getting drive handle\n");
    burn_drive* drive = m_drive->raw();
    fprintf(stderr, "burnFromSource: Got drive=%p\n", static_cast<void*>(drive));

    // Check disc status
    reportStatus(BurnStatus::Preparing, "Checking disc...");

    fprintf(stderr, "burnFromSource: Checking disc status\n");
    burn_disc_status discStatus = burn_disc_get_status(drive);
    fprintf(stderr, "burnFromSource: Disc status=%d\n", discStatus);

    if (discStatus == BURN_DISC_EMPTY) {
        burn_source_free(source);
        return createError(-6, "No disc inserted");
    }

    if (discStatus == BURN_DISC_UNSUITABLE) {
        burn_source_free(source);
        return createError(-7, "Unsuitable disc type");
    }

    // Blank if needed
    if (options.blankBeforeBurn &&
        (discStatus == BURN_DISC_FULL || discStatus == BURN_DISC_APPENDABLE)) {

        // Check if rewritable
        int profileNum = 0;
        burn_disc_get_profile(drive, &profileNum, nullptr);

        bool isRewritable = (profileNum == 0x0A || // CD-RW
                            profileNum == 0x13 ||  // DVD-RW restricted
                            profileNum == 0x14 ||  // DVD-RW sequential
                            profileNum == 0x1A ||  // DVD+RW
                            profileNum == 0x43);   // BD-RE

        if (isRewritable) {
            auto blankResult = blankDisc(false);
            if (blankResult.hasError()) {
                burn_source_free(source);
                return blankResult;
            }
        }
    }

    // Wait for disc to be ready
    fprintf(stderr, "burnFromSource: Rechecking disc status\n");
    discStatus = burn_disc_get_status(drive);
    fprintf(stderr, "burnFromSource: Disc status after recheck=%d\n", discStatus);
    if (discStatus != BURN_DISC_BLANK && discStatus != BURN_DISC_APPENDABLE) {
        burn_source_free(source);
        return createError(-8, "Disc is not writable (status: " + std::to_string(discStatus) + ")");
    }

    // Get disc profile info for debugging
    int profileNum = 0;
    char profileName[80] = {0};
    burn_disc_get_profile(drive, &profileNum, profileName);
    fprintf(stderr, "burnFromSource: Disc profile=%d (%s)\n", profileNum, profileName);

    // Warn about simulation limitations on DVD-R/BD-R (write-once media)
    // Simulation may not work correctly and can affect disc state
    if (options.simulate) {
        bool isWriteOnce = (profileNum == 0x11 || // DVD-R
                           profileNum == 0x15 || // DVD-R DL
                           profileNum == 0x1B || // DVD+R
                           profileNum == 0x2B || // DVD+R DL
                           profileNum == 0x41 || // BD-R SRM
                           profileNum == 0x42);  // BD-R RRM
        if (isWriteOnce) {
            fprintf(stderr, "burnFromSource: WARNING - Simulation on write-once media (DVD-R/BD-R) may not work correctly\n");
            reportStatus(BurnStatus::Preparing,
                "Note: Simulation on write-once media may not work correctly on all drives");
        }
    }

    // Use source directly without FIFO buffer
    // FIFO can cause issues with certain disc states and doesn't provide much benefit
    // for modern drives with large internal buffers
    fprintf(stderr, "burnFromSource: Using source directly (no FIFO)\n");
    burn_source* fifoSource = source;
    source = nullptr;  // Prevent double-free

    // Create disc structure
    fprintf(stderr, "burnFromSource: Creating disc structure\n");
    burn_disc* disc = burn_disc_create();
    burn_session* session = burn_session_create();
    burn_track* track = burn_track_create();

    fprintf(stderr, "burnFromSource: Defining track data\n");
    burn_track_define_data(track, 0, 0, 1, BURN_MODE1);
    burn_track_set_source(track, fifoSource);

    burn_session_add_track(session, track, BURN_POS_END);
    burn_disc_add_session(disc, session, BURN_POS_END);
    fprintf(stderr, "burnFromSource: Disc structure created\n");

    // Configure write options
    fprintf(stderr, "burnFromSource: Creating write options\n");
    burn_write_opts* writeOpts = burn_write_opts_new(drive);
    fprintf(stderr, "burnFromSource: Configuring write options\n");
    configureWriteOpts(writeOpts, options);
    fprintf(stderr, "burnFromSource: Write options configured\n");

    // Check disc capacity with write options for accurate calculation
    off_t availableSpace = burn_disc_available_space(drive, writeOpts);
    fprintf(stderr, "burnFromSource: Reported available space=%ld bytes (%ld MB), needed=%lu bytes (%lu MB)\n",
            static_cast<long>(availableSpace), static_cast<long>(availableSpace / (1024*1024)),
            sizeInBytes, sizeInBytes / (1024*1024));

    // For blank media, the reported available space might be wrong after a failed/simulated burn.
    // Use known capacities based on profile as fallback.
    off_t expectedCapacity = 0;
    switch (profileNum) {
        case 0x09: // CD-R
        case 0x0A: // CD-RW
            expectedCapacity = 737280000LL; // 700 MB
            break;
        case 0x11: // DVD-R
        case 0x13: // DVD-RW restricted
        case 0x14: // DVD-RW sequential
        case 0x1A: // DVD+RW
        case 0x1B: // DVD+R
            expectedCapacity = 4707319808LL; // 4.7 GB (single layer)
            break;
        case 0x15: // DVD-R DL
        case 0x2B: // DVD+R DL
            expectedCapacity = 8543666176LL; // 8.5 GB (dual layer)
            break;
        case 0x41: // BD-R SRM
        case 0x42: // BD-R RRM
        case 0x43: // BD-RE
            expectedCapacity = 25025314816LL; // 25 GB (single layer)
            break;
    }

    // If reported capacity seems too small for a blank disc, use expected capacity
    if (discStatus == BURN_DISC_BLANK && expectedCapacity > 0 &&
        availableSpace < expectedCapacity / 2) {
        fprintf(stderr, "burnFromSource: Reported capacity seems too small for blank disc, using expected %ld bytes (%ld MB)\n",
                static_cast<long>(expectedCapacity), static_cast<long>(expectedCapacity / (1024*1024)));
        availableSpace = expectedCapacity;
    }

    if (availableSpace > 0 && sizeInBytes > static_cast<std::uint64_t>(availableSpace)) {
        burn_write_opts_free(writeOpts);
        burn_track_free(track);
        burn_session_free(session);
        burn_disc_free(disc);
        burn_source_free(fifoSource);
        return createError(-11, "Data too large for disc. Need " +
            std::to_string(sizeInBytes / (1024*1024)) + " MB, but disc has only " +
            std::to_string(availableSpace / (1024*1024)) + " MB available.");
    }

    // Determine best write type using burn_write_opts_auto_write_type
    char reasons[BURN_REASONS_LEN];
    enum burn_write_types writeType = burn_write_opts_auto_write_type(writeOpts, disc, reasons, 0);

    if (writeType == BURN_WRITE_NONE) {
        // No suitable write type found
        burn_write_opts_free(writeOpts);
        burn_track_free(track);
        burn_session_free(session);
        burn_disc_free(disc);
        burn_source_free(fifoSource);
        return createError(-10, std::string("No suitable write mode available: ") + reasons);
    }

    // Set the determined write type
    // For TAO mode, use BURN_BLOCK_MODE1; for SAO/DAO, let libburn choose (pass 0)
    enum burn_block_types blockType = (writeType == BURN_WRITE_TAO) ? BURN_BLOCK_MODE1 : BURN_BLOCK_MODE1;
    fprintf(stderr, "burnFromSource: Setting write type=%d, block type=%d\n", writeType, blockType);
    burn_write_opts_set_write_type(writeOpts, writeType, blockType);

    // Start burning
    reportStatus(BurnStatus::Writing, "Starting burn...");

    fprintf(stderr, "burnFromSource: Calling burn_disc_write\n");
    burn_disc_write(writeOpts, disc);
    fprintf(stderr, "burnFromSource: burn_disc_write returned, entering poll loop\n");

    // Don't check messages immediately - go straight to polling
    // libburn uses async operation, errors will be reported during polling

    // Poll for progress
    BurnProgress progress;
    progress.totalBytes = sizeInBytes;

    fprintf(stderr, "burnFromSource: Starting poll loop\n");

    while (true) {
        if (m_cancelled) {
            fprintf(stderr, "burnFromSource: Cancelled by user\n");
            burn_drive_cancel(drive);
            reportStatus(BurnStatus::Cancelled, "Burn cancelled by user");
            break;
        }

        burn_progress burnProgress;
        memset(&burnProgress, 0, sizeof(burnProgress));
        fprintf(stderr, "burnFromSource: Calling burn_drive_get_status\n");
        burn_drive_status driveStatus = burn_drive_get_status(drive, &burnProgress);
        fprintf(stderr, "burnFromSource: Drive status=%d\n", driveStatus);

        if (driveStatus == BURN_DRIVE_IDLE) {
            fprintf(stderr, "burnFromSource: Drive is idle, exiting loop\n");
            break;
        }

        // Update progress for all active states
        // Cap at 99% until we're actually done - actual completion comes from IDLE status
        if (burnProgress.sectors > 0 && burnProgress.sector > 0) {
            double rawPercent = 100.0 * burnProgress.sector / burnProgress.sectors;
            progress.percent = std::min(rawPercent, 99.0);
        }
        progress.bytesWritten = static_cast<std::uint64_t>(burnProgress.sector) * 2048;
        progress.currentTrack = burnProgress.track;
        progress.totalTracks = burnProgress.tracks;
        progress.currentSession = burnProgress.session;

        // Buffer status from drive (if available)
        progress.bufferFillPercent = burnProgress.buffer_capacity > 0
            ? (100 * burnProgress.buffer_available / burnProgress.buffer_capacity)
            : 0;

        // Calculate times and speed
        auto now = std::chrono::steady_clock::now();
        progress.elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_startTime);

        // Calculate write speed (KB/s) from bytes written over elapsed time
        if (progress.elapsed.count() > 0 && progress.bytesWritten > 0) {
            progress.writeSpeedKBps = static_cast<int>(
                progress.bytesWritten / 1024 / progress.elapsed.count());
        }

        if (progress.percent > 0 && progress.percent < 100) {
            double totalTime = progress.elapsed.count() / (progress.percent / 100.0);
            progress.remaining = std::chrono::seconds(
                static_cast<int>(totalTime - progress.elapsed.count()));
        }

        // Set operation based on drive state
        switch (driveStatus) {
            case BURN_DRIVE_SPAWNING:
                progress.currentOperation = "Preparing...";
                reportStatus(BurnStatus::Preparing, "Preparing...");
                break;
            case BURN_DRIVE_WRITING:
            case BURN_DRIVE_WRITING_LEADIN:
            case BURN_DRIVE_WRITING_PREGAP:
            case BURN_DRIVE_WRITING_SYNC:
                progress.currentOperation = "Writing...";
                reportStatus(BurnStatus::Writing, "Writing...");
                break;
            case BURN_DRIVE_WRITING_LEADOUT:
            case BURN_DRIVE_CLOSING_TRACK:
            case BURN_DRIVE_CLOSING_SESSION:
                progress.currentOperation = "Closing disc...";
                reportStatus(BurnStatus::Closing, "Closing disc...");
                break;
            case BURN_DRIVE_FORMATTING:
                progress.currentOperation = "Formatting...";
                reportStatus(BurnStatus::Preparing, "Formatting...");
                break;
            case BURN_DRIVE_ERASING:
                progress.currentOperation = "Erasing...";
                reportStatus(BurnStatus::Blanking, "Erasing...");
                break;
            case BURN_DRIVE_GRABBING:
                progress.currentOperation = "Accessing drive...";
                break;
            default:
                progress.currentOperation = "Working...";
                break;
        }

        reportProgress(progress);
        checkMessages();

        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    // Check for errors from libburn
    checkMessages();

    // Clean up
    burn_write_opts_free(writeOpts);
    burn_track_free(track);
    burn_session_free(session);
    burn_disc_free(disc);
    burn_source_free(fifoSource);

    if (m_cancelled) {
        // Explicitly cancel any ongoing operation
        burn_drive_cancel(drive);
        m_lastError = createError(-2, "Operation cancelled");
        return m_lastError;
    }

    // Check if libburn reported any errors during the burn
    if (m_lastError.hasError()) {
        // Cancel to clean up drive state after error
        burn_drive_cancel(drive);
        fprintf(stderr, "burnFromSource: Burn failed with error: %s\n", m_lastError.message.c_str());
        reportStatus(BurnStatus::Failed, m_lastError.message);
        return m_lastError;
    }

    // Report 100% now that we're done
    progress.percent = 100.0;
    progress.currentOperation = "Finalizing...";
    reportProgress(progress);

    // Verify if requested (skip for simulation)
    if (options.verify && !options.simulate) {
        reportStatus(BurnStatus::Verifying, "Verifying burned data...");
        BurnError verifyError = verifyBurnedData(sizeInBytes);
        if (verifyError.hasError()) {
            reportStatus(BurnStatus::Failed, "Verification failed: " + verifyError.message);
            return verifyError;
        }
    }

    // Eject if requested (skip for simulation)
    if (options.ejectAfter && !options.simulate) {
        m_drive->release(true);
    }

    if (options.simulate) {
        reportStatus(BurnStatus::Completed, "Simulation completed successfully");
    } else {
        reportStatus(BurnStatus::Completed, "Burn completed successfully");
    }

    return BurnError{}; // Success
}

BurnError BurnEngine::blankDisc(bool full) {
    if (!isValid()) {
        return createError(-1, "Drive not available");
    }

    burn_drive* drive = m_drive->raw();

    reportStatus(BurnStatus::Blanking, full ? "Full blanking disc..." : "Quick blanking disc...");

    burn_disc_erase(drive, full ? 1 : 0);

    // Wait for completion
    while (true) {
        if (m_cancelled) {
            burn_drive_cancel(drive);
            return createError(-2, "Blanking cancelled");
        }

        burn_progress progress;
        burn_drive_status status = burn_drive_get_status(drive, &progress);

        if (status == BURN_DRIVE_IDLE) {
            break;
        }

        if (status == BURN_DRIVE_ERASING) {
            BurnProgress p;
            if (progress.sectors > 0) {
                p.percent = 100.0 * progress.sector / progress.sectors;
            }
            p.currentOperation = "Blanking disc...";
            reportProgress(p);
        }

        checkMessages();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    reportStatus(BurnStatus::Idle, "Blanking complete");
    return BurnError{};
}

BurnError BurnEngine::formatDisc() {
    if (!isValid()) {
        return createError(-1, "Drive not available");
    }

    burn_drive* drive = m_drive->raw();

    reportStatus(BurnStatus::Preparing, "Formatting disc...");

    // Format for DVD+RW or BD-RE (burn_disc_format returns void)
    burn_disc_format(drive, 0, 0);

    // Wait for completion
    while (true) {
        if (m_cancelled) {
            burn_drive_cancel(drive);
            return createError(-2, "Formatting cancelled");
        }

        burn_progress progress;
        burn_drive_status status = burn_drive_get_status(drive, &progress);

        if (status == BURN_DRIVE_IDLE) {
            break;
        }

        checkMessages();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    reportStatus(BurnStatus::Idle, "Format complete");
    return BurnError{};
}

IntegrityResult BurnEngine::checkIntegrity(bool stopOnFirstError) {
    IntegrityResult result;

    if (!isValid()) {
        result.message = "Drive not available";
        return result;
    }

    m_cancelled = false;
    m_startTime = std::chrono::steady_clock::now();

    reportStatus(BurnStatus::Verifying, "Checking disc integrity...");

    // Open device for reading
    int fd = open(m_devicePath.c_str(), O_RDONLY);
    if (fd < 0) {
        result.message = "Failed to open device: " + std::string(strerror(errno));
        reportStatus(BurnStatus::Failed, result.message);
        return result;
    }

    // Get disc size via ioctl
    std::uint64_t discSize = 0;
    if (ioctl(fd, BLKGETSIZE64, &discSize) < 0) {
        close(fd);
        result.message = "Failed to get disc size: " + std::string(strerror(errno));
        reportStatus(BurnStatus::Failed, result.message);
        return result;
    }

    if (discSize == 0) {
        close(fd);
        result.message = "No data on disc";
        reportStatus(BurnStatus::Failed, result.message);
        return result;
    }

    constexpr std::uint64_t SECTOR_SIZE = 2048;
    constexpr std::uint64_t SECTORS_PER_READ = 64;  // 128KB chunks
    constexpr std::uint64_t BUFFER_SIZE = SECTOR_SIZE * SECTORS_PER_READ;

    std::uint64_t totalSectors = discSize / SECTOR_SIZE;
    std::uint64_t currentSector = 0;

    std::vector<char> buffer(BUFFER_SIZE);
    BurnProgress progress;
    progress.totalBytes = discSize;

    while (currentSector < totalSectors && !m_cancelled) {
        // Calculate how many sectors to read this iteration
        std::uint64_t sectorsToRead = std::min(SECTORS_PER_READ, totalSectors - currentSector);
        std::uint64_t bytesToRead = sectorsToRead * SECTOR_SIZE;

        // Seek to current position
        off_t offset = static_cast<off_t>(currentSector * SECTOR_SIZE);
        if (lseek(fd, offset, SEEK_SET) < 0) {
            // Seek error - mark these sectors as bad
            for (std::uint64_t i = 0; i < sectorsToRead; ++i) {
                result.badSectorList.push_back(currentSector + i);
            }
            result.badSectors += sectorsToRead;

            if (stopOnFirstError) {
                break;
            }

            currentSector += sectorsToRead;
            result.sectorsChecked += sectorsToRead;
            continue;
        }

        ssize_t bytesRead = read(fd, buffer.data(), bytesToRead);

        if (bytesRead < 0) {
            // Read error - try individual sectors to find bad ones
            for (std::uint64_t i = 0; i < sectorsToRead; ++i) {
                off_t sectorOffset = static_cast<off_t>((currentSector + i) * SECTOR_SIZE);
                if (lseek(fd, sectorOffset, SEEK_SET) >= 0) {
                    if (read(fd, buffer.data(), SECTOR_SIZE) < 0) {
                        result.badSectorList.push_back(currentSector + i);
                        ++result.badSectors;
                    }
                } else {
                    result.badSectorList.push_back(currentSector + i);
                    ++result.badSectors;
                }
                ++result.sectorsChecked;

                if (stopOnFirstError && result.badSectors > 0) {
                    break;
                }
            }

            if (stopOnFirstError && result.badSectors > 0) {
                break;
            }
        } else {
            // Successful read
            std::uint64_t sectorsRead = static_cast<std::uint64_t>(bytesRead) / SECTOR_SIZE;
            result.sectorsChecked += sectorsRead;

            // Check for partial read (might indicate problem at boundary)
            if (static_cast<std::uint64_t>(bytesRead) < bytesToRead && currentSector + sectorsRead < totalSectors) {
                // Mark unread sectors as bad
                for (std::uint64_t i = sectorsRead; i < sectorsToRead; ++i) {
                    result.badSectorList.push_back(currentSector + i);
                    ++result.badSectors;
                    ++result.sectorsChecked;
                }

                if (stopOnFirstError && result.badSectors > 0) {
                    break;
                }
            }
        }

        currentSector += sectorsToRead;

        // Update progress
        progress.bytesWritten = currentSector * SECTOR_SIZE;
        progress.percent = (100.0 * currentSector) / totalSectors;
        progress.currentOperation = "Checking sector " + std::to_string(currentSector) +
                                   " / " + std::to_string(totalSectors);

        auto now = std::chrono::steady_clock::now();
        progress.elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_startTime);

        if (progress.percent > 0 && progress.percent < 100) {
            double totalTime = progress.elapsed.count() / (progress.percent / 100.0);
            progress.remaining = std::chrono::seconds(
                static_cast<int>(totalTime - progress.elapsed.count()));
        }

        reportProgress(progress);
    }

    close(fd);

    if (m_cancelled) {
        result.message = "Integrity check cancelled";
        reportStatus(BurnStatus::Cancelled, result.message);
        return result;
    }

    // Set final result
    result.success = (result.badSectors == 0);
    if (result.success) {
        result.message = "All " + std::to_string(result.sectorsChecked) +
                        " sectors readable - no errors found";
        reportStatus(BurnStatus::Completed, result.message);
    } else {
        result.message = std::to_string(result.badSectors) + " bad sector(s) found out of " +
                        std::to_string(result.sectorsChecked) + " checked";
        reportStatus(BurnStatus::Completed, result.message);
    }

    return result;
}

void BurnEngine::cancel() {
    m_cancelled = true;
}

bool BurnEngine::isCancelled() const {
    return m_cancelled;
}

void BurnEngine::reportProgress(const BurnProgress& progress) {
    if (m_progressCallback) {
        m_progressCallback(progress);
    }
}

void BurnEngine::reportStatus(BurnStatus status, const std::string& message) {
    m_status = status;
    if (m_statusCallback) {
        m_statusCallback(status, message);
    }
}

void BurnEngine::checkMessages() {
    char message[1024];
    int errorCode = 0;
    int osErrno = 0;
    char severity[80];
    int ret;

    // burn_msgs_obtain signature: (char *minimum_severity, int *error_code, char msg_text[], int *os_errno, char severity[])
    while ((ret = burn_msgs_obtain(const_cast<char*>("ALL"), &errorCode, message, &osErrno, severity)) == 1) {
        fprintf(stderr, "libburn message: [%s] code=%d errno=%d: %s\n",
                severity, errorCode, osErrno, message);

        // Only treat FATAL and FAILURE as actual errors
        // SORRY, WARNING, HINT, NOTE, DEBUG are informational
        if (std::strcmp(severity, "FATAL") == 0 || std::strcmp(severity, "FAILURE") == 0) {
            // Some "errors" during simulation are expected - the drive reports
            // that nothing was actually written, which isn't a real error
            // Skip messages that are clearly just simulation-related info
            std::string msg(message);
            if (msg.find("simulation") != std::string::npos ||
                msg.find("Simulation") != std::string::npos ||
                msg.find("dummy") != std::string::npos) {
                continue;
            }

            m_lastError.message = message;
            // Ensure we have a non-zero error code (libburn might return 0)
            m_lastError.code = (errorCode != 0) ? errorCode : -100;
        }
    }
}

BurnError BurnEngine::createError(int code, const std::string& message) {
    BurnError error;
    error.code = code;
    error.message = message;
    m_lastError = error;
    reportStatus(BurnStatus::Failed, message);
    return error;
}

BurnError BurnEngine::verifyBurnedData(std::uint64_t expectedSize) {
    if (!isValid()) {
        return createError(-1, "Drive not available for verification");
    }

    burn_drive* drive = m_drive->raw();

    // Get disc status
    burn_disc_status discStatus = burn_disc_get_status(drive);
    if (discStatus == BURN_DISC_EMPTY) {
        return createError(-20, "No disc found for verification");
    }

    // Read back the data to verify it's readable
    // We use direct read via the device path
    constexpr int SECTOR_SIZE = 2048;
    constexpr int SECTORS_PER_READ = 64;  // 128KB at a time
    constexpr int BUFFER_SIZE = SECTOR_SIZE * SECTORS_PER_READ;

    int fd = open(m_devicePath.c_str(), O_RDONLY);
    if (fd < 0) {
        return createError(-21, "Failed to open device for verification: " + std::string(strerror(errno)));
    }

    std::vector<char> buffer(BUFFER_SIZE);
    std::uint64_t bytesRead = 0;
    std::uint64_t totalToRead = expectedSize > 0 ? expectedSize : UINT64_MAX;

    BurnProgress progress;
    progress.totalBytes = expectedSize;
    progress.currentOperation = "Verifying...";

    while (bytesRead < totalToRead && !m_cancelled) {
        ssize_t result = read(fd, buffer.data(), BUFFER_SIZE);

        if (result < 0) {
            close(fd);
            return createError(-22, "Read error during verification: " + std::string(strerror(errno)));
        }

        if (result == 0) {
            // End of data
            break;
        }

        bytesRead += static_cast<std::uint64_t>(result);

        // Update progress
        progress.bytesWritten = bytesRead;
        if (expectedSize > 0) {
            progress.percent = (bytesRead * 100.0) / expectedSize;
        }
        reportProgress(progress);
    }

    close(fd);

    if (m_cancelled) {
        return createError(-2, "Verification cancelled");
    }

    // Check if we read the expected amount
    if (expectedSize > 0 && bytesRead < expectedSize) {
        return createError(-23, "Verification failed: read " + std::to_string(bytesRead) +
                          " bytes, expected " + std::to_string(expectedSize));
    }

    return BurnError{};  // Success
}

void BurnEngine::configureWriteOpts(burn_write_opts* opts, const BurnOptions& options) {
    // Set simulation mode
    burn_write_opts_set_simulate(opts, options.simulate ? 1 : 0);

    // Set underrun protection
    burn_write_opts_set_underrun_proof(opts, options.burnProof ? 1 : 0);

    // Set write speed (0 = max)
    burn_drive_set_speed(m_drive->raw(), 0, options.speed);

    // Multi-session handling
    burn_write_opts_set_multi(opts, options.closeDisc ? 0 : 1);

    // OPC (Optimum Power Calibration)
    burn_write_opts_set_perform_opc(opts, 1);
}

} // namespace Burner::Core
