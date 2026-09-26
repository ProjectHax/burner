#include "IsoReader.hpp"

#include <libisofs/libisofs.h>
#include <fstream>
#include <atomic>

namespace Burner::Core {

class IsoReader::Impl {
public:
    ~Impl() {
        close();
    }

    bool open(const std::filesystem::path& isoPath) {
        close();

        // Use libisofs convenience function to create data source from file path
        IsoDataSource* dataSource = nullptr;
        int ret = iso_data_source_new_from_file(isoPath.c_str(), &dataSource);
        if (ret < 0 || !dataSource) {
            m_lastError = "Could not open file: " + isoPath.string();
            return false;
        }

        // Create new image
        IsoImage* image = nullptr;
        ret = iso_image_new("", &image);
        if (ret < 0 || !image) {
            iso_data_source_unref(dataSource);
            m_lastError = "Failed to create image";
            return false;
        }

        // Read options
        IsoReadOpts* readOpts = nullptr;
        ret = iso_read_opts_new(&readOpts, 0);
        if (ret < 0 || !readOpts) {
            iso_image_unref(image);
            iso_data_source_unref(dataSource);
            m_lastError = "Failed to create read options";
            return false;
        }

        // Enable all extensions
        iso_read_opts_set_no_rockridge(readOpts, 0);
        iso_read_opts_set_no_joliet(readOpts, 0);

        // Import the image
        IsoReadImageFeatures* features = nullptr;
        ret = iso_image_import(image, dataSource, readOpts, &features);
        if (features) {
            iso_read_image_features_destroy(features);
        }
        iso_read_opts_free(readOpts);
        iso_data_source_unref(dataSource);

        if (ret < 0) {
            iso_image_unref(image);
            m_lastError = "Failed to import ISO image (error " + std::to_string(ret) + ")";
            return false;
        }

        m_image = image;
        m_isoPath = isoPath;

        // Build entry tree
        IsoDir* root = iso_image_get_root(m_image);
        if (root) {
            m_rootEntry = IsoEntry{"", "/", 0, 0, true, {}};
            buildEntryTree(reinterpret_cast<IsoNode*>(root), m_rootEntry);
        }

        return true;
    }

    void close() {
        if (m_image) {
            iso_image_unref(m_image);
            m_image = nullptr;
        }
        m_rootEntry = IsoEntry{};
        m_totalSize = 0;
        m_fileCount = 0;
    }

    bool isOpen() const { return m_image != nullptr; }

    const IsoEntry& rootEntry() const { return m_rootEntry; }

    std::string volumeId() const {
        if (!m_image) return {};
        const char* id = iso_image_get_volume_id(m_image);
        return id ? id : "";
    }

    std::uint64_t totalSize() const { return m_totalSize; }
    int fileCount() const { return m_fileCount; }

    bool extractFile(const std::string& isoPath,
                     const std::filesystem::path& localPath) {
        if (!m_image) {
            m_lastError = "No image open";
            return false;
        }

        IsoNode* node = nullptr;
        int ret = iso_tree_path_to_node(m_image, isoPath.c_str(), &node);
        if (ret < 0 || !node) {
            m_lastError = "Path not found: " + isoPath;
            return false;
        }

        if (ISO_NODE_IS_DIR(node)) {
            // Recursive directory extraction
            return extractDirectory(isoPath, localPath);
        }

        if (!ISO_NODE_IS_FILE(node)) {
            m_lastError = "Not a file: " + isoPath;
            return false;
        }

        auto* file = reinterpret_cast<IsoFile*>(node);
        IsoStream* stream = iso_file_get_stream(file);
        if (!stream) {
            m_lastError = "Could not get stream for: " + isoPath;
            return false;
        }

        // Create parent directories
        std::filesystem::create_directories(localPath.parent_path());

        ret = iso_stream_open(stream);
        if (ret < 0) {
            m_lastError = "Could not open stream for: " + isoPath;
            return false;
        }

        std::ofstream output(localPath, std::ios::binary);
        if (!output.is_open()) {
            iso_stream_close(stream);
            m_lastError = "Could not create file: " + localPath.string();
            return false;
        }

        char buffer[65536];
        int bytesRead;
        while ((bytesRead = iso_stream_read(stream, buffer, sizeof(buffer))) > 0) {
            output.write(buffer, bytesRead);
            if (m_cancelled.load()) {
                iso_stream_close(stream);
                m_lastError = "Cancelled";
                return false;
            }
        }

        iso_stream_close(stream);
        return bytesRead >= 0; // 0 = EOF, <0 = error
    }

