#include "CdRipperPage.hpp"
#include "../Dialogs.hpp"
#include "../../models/CdTrackModel.hpp"
#include "../../engine/QtRipEngine.hpp"
#include "../../core/AudioOutputEncoder.hpp"
#include "../../core/FileNamingPattern.hpp"

#include <QDebug>
#include <QTableView>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QFileDialog>
#include <QStandardPaths>
#include <QSettings>
#include <QMessageBox>
#include <QPixmap>
#include <QFont>

namespace Burner::UI {

CdRipperPage::CdRipperPage(QWidget* parent)
    : QWidget(parent)
    , m_model(new Models::CdTrackModel(this))
    , m_engine(new Engine::QtRipEngine(this)) {
    setupUi();
    connectSignals();
    loadSettings();
}

CdRipperPage::~CdRipperPage() {
    saveSettings();
}

bool CdRipperPage::hasDisc() const {
    return m_hasDisc && m_model->trackCount() > 0;
}

void CdRipperPage::onAudioCdInserted(const QString& device) {
    m_currentDevice = device;
    refresh();
}

void CdRipperPage::setCurrentDevice(const QString& device) {
    m_currentDevice = device;
}

void CdRipperPage::onMediaRemoved(const QString& device) {
    if (device == m_currentDevice) {
        m_hasDisc = false;
        m_discId.clear();
        m_releaseId.clear();
        m_coverArtData.clear();
        m_model->clear();

        m_albumEdit->clear();
        m_artistEdit->clear();
        m_yearSpin->setValue(0);
        m_genreEdit->clear();
        m_coverLabel->clear();
        m_coverLabel->setText(tr("No Cover"));

        updateSelectionInfo();
    }
}

void CdRipperPage::refresh() {
    if (m_currentDevice.isEmpty()) {
        m_statusLabel->setText(tr("No drive selected. Please select a drive."));
        return;
    }

    m_statusLabel->setText(tr("Scanning disc..."));
    m_engine->scanDisc(m_currentDevice);
}

void CdRipperPage::startRip() {
    if (!hasDisc()) {
        Dialogs::warning(this, tr("No Disc"),
            tr("Please insert an audio CD and wait for it to be scanned."));
        return;
    }

    if (m_model->selectedCount() == 0) {
        Dialogs::warning(this, tr("No Tracks Selected"),
            tr("Please select at least one track to rip."));
        return;
    }

    QString outputDir = m_outputEdit->text();
    if (outputDir.isEmpty()) {
        Dialogs::warning(this, tr("No Output Directory"),
            tr("Please select an output directory."));
        return;
    }

    // Get format and quality
    Core::AudioFormat format = static_cast<Core::AudioFormat>(
        m_formatCombo->currentData().toInt());
    int quality = m_qualityCombo->currentData().toInt();

    // Start rip
    setRippingState(true);
    emit ripStarted();

    m_engine->ripTracks(m_currentDevice,
                         outputDir,
                         format,
                         quality,
                         m_patternEdit->text(),
                         m_model->selectedTrackNumbers(),
                         m_embedCoverCheck->isChecked(),
                         m_saveCoverCheck->isChecked(),
                         m_albumEdit->text(),
                         m_artistEdit->text(),
                         m_yearSpin->value(),
                         m_genreEdit->text(),
                         m_releaseId,
                         m_model->allTrackTitles(),
                         m_model->allTrackArtists());
}

void CdRipperPage::cancelRip() {
    m_cancelButton->setEnabled(false);
    m_cancelButton->setText(tr("Cancelling..."));
    m_statusLabel->setText(tr("Cancelling..."));
    m_engine->cancel();
}

void CdRipperPage::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);

    // Header
    auto* headerLabel = new QLabel(tr("Rip Audio CD"), this);
    QFont headerFont = headerLabel->font();
    headerFont.setPointSize(headerFont.pointSize() + 2);
    headerFont.setBold(true);
    headerLabel->setFont(headerFont);
    mainLayout->addWidget(headerLabel);

    // Metadata section
    auto* metaGroup = new QGroupBox(tr("Album Information"), this);
    auto* metaLayout = new QGridLayout(metaGroup);

    // Cover art
    m_coverLabel = new QLabel(this);
    m_coverLabel->setFixedSize(120, 120);
    m_coverLabel->setAlignment(Qt::AlignCenter);
    m_coverLabel->setFrameStyle(QFrame::Box | QFrame::Sunken);
    m_coverLabel->setText(tr("No Cover"));
    m_coverLabel->setStyleSheet("background-color: palette(base); color: palette(placeholderText);");
    metaLayout->addWidget(m_coverLabel, 0, 0, 4, 1);

    metaLayout->addWidget(new QLabel(tr("Album:"), this), 0, 1);
    m_albumEdit = new QLineEdit(this);
    metaLayout->addWidget(m_albumEdit, 0, 2, 1, 3);

    metaLayout->addWidget(new QLabel(tr("Artist:"), this), 1, 1);
    m_artistEdit = new QLineEdit(this);
    metaLayout->addWidget(m_artistEdit, 1, 2, 1, 3);

    metaLayout->addWidget(new QLabel(tr("Year:"), this), 2, 1);
    m_yearSpin = new QSpinBox(this);
    m_yearSpin->setRange(0, 2100);
    m_yearSpin->setSpecialValueText(tr("Unknown"));
    metaLayout->addWidget(m_yearSpin, 2, 2);

    metaLayout->addWidget(new QLabel(tr("Genre:"), this), 2, 3);
    m_genreEdit = new QLineEdit(this);
    metaLayout->addWidget(m_genreEdit, 2, 4);

    metaLayout->setColumnStretch(2, 1);
    metaLayout->setColumnStretch(4, 1);
    mainLayout->addWidget(metaGroup);

    // Track list
    m_trackView = new QTableView(this);
    m_trackView->setModel(m_model);
    m_trackView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_trackView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_trackView->setAlternatingRowColors(true);
    m_trackView->horizontalHeader()->setStretchLastSection(false);
    m_trackView->horizontalHeader()->setSectionResizeMode(Models::CdTrackModel::ColumnCheck, QHeaderView::Fixed);
    m_trackView->horizontalHeader()->setSectionResizeMode(Models::CdTrackModel::ColumnNumber, QHeaderView::Fixed);
    m_trackView->horizontalHeader()->setSectionResizeMode(Models::CdTrackModel::ColumnTitle, QHeaderView::Stretch);
    m_trackView->horizontalHeader()->setSectionResizeMode(Models::CdTrackModel::ColumnArtist, QHeaderView::Stretch);
    m_trackView->horizontalHeader()->setSectionResizeMode(Models::CdTrackModel::ColumnDuration, QHeaderView::Fixed);
    m_trackView->setColumnWidth(Models::CdTrackModel::ColumnCheck, 30);
    m_trackView->setColumnWidth(Models::CdTrackModel::ColumnNumber, 40);
    m_trackView->setColumnWidth(Models::CdTrackModel::ColumnDuration, 70);
    m_trackView->verticalHeader()->setVisible(false);
    mainLayout->addWidget(m_trackView, 1);

    // Selection controls
    auto* selectionLayout = new QHBoxLayout();
    m_selectAllButton = new QPushButton(tr("Select All"), this);
    selectionLayout->addWidget(m_selectAllButton);
    m_deselectAllButton = new QPushButton(tr("Deselect All"), this);
    selectionLayout->addWidget(m_deselectAllButton);
    selectionLayout->addStretch();
    m_selectionLabel = new QLabel(this);
    selectionLayout->addWidget(m_selectionLabel);
    mainLayout->addLayout(selectionLayout);

    // Output options
    auto* outputGroup = new QGroupBox(tr("Output Settings"), this);
    auto* outputLayout = new QGridLayout(outputGroup);

    outputLayout->addWidget(new QLabel(tr("Format:"), this), 0, 0);
    m_formatCombo = new QComboBox(this);
    m_formatCombo->addItem("FLAC", static_cast<int>(Core::AudioFormat::FLAC));
    m_formatCombo->addItem("MP3", static_cast<int>(Core::AudioFormat::MP3));
    m_formatCombo->addItem("OGG Vorbis", static_cast<int>(Core::AudioFormat::OGG));
    m_formatCombo->addItem("AAC (M4A)", static_cast<int>(Core::AudioFormat::AAC));
    m_formatCombo->addItem("WAV", static_cast<int>(Core::AudioFormat::WAV));
    outputLayout->addWidget(m_formatCombo, 0, 1);

    outputLayout->addWidget(new QLabel(tr("Quality:"), this), 0, 2);
    m_qualityCombo = new QComboBox(this);
    outputLayout->addWidget(m_qualityCombo, 0, 3);

    outputLayout->addWidget(new QLabel(tr("Output:"), this), 1, 0);
    m_outputEdit = new QLineEdit(this);
    m_outputEdit->setPlaceholderText(tr("Select output directory..."));
    outputLayout->addWidget(m_outputEdit, 1, 1, 1, 2);
    m_browseButton = new QPushButton(tr("Browse..."), this);
    outputLayout->addWidget(m_browseButton, 1, 3);

    outputLayout->addWidget(new QLabel(tr("Naming:"), this), 2, 0);
    m_patternEdit = new QLineEdit(this);
    m_patternEdit->setText(Core::FileNamingPattern::DEFAULT_PATTERN);
    outputLayout->addWidget(m_patternEdit, 2, 1, 1, 2);
    m_presetCombo = new QComboBox(this);
    m_presetCombo->addItem(tr("Artist - Album/Track"), Core::FileNamingPattern::PRESET_ARTIST_ALBUM);
    m_presetCombo->addItem(tr("Artist/Album/Track"), Core::FileNamingPattern::PRESET_FOLDER_STRUCT);
    m_presetCombo->addItem(tr("Track - Artist"), Core::FileNamingPattern::PRESET_TRACK_FIRST);
    m_presetCombo->addItem(tr("Flat (no folders)"), Core::FileNamingPattern::PRESET_FLAT);
    m_presetCombo->addItem(tr("With Year"), Core::FileNamingPattern::PRESET_FULL);
    outputLayout->addWidget(m_presetCombo, 2, 3);

    outputLayout->addWidget(new QLabel(tr("Preview:"), this), 3, 0);
    m_previewLabel = new QLabel(this);
    m_previewLabel->setStyleSheet("color: palette(placeholderText); font-style: italic;");
    outputLayout->addWidget(m_previewLabel, 3, 1, 1, 3);

    outputLayout->setColumnStretch(1, 2);
    outputLayout->setColumnStretch(2, 1);
    mainLayout->addWidget(outputGroup);

    // Options row
    auto* optionsLayout = new QHBoxLayout();
    m_embedCoverCheck = new QCheckBox(tr("Embed cover art"), this);
    m_embedCoverCheck->setChecked(true);
    optionsLayout->addWidget(m_embedCoverCheck);
    m_saveCoverCheck = new QCheckBox(tr("Save cover.jpg"), this);
    m_saveCoverCheck->setChecked(true);
    optionsLayout->addWidget(m_saveCoverCheck);
    m_ejectCheck = new QCheckBox(tr("Eject when done"), this);
    optionsLayout->addWidget(m_ejectCheck);
    optionsLayout->addStretch();
    mainLayout->addLayout(optionsLayout);

    // Progress section (hidden initially)
    m_progressWidget = new QWidget(this);
    auto* progressLayout = new QVBoxLayout(m_progressWidget);
    progressLayout->setContentsMargins(0, 0, 0, 0);
    auto* progressBarLayout = new QHBoxLayout();
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    progressBarLayout->addWidget(m_progressBar, 1);
    m_cancelButton = new QPushButton(tr("Cancel"), this);
    progressBarLayout->addWidget(m_cancelButton);
    progressLayout->addLayout(progressBarLayout);
    m_statusLabel = new QLabel(tr("Ready"), this);
    progressLayout->addWidget(m_statusLabel);
    m_progressWidget->setVisible(false);
    mainLayout->addWidget(m_progressWidget);

    // Button row
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    m_refreshButton = new QPushButton(tr("Refresh"), this);
    buttonLayout->addWidget(m_refreshButton);
    m_ripButton = new QPushButton(tr("Rip"), this);
    m_ripButton->setDefault(true);
    buttonLayout->addWidget(m_ripButton);
    mainLayout->addLayout(buttonLayout);

    // Set default output directory
    QString musicDir = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    if (!musicDir.isEmpty()) {
        m_outputEdit->setText(musicDir);
    }

    // Initialize quality options
    onFormatChanged(0);
    updatePreview();
    updateSelectionInfo();
}

