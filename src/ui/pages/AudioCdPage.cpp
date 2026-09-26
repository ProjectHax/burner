#include "AudioCdPage.hpp"
#include "../widgets/FileDropArea.hpp"
#include "../../models/TrackListModel.hpp"

#include <QListView>
#include <QLabel>
#include <QFont>
#include <QShortcut>
#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QSplitter>

namespace Burner::UI {

AudioCdPage::AudioCdPage(QWidget* parent)
    : QWidget(parent)
    , m_model(new Models::TrackListModel(this)) {
    setupUi();
    connectSignals();
}

AudioCdPage::~AudioCdPage() = default;

QStringList AudioCdPage::audioFiles() const {
    return m_model->trackPaths();
}

int AudioCdPage::totalDurationSeconds() const {
    return m_model->totalDurationSeconds();
}

bool AudioCdPage::fitsOnCD() const {
    return m_model->fitsOnCD();
}

void AudioCdPage::setAudioFiles(const QStringList& files) {
    m_model->clear();
    m_model->addTracks(files);
}

void AudioCdPage::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);

    // Header
    auto* headerLabel = new QLabel(tr("Audio CD"), this);
    QFont headerFont = headerLabel->font();
    headerFont.setPointSize(headerFont.pointSize() + 2);
    headerFont.setBold(true);
    headerLabel->setFont(headerFont);
    mainLayout->addWidget(headerLabel);

    // Info row
    auto* infoLayout = new QHBoxLayout();
    infoLayout->addWidget(new QLabel(tr("Red Book CD-DA (44.1kHz 16-bit Stereo)"), this));
    infoLayout->addStretch();
    m_durationLabel = new QLabel(tr("Total: 0:00 / 80:00"), this);
    infoLayout->addWidget(m_durationLabel);
    mainLayout->addLayout(infoLayout);

    // Capacity warning
    m_capacityWarning = new QLabel(this);
    m_capacityWarning->setStyleSheet("QLabel { color: #d32f2f; font-weight: bold; }");
    m_capacityWarning->hide();
    mainLayout->addWidget(m_capacityWarning);

    // Splitter with list view and drop area
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    // Track list
    m_listView = new QListView(this);
    m_listView->setModel(m_model);
    m_listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_listView->setDragEnabled(true);
    m_listView->setAcceptDrops(true);
    m_listView->setDropIndicatorShown(true);
    m_listView->setDragDropMode(QAbstractItemView::InternalMove);
    m_listView->setDefaultDropAction(Qt::MoveAction);

    // Context menu for right-click
    m_listView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_listView, &QListView::customContextMenuRequested, this, [this](const QPoint& pos) {
        QMenu menu(this);
        menu.addAction(tr("Add Tracks..."), this, &AudioCdPage::addTracks);
        menu.addSeparator();
        bool hasSelection = m_listView->selectionModel()->hasSelection();
        auto* upAction = menu.addAction(tr("Move Up"), this, &AudioCdPage::moveUp);
        upAction->setEnabled(hasSelection);
        auto* downAction = menu.addAction(tr("Move Down"), this, &AudioCdPage::moveDown);
        downAction->setEnabled(hasSelection);
        menu.addSeparator();
        auto* removeAction = menu.addAction(tr("Remove"), this, &AudioCdPage::removeSelected);
        removeAction->setEnabled(hasSelection);
        auto* clearAction = menu.addAction(tr("Clear All"), this, &AudioCdPage::clearAll);
        clearAction->setEnabled(m_model->rowCount() > 0);
        menu.exec(m_listView->viewport()->mapToGlobal(pos));
    });

    // Delete key shortcut
    auto* deleteShortcut = new QShortcut(QKeySequence::Delete, m_listView);
    connect(deleteShortcut, &QShortcut::activated, this, &AudioCdPage::removeSelected);

    splitter->addWidget(m_listView);

    // Drop area
    m_dropArea = new FileDropArea(this);
    m_dropArea->setHint(tr("Drop audio files here\n(MP3, FLAC, WAV, OGG)"));
    m_dropArea->setFileFilter({"mp3", "flac", "wav", "ogg", "aac", "m4a", "wma"});
    splitter->addWidget(m_dropArea);

    splitter->setSizes({400, 200});
    mainLayout->addWidget(splitter, 1);

    // Button row
    auto* buttonLayout = new QHBoxLayout();

    m_addButton = new QPushButton(tr("Add Tracks..."), this);
    buttonLayout->addWidget(m_addButton);

    m_upButton = new QPushButton(tr("Move Up"), this);
    buttonLayout->addWidget(m_upButton);

    m_downButton = new QPushButton(tr("Move Down"), this);
    buttonLayout->addWidget(m_downButton);

    buttonLayout->addStretch();

    m_removeButton = new QPushButton(tr("Remove"), this);
    buttonLayout->addWidget(m_removeButton);

    m_clearButton = new QPushButton(tr("Clear All"), this);
    buttonLayout->addWidget(m_clearButton);

    mainLayout->addLayout(buttonLayout);
}

