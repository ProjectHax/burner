#pragma once

#include <QWidget>
#include <memory>

class QTreeView;
class QPushButton;
class QLabel;
class QLineEdit;
class QGroupBox;

namespace Burner::Core {
class IsoReader;
}

namespace Burner::Models {
class IsoTreeModel;
}

namespace Burner::UI {

/// Page for browsing and extracting files from ISO images.
class IsoBrowserPage : public QWidget {
    Q_OBJECT

public:
    explicit IsoBrowserPage(QWidget* parent = nullptr);
    ~IsoBrowserPage() override;

public slots:
    void openIso();
    void openIsoFile(const QString& path);
    void extractSelected();
    void extractAll();
    void clearIso();

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void setupUi();
    void updateInfo();

    QLineEdit* m_pathEdit{nullptr};
    QPushButton* m_browseButton{nullptr};
    QPushButton* m_clearButton{nullptr};
    QGroupBox* m_infoGroup{nullptr};
    QLabel* m_volumeLabel{nullptr};
    QLabel* m_sizeLabel{nullptr};
    QLabel* m_fileCountLabel{nullptr};
    QLabel* m_dropHint{nullptr};
    QTreeView* m_treeView{nullptr};
    QPushButton* m_extractSelectedButton{nullptr};
    QPushButton* m_extractAllButton{nullptr};

    Models::IsoTreeModel* m_model{nullptr};
    std::unique_ptr<Core::IsoReader> m_reader;
};

} // namespace Burner::UI
