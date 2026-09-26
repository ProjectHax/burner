#include "IsoTreeModel.hpp"
#include "../core/IsoReader.hpp"

#include <QLocale>
#include <QIcon>

namespace Burner::Models {

IsoTreeModel::IsoTreeModel(QObject* parent)
    : QAbstractItemModel(parent)
    , m_rootNode(std::make_unique<Node>()) {
}

IsoTreeModel::~IsoTreeModel() = default;

void IsoTreeModel::loadFromIsoEntry(const Core::IsoEntry& root) {
    beginResetModel();
    m_rootNode = std::make_unique<Node>();
    m_rootNode->name = "/";
    m_rootNode->fullPath = "/";
    m_rootNode->isDirectory = true;

    buildTree(root, m_rootNode.get());
    endResetModel();
}

void IsoTreeModel::clear() {
    beginResetModel();
    m_rootNode = std::make_unique<Node>();
    endResetModel();
}

void IsoTreeModel::buildTree(const Core::IsoEntry& entry, Node* parent) {
    for (const auto& child : entry.children) {
        auto node = std::make_unique<Node>();
        node->name = QString::fromStdString(child.name);
        node->fullPath = QString::fromStdString(child.fullPath);
        node->size = child.size;
        node->modTime = QDateTime::fromSecsSinceEpoch(child.modTime);
        node->isDirectory = child.isDirectory;
        node->parent = parent;

        if (child.isDirectory) {
            buildTree(child, node.get());
        }

        parent->children.push_back(std::move(node));
    }
}

QModelIndex IsoTreeModel::index(int row, int column, const QModelIndex& parent) const {
    if (!hasIndex(row, column, parent)) {
        return {};
    }

    Node* parentNode = parent.isValid()
        ? nodeFromIndex(parent)
        : m_rootNode.get();

    if (!parentNode || row < 0 || row >= static_cast<int>(parentNode->children.size())) {
        return {};
    }

    return createIndex(row, column, parentNode->children[row].get());
}

QModelIndex IsoTreeModel::parent(const QModelIndex& index) const {
    if (!index.isValid()) {
        return {};
    }

    auto* node = nodeFromIndex(index);
    if (!node || !node->parent || node->parent == m_rootNode.get()) {
        return {};
    }

    Node* parent = node->parent;
    Node* grandparent = parent->parent;
    if (!grandparent) {
        return {};
    }

    // Find parent's row in grandparent
    for (int i = 0; i < static_cast<int>(grandparent->children.size()); ++i) {
        if (grandparent->children[i].get() == parent) {
            return createIndex(i, 0, parent);
        }
    }

    return {};
}

int IsoTreeModel::rowCount(const QModelIndex& parent) const {
    Node* node = parent.isValid()
        ? nodeFromIndex(parent)
        : m_rootNode.get();

    if (!node) {
        return 0;
    }

    return static_cast<int>(node->children.size());
}

int IsoTreeModel::columnCount(const QModelIndex&) const {
    return ColumnCount;
}

QVariant IsoTreeModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) {
        return {};
    }

    auto* node = nodeFromIndex(index);
    if (!node) {
        return {};
    }

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case NameColumn:
                return node->name;
            case SizeColumn:
                return node->isDirectory ? QString() : formatSize(node->size);
            case DateColumn:
                return node->modTime.isValid()
                    ? QLocale().toString(node->modTime, QLocale::ShortFormat)
                    : QString();
        }
    } else if (role == Qt::DecorationRole && index.column() == NameColumn) {
        return node->isDirectory
            ? QIcon::fromTheme("folder")
            : QIcon::fromTheme("text-x-generic");
    }

    return {};
}

QVariant IsoTreeModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }

    switch (section) {
        case NameColumn: return tr("Name");
        case SizeColumn: return tr("Size");
        case DateColumn: return tr("Date Modified");
    }

    return {};
}

Qt::ItemFlags IsoTreeModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QString IsoTreeModel::isoPathForIndex(const QModelIndex& index) const {
    auto* node = nodeFromIndex(index);
    return node ? node->fullPath : QString();
}

QStringList IsoTreeModel::isoPathsForIndices(const QModelIndexList& indices) const {
    QStringList paths;
    for (const auto& idx : indices) {
        if (idx.column() == 0) { // Only count once per row
            QString path = isoPathForIndex(idx);
            if (!path.isEmpty()) {
                paths.append(path);
            }
        }
    }
    return paths;
}

IsoTreeModel::Node* IsoTreeModel::nodeFromIndex(const QModelIndex& index) const {
    if (!index.isValid()) {
        return nullptr;
    }
    return static_cast<Node*>(index.internalPointer());
}

QString IsoTreeModel::formatSize(quint64 bytes) const {
    if (bytes >= 1024ULL * 1024 * 1024) {
        return QString("%1 GB").arg(static_cast<double>(bytes) / (1024.0 * 1024 * 1024), 0, 'f', 2);
    } else if (bytes >= 1024ULL * 1024) {
        return QString("%1 MB").arg(static_cast<double>(bytes) / (1024.0 * 1024), 0, 'f', 1);
    } else if (bytes >= 1024) {
        return QString("%1 KB").arg(bytes / 1024);
    } else {
        return QString("%1 B").arg(bytes);
    }
}

} // namespace Burner::Models
