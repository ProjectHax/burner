#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cstdint>
#include <filesystem>

namespace Burner::Core {

/// Audio format information
struct AudioInfo {
    std::string filePath;
    std::string title;
    std::string artist;
    std::string album;
    int trackNumber{0};
    int durationMs{0};          ///< Duration in milliseconds
    int sampleRate{0};          ///< Sample rate in Hz
    int channels{0};            ///< Number of channels
    int bitsPerSample{0};       ///< Bits per sample
    int bitRate{0};             ///< Bit rate in bps
    std::string codecName;      ///< Codec name (MP3, FLAC, etc.)
    std::int64_t fileSize{0};   ///< File size in bytes
};

/// CD-DA audio specifications
struct CdDaSpec {
    static constexpr int SAMPLE_RATE = 44100;
    static constexpr int CHANNELS = 2;
    static constexpr int BITS_PER_SAMPLE = 16;
    static constexpr int BYTES_PER_SAMPLE = 2;
    static constexpr int BYTES_PER_FRAME = CHANNELS * BYTES_PER_SAMPLE;
    static constexpr int BYTES_PER_SECOND = SAMPLE_RATE * BYTES_PER_FRAME;
    static constexpr int SAMPLES_PER_CD_FRAME = 588;  // CD sector = 2352 bytes
    static constexpr int CD_FRAME_SIZE = 2352;
};

/// Audio encoder/decoder using FFmpeg.
/// Handles reading audio metadata and converting to CD-DA format.
class AudioEncoder {
public:
    /// Progress callback: (bytes processed, total bytes)
    using ProgressCallback = std::function<void(std::int64_t, std::int64_t)>;

    AudioEncoder();
    ~AudioEncoder();

    // Non-copyable
    AudioEncoder(const AudioEncoder&) = delete;
    AudioEncoder& operator=(const AudioEncoder&) = delete;

    // Movable
    AudioEncoder(AudioEncoder&&) noexcept;
    AudioEncoder& operator=(AudioEncoder&&) noexcept;

    /// Check if FFmpeg is available
    [[nodiscard]] static bool isAvailable();

    /// Get supported audio formats
    [[nodiscard]] static std::vector<std::string> supportedFormats();

    /// Check if a file format is supported
    [[nodiscard]] static bool isFormatSupported(const std::string& extension);

    /// Read audio file information
    /// @param filePath Path to audio file
    /// @return AudioInfo with metadata, or empty if failed
    [[nodiscard]] static AudioInfo getAudioInfo(const std::filesystem::path& filePath);

    /// Decode audio file to raw CD-DA PCM data
    /// @param inputPath Path to source audio file
    /// @param outputPath Path to output raw PCM file
    /// @param progress Optional progress callback
    /// @return true if successful
    bool decodeToCdda(const std::filesystem::path& inputPath,
                      const std::filesystem::path& outputPath,
                      ProgressCallback progress = nullptr);

    /// Decode audio file to memory buffer (CD-DA format)
    /// @param inputPath Path to source audio file
    /// @param progress Optional progress callback
    /// @return Raw PCM data (44.1kHz, 16-bit stereo, little-endian)
    [[nodiscard]] std::vector<std::uint8_t> decodeToCddaBuffer(
        const std::filesystem::path& inputPath,
        ProgressCallback progress = nullptr);

    /// Get the expected output size for CD-DA conversion
    /// @param durationMs Duration in milliseconds
    /// @return Size in bytes of raw PCM output
    [[nodiscard]] static std::int64_t estimateCddaSize(int durationMs);

    /// Get last error message
    [[nodiscard]] const std::string& lastError() const { return m_lastError; }

private:
    std::string m_lastError;

    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Burner::Core
