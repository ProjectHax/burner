#include "FileTreeModel.hpp"
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QBrush>

namespace Burner::Models {

FileTreeModel::FileTreeModel(QObject* parent)
    : QAbstractItemModel(parent)
    , m_rootItem(std::make_unique<ProjectItem>()) {
    m_rootItem->name = "/";
    m_rootItem->isDirectory = true;
}

FileTreeModel::~FileTreeModel() = default;

QModelIndex FileTreeModel::index(int row, int column, const QModelIndex& parent) const {
    if (!hasIndex(row, column, parent)) {
        return {};
    }

    ProjectItem* parentItem = itemFromIndex(parent);
    if (!parentItem || row >= static_cast<int>(parentItem->children.size())) {
        return {};
    }

    return createIndex(row, column, parentItem->children[row].get());
}

QModelIndex FileTreeModel::parent(const QModelIndex& index) const {
    if (!index.isValid()) {
        return {};
    }

    ProjectItem* item = itemFromIndex(index);
    if (!item || !item->parent || item->parent == m_rootItem.get()) {
        return {};
    }

    return indexFromItem(item->parent);
}

int FileTreeModel::rowCount(const QModelIndex& parent) const {
    ProjectItem* item = itemFromIndex(parent);
    return item ? static_cast<int>(item->children.size()) : 0;
}

int FileTreeModel::columnCount(const QModelIndex& parent) const {
    Q_UNUSED(parent);
    return 3; // Name, Size, Local Path
}

QVariant FileTreeModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) {
        return {};
    }

    ProjectItem* item = static_cast<ProjectItem*>(index.internalPointer());
    if (!item) {
        return {};
    }

    switch (role) {
        case Qt::DisplayRole:
            switch (index.column()) {
                case 0: return item->name;
                case 1: return item->isDirectory ? QString() : formatSize(item->size);
                case 2: return item->localPath;
                default: return {};
            }

        case Qt::CheckStateRole:
            if (index.column() == 0 && !item->localPath.isEmpty()) {
                return item->fileExists ? Qt::Checked : Qt::Unchecked;
            }
            return {};

        case Qt::ForegroundRole:
            if (!item->fileExists && !item->localPath.isEmpty()) {
                return QBrush(Qt::red);
            }
            return {};

        case Qt::ToolTipRole:
            if (!item->fileExists && !item->localPath.isEmpty()) {
                return tr("File not found: %1").arg(item->localPath);
            }
            return item->localPath;

        case NameRole:
            return item->name;

        case LocalPathRole:
            return item->localPath;

        case IsoPathRole:
            return item->isoPath;

        case SizeRole:
            return item->size;

        case IsDirectoryRole:
            return item->isDirectory;

        case SizeStringRole:
            return formatSize(item->size);

        case FileExistsRole:
            return item->fileExists;

        default:
            return {};
    }
}

QVariant FileTreeModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }

    switch (section) {
        case 0: return tr("Name");
        case 1: return tr("Size");
        case 2: return tr("Local Path");
        default: return {};
    }
}

Qt::ItemFlags FileTreeModel::flags(const QModelIndex& index) const {
    Qt::ItemFlags defaultFlags = QAbstractItemModel::flags(index);

    if (index.isValid()) {
        return defaultFlags | Qt::ItemIsDropEnabled;
    }
    return defaultFlags | Qt::ItemIsDropEnabled;
}

Qt::DropActions FileTreeModel::supportedDropActions() const {
    return Qt::CopyAction;
}

QStringList FileTreeModel::mimeTypes() const {
    return {"text/uri-list"};
}

bool FileTreeModel::canDropMimeData(const QMimeData* data, Qt::DropAction action,
                                     int row, int column, const QModelIndex& parent) const {
    Q_UNUSED(action);
    Q_UNUSED(row);
    Q_UNUSED(column);
    Q_UNUSED(parent);
    return data->hasUrls();
}

bool FileTreeModel::dropMimeData(const QMimeData* data, Qt::DropAction action,
                                  int row, int column, const QModelIndex& parent) {
    Q_UNUSED(action);
    Q_UNUSED(row);
    Q_UNUSED(column);

    if (!data->hasUrls()) {
        return false;
    }

    QStringList paths;
    for (const QUrl& url : data->urls()) {
        if (url.isLocalFile()) {
            paths.append(url.toLocalFile());
        }
    }

    return addFiles(paths, parent);
}

