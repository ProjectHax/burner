#include "IsoImageHandle.hpp"

#include <cstdint>
#include <libisofs/libisofs.h>
#include <libburn/libburn.h>
#include <stdexcept>
#include <queue>

using namespace burn;

namespace Burner::Core {

IsoImageHandle::IsoImageHandle(const std::string& volumeId) {
    int ret = iso_image_new(volumeId.c_str(), &m_image);
    if (ret < 0 || m_image == nullptr) {
        throw std::runtime_error("Failed to create ISO image: error " + std::to_string(ret));
    }

    // Set default application ID
    iso_image_set_application_id(m_image, "Burner CD/DVD Application");
}

IsoImageHandle::~IsoImageHandle() {
    if (m_image) {
        iso_image_unref(m_image);
        m_image = nullptr;
    }
}

IsoImageHandle::IsoImageHandle(IsoImageHandle&& other) noexcept
    : m_image(other.m_image)
    , m_fsType(other.m_fsType)
    , m_fileCount(other.m_fileCount) {
    other.m_image = nullptr;
    other.m_fileCount = 0;
}

IsoImageHandle& IsoImageHandle::operator=(IsoImageHandle&& other) noexcept {
    if (this != &other) {
        if (m_image) {
            iso_image_unref(m_image);
        }
        m_image = other.m_image;
        m_fsType = other.m_fsType;
        m_fileCount = other.m_fileCount;
        other.m_image = nullptr;
        other.m_fileCount = 0;
    }
    return *this;
}

void IsoImageHandle::setVolumeId(const std::string& volumeId) {
    if (m_image) {
        iso_image_set_volume_id(m_image, volumeId.c_str());
    }
}

void IsoImageHandle::setPublisher(const std::string& publisher) {
    if (m_image) {
        iso_image_set_publisher_id(m_image, publisher.c_str());
    }
}

void IsoImageHandle::setDataPreparer(const std::string& preparer) {
    if (m_image) {
        iso_image_set_data_preparer_id(m_image, preparer.c_str());
    }
}

void IsoImageHandle::setApplicationId(const std::string& appId) {
    if (m_image) {
        iso_image_set_application_id(m_image, appId.c_str());
    }
}

void IsoImageHandle::setFilesystemType(FilesystemType type) {
    m_fsType = type;
}

bool IsoImageHandle::addFile(const std::filesystem::path& localPath,
                              const std::filesystem::path& isoPath) {
    if (!m_image) {
        return false;
    }

    // Get parent directory
    std::filesystem::path parentPath = isoPath.parent_path();
    if (parentPath.empty()) {
        parentPath = "/";
    }

    IsoDir* parentDir = getOrCreateDir(parentPath);
    if (!parentDir) {
        return false;
    }

    // Add the file
    IsoNode* node = nullptr;
    int ret = iso_tree_add_node(m_image, parentDir,
                                 localPath.c_str(), &node);
    if (ret < 0) {
        return false;
    }

    // Rename if the filename differs
    std::string filename = isoPath.filename().string();
    if (!filename.empty() && filename != localPath.filename().string()) {
        iso_node_set_name(node, filename.c_str());
    }

    ++m_fileCount;
    return true;
}

bool IsoImageHandle::addDirectory(const std::filesystem::path& isoPath) {
    if (!m_image) {
        return false;
    }

    return getOrCreateDir(isoPath) != nullptr;
}

int IsoImageHandle::addTree(const std::filesystem::path& localPath,
                             const std::filesystem::path& isoPath,
                             ProgressCallback progress) {
    if (!m_image || !std::filesystem::exists(localPath)) {
        return 0;
    }

    int totalFiles = 0;
    int processedFiles = 0;

    // Count files first if progress callback provided
    if (progress) {
        totalFiles = countFiles(localPath);
    }

    // Use a queue for breadth-first traversal
    struct DirEntry {
        std::filesystem::path local;
        std::filesystem::path iso;
    };

    std::queue<DirEntry> dirs;
    dirs.push({localPath, isoPath});

    while (!dirs.empty()) {
        DirEntry current = dirs.front();
        dirs.pop();

        // Ensure ISO directory exists
        getOrCreateDir(current.iso);

        try {
            for (const auto& entry : std::filesystem::directory_iterator(current.local)) {
                std::filesystem::path entryIsoPath = current.iso / entry.path().filename();

                if (entry.is_directory()) {
                    dirs.push({entry.path(), entryIsoPath});
                } else if (entry.is_regular_file()) {
                    if (addFile(entry.path(), entryIsoPath)) {
                        ++processedFiles;
                        if (progress) {
                            progress(processedFiles, totalFiles);
                        }
                    }
                }
                // Skip symlinks and special files for now
            }
        } catch (const std::filesystem::filesystem_error&) {
            // Skip directories we can't access
        }
    }

    return processedFiles;
}

bool IsoImageHandle::removeNode(const std::filesystem::path& isoPath) {
    if (!m_image) {
        return false;
    }

    IsoDir* root = iso_image_get_root(m_image);
    if (!root) {
        return false;
    }

    // Find the node
    IsoNode* node = nullptr;
    int ret = iso_tree_path_to_node(m_image, isoPath.c_str(), &node);
    if (ret < 0 || !node) {
        return false;
    }

    // Remove it
    iso_node_remove(node);
    --m_fileCount;
    return true;
}

bool IsoImageHandle::exists(const std::filesystem::path& isoPath) const {
    if (!m_image) {
        return false;
    }

    IsoNode* node = nullptr;
    int ret = iso_tree_path_to_node(m_image, isoPath.c_str(), &node);
    return ret >= 0 && node != nullptr;
}

bool IsoImageHandle::importPreviousSession(const std::string& devicePath) {
    if (!m_image) {
        return false;
    }

    // Create a data source from the device/file
    IsoDataSource* dataSource = nullptr;
    int ret = iso_data_source_new_from_file(devicePath.c_str(), &dataSource);
    if (ret < 0 || !dataSource) {
        return false;
    }

    // Create read options
    IsoReadOpts* readOpts = nullptr;
    ret = iso_read_opts_new(&readOpts, 0);
    if (ret < 0 || !readOpts) {
        iso_data_source_unref(dataSource);
        return false;
    }

    // Enable all extensions
    iso_read_opts_set_no_rockridge(readOpts, 0);
    iso_read_opts_set_no_joliet(readOpts, 0);

    // Import the previous session's filesystem into our image
    IsoReadImageFeatures* features = nullptr;
    ret = iso_image_import(m_image, dataSource, readOpts, &features);
    if (features) {
        iso_read_image_features_destroy(features);
    }
    iso_read_opts_free(readOpts);
    iso_data_source_unref(dataSource);

    return ret >= 0;
}

burn_source* IsoImageHandle::createBurnSource(int nwa) {
    if (!m_image) {
        return nullptr;
    }

    // Create write options
    IsoWriteOpts* opts = nullptr;
    int ret = iso_write_opts_new(&opts, 0);
    if (ret < 0 || !opts) {
        return nullptr;
    }

    // Configure based on filesystem type
    switch (m_fsType) {
        case FilesystemType::ISO9660:
            iso_write_opts_set_iso_level(opts, 2);
            break;

        case FilesystemType::ISO9660_RockRidge:
            iso_write_opts_set_iso_level(opts, 3);
            iso_write_opts_set_rockridge(opts, 1);
            break;

        case FilesystemType::ISO9660_Joliet:
            iso_write_opts_set_iso_level(opts, 3);
            iso_write_opts_set_joliet(opts, 1);
            break;

        case FilesystemType::ISO9660_Both:
            iso_write_opts_set_iso_level(opts, 3);
            iso_write_opts_set_rockridge(opts, 1);
            iso_write_opts_set_joliet(opts, 1);
            break;

        case FilesystemType::UDF:
            iso_write_opts_set_iso_level(opts, 3);
            iso_write_opts_set_iso1999(opts, 0);
            // UDF requires additional configuration
            break;

        case FilesystemType::ISO9660_UDF:
            iso_write_opts_set_iso_level(opts, 3);
            iso_write_opts_set_rockridge(opts, 1);
            iso_write_opts_set_joliet(opts, 1);
            break;
    }

    // Multi-session: set the next writable address
    if (nwa > 0) {
        iso_write_opts_set_ms_block(opts, nwa);
        iso_write_opts_set_appendable(opts, 1);
    }

    // Create the burn source
    burn_source* src = nullptr;
    ret = iso_image_create_burn_source(m_image, opts, &src);

    iso_write_opts_free(opts);

    if (ret < 0) {
        return nullptr;
    }

    return src;
}

std::uint64_t IsoImageHandle::estimatedSize() const {
    if (!m_image || m_fileCount == 0) {
        return 0;
    }

    // Estimate size based on file data
    // This is a rough estimate - actual size depends on filesystem overhead,
    // directory structures, Joliet/RockRidge extensions, etc.

    // Base overhead for ISO header and directory structure (approx 300KB min)
    constexpr std::uint64_t BASE_OVERHEAD = 300 * 1024;

    // Per-file overhead for directory entries and metadata (approx 2KB per file)
    constexpr std::uint64_t PER_FILE_OVERHEAD = 2 * 1024;

    // Round up to 2KB sector alignment
    constexpr std::uint64_t SECTOR_SIZE = 2048;

    std::uint64_t estimate = BASE_OVERHEAD + (m_fileCount * PER_FILE_OVERHEAD);

    // Add file data size (computed when adding files, stored in m_totalDataSize)
    // For now, return the base estimate - actual size is computed by createBurnSource
    return ((estimate + SECTOR_SIZE - 1) / SECTOR_SIZE) * SECTOR_SIZE;
}

IsoDir* IsoImageHandle::getOrCreateDir(const std::filesystem::path& isoPath) {
    if (!m_image) {
        return nullptr;
    }

    IsoDir* root = iso_image_get_root(m_image);
    if (!root) {
        return nullptr;
    }

    // Handle root path
    std::string pathStr = isoPath.string();
    if (pathStr.empty() || pathStr == "/" || pathStr == ".") {
        return root;
    }

    // Check if it already exists
    IsoNode* existingNode = nullptr;
    int ret = iso_tree_path_to_node(m_image, pathStr.c_str(), &existingNode);
    if (ret >= 0 && existingNode && ISO_NODE_IS_DIR(existingNode)) {
        return reinterpret_cast<IsoDir*>(existingNode);
    }

    // Create parent directories as needed
    IsoDir* current = root;
    for (const auto& component : isoPath) {
        std::string name = component.string();
        if (name.empty() || name == "/" || name == ".") {
            continue;
        }

        // Look for existing child
        IsoNode* child = nullptr;
        ret = iso_dir_get_node(current, name.c_str(), &child);

        if (ret >= 0 && child && ISO_NODE_IS_DIR(child)) {
            current = reinterpret_cast<IsoDir*>(child);
        } else {
            // Create new directory
            IsoDir* newDir = nullptr;
            ret = iso_tree_add_new_dir(current, name.c_str(), &newDir);
            if (ret < 0 || !newDir) {
                return nullptr;
            }
            current = newDir;
        }
    }

    return current;
}

int IsoImageHandle::countFiles(const std::filesystem::path& path) const {
    int count = 0;
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
            if (entry.is_regular_file()) {
                ++count;
            }
        }
    } catch (const std::filesystem::filesystem_error&) {
        // Ignore errors
    }
    return count;
}

} // namespace Burner::Core
