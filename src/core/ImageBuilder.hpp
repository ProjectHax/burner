#pragma once

#include "libburnia/IsoImageHandle.hpp"
#include <filesystem>
#include <vector>
#include <string>
#include <functional>
#include <cstdint>

namespace Burner::Core {

/// Entry representing a file or directory to add to the disc
struct FileEntry {
    std::filesystem::path localPath;  ///< Path on local filesystem
    std::filesystem::path isoPath;    ///< Path in the ISO image
    std::uint64_t size{0};            ///< Size in bytes (0 for directories)
    bool isDirectory{false};

    FileEntry() = default;
    FileEntry(const std::filesystem::path& local, const std::filesystem::path& iso)
        : localPath(local)
        , isoPath(iso)
        , isDirectory(std::filesystem::is_directory(local)) {
        if (!isDirectory && std::filesystem::exists(local)) {
            size = std::filesystem::file_size(local);
        }
    }
};

/// High-level class for building ISO images from file lists.
/// Manages the file tree and generates IsoImageHandle for burning.
class ImageBuilder {
public:
    /// Progress callback for building: (files processed, total files, current file name)
    using ProgressCallback = std::function<void(int, int, const std::string&)>;

    /// Progress callback for writing: (bytes written, total bytes)
    using WriteProgressCallback = std::function<void(std::uint64_t, std::uint64_t)>;

    ImageBuilder();
    ~ImageBuilder();

    // Configuration

    /// Set the volume label
    void setVolumeLabel(const std::string& label);

    /// Get the volume label
    [[nodiscard]] const std::string& volumeLabel() const { return m_volumeLabel; }

    /// Set the filesystem type
    void setFilesystemType(FilesystemType type);

    /// Get the filesystem type
    [[nodiscard]] FilesystemType filesystemType() const { return m_fsType; }

    /// Set publisher ID
    void setPublisher(const std::string& publisher);

    /// Set data preparer ID
    void setDataPreparer(const std::string& preparer);

    // File management

    /// Add a single file
    /// @param localPath Path on local filesystem
    /// @param isoPath Path in the ISO (if empty, uses root + filename)
    /// @return true if added successfully
    bool addFile(const std::filesystem::path& localPath,
                 const std::filesystem::path& isoPath = {});

    /// Add multiple files
    /// @param localPaths Paths on local filesystem
    /// @param isoBasePath Base path in the ISO
    /// @return Number of files added
    int addFiles(const std::vector<std::filesystem::path>& localPaths,
                 const std::filesystem::path& isoBasePath = "/");

    /// Add a directory recursively
    /// @param localPath Path to directory on local filesystem
    /// @param isoPath Path in the ISO
    /// @return Number of files added
    int addDirectory(const std::filesystem::path& localPath,
                     const std::filesystem::path& isoPath = {});

    /// Remove an entry by ISO path
    bool removeEntry(const std::filesystem::path& isoPath);

    /// Clear all entries
    void clear();

    // Queries

    /// Get all entries
    [[nodiscard]] const std::vector<FileEntry>& entries() const { return m_entries; }

    /// Get total size of all files
    [[nodiscard]] std::uint64_t totalSize() const { return m_totalSize; }

    /// Get total file count
    [[nodiscard]] int fileCount() const { return static_cast<int>(m_entries.size()); }

    /// Check if a path exists in the entries
    [[nodiscard]] bool hasEntry(const std::filesystem::path& isoPath) const;

    // Building

    /// Set the device path for multi-session import.
    /// When set, build() will import the previous session before adding files.
    void setMultiSessionDevice(const std::string& devicePath, int nwa = 0);

    /// Check if configured for multi-session
    [[nodiscard]] bool isMultiSession() const { return !m_multiSessionDevice.empty(); }

    /// Get the NWA for multi-session
    [[nodiscard]] int multiSessionNwa() const { return m_multiSessionNwa; }

    /// Build an IsoImageHandle from the current entries
    /// @param progress Optional progress callback
    /// @return The built IsoImageHandle
    [[nodiscard]] IsoImageHandle build(ProgressCallback progress = nullptr) const;

    /// Save the image directly to an ISO file
    /// @param outputPath Path to output ISO file
    /// @param buildProgress Optional progress callback for building phase
    /// @param writeProgress Optional progress callback for writing phase
    /// @return true if successful
    bool saveToFile(const std::filesystem::path& outputPath,
                    ProgressCallback buildProgress = nullptr,
                    WriteProgressCallback writeProgress = nullptr) const;

private:
    void recalculateSize();

    std::string m_volumeLabel{"CDROM"};
    std::string m_publisher;
    std::string m_dataPreparer;
    FilesystemType m_fsType{FilesystemType::ISO9660_Both};
    std::vector<FileEntry> m_entries;
    std::uint64_t m_totalSize{0};
    std::string m_multiSessionDevice;
    int m_multiSessionNwa{0};
};

} // namespace Burner::Core