bool FileTreeModel::addFile(const QString& localPath, const QModelIndex& parent) {
    QFileInfo fileInfo(localPath);
    if (!fileInfo.exists()) {
        return false;
    }

    ProjectItem* parentItem = itemFromIndex(parent);
    if (!parentItem) {
        parentItem = m_rootItem.get();
    }

    int row = static_cast<int>(parentItem->children.size());
    beginInsertRows(parent, row, row);

    auto item = std::make_unique<ProjectItem>();
    item->name = fileInfo.fileName();
    item->localPath = localPath;
    item->isoPath = parentItem->isoPath + "/" + item->name;
    item->size = fileInfo.size();
    item->isDirectory = fileInfo.isDir();
    item->fileExists = fileInfo.exists() && fileInfo.isReadable();
    item->parent = parentItem;

    if (item->isDirectory) {
        addFileRecursive(fileInfo.filePath().toStdString(), item.get());
    }

    parentItem->children.push_back(std::move(item));

    endInsertRows();

    recalculateTotalSize();
    emit filesAdded(1);
    return true;
}

bool FileTreeModel::addFiles(const QStringList& localPaths, const QModelIndex& parent) {
    int added = 0;
    for (const QString& path : localPaths) {
        if (addFile(path, parent)) {
            ++added;
        }
    }
    return added > 0;
}

bool FileTreeModel::addDirectory(const QString& localPath, const QModelIndex& parent) {
    return addFile(localPath, parent);
}

bool FileTreeModel::addFileWithIsoPath(const QString& localPath, const QString& isoPath) {
    QFileInfo fileInfo(localPath);
    bool exists = fileInfo.exists() && fileInfo.isReadable();

    // Parse the isoPath to get the directory structure
    // e.g., "/folder/subfolder/file.txt" -> ["folder", "subfolder", "file.txt"]
    QStringList pathParts = isoPath.split('/', Qt::SkipEmptyParts);
    if (pathParts.isEmpty()) {
        return false;
    }

    // The last part is the filename, the rest are directories
    QString fileName = pathParts.takeLast();

    // Navigate/create the directory structure
    ProjectItem* currentParent = m_rootItem.get();
    QString currentIsoPath;

    for (const QString& dirName : pathParts) {
        currentIsoPath += "/" + dirName;

        // Look for existing directory
        ProjectItem* foundDir = nullptr;
        for (const auto& child : currentParent->children) {
            if (child->name == dirName && child->isDirectory) {
                foundDir = child.get();
                break;
            }
        }

        if (foundDir) {
            currentParent = foundDir;
        } else {
            // Create the directory
            QModelIndex parentIndex = indexFromItem(currentParent);
            int row = static_cast<int>(currentParent->children.size());
            beginInsertRows(parentIndex, row, row);

            auto dirItem = std::make_unique<ProjectItem>();
            dirItem->name = dirName;
            dirItem->isoPath = currentIsoPath;
            dirItem->isDirectory = true;
            dirItem->fileExists = true;
            dirItem->parent = currentParent;

            ProjectItem* newDir = dirItem.get();
            currentParent->children.push_back(std::move(dirItem));

            endInsertRows();
            currentParent = newDir;
        }
    }

    // Now add the file to currentParent
    QModelIndex parentIndex = indexFromItem(currentParent);
    int row = static_cast<int>(currentParent->children.size());
    beginInsertRows(parentIndex, row, row);

    auto item = std::make_unique<ProjectItem>();
    item->name = fileName;
    item->localPath = localPath;
    item->isoPath = isoPath;
    item->size = exists ? fileInfo.size() : 0;
    item->isDirectory = exists ? fileInfo.isDir() : false;
    item->fileExists = exists;
    item->parent = currentParent;

    if (item->isDirectory && exists) {
        addFileRecursive(fileInfo.filePath().toStdString(), item.get());
    }

    currentParent->children.push_back(std::move(item));
    endInsertRows();

    recalculateTotalSize();
    emit filesAdded(1);
    return true;
}

bool FileTreeModel::createFolder(const QString& name, const QModelIndex& parent) {
    ProjectItem* parentItem = itemFromIndex(parent);
    if (!parentItem) {
        parentItem = m_rootItem.get();
    }

    int row = static_cast<int>(parentItem->children.size());
    beginInsertRows(parent, row, row);

    auto item = std::make_unique<ProjectItem>();
    item->name = name;
    item->isoPath = parentItem->isoPath + "/" + name;
    item->isDirectory = true;
    item->parent = parentItem;

    parentItem->children.push_back(std::move(item));

    endInsertRows();
    return true;
}

bool FileTreeModel::removeItem(const QModelIndex& index) {
    if (!index.isValid()) {
        return false;
    }

    ProjectItem* item = itemFromIndex(index);
    if (!item || !item->parent) {
        return false;
    }

    ProjectItem* parent = item->parent;
    QModelIndex parentIndex = indexFromItem(parent);

    auto it = std::find_if(parent->children.begin(), parent->children.end(),
        [item](const auto& ptr) { return ptr.get() == item; });

    if (it == parent->children.end()) {
        return false;
    }

    int row = static_cast<int>(std::distance(parent->children.begin(), it));

    beginRemoveRows(parentIndex, row, row);
    parent->children.erase(it);
    endRemoveRows();

    recalculateTotalSize();
    return true;
}

