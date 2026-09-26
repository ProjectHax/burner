#include "IsoBrowserPage.hpp"
#include "../Dialogs.hpp"
#include "../../core/IsoReader.hpp"
#include "../../models/IsoTreeModel.hpp"

#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QTreeView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QProgressDialog>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QUrl>
#include <QLocale>
#include <QMenu>

namespace Burner::UI {

IsoBrowserPage::IsoBrowserPage(QWidget* parent)
    : QWidget(parent)
    , m_model(new Models::IsoTreeModel(this))
    , m_reader(std::make_unique<Core::IsoReader>()) {
    setAcceptDrops(true);
    setupUi();
}

IsoBrowserPage::~IsoBrowserPage() = default;

void IsoBrowserPage::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);

    // Header
    auto* headerLabel = new QLabel(tr("Browse ISO Image"), this);
    QFont headerFont = headerLabel->font();
    headerFont.setPointSize(headerFont.pointSize() + 2);
    headerFont.setBold(true);
    headerLabel->setFont(headerFont);
    mainLayout->addWidget(headerLabel);

    auto* descLabel = new QLabel(tr(
        "Open an ISO image to browse its contents. "
        "You can extract individual files or the entire image."
    ), this);
    descLabel->setWordWrap(true);
    mainLayout->addWidget(descLabel);

    // Drop zone hint
    m_dropHint = new QLabel(tr("Drag and drop an ISO file here, or use the Browse button below"), this);
    m_dropHint->setAlignment(Qt::AlignCenter);
    m_dropHint->setStyleSheet(
        "QLabel {"
        "  border: 2px dashed palette(mid);"
        "  border-radius: 8px;"
        "  padding: 32px;"
        "  background-color: palette(alternate-base);"
        "  color: palette(mid);"
        "}"
    );
    m_dropHint->setMinimumHeight(80);
    mainLayout->addWidget(m_dropHint);

    // File path row
    auto* pathLayout = new QHBoxLayout();
    pathLayout->addWidget(new QLabel(tr("ISO File:"), this));

    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setPlaceholderText(tr("Select an ISO image file..."));
    m_pathEdit->setReadOnly(true);
    pathLayout->addWidget(m_pathEdit, 1);

    m_browseButton = new QPushButton(tr("Browse..."), this);
    pathLayout->addWidget(m_browseButton);

    m_clearButton = new QPushButton(tr("Clear"), this);
    m_clearButton->setEnabled(false);
    pathLayout->addWidget(m_clearButton);

    mainLayout->addLayout(pathLayout);

    // Info group
    m_infoGroup = new QGroupBox(tr("Image Information"), this);
    m_infoGroup->setVisible(false);
    auto* infoLayout = new QGridLayout(m_infoGroup);
    infoLayout->setColumnStretch(1, 1);

    int row = 0;
    infoLayout->addWidget(new QLabel(tr("Volume Label:"), this), row, 0);
    m_volumeLabel = new QLabel(this);
    m_volumeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    infoLayout->addWidget(m_volumeLabel, row++, 1);

    infoLayout->addWidget(new QLabel(tr("Total Size:"), this), row, 0);
    m_sizeLabel = new QLabel(this);
    infoLayout->addWidget(m_sizeLabel, row++, 1);

    infoLayout->addWidget(new QLabel(tr("Files:"), this), row, 0);
    m_fileCountLabel = new QLabel(this);
    infoLayout->addWidget(m_fileCountLabel, row++, 1);

    mainLayout->addWidget(m_infoGroup);

    // Tree view
    m_treeView = new QTreeView(this);
    m_treeView->setModel(m_model);
    m_treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_treeView->setAlternatingRowColors(true);
    m_treeView->setAnimated(true);
    m_treeView->setSortingEnabled(true);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeView->setVisible(false);

    // Set column widths
    m_treeView->header()->setStretchLastSection(false);
    m_treeView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_treeView->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_treeView->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    mainLayout->addWidget(m_treeView, 1);

    // Extract buttons
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_extractSelectedButton = new QPushButton(tr("Extract Selected..."), this);
    m_extractSelectedButton->setEnabled(false);
    buttonLayout->addWidget(m_extractSelectedButton);

    m_extractAllButton = new QPushButton(tr("Extract All..."), this);
    m_extractAllButton->setEnabled(false);
    buttonLayout->addWidget(m_extractAllButton);

    mainLayout->addLayout(buttonLayout);

    // Connections
    connect(m_browseButton, &QPushButton::clicked, this, &IsoBrowserPage::openIso);
    connect(m_clearButton, &QPushButton::clicked, this, &IsoBrowserPage::clearIso);
    connect(m_extractSelectedButton, &QPushButton::clicked, this, &IsoBrowserPage::extractSelected);
    connect(m_extractAllButton, &QPushButton::clicked, this, &IsoBrowserPage::extractAll);

    connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this]() {
                m_extractSelectedButton->setEnabled(
                    m_treeView->selectionModel()->hasSelection());
            });

    connect(m_treeView, &QTreeView::customContextMenuRequested,
            this, [this](const QPoint& pos) {
                QModelIndex index = m_treeView->indexAt(pos);
                if (!index.isValid()) return;

                QMenu menu(this);
                menu.addAction(tr("Extract..."), this, &IsoBrowserPage::extractSelected);
                menu.exec(m_treeView->viewport()->mapToGlobal(pos));
            });
}

void IsoBrowserPage::openIso() {
    QString file = QFileDialog::getOpenFileName(
        this,
        tr("Open ISO Image"),
        QString(),
        tr("ISO Images (*.iso *.img);;All Files (*)")
    );

    if (!file.isEmpty()) {
        openIsoFile(file);
    }
}

