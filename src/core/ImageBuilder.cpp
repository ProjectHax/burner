#include "ImageBuilder.hpp"

#include <libburn/libburn.h>
#include <algorithm>
#include <fstream>

using namespace burn;

namespace Burner::Core {

ImageBuilder::ImageBuilder() = default;
ImageBuilder::~ImageBuilder() = default;

void ImageBuilder::setVolumeLabel(const std::string& label) {
    // Sanitize label - ISO9660 allows up to 32 chars
    m_volumeLabel = label.substr(0, 32);
}

void ImageBuilder::setFilesystemType(FilesystemType type) {
    m_fsType = type;
}

void ImageBuilder::setPublisher(const std::string& publisher) {
    m_publisher = publisher;
}

void ImageBuilder::setDataPreparer(const std::string& preparer) {
    m_dataPreparer = preparer;
}

bool ImageBuilder::addFile(const std::filesystem::path& localPath,
                            const std::filesystem::path& isoPath) {
    if (!std::filesystem::exists(localPath)) {
        return false;
    }

    FileEntry entry;
    entry.localPath = localPath;
    entry.isDirectory = std::filesystem::is_directory(localPath);

    if (isoPath.empty()) {
        // Use filename at root
        entry.isoPath = "/" / localPath.filename();
    } else {
        entry.isoPath = isoPath;
    }

    if (!entry.isDirectory) {
        entry.size = std::filesystem::file_size(localPath);
    }

    // Check for duplicates
    auto it = std::find_if(m_entries.begin(), m_entries.end(),
        [&entry](const FileEntry& e) {
            return e.isoPath == entry.isoPath;
        });

    if (it != m_entries.end()) {
        // Replace existing
        m_totalSize -= it->size;
        *it = entry;
    } else {
        m_entries.push_back(entry);
    }

    m_totalSize += entry.size;
    return true;
}

int ImageBuilder::addFiles(const std::vector<std::filesystem::path>& localPaths,
                            const std::filesystem::path& isoBasePath) {
    int count = 0;
    for (const auto& path : localPaths) {
        std::filesystem::path isoPath = isoBasePath / path.filename();
        if (addFile(path, isoPath)) {
            ++count;
        }
    }
    return count;
}

int ImageBuilder::addDirectory(const std::filesystem::path& localPath,
                                const std::filesystem::path& isoPath) {
    if (!std::filesystem::exists(localPath) ||
        !std::filesystem::is_directory(localPath)) {
        return 0;
    }

    std::filesystem::path basePath = isoPath.empty()
        ? ("/" / localPath.filename())
        : isoPath;

    int count = 0;

    try {
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(localPath)) {
            // Calculate relative path from base
            auto relativePath = std::filesystem::relative(entry.path(), localPath);
            auto entryIsoPath = basePath / relativePath;

            if (entry.is_regular_file()) {
                if (addFile(entry.path(), entryIsoPath)) {
                    ++count;
                }
            }
            // Directories are created implicitly when files are added
        }
    } catch (const std::filesystem::filesystem_error&) {
        // Skip inaccessible files
    }

    return count;
}

bool ImageBuilder::removeEntry(const std::filesystem::path& isoPath) {
    auto it = std::find_if(m_entries.begin(), m_entries.end(),
        [&isoPath](const FileEntry& e) {
            return e.isoPath == isoPath;
        });

    if (it != m_entries.end()) {
        m_totalSize -= it->size;
        m_entries.erase(it);
        return true;
    }
    return false;
}

void ImageBuilder::clear() {
    m_entries.clear();
    m_totalSize = 0;
}

bool ImageBuilder::hasEntry(const std::filesystem::path& isoPath) const {
    return std::any_of(m_entries.begin(), m_entries.end(),
        [&isoPath](const FileEntry& e) {
            return e.isoPath == isoPath;
        });
}

void ImageBuilder::setMultiSessionDevice(const std::string& devicePath, int nwa) {
    m_multiSessionDevice = devicePath;
    m_multiSessionNwa = nwa;
}

IsoImageHandle ImageBuilder::build(ProgressCallback progress) const {
    IsoImageHandle image(m_volumeLabel);

    image.setFilesystemType(m_fsType);

    if (!m_publisher.empty()) {
        image.setPublisher(m_publisher);
    }
    if (!m_dataPreparer.empty()) {
        image.setDataPreparer(m_dataPreparer);
    }

    // Import previous session for multi-session append
    if (!m_multiSessionDevice.empty()) {
        image.importPreviousSession(m_multiSessionDevice);
    }

    int total = static_cast<int>(m_entries.size());
    int processed = 0;

    for (const auto& entry : m_entries) {
        if (entry.isDirectory) {
            image.addDirectory(entry.isoPath);
        } else {
            image.addFile(entry.localPath, entry.isoPath);
        }

        ++processed;
        if (progress) {
            progress(processed, total, entry.localPath.filename().string());
        }
    }

    return image;
}

bool ImageBuilder::saveToFile(const std::filesystem::path& outputPath,
                               ProgressCallback buildProgress,
                               WriteProgressCallback writeProgress) const {
    auto image = build(buildProgress);

    burn_source* src = image.createBurnSource();
    if (!src) {
        return false;
    }

    // Get the total size if available
    off_t totalSize = 0;
    if (src->get_size) {
        totalSize = src->get_size(src);
    }

    // Open output file
    std::ofstream output(outputPath, std::ios::binary);
    if (!output) {
        burn_source_free(src);
        return false;
    }

    // Check if we have a read function
    // libisofs uses read_xt (version >= 1) instead of read
    bool hasRead = src->read != nullptr;
    bool hasReadXt = (src->version >= 1) && (src->read_xt != nullptr);

    if (!hasRead && !hasReadXt) {
        burn_source_free(src);
        return false;
    }

    // Read from burn source and write to file
    // Use 2048-byte sectors as that's the ISO9660 sector size
    constexpr size_t BUFFER_SIZE = 2048 * 32; // 32 sectors at a time
    std::vector<unsigned char> buffer(BUFFER_SIZE);

    std::uint64_t totalWritten = 0;
    int bytesRead;

    while (true) {
        // Use read_xt if available (version >= 1), otherwise use read
        if (hasReadXt) {
            bytesRead = src->read_xt(src, buffer.data(), static_cast<int>(BUFFER_SIZE));
        } else {
            bytesRead = src->read(src, buffer.data(), static_cast<int>(BUFFER_SIZE));
        }

        if (bytesRead <= 0) {
            break;
        }

        output.write(reinterpret_cast<char*>(buffer.data()), bytesRead);
        totalWritten += bytesRead;

        if (!output) {
            burn_source_free(src);
            return false;
        }

        // Report write progress
        if (writeProgress) {
            writeProgress(totalWritten, static_cast<std::uint64_t>(totalSize));
        }
    }

    burn_source_free(src);
    return totalWritten > 0 && output.good();
}

void ImageBuilder::recalculateSize() {
    m_totalSize = 0;
    for (const auto& entry : m_entries) {
        m_totalSize += entry.size;
    }
}

} // namespace Burner::Core