void FileTreeModel::clear() {
    beginResetModel();
    m_rootItem->children.clear();
    m_totalSize = 0;
    endResetModel();
    emit totalSizeChanged(m_totalSize);
}

QStringList FileTreeModel::allLocalPaths() const {
    QStringList paths;

    std::function<void(const ProjectItem*)> collect = [&](const ProjectItem* item) {
        if (!item->localPath.isEmpty() && !item->isDirectory) {
            paths.append(item->localPath);
        }
        for (const auto& child : item->children) {
            collect(child.get());
        }
    };

    collect(m_rootItem.get());
    return paths;
}

QList<QPair<QString, QString>> FileTreeModel::allFileMappings() const {
    QList<QPair<QString, QString>> mappings;

    std::function<void(const ProjectItem*)> collect = [&](const ProjectItem* item) {
        if (!item->localPath.isEmpty() && !item->isDirectory) {
            // Return pair of (localPath, isoPath)
            mappings.append({item->localPath, item->isoPath});
        }
        for (const auto& child : item->children) {
            collect(child.get());
        }
    };

    collect(m_rootItem.get());
    return mappings;
}

void FileTreeModel::setVolumeLabel(const QString& label) {
    if (m_volumeLabel != label) {
        m_volumeLabel = label;
        emit volumeLabelChanged(label);
    }
}

void FileTreeModel::refreshFileExistence() {
    std::function<void(ProjectItem*)> refresh = [&](ProjectItem* item) {
        if (!item->localPath.isEmpty() && !item->isDirectory) {
            item->fileExists = QFileInfo::exists(item->localPath);
        }
        for (auto& child : item->children) {
            refresh(child.get());
        }
    };

    refresh(m_rootItem.get());
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
}

bool FileTreeModel::allFilesExist() const {
    std::function<bool(const ProjectItem*)> check = [&](const ProjectItem* item) -> bool {
        if (!item->localPath.isEmpty() && !item->isDirectory && !item->fileExists) {
            return false;
        }
        for (const auto& child : item->children) {
            if (!check(child.get())) {
                return false;
            }
        }
        return true;
    };

    return check(m_rootItem.get());
}

ProjectItem* FileTreeModel::itemFromIndex(const QModelIndex& index) const {
    if (!index.isValid()) {
        return m_rootItem.get();
    }
    return static_cast<ProjectItem*>(index.internalPointer());
}

QModelIndex FileTreeModel::indexFromItem(ProjectItem* item) const {
    if (!item || item == m_rootItem.get()) {
        return {};
    }

    ProjectItem* parent = item->parent;
    if (!parent) {
        return {};
    }

    auto it = std::find_if(parent->children.begin(), parent->children.end(),
        [item](const auto& ptr) { return ptr.get() == item; });

    if (it == parent->children.end()) {
        return {};
    }

    int row = static_cast<int>(std::distance(parent->children.begin(), it));
    return createIndex(row, 0, item);
}

void FileTreeModel::addFileRecursive(const std::filesystem::path& path, ProjectItem* parent) {
    try {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            auto item = std::make_unique<ProjectItem>();
            item->name = QString::fromStdString(entry.path().filename().string());
            item->localPath = QString::fromStdString(entry.path().string());
            item->isoPath = parent->isoPath + "/" + item->name;
            item->isDirectory = entry.is_directory();
            item->fileExists = entry.exists();
            item->parent = parent;

            if (entry.is_regular_file()) {
                item->size = static_cast<qint64>(entry.file_size());
            }

            if (entry.is_directory()) {
                addFileRecursive(entry.path(), item.get());
            }

            parent->children.push_back(std::move(item));
        }
    } catch (const std::filesystem::filesystem_error&) {
        // Ignore permission errors, etc.
    }
}

void FileTreeModel::recalculateTotalSize() {
    qint64 total = 0;

    std::function<void(const ProjectItem*)> calculate = [&](const ProjectItem* item) {
        if (!item->isDirectory) {
            total += item->size;
        }
        for (const auto& child : item->children) {
            calculate(child.get());
        }
    };

    calculate(m_rootItem.get());

    if (m_totalSize != total) {
        m_totalSize = total;
        emit totalSizeChanged(m_totalSize);
    }
}

QString FileTreeModel::formatSize(qint64 bytes) const {
    constexpr qint64 KB = 1024;
    constexpr qint64 MB = KB * 1024;
    constexpr qint64 GB = MB * 1024;

    if (bytes >= GB) {
        return QString::number(static_cast<double>(bytes) / GB, 'f', 2) + " GB";
    } else if (bytes >= MB) {
        return QString::number(static_cast<double>(bytes) / MB, 'f', 2) + " MB";
    } else if (bytes >= KB) {
        return QString::number(static_cast<double>(bytes) / KB, 'f', 2) + " KB";
    } else {
        return QString::number(bytes) + " B";
    }
}

} // namespace Burner::Models
