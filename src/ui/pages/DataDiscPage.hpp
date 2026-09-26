#pragma once

#include <QWidget>
#include <memory>

class QTreeView;
class QPushButton;
class QLabel;
class QFrame;

namespace Burner::Models {
class FileTreeModel;
}

namespace Burner::UI {

class FileDropArea;

/// Page for creating data CDs/DVDs.
/// Allows adding files and folders to be burned.
class DataDiscPage : public QWidget {
    Q_OBJECT

public:
    explicit DataDiscPage(QWidget* parent = nullptr);
    ~DataDiscPage() override;

    /// Get the file tree model
    [[nodiscard]] Models::FileTreeModel* model() const { return m_model; }

    /// Get all file paths to burn
    [[nodiscard]] QStringList files() const;

    /// Get all file mappings (localPath -> isoPath) for burning with structure
    [[nodiscard]] QList<QPair<QString, QString>> fileMappings() const;

    /// Get total size of files
    [[nodiscard]] qint64 totalSize() const;

    /// Set file mappings (for loading projects)
    void setFileMappings(const QList<QPair<QString, QString>>& mappings);

    /// Update the multi-session info banner
    /// @param sessions Number of existing sessions (0 = no multi-session info)
    /// @param appendable Whether the disc is appendable
    void setMultiSessionInfo(int sessions, bool appendable);

public slots:
    void addFiles();
    void addFolder();
    void createFolder();
    void removeSelected();
    void clearAll();
    void saveAsIso();

signals:
    void filesChanged();
    void burnRequested();
    void saveIsoRequested(const QString& outputPath);

private:
    void setupUi();
    void connectSignals();

    QTreeView* m_treeView{nullptr};
    FileDropArea* m_dropArea{nullptr};
    QPushButton* m_addFilesButton{nullptr};
    QPushButton* m_addFolderButton{nullptr};
    QPushButton* m_newFolderButton{nullptr};
    QPushButton* m_removeButton{nullptr};
    QPushButton* m_clearButton{nullptr};
    QPushButton* m_saveIsoButton{nullptr};
    Models::FileTreeModel* m_model{nullptr};
    QFrame* m_sessionBanner{nullptr};
    QLabel* m_sessionLabel{nullptr};
};

} // namespace Burner::UI