void CdRipperPage::connectSignals() {
    // Engine signals
    connect(m_engine, &Engine::QtRipEngine::scanComplete,
            this, &CdRipperPage::onScanComplete);
    connect(m_engine, &Engine::QtRipEngine::metadataComplete,
            this, &CdRipperPage::onMetadataComplete);
    connect(m_engine, &Engine::QtRipEngine::coverArtFetched,
            this, &CdRipperPage::onCoverArtFetched);
    connect(m_engine, &Engine::QtRipEngine::ripProgress,
            this, &CdRipperPage::onRipProgress);
    connect(m_engine, &Engine::QtRipEngine::ripComplete,
            this, &CdRipperPage::onRipComplete);

    // Model signals
    connect(m_model, &Models::CdTrackModel::selectionChanged,
            this, &CdRipperPage::updateSelectionInfo);

    // UI signals
    connect(m_formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CdRipperPage::onFormatChanged);
    connect(m_qualityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() { saveSettings(); });
    connect(m_patternEdit, &QLineEdit::textChanged,
            this, &CdRipperPage::onPatternChanged);
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                m_patternEdit->setText(m_presetCombo->itemData(index).toString());
            });
    connect(m_browseButton, &QPushButton::clicked,
            this, &CdRipperPage::browseOutputDir);
    connect(m_selectAllButton, &QPushButton::clicked,
            this, &CdRipperPage::selectAllTracks);
    connect(m_deselectAllButton, &QPushButton::clicked,
            this, &CdRipperPage::deselectAllTracks);
    connect(m_refreshButton, &QPushButton::clicked,
            this, &CdRipperPage::refresh);
    connect(m_ripButton, &QPushButton::clicked,
            this, &CdRipperPage::startRip);
    connect(m_cancelButton, &QPushButton::clicked,
            this, &CdRipperPage::cancelRip);

    // Update preview when metadata changes
    connect(m_albumEdit, &QLineEdit::textChanged, this, &CdRipperPage::updatePreview);
    connect(m_artistEdit, &QLineEdit::textChanged, this, &CdRipperPage::updatePreview);
}

