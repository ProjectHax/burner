#pragma once

#include <memory>
#include <string>
#include <filesystem>
#include <functional>
#include <cstdint>

// Forward declarations for libisofs types (matches libisofs typedefs)
struct Iso_Image;
struct Iso_Dir;
typedef struct Iso_Image IsoImage;
typedef struct Iso_Dir IsoDir;

// Forward declarations for libburn types (in burn:: namespace when compiled as C++)
namespace burn {
struct burn_source;
}

using burn::burn_source;

namespace Burner::Core {

/// Filesystem type options for ISO images
enum class FilesystemType {
    ISO9660,            ///< Basic ISO9660 (8.3 filenames)
    ISO9660_RockRidge,  ///< ISO9660 with Rock Ridge (POSIX extensions)
    ISO9660_Joliet,     ///< ISO9660 with Joliet (Windows long names)
    ISO9660_Both,       ///< ISO9660 with both Rock Ridge and Joliet
    UDF,                ///< UDF only (for DVD/BD)
    ISO9660_UDF         ///< Combined ISO9660 + UDF
};

/// RAII wrapper for libisofs IsoImage.
/// Manages ISO filesystem creation and manipulation.
class IsoImageHandle {
public:
    /// Progress callback: (files processed, total files)
    using ProgressCallback = std::function<void(int, int)>;

    /// Create a new empty ISO image
    /// @param volumeId Volume label for the disc
    explicit IsoImageHandle(const std::string& volumeId = "CDROM");

    /// Destructor - releases the IsoImage
    ~IsoImageHandle();

    // Non-copyable
    IsoImageHandle(const IsoImageHandle&) = delete;
    IsoImageHandle& operator=(const IsoImageHandle&) = delete;

    // Movable
    IsoImageHandle(IsoImageHandle&& other) noexcept;
    IsoImageHandle& operator=(IsoImageHandle&& other) noexcept;

    /// Check if the handle is valid
    [[nodiscard]] bool isValid() const noexcept { return m_image != nullptr; }

    /// Get raw IsoImage pointer
    [[nodiscard]] IsoImage* raw() const noexcept { return m_image; }

    // Configuration

    /// Set volume ID (label)
    void setVolumeId(const std::string& volumeId);

    /// Set publisher ID
    void setPublisher(const std::string& publisher);

    /// Set data preparer ID
    void setDataPreparer(const std::string& preparer);

    /// Set application ID
    void setApplicationId(const std::string& appId);

    /// Set filesystem type
    void setFilesystemType(FilesystemType type);

    /// Get configured filesystem type
    [[nodiscard]] FilesystemType filesystemType() const { return m_fsType; }

    // Tree manipulation

    /// Add a file to the ISO image
    /// @param localPath Path to the file on disk
    /// @param isoPath Path in the ISO (e.g., "/folder/file.txt")
    /// @return true if successful
    bool addFile(const std::filesystem::path& localPath,
                 const std::filesystem::path& isoPath);

    /// Add a directory to the ISO image (creates empty directory)
    /// @param isoPath Path in the ISO
    /// @return true if successful
    bool addDirectory(const std::filesystem::path& isoPath);

    /// Add a directory tree recursively
    /// @param localPath Path to directory on disk
    /// @param isoPath Base path in the ISO
    /// @param progress Optional progress callback
    /// @return Number of files added
    int addTree(const std::filesystem::path& localPath,
                const std::filesystem::path& isoPath = "/",
                ProgressCallback progress = nullptr);

    /// Remove a node from the ISO
    /// @param isoPath Path in the ISO to remove
    /// @return true if successful
    bool removeNode(const std::filesystem::path& isoPath);

    /// Check if a path exists in the ISO
    [[nodiscard]] bool exists(const std::filesystem::path& isoPath) const;

    // Image generation

    /// Import a previous session's filesystem for multi-session append.
    /// Uses libisofs iso_image_import to load existing filesystem from an ISO file.
    /// @param isoFilePath Path to the ISO file or disc device
    /// @return true if import succeeded (or disc is blank)
    bool importPreviousSession(const std::string& devicePath);

    /// Create a burn source from the ISO image
    /// @param nwa Next writable address for multi-session (0 = not multi-session)
    /// @return burn_source pointer (caller must free with burn_source_free)
    [[nodiscard]] burn_source* createBurnSource(int nwa = 0);

    /// Estimate the size of the resulting ISO in bytes
    [[nodiscard]] std::uint64_t estimatedSize() const;

    /// Get the number of files in the image
    [[nodiscard]] int fileCount() const { return m_fileCount; }

private:
    /// Get or create a directory in the ISO tree
    IsoDir* getOrCreateDir(const std::filesystem::path& isoPath);

    /// Count files recursively
    int countFiles(const std::filesystem::path& path) const;

    IsoImage* m_image{nullptr};
    FilesystemType m_fsType{FilesystemType::ISO9660_Both};
    int m_fileCount{0};
};

} // namespace Burner::Core
