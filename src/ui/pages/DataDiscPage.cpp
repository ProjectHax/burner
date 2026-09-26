#include "DataDiscPage.hpp"
#include "../Dialogs.hpp"
#include "../widgets/FileDropArea.hpp"
#include "../../models/FileTreeModel.hpp"

#include <QTreeView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QInputDialog>
#include <QHeaderView>
#include <QLabel>
#include <QFont>
#include <QFrame>
#include <QSplitter>
#include <QShortcut>
#include <QMenu>

namespace Burner::UI {

DataDiscPage::DataDiscPage(QWidget* parent)
    : QWidget(parent)
    , m_model(new Models::FileTreeModel(this)) {
    setupUi();
    connectSignals();
}

DataDiscPage::~DataDiscPage() = default;

QStringList DataDiscPage::files() const {
    return m_model->allLocalPaths();
}

QList<QPair<QString, QString>> DataDiscPage::fileMappings() const {
    return m_model->allFileMappings();
}

qint64 DataDiscPage::totalSize() const {
    return m_model->totalSize();
}

void DataDiscPage::setFileMappings(const QList<QPair<QString, QString>>& mappings) {
    m_model->clear();
    for (const auto& mapping : mappings) {
        m_model->addFileWithIsoPath(mapping.first, mapping.second);
    }
}

void DataDiscPage::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);

    // Header
    auto* headerLabel = new QLabel(tr("Data CD/DVD"), this);
    QFont headerFont = headerLabel->font();
    headerFont.setPointSize(headerFont.pointSize() + 2);
    headerFont.setBold(true);
    headerLabel->setFont(headerFont);
    mainLayout->addWidget(headerLabel);

    // Multi-session info banner (hidden by default)
    m_sessionBanner = new QFrame(this);
    m_sessionBanner->setFrameStyle(QFrame::StyledPanel);
    m_sessionBanner->setStyleSheet(
        "QFrame { background-color: #e8f4fd; border: 1px solid #b8d4e8; "
        "border-radius: 4px; padding: 6px; }"
    );
    auto* bannerLayout = new QHBoxLayout(m_sessionBanner);
    bannerLayout->setContentsMargins(8, 4, 8, 4);
    m_sessionLabel = new QLabel(this);
    bannerLayout->addWidget(m_sessionLabel);
    m_sessionBanner->setVisible(false);
    mainLayout->addWidget(m_sessionBanner);

    // Splitter with tree view and drop area
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    // Tree view for file structure
    m_treeView = new QTreeView(this);
    m_treeView->setModel(m_model);
    m_treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_treeView->setDragEnabled(true);
    m_treeView->setAcceptDrops(true);
    m_treeView->setDropIndicatorShown(true);
    m_treeView->setDragDropMode(QAbstractItemView::DragDrop);
    m_treeView->setDefaultDropAction(Qt::CopyAction);
    m_treeView->header()->setStretchLastSection(true);
    m_treeView->header()->setSectionResizeMode(0, QHeaderView::Stretch);

    // Context menu for right-click
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_treeView, &QTreeView::customContextMenuRequested, this, [this](const QPoint& pos) {
        QMenu menu(this);
        menu.addAction(tr("Add Files..."), this, &DataDiscPage::addFiles);
        menu.addAction(tr("Add Folder..."), this, &DataDiscPage::addFolder);
        menu.addAction(tr("New Folder"), this, &DataDiscPage::createFolder);
        menu.addSeparator();
        bool hasSelection = m_treeView->selectionModel()->hasSelection();
        auto* removeAction = menu.addAction(tr("Remove"), this, &DataDiscPage::removeSelected);
        removeAction->setEnabled(hasSelection);
        auto* clearAction = menu.addAction(tr("Clear All"), this, &DataDiscPage::clearAll);
        clearAction->setEnabled(m_model->rowCount() > 0);
        menu.exec(m_treeView->viewport()->mapToGlobal(pos));
    });

    // Delete key shortcut
    auto* deleteShortcut = new QShortcut(QKeySequence::Delete, m_treeView);
    connect(deleteShortcut, &QShortcut::activated, this, &DataDiscPage::removeSelected);

    splitter->addWidget(m_treeView);

    // Drop area
    m_dropArea = new FileDropArea(this);
    m_dropArea->setHint(tr("Drop files or folders here\nto add to disc"));
    splitter->addWidget(m_dropArea);

    splitter->setSizes({400, 200});
    mainLayout->addWidget(splitter, 1);

    // Button row
    auto* buttonLayout = new QHBoxLayout();

    m_addFilesButton = new QPushButton(tr("Add Files..."), this);
    buttonLayout->addWidget(m_addFilesButton);

    m_addFolderButton = new QPushButton(tr("Add Folder..."), this);
    buttonLayout->addWidget(m_addFolderButton);

    m_newFolderButton = new QPushButton(tr("New Folder"), this);
    buttonLayout->addWidget(m_newFolderButton);

    buttonLayout->addStretch();

    m_removeButton = new QPushButton(tr("Remove"), this);
    buttonLayout->addWidget(m_removeButton);

    m_clearButton = new QPushButton(tr("Clear All"), this);
    buttonLayout->addWidget(m_clearButton);

    buttonLayout->addSpacing(20);

    m_saveIsoButton = new QPushButton(tr("Save as ISO..."), this);
    m_saveIsoButton->setToolTip(tr("Save disc contents to an ISO file"));
    buttonLayout->addWidget(m_saveIsoButton);

    mainLayout->addLayout(buttonLayout);
}

