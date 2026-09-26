#include "VideoCdPage.hpp"
#include "../widgets/FileDropArea.hpp"

#include <QListWidget>
#include <QLabel>
#include <QFont>
#include <QShortcut>
#include <QMenu>
#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QSplitter>
#include <QFileInfo>
#include <QBrush>

namespace Burner::UI {

VideoCdPage::VideoCdPage(QWidget* parent)
    : QWidget(parent) {
    setupUi();
    connectSignals();
}

VideoCdPage::~VideoCdPage() = default;

QStringList VideoCdPage::videoFiles() const {
    QStringList files;
    for (int i = 0; i < m_listWidget->count(); ++i) {
        files.append(m_listWidget->item(i)->data(Qt::UserRole).toString());
    }
    return files;
}

QString VideoCdPage::vcdType() const {
    return m_typeCombo->currentText();
}

void VideoCdPage::setVideoFiles(const QStringList& files) {
    clearAll();
    addVideos(files);
}

void VideoCdPage::setVcdType(const QString& type) {
    int index = m_typeCombo->findText(type);
    if (index >= 0) {
        m_typeCombo->setCurrentIndex(index);
    }
}

void VideoCdPage::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);

    // Header
    auto* headerLabel = new QLabel(tr("Video CD"), this);
    QFont headerFont = headerLabel->font();
    headerFont.setPointSize(headerFont.pointSize() + 2);
    headerFont.setBold(true);
    headerLabel->setFont(headerFont);
    mainLayout->addWidget(headerLabel);

    // Type selection row
    auto* typeLayout = new QHBoxLayout();
    typeLayout->addWidget(new QLabel(tr("Disc Type:"), this));
    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItems({tr("VCD 2.0"), tr("SVCD")});
    typeLayout->addWidget(m_typeCombo);
    typeLayout->addStretch();
    mainLayout->addLayout(typeLayout);

    // Info label
    m_infoLabel = new QLabel(this);
    m_infoLabel->setWordWrap(true);
    m_infoLabel->setText(tr(
        "VCD: MPEG-1 352x288 (PAL) / 352x240 (NTSC) - Up to 80 minutes | "
        "SVCD: MPEG-2 480x576 (PAL) / 480x480 (NTSC) - Up to 60 minutes"
    ));
    mainLayout->addWidget(m_infoLabel);

    // Splitter with list and drop area
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    // Video list
    m_listWidget = new QListWidget(this);
    m_listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // Context menu for right-click
    m_listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_listWidget, &QListWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QMenu menu(this);
        menu.addAction(tr("Add Videos..."), this, [this]() { addVideos(); });
        menu.addSeparator();
        bool hasSelection = !m_listWidget->selectedItems().isEmpty();
        auto* removeAction = menu.addAction(tr("Remove"), this, &VideoCdPage::removeSelected);
        removeAction->setEnabled(hasSelection);
        auto* clearAction = menu.addAction(tr("Clear All"), this, &VideoCdPage::clearAll);
        clearAction->setEnabled(m_listWidget->count() > 0);
        menu.exec(m_listWidget->viewport()->mapToGlobal(pos));
    });

    // Delete key shortcut
    auto* deleteShortcut = new QShortcut(QKeySequence::Delete, m_listWidget);
    connect(deleteShortcut, &QShortcut::activated, this, &VideoCdPage::removeSelected);

    splitter->addWidget(m_listWidget);

    // Drop area
    m_dropArea = new FileDropArea(this);
    m_dropArea->setHint(tr("Drop video files here\n(MP4, AVI, MKV, MOV, MPG)"));
    m_dropArea->setFileFilter({"mp4", "avi", "mkv", "mov", "wmv", "mpg", "mpeg", "m4v", "webm"});
    splitter->addWidget(m_dropArea);

    splitter->setSizes({400, 200});
    mainLayout->addWidget(splitter, 1);

    // Button row
    auto* buttonLayout = new QHBoxLayout();

    m_addButton = new QPushButton(tr("Add Videos..."), this);
    buttonLayout->addWidget(m_addButton);

    buttonLayout->addStretch();

    m_removeButton = new QPushButton(tr("Remove"), this);
    buttonLayout->addWidget(m_removeButton);

    m_clearButton = new QPushButton(tr("Clear All"), this);
    buttonLayout->addWidget(m_clearButton);

    mainLayout->addLayout(buttonLayout);
}

void VideoCdPage::connectSignals() {
    connect(m_addButton, &QPushButton::clicked, this, qOverload<>(&VideoCdPage::addVideos));
    connect(m_removeButton, &QPushButton::clicked, this, &VideoCdPage::removeSelected);
    connect(m_clearButton, &QPushButton::clicked, this, &VideoCdPage::clearAll);

    connect(m_dropArea, &FileDropArea::filesDropped, this, qOverload<const QStringList&>(&VideoCdPage::addVideos));
}

void VideoCdPage::addVideos() {
    QStringList files = QFileDialog::getOpenFileNames(
        this,
        tr("Select Video Files"),
        QString(),
        tr("Video Files (*.mp4 *.avi *.mkv *.mov *.wmv *.mpg *.mpeg *.m4v *.webm);;All Files (*)")
    );

    addVideos(files);
}

void VideoCdPage::addVideos(const QStringList& files) {
    if (files.isEmpty()) {
        return;
    }

    for (const QString& file : files) {
        QFileInfo info(file);
        bool exists = info.exists() && info.isReadable();

        QString displayName = exists ? info.fileName() : QString("[!] %1").arg(info.fileName());
        auto* item = new QListWidgetItem(displayName);
        item->setData(Qt::UserRole, file);
        item->setData(Qt::UserRole + 1, exists);  // Store exists status
        item->setToolTip(exists ? file : tr("File not found: %1").arg(file));

        if (!exists) {
            item->setForeground(Qt::red);
        }

        m_listWidget->addItem(item);
    }

    emit videosChanged();
}

void VideoCdPage::removeSelected() {
    QList<QListWidgetItem*> selected = m_listWidget->selectedItems();
    for (QListWidgetItem* item : selected) {
        delete item;
    }
    if (!selected.isEmpty()) {
        emit videosChanged();
    }
}

void VideoCdPage::clearAll() {
    m_listWidget->clear();
    emit videosChanged();
}

void VideoCdPage::refreshFileExistence() {
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem* item = m_listWidget->item(i);
        QString filePath = item->data(Qt::UserRole).toString();
        QFileInfo info(filePath);
        bool exists = info.exists() && info.isReadable();

        item->setData(Qt::UserRole + 1, exists);
        item->setText(exists ? info.fileName() : QString("[!] %1").arg(info.fileName()));
        item->setToolTip(exists ? filePath : tr("File not found: %1").arg(filePath));
        item->setForeground(exists ? palette().text() : QBrush(Qt::red));
    }
}

bool VideoCdPage::allFilesExist() const {
    for (int i = 0; i < m_listWidget->count(); ++i) {
        if (!m_listWidget->item(i)->data(Qt::UserRole + 1).toBool()) {
            return false;
        }
    }
    return true;
}

} // namespace Burner::UI