    bool extractDirectory(const std::string& isoPath,
                          const std::filesystem::path& localPath) {
        std::filesystem::create_directories(localPath);

        // Find the entry in our tree
        const IsoEntry* entry = findEntry(m_rootEntry, isoPath);
        if (!entry || !entry->isDirectory) {
            m_lastError = "Directory not found: " + isoPath;
            return false;
        }

        bool allOk = true;
        for (const auto& child : entry->children) {
            auto childLocal = localPath / child.name;
            if (child.isDirectory) {
                if (!extractDirectory(child.fullPath, childLocal)) {
                    allOk = false;
                }
            } else {
                if (!extractFile(child.fullPath, childLocal)) {
                    allOk = false;
                }
            }
            if (m_cancelled.load()) {
                return false;
            }
        }
        return allOk;
    }

    ExtractResult extractEntries(const std::vector<std::string>& isoPaths,
                                  const std::filesystem::path& localBasePath,
                                  ProgressCallback progress) {
        ExtractResult result;
        m_cancelled.store(false);

        int total = static_cast<int>(isoPaths.size());
        int current = 0;

        for (const auto& isoPath : isoPaths) {
            if (m_cancelled.load()) {
                result.errorMessage = "Cancelled";
                break;
            }

            if (progress) {
                progress(current, total, isoPath);
            }

            // Determine local path: use the last component of the ISO path
            std::filesystem::path name = std::filesystem::path(isoPath).filename();
            auto localPath = localBasePath / name;

            if (extractFile(isoPath, localPath)) {
                ++result.filesExtracted;
            } else {
                ++result.errors;
                result.errorMessage = m_lastError;
            }

            ++current;
        }

        if (progress) {
            progress(total, total, "");
        }

        result.success = (result.errors == 0 && !m_cancelled.load());
        return result;
    }

    const std::string& lastError() const { return m_lastError; }

    void cancel() { m_cancelled.store(true); }

private:
    void buildEntryTree(IsoNode* node, IsoEntry& parent) {
        if (!ISO_NODE_IS_DIR(node)) {
            return;
        }

        auto* dir = reinterpret_cast<IsoDir*>(node);
        IsoDirIter* iter = nullptr;
        int ret = iso_dir_get_children(dir, &iter);
        if (ret < 0 || !iter) {
            return;
        }

        IsoNode* child = nullptr;
        while (iso_dir_iter_next(iter, &child) == 1 && child) {
            IsoEntry entry;
            entry.name = iso_node_get_name(child);
            entry.fullPath = parent.fullPath;
            if (entry.fullPath.back() != '/') {
                entry.fullPath += '/';
            }
            entry.fullPath += entry.name;
            entry.modTime = iso_node_get_mtime(child);

            if (ISO_NODE_IS_DIR(child)) {
                entry.isDirectory = true;
                entry.size = 0;
                buildEntryTree(child, entry);
            } else if (ISO_NODE_IS_FILE(child)) {
                entry.isDirectory = false;
                entry.size = iso_file_get_size(reinterpret_cast<IsoFile*>(child));
                m_totalSize += entry.size;
                ++m_fileCount;
            }

            parent.children.push_back(std::move(entry));
        }

        iso_dir_iter_free(iter);
    }

    const IsoEntry* findEntry(const IsoEntry& root, const std::string& path) const {
        if (root.fullPath == path) {
            return &root;
        }
        for (const auto& child : root.children) {
            if (child.fullPath == path) {
                return &child;
            }
            if (child.isDirectory) {
                const IsoEntry* found = findEntry(child, path);
                if (found) return found;
            }
        }
        return nullptr;
    }

    IsoImage* m_image{nullptr};
    std::filesystem::path m_isoPath;
    IsoEntry m_rootEntry;
    std::uint64_t m_totalSize{0};
    int m_fileCount{0};
    std::string m_lastError;
    std::atomic<bool> m_cancelled{false};
};

// Public interface delegating to Impl

IsoReader::IsoReader() : m_impl(std::make_unique<Impl>()) {}
IsoReader::~IsoReader() = default;

bool IsoReader::open(const std::filesystem::path& isoPath) {
    return m_impl->open(isoPath);
}

void IsoReader::close() {
    m_impl->close();
}

bool IsoReader::isOpen() const {
    return m_impl->isOpen();
}

const IsoEntry& IsoReader::rootEntry() const {
    return m_impl->rootEntry();
}

std::string IsoReader::volumeId() const {
    return m_impl->volumeId();
}

std::uint64_t IsoReader::totalSize() const {
    return m_impl->totalSize();
}

int IsoReader::fileCount() const {
    return m_impl->fileCount();
}

bool IsoReader::extractFile(const std::string& isoPath,
                             const std::filesystem::path& localPath) {
    return m_impl->extractFile(isoPath, localPath);
}

ExtractResult IsoReader::extractEntries(const std::vector<std::string>& isoPaths,
                                          const std::filesystem::path& localBasePath,
                                          ProgressCallback progress) {
    return m_impl->extractEntries(isoPaths, localBasePath, progress);
}

const std::string& IsoReader::lastError() const {
    return m_impl->lastError();
}

void IsoReader::cancel() {
    m_impl->cancel();
}

} // namespace Burner::Core