void DataDiscPage::connectSignals() {
    connect(m_addFilesButton, &QPushButton::clicked, this, &DataDiscPage::addFiles);
    connect(m_addFolderButton, &QPushButton::clicked, this, &DataDiscPage::addFolder);
    connect(m_newFolderButton, &QPushButton::clicked, this, &DataDiscPage::createFolder);
    connect(m_removeButton, &QPushButton::clicked, this, &DataDiscPage::removeSelected);
    connect(m_clearButton, &QPushButton::clicked, this, &DataDiscPage::clearAll);
    connect(m_saveIsoButton, &QPushButton::clicked, this, &DataDiscPage::saveAsIso);

    connect(m_dropArea, &FileDropArea::filesDropped, this, [this](const QStringList& paths) {
        m_model->addFiles(paths);
    });

    connect(m_model, &Models::FileTreeModel::totalSizeChanged, this, [this]() {
        emit filesChanged();
    });
}

void DataDiscPage::addFiles() {
    QStringList files = QFileDialog::getOpenFileNames(
        this,
        tr("Select Files"),
        QString(),
        tr("All Files (*)")
    );

    if (!files.isEmpty()) {
        m_model->addFiles(files, m_treeView->currentIndex());
    }
}

void DataDiscPage::addFolder() {
    QString dir = QFileDialog::getExistingDirectory(
        this,
        tr("Select Folder"),
        QString(),
        QFileDialog::ShowDirsOnly
    );

    if (!dir.isEmpty()) {
        m_model->addDirectory(dir, m_treeView->currentIndex());
    }
}

void DataDiscPage::createFolder() {
    bool ok;
    QString name = Dialogs::getText(
        this,
        tr("New Folder"),
        tr("Folder name:"),
        QLineEdit::Normal,
        tr("New Folder"),
        &ok
    );

    if (ok && !name.isEmpty()) {
        m_model->createFolder(name, m_treeView->currentIndex());
    }
}

void DataDiscPage::removeSelected() {
    QModelIndexList selected = m_treeView->selectionModel()->selectedIndexes();

    // Remove in reverse order to maintain index validity
    std::vector<QPersistentModelIndex> persistent;
    for (const auto& index : selected) {
        if (index.column() == 0) {
            persistent.emplace_back(index);
        }
    }

    for (const auto& index : persistent) {
        m_model->removeItem(index);
    }
}

void DataDiscPage::clearAll() {
    m_model->clear();
}

void DataDiscPage::setMultiSessionInfo(int sessions, bool appendable) {
    if (sessions > 0 && appendable) {
        m_sessionLabel->setText(
            tr("Disc contains %n existing session(s) \u2014 new data will be appended", "", sessions));
        m_sessionBanner->setVisible(true);
    } else {
        m_sessionBanner->setVisible(false);
    }
}

void DataDiscPage::saveAsIso() {
    if (m_model->rowCount() == 0) {
        return;
    }

    QString defaultName = "disc.iso";

    QString outputPath = QFileDialog::getSaveFileName(
        this,
        tr("Save ISO Image"),
        defaultName,
        tr("ISO Images (*.iso);;All Files (*)")
    );

    if (!outputPath.isEmpty()) {
        emit saveIsoRequested(outputPath);
    }
}

} // namespace Burner::UI