void CdRipperPage::onScanComplete(bool success, const QString& discId, int trackCount,
                                   const QList<int>& trackNumbers, const QList<int>& durations,
                                   const QStringList& titles, const QStringList& artists,
                                   const QString& albumTitle, const QString& albumArtist) {
    if (!success) {
        m_hasDisc = false;
        m_statusLabel->setText(tr("Scan failed"));
        return;
    }

    m_hasDisc = true;
    m_discId = discId;

    m_model->setTracks(trackNumbers, durations, titles, artists);

    if (!albumTitle.isEmpty()) {
        m_albumEdit->setText(albumTitle);
    }
    if (!albumArtist.isEmpty()) {
        m_artistEdit->setText(albumArtist);
    }

    m_statusLabel->setText(tr("Found %1 tracks. Looking up metadata...").arg(trackCount));

    // Look up metadata from MusicBrainz
    if (!discId.isEmpty()) {
        m_engine->lookupMetadata(m_currentDevice, discId);
    }

    updateSelectionInfo();
    updatePreview();
}

void CdRipperPage::onMetadataComplete(bool success, const QString& albumTitle,
                                       const QString& albumArtist, int year,
                                       const QString& genre, const QString& releaseId,
                                       const QStringList& trackTitles,
                                       const QStringList& trackArtists) {
    qDebug() << "CdRipperPage::onMetadataComplete - success:" << success
             << "album:" << albumTitle << "artist:" << albumArtist
             << "tracks:" << trackTitles.size();

    if (!success) {
        m_statusLabel->setText(tr("Metadata not found. You can enter information manually."));
        return;
    }

    m_releaseId = releaseId;

    if (!albumTitle.isEmpty()) {
        m_albumEdit->setText(albumTitle);
    }
    if (!albumArtist.isEmpty()) {
        m_artistEdit->setText(albumArtist);
    }
    if (year > 0) {
        m_yearSpin->setValue(year);
    }
    if (!genre.isEmpty()) {
        m_genreEdit->setText(genre);
    }

    m_model->updateMetadata(trackTitles, trackArtists);

    m_statusLabel->setText(tr("Metadata loaded. Fetching cover art..."));

    // Fetch cover art
    if (!releaseId.isEmpty()) {
        m_engine->fetchCoverArt(releaseId);
    } else {
        m_statusLabel->setText(tr("Ready to rip"));
    }

    updatePreview();
}

