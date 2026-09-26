#pragma once

#include <filesystem>
#include <vector>
#include <string>
#include <functional>
#include <cstdint>
#include <ctime>
#include <memory>

namespace Burner::Core {

/// Information about a file/directory inside an ISO image
struct IsoEntry {
    std::string name;
    std::string fullPath;         ///< Full path within the ISO
    std::uint64_t size{0};
    std::time_t modTime{0};
    bool isDirectory{false};
    std::vector<IsoEntry> children;
};

/// Result of an extraction operation
struct ExtractResult {
    bool success{false};
    int filesExtracted{0};
    int errors{0};
    std::string errorMessage;
};

/// Reads and browses ISO image files using libisofs.
class IsoReader {
public:
    /// Progress callback: (currentFile, totalFiles, currentFilePath)
    using ProgressCallback = std::function<void(int, int, const std::string&)>;

    IsoReader();
    ~IsoReader();

    IsoReader(const IsoReader&) = delete;
    IsoReader& operator=(const IsoReader&) = delete;

    /// Open an ISO image file
    bool open(const std::filesystem::path& isoPath);

    /// Close the currently opened image
    void close();

    /// Check if an image is open
    [[nodiscard]] bool isOpen() const;

    /// Get the root entry of the ISO filesystem
    [[nodiscard]] const IsoEntry& rootEntry() const;

    /// Get the volume ID (label)
    [[nodiscard]] std::string volumeId() const;

    /// Get the total size of all files
    [[nodiscard]] std::uint64_t totalSize() const;

    /// Get total number of files
    [[nodiscard]] int fileCount() const;

    /// Extract a single file from the ISO
    bool extractFile(const std::string& isoPath,
                     const std::filesystem::path& localPath);

    /// Extract selected entries
    ExtractResult extractEntries(const std::vector<std::string>& isoPaths,
                                  const std::filesystem::path& localBasePath,
                                  ProgressCallback progress = nullptr);

    /// Get the last error message
    [[nodiscard]] const std::string& lastError() const;

    /// Cancel an ongoing extraction
    void cancel();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Burner::Core