void AudioCdPage::connectSignals() {
    connect(m_addButton, &QPushButton::clicked, this, &AudioCdPage::addTracks);
    connect(m_removeButton, &QPushButton::clicked, this, &AudioCdPage::removeSelected);
    connect(m_upButton, &QPushButton::clicked, this, &AudioCdPage::moveUp);
    connect(m_downButton, &QPushButton::clicked, this, &AudioCdPage::moveDown);
    connect(m_clearButton, &QPushButton::clicked, this, &AudioCdPage::clearAll);

    connect(m_dropArea, &FileDropArea::filesDropped, this, [this](const QStringList& paths) {
        m_model->addTracks(paths);
    });

    connect(m_model, &Models::TrackListModel::totalDurationChanged, this, [this]() {
        updateDurationLabel();
        emit tracksChanged();
    });

    connect(m_model, &Models::TrackListModel::capacityStatusChanged, this, [this](bool fits) {
        if (!fits) {
            m_capacityWarning->setText(tr("Warning: Total duration exceeds 80 minutes!"));
            m_capacityWarning->show();
        } else {
            m_capacityWarning->hide();
        }
        emit capacityExceeded(!fits);
    });
}

void AudioCdPage::updateDurationLabel() {
    int total = m_model->totalDurationSeconds();
    int mins = total / 60;
    int secs = total % 60;
    m_durationLabel->setText(tr("Total: %1:%2 / 80:00")
        .arg(mins)
        .arg(secs, 2, 10, QChar('0')));
}

void AudioCdPage::addTracks() {
    QStringList files = QFileDialog::getOpenFileNames(
        this,
        tr("Select Audio Files"),
        QString(),
        tr("Audio Files (*.mp3 *.flac *.wav *.ogg *.aac *.m4a *.wma);;All Files (*)")
    );

    if (!files.isEmpty()) {
        m_model->addTracks(files);
    }
}

void AudioCdPage::removeSelected() {
    QModelIndexList selected = m_listView->selectionModel()->selectedIndexes();

    // Remove in reverse order
    std::vector<int> rows;
    for (const auto& index : selected) {
        rows.push_back(index.row());
    }
    std::sort(rows.rbegin(), rows.rend());

    for (int row : rows) {
        m_model->removeTrack(row);
    }
}

void AudioCdPage::moveUp() {
    QModelIndex current = m_listView->currentIndex();
    if (!current.isValid() || current.row() == 0) {
        return;
    }

    m_model->moveTrack(current.row(), current.row() - 1);
    m_listView->setCurrentIndex(m_model->index(current.row() - 1));
}

void AudioCdPage::moveDown() {
    QModelIndex current = m_listView->currentIndex();
    if (!current.isValid() || current.row() >= m_model->trackCount() - 1) {
        return;
    }

    m_model->moveTrack(current.row(), current.row() + 1);
    m_listView->setCurrentIndex(m_model->index(current.row() + 1));
}

void AudioCdPage::clearAll() {
    m_model->clear();
}

} // namespace Burner::UI