void CdRipperPage::onCoverArtFetched(bool success, const QByteArray& imageData,
                                      const QString& mimeType) {
    if (success && !imageData.isEmpty()) {
        m_coverArtData = imageData;
        m_coverArtMimeType = mimeType;

        QPixmap pixmap;
        if (pixmap.loadFromData(imageData)) {
            m_coverLabel->setPixmap(pixmap.scaled(m_coverLabel->size(),
                                                   Qt::KeepAspectRatio,
                                                   Qt::SmoothTransformation));
        }
        m_statusLabel->setText(tr("Ready to rip"));
    } else {
        m_statusLabel->setText(tr("Cover art not available. Ready to rip."));
    }
}

void CdRipperPage::onRipProgress(double overallPercent, int currentTrack, int totalTracks,
                                  double trackPercent, const QString& operation) {
    Q_UNUSED(trackPercent);
    m_progressBar->setValue(static_cast<int>(overallPercent));
    m_statusLabel->setText(tr("Track %1/%2: %3")
        .arg(currentTrack)
        .arg(totalTracks)
        .arg(operation));
}

void CdRipperPage::onRipComplete(bool success, int tracksRipped, int tracksFailed,
                                  const QString& message) {
    setRippingState(false);

    if (success) {
        m_statusLabel->setText(tr("Complete! Ripped %1 track(s).").arg(tracksRipped));
        Dialogs::information(this, tr("Rip Complete"),
            tr("Successfully ripped %1 track(s) to:\n%2")
                .arg(tracksRipped)
                .arg(m_outputEdit->text()));
    } else {
        m_statusLabel->setText(tr("Rip failed: %1").arg(message));
        if (tracksFailed > 0) {
            Dialogs::warning(this, tr("Rip Failed"),
                tr("Ripped %1 track(s), %2 failed.\n\n%3")
                    .arg(tracksRipped)
                    .arg(tracksFailed)
                    .arg(message));
        } else {
            Dialogs::critical(this, tr("Rip Failed"), message);
        }
    }

    emit ripComplete(success, message);
}