void IsoBrowserPage::openIsoFile(const QString& path) {
    if (!m_reader->open(path.toStdString())) {
        Dialogs::critical(this, tr("Error"),
            tr("Failed to open ISO image:\n%1")
            .arg(QString::fromStdString(m_reader->lastError())));
        return;
    }

    m_pathEdit->setText(path);
    m_clearButton->setEnabled(true);
    m_dropHint->setVisible(false);

    // Load into model
    m_model->loadFromIsoEntry(m_reader->rootEntry());

    // Update info
    updateInfo();

    m_infoGroup->setVisible(true);
    m_treeView->setVisible(true);
    m_extractAllButton->setEnabled(true);

    // Expand first level
    m_treeView->expandToDepth(0);
}

void IsoBrowserPage::clearIso() {
    m_reader->close();
    m_model->clear();
    m_pathEdit->clear();
    m_clearButton->setEnabled(false);
    m_infoGroup->setVisible(false);
    m_treeView->setVisible(false);
    m_dropHint->setVisible(true);
    m_extractSelectedButton->setEnabled(false);
    m_extractAllButton->setEnabled(false);
}

void IsoBrowserPage::updateInfo() {
    if (!m_reader->isOpen()) return;

    QString volumeId = QString::fromStdString(m_reader->volumeId());
    m_volumeLabel->setText(volumeId.isEmpty() ? tr("(none)") : volumeId);

    auto totalSize = m_reader->totalSize();
    if (totalSize >= 1024ULL * 1024 * 1024) {
        m_sizeLabel->setText(QString("%1 GB")
            .arg(static_cast<double>(totalSize) / (1024.0 * 1024 * 1024), 0, 'f', 2));
    } else if (totalSize >= 1024ULL * 1024) {
        m_sizeLabel->setText(QString("%1 MB")
            .arg(static_cast<double>(totalSize) / (1024.0 * 1024), 0, 'f', 1));
    } else {
        m_sizeLabel->setText(QString("%1 KB").arg(totalSize / 1024));
    }

    m_fileCountLabel->setText(QString::number(m_reader->fileCount()));
}

void IsoBrowserPage::extractSelected() {
    auto indices = m_treeView->selectionModel()->selectedIndexes();
    QStringList paths = m_model->isoPathsForIndices(indices);

    if (paths.isEmpty()) {
        Dialogs::information(this, tr("No Selection"),
            tr("Please select files or folders to extract."));
        return;
    }

    QString outputDir = QFileDialog::getExistingDirectory(
        this, tr("Extract to Directory"));
    if (outputDir.isEmpty()) return;

    std::vector<std::string> isoPaths;
    for (const auto& p : paths) {
        isoPaths.push_back(p.toStdString());
    }

    QProgressDialog progress(tr("Extracting files..."), tr("Cancel"), 0, 100, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(500);

    auto result = m_reader->extractEntries(
        isoPaths, outputDir.toStdString(),
        [&progress](int current, int total, const std::string&) {
            if (total > 0) {
                progress.setValue(current * 100 / total);
            }
            if (progress.wasCanceled()) {
                // Note: reader cancellation is handled via atomic flag
            }
        });

    progress.setValue(100);

    if (result.success) {
        Dialogs::information(this, tr("Extraction Complete"),
            tr("Successfully extracted %1 file(s).").arg(result.filesExtracted));
    } else {
        Dialogs::warning(this, tr("Extraction"),
            tr("Extracted %1 file(s) with %2 error(s).\n\n%3")
            .arg(result.filesExtracted)
            .arg(result.errors)
            .arg(QString::fromStdString(result.errorMessage)));
    }
}

void IsoBrowserPage::extractAll() {
    QString outputDir = QFileDialog::getExistingDirectory(
        this, tr("Extract All to Directory"));
    if (outputDir.isEmpty()) return;

    // Get all top-level paths
    std::vector<std::string> allPaths;
    for (const auto& child : m_reader->rootEntry().children) {
        allPaths.push_back(child.fullPath);
    }

    QProgressDialog progress(tr("Extracting all files..."), tr("Cancel"), 0, 100, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(500);

    auto result = m_reader->extractEntries(
        allPaths, outputDir.toStdString(),
        [&progress](int current, int total, const std::string&) {
            if (total > 0) {
                progress.setValue(current * 100 / total);
            }
        });

    progress.setValue(100);

    if (result.success) {
        Dialogs::information(this, tr("Extraction Complete"),
            tr("Successfully extracted %1 file(s).").arg(result.filesExtracted));
    } else {
        Dialogs::warning(this, tr("Extraction"),
            tr("Extracted %1 file(s) with %2 error(s).\n\n%3")
            .arg(result.filesExtracted)
            .arg(result.errors)
            .arg(QString::fromStdString(result.errorMessage)));
    }
}

void IsoBrowserPage::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        for (const QUrl& url : event->mimeData()->urls()) {
            if (url.isLocalFile()) {
                QString path = url.toLocalFile().toLower();
                if (path.endsWith(".iso") || path.endsWith(".img")) {
                    event->acceptProposedAction();
                    return;
                }
            }
        }
    }
    event->ignore();
}

void IsoBrowserPage::dropEvent(QDropEvent* event) {
    if (event->mimeData()->hasUrls()) {
        for (const QUrl& url : event->mimeData()->urls()) {
            if (url.isLocalFile()) {
                QString path = url.toLocalFile();
                QString lower = path.toLower();
                if (lower.endsWith(".iso") || lower.endsWith(".img")) {
                    openIsoFile(path);
                    event->acceptProposedAction();
                    return;
                }
            }
        }
    }
}

} // namespace Burner::UI
