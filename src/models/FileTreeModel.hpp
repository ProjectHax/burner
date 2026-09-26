#pragma once

#include <QAbstractItemModel>
#include <QMimeData>
#include <memory>
#include <filesystem>

namespace Burner::Models {

/// Represents an item in the disc project file tree.
struct ProjectItem {
    QString name;
    QString localPath;        ///< Path on local filesystem (empty for virtual folders)
    QString isoPath;          ///< Path in the disc image
    qint64 size{0};
    bool isDirectory{false};
    bool fileExists{true};    ///< Whether the file is accessible
    std::vector<std::unique_ptr<ProjectItem>> children;
    ProjectItem* parent{nullptr};

    ProjectItem() = default;
    ProjectItem(QString name, QString localPath, QString isoPath, qint64 size, bool isDir)
        : name(std::move(name))
        , localPath(std::move(localPath))
        , isoPath(std::move(isoPath))
        , size(size)
        , isDirectory(isDir)
        , fileExists(true) {}
};

/// Qt tree model for disc project contents.
/// Supports drag-and-drop for adding files.
class FileTreeModel : public QAbstractItemModel {
    Q_OBJECT
    Q_PROPERTY(qint64 totalSize READ totalSize NOTIFY totalSizeChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        LocalPathRole,
        IsoPathRole,
        SizeRole,
        IsDirectoryRole,
        SizeStringRole,
        FileExistsRole
    };

    explicit FileTreeModel(QObject* parent = nullptr);
    ~FileTreeModel() override;

    // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    // Drag and drop
    Qt::DropActions supportedDropActions() const override;
    QStringList mimeTypes() const override;
    bool canDropMimeData(const QMimeData* data, Qt::DropAction action,
                         int row, int column, const QModelIndex& parent) const override;
    bool dropMimeData(const QMimeData* data, Qt::DropAction action,
                      int row, int column, const QModelIndex& parent) override;

    // File operations
    bool addFile(const QString& localPath, const QModelIndex& parent = QModelIndex());
    bool addFiles(const QStringList& localPaths, const QModelIndex& parent = QModelIndex());
    bool addDirectory(const QString& localPath, const QModelIndex& parent = QModelIndex());
    bool addFileWithIsoPath(const QString& localPath, const QString& isoPath);
    bool createFolder(const QString& name, const QModelIndex& parent = QModelIndex());
    bool removeItem(const QModelIndex& index);
    void clear();

    /// Get total size of all files
    [[nodiscard]] qint64 totalSize() const { return m_totalSize; }

    /// Get list of all local file paths
    [[nodiscard]] QStringList allLocalPaths() const;

    /// Get all file mappings (localPath -> isoPath)
    /// Used for burning with proper directory structure
    [[nodiscard]] QList<QPair<QString, QString>> allFileMappings() const;

    /// Get the volume label
    [[nodiscard]] QString volumeLabel() const { return m_volumeLabel; }

    /// Set the volume label
    void setVolumeLabel(const QString& label);

    /// Refresh file existence status for all items
    void refreshFileExistence();

    /// Check if all files exist
    [[nodiscard]] bool allFilesExist() const;

signals:
    void totalSizeChanged(qint64 size);
    void filesAdded(int count);
    void volumeLabelChanged(const QString& label);

private:
    ProjectItem* itemFromIndex(const QModelIndex& index) const;
    QModelIndex indexFromItem(ProjectItem* item) const;
    void addFileRecursive(const std::filesystem::path& path, ProjectItem* parent);
    void recalculateTotalSize();
    QString formatSize(qint64 bytes) const;

    std::unique_ptr<ProjectItem> m_rootItem;
    qint64 m_totalSize{0};
    QString m_volumeLabel;
};

} // namespace Burner::Models