void CdRipperPage::onFormatChanged(int index) {
    Q_UNUSED(index);

    Core::AudioFormat format = static_cast<Core::AudioFormat>(
        m_formatCombo->currentData().toInt());

    m_qualityCombo->clear();

    switch (format) {
        case Core::AudioFormat::FLAC:
            m_qualityCombo->addItem(tr("0 (Fastest)"), 0);
            m_qualityCombo->addItem(tr("5 (Default)"), 5);
            m_qualityCombo->addItem(tr("8 (Best)"), 8);
            m_qualityCombo->setCurrentIndex(1);
            break;

        case Core::AudioFormat::MP3:
            m_qualityCombo->addItem("128 kbps", 128);
            m_qualityCombo->addItem("192 kbps", 192);
            m_qualityCombo->addItem("256 kbps", 256);
            m_qualityCombo->addItem("320 kbps", 320);
            m_qualityCombo->setCurrentIndex(3);
            break;

        case Core::AudioFormat::OGG:
            m_qualityCombo->addItem(tr("Quality 3 (Low)"), 3);
            m_qualityCombo->addItem(tr("Quality 6 (Default)"), 6);
            m_qualityCombo->addItem(tr("Quality 10 (Best)"), 10);
            m_qualityCombo->setCurrentIndex(1);
            break;

        case Core::AudioFormat::AAC:
            m_qualityCombo->addItem("128 kbps", 128);
            m_qualityCombo->addItem("192 kbps", 192);
            m_qualityCombo->addItem("256 kbps", 256);
            m_qualityCombo->addItem("320 kbps", 320);
            m_qualityCombo->setCurrentIndex(2);
            break;

        case Core::AudioFormat::WAV:
            m_qualityCombo->addItem(tr("Uncompressed"), 0);
            break;
    }

    updatePreview();
}

