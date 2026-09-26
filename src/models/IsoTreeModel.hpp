#pragma once

#include <QAbstractItemModel>
#include <QDateTime>
#include <memory>
#include <vector>

namespace Burner::Core {
struct IsoEntry;
}

namespace Burner::Models {

/// Read-only tree model for browsing ISO image contents.
class IsoTreeModel : public QAbstractItemModel {
    Q_OBJECT

public:
    enum Column {
        NameColumn = 0,
        SizeColumn,
        DateColumn,
        ColumnCount
    };

    explicit IsoTreeModel(QObject* parent = nullptr);
    ~IsoTreeModel() override;

    /// Load from an IsoEntry tree
    void loadFromIsoEntry(const Burner::Core::IsoEntry& root);

    /// Clear the model
    void clear();

    // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    /// Get the ISO path for a given model index
    QString isoPathForIndex(const QModelIndex& index) const;

    /// Get ISO paths for all selected indices
    QStringList isoPathsForIndices(const QModelIndexList& indices) const;

private:
    struct Node {
        QString name;
        QString fullPath;
        quint64 size{0};
        QDateTime modTime;
        bool isDirectory{false};
        std::vector<std::unique_ptr<Node>> children;
        Node* parent{nullptr};
    };

    void buildTree(const Burner::Core::IsoEntry& entry, Node* parent);
    Node* nodeFromIndex(const QModelIndex& index) const;
    QString formatSize(quint64 bytes) const;

    std::unique_ptr<Node> m_rootNode;
};

} // namespace Burner::Models
