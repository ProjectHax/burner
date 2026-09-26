#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <memory>

namespace Burner::Core {

/// Audio output format
enum class AudioFormat {
    FLAC,
    MP3,
    OGG,
    AAC,
    WAV
};

/// Audio quality settings
struct AudioQuality {
    int bitrate{0};           ///< For lossy formats (kbps): 128, 192, 256, 320
    int vbrQuality{-1};       ///< For VBR: 0-9 for MP3, 0-10 for OGG (-1 = use bitrate)
    int compression{5};       ///< For FLAC: 0-8 (default 5)
};

/// Metadata to embed in audio file
struct AudioMetadata {
    std::string title;
    std::string artist;
    std::string album;
    std::string albumArtist;
    int trackNumber{0};
    int totalTracks{0};
    int discNumber{1};
    int totalDiscs{1};
    int year{0};
    std::string genre;
    std::string comment;

    /// Cover art data (JPEG or PNG)
    std::vector<std::uint8_t> coverArt;
    std::string coverArtMimeType;  ///< "image/jpeg" or "image/png"
};

/// Encoder for converting raw PCM audio to various formats.
/// Uses FFmpeg for encoding.
class AudioOutputEncoder {
public:
    /// Progress callback: (bytesEncoded, totalBytes)
    using ProgressCallback = std::function<void(std::int64_t, std::int64_t)>;

    AudioOutputEncoder();
    ~AudioOutputEncoder();

    // Non-copyable
    AudioOutputEncoder(const AudioOutputEncoder&) = delete;
    AudioOutputEncoder& operator=(const AudioOutputEncoder&) = delete;

    // Movable
    AudioOutputEncoder(AudioOutputEncoder&&) noexcept;
    AudioOutputEncoder& operator=(AudioOutputEncoder&&) noexcept;

    /// Check if FFmpeg encoding is available
    [[nodiscard]] static bool isAvailable();

    /// Check if a specific format is available
    [[nodiscard]] static bool isFormatAvailable(AudioFormat format);

    /// Get the file extension for a format
    [[nodiscard]] static std::string formatExtension(AudioFormat format);

    /// Get human-readable format name
    [[nodiscard]] static std::string formatName(AudioFormat format);

    /// Encode raw PCM to output format
    /// @param inputData Raw PCM data (44.1kHz, 16-bit stereo, little-endian)
    /// @param outputPath Output file path
    /// @param format Output format
    /// @param quality Quality settings
    /// @param metadata Optional metadata to embed
    /// @param progress Optional progress callback
    /// @return true if successful
    bool encode(const std::vector<std::uint8_t>& inputData,
                const std::filesystem::path& outputPath,
                AudioFormat format,
                const AudioQuality& quality,
                const AudioMetadata* metadata = nullptr,
                ProgressCallback progress = nullptr);

    /// Encode raw PCM file to output format
    /// @param inputPath Raw PCM file (44.1kHz, 16-bit stereo, little-endian)
    /// @param outputPath Output file path
    /// @param format Output format
    /// @param quality Quality settings
    /// @param metadata Optional metadata to embed
    /// @param progress Optional progress callback
    /// @return true if successful
    bool encodeFile(const std::filesystem::path& inputPath,
                    const std::filesystem::path& outputPath,
                    AudioFormat format,
                    const AudioQuality& quality,
                    const AudioMetadata* metadata = nullptr,
                    ProgressCallback progress = nullptr);

    /// Request cancellation
    void cancel();

    /// Check if cancelled
    [[nodiscard]] bool isCancelled() const { return m_cancelled; }

    /// Get last error message
    [[nodiscard]] const std::string& lastError() const { return m_lastError; }

private:
    std::string m_lastError;
    bool m_cancelled{false};

    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Burner::Core