void CdRipperPage::onPatternChanged() {
    updatePreview();
    saveSettings();
}

void CdRipperPage::browseOutputDir() {
    QString dir = QFileDialog::getExistingDirectory(this,
        tr("Select Output Directory"),
        m_outputEdit->text());

    if (!dir.isEmpty()) {
        m_outputEdit->setText(dir);
        saveSettings();
    }
}

void CdRipperPage::updateSelectionInfo() {
    int selected = m_model->selectedCount();
    int total = m_model->trackCount();
    QString duration = m_model->selectedDurationString();

    m_selectionLabel->setText(tr("%1 of %2 tracks selected (%3)")
        .arg(selected)
        .arg(total)
        .arg(duration));
}

void CdRipperPage::selectAllTracks() {
    m_model->selectAll();
}

void CdRipperPage::deselectAllTracks() {
    m_model->deselectAll();
}

void CdRipperPage::updatePreview() {
    Core::TrackNamingInfo info;
    info.albumArtist = m_artistEdit->text().isEmpty() ? "Artist" : m_artistEdit->text().toStdString();
    info.albumTitle = m_albumEdit->text().isEmpty() ? "Album" : m_albumEdit->text().toStdString();
    info.trackTitle = "Track Title";
    info.trackNumber = 1;
    info.totalTracks = m_model->trackCount() > 0 ? m_model->trackCount() : 12;
    info.year = m_yearSpin->value();

    Core::AudioFormat format = static_cast<Core::AudioFormat>(
        m_formatCombo->currentData().toInt());
    std::string extension = Core::AudioOutputEncoder::formatExtension(format);

    Core::FileNamingPattern pattern(m_patternEdit->text().toStdString());
    std::string preview = pattern.preview(info, extension);

    m_previewLabel->setText(QString::fromStdString(preview));
}

void CdRipperPage::setRippingState(bool ripping) {
    m_isRipping = ripping;
    m_progressWidget->setVisible(ripping);
    m_ripButton->setEnabled(!ripping);
    m_refreshButton->setEnabled(!ripping);
    m_formatCombo->setEnabled(!ripping);
    m_qualityCombo->setEnabled(!ripping);
    m_outputEdit->setEnabled(!ripping);
    m_browseButton->setEnabled(!ripping);
    m_patternEdit->setEnabled(!ripping);
    m_presetCombo->setEnabled(!ripping);

    if (ripping) {
        m_progressBar->setValue(0);
        m_cancelButton->setEnabled(true);
        m_cancelButton->setText(tr("Cancel"));
    }
}

void CdRipperPage::loadSettings() {
    QSettings settings("Burner", "Burner");
    settings.beginGroup("CdRipper");

    QString outputDir = settings.value("outputDir").toString();
    if (!outputDir.isEmpty()) {
        m_outputEdit->setText(outputDir);
    }

    QString pattern = settings.value("pattern").toString();
    if (!pattern.isEmpty()) {
        m_patternEdit->setText(pattern);
    }

    int formatIndex = settings.value("formatIndex", 0).toInt();
    m_formatCombo->setCurrentIndex(formatIndex);

    m_embedCoverCheck->setChecked(settings.value("embedCover", true).toBool());
    m_saveCoverCheck->setChecked(settings.value("saveCover", true).toBool());
    m_ejectCheck->setChecked(settings.value("ejectAfter", false).toBool());

    settings.endGroup();
}

void CdRipperPage::saveSettings() {
    QSettings settings("Burner", "Burner");
    settings.beginGroup("CdRipper");

    settings.setValue("outputDir", m_outputEdit->text());
    settings.setValue("pattern", m_patternEdit->text());
    settings.setValue("formatIndex", m_formatCombo->currentIndex());
    settings.setValue("embedCover", m_embedCoverCheck->isChecked());
    settings.setValue("saveCover", m_saveCoverCheck->isChecked());
    settings.setValue("ejectAfter", m_ejectCheck->isChecked());

    settings.endGroup();
}

} // namespace Burner::UI
