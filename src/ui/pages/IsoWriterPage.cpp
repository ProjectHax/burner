#include "IsoWriterPage.hpp"
#include "../../core/CueSheet.hpp"
#include "../../engine/ChecksumWorker.hpp"

#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QStyle>
#include <QLocale>
#include <QUrl>
#include <QThread>
#include <QFont>

namespace Burner::UI {

// ISO 9660 Primary Volume Descriptor offset and size
constexpr qint64 ISO_PVD_OFFSET = 0x8000;  // 32KB
constexpr qint64 ISO_SECTOR_SIZE = 2048;

IsoWriterPage::IsoWriterPage(QWidget* parent)
    : QWidget(parent) {
    setAcceptDrops(true);
    setupUi();
}

IsoWriterPage::~IsoWriterPage() {
    cleanupChecksumWorker();
}

QString IsoWriterPage::isoPath() const {
    return m_pathEdit->text();
}

void IsoWriterPage::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);

    // Header
    auto* headerLabel = new QLabel(tr("Burn Disc Image"), this);
    QFont headerFont = headerLabel->font();
    headerFont.setPointSize(headerFont.pointSize() + 2);
    headerFont.setBold(true);
    headerLabel->setFont(headerFont);
    mainLayout->addWidget(headerLabel);

    // Description
    auto* descLabel = new QLabel(tr(
        "Select an ISO, BIN/CUE, or other disc image file to burn to disc. "
        "The disc will be an exact copy of the image."
    ), this);
    descLabel->setWordWrap(true);
    mainLayout->addWidget(descLabel);

    // Drop zone hint
    m_dropHint = new QLabel(tr("Drag and drop an image file here, or use the Browse button below"), this);
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
    m_dropHint->setMinimumHeight(100);
    mainLayout->addWidget(m_dropHint);

    // ISO path row
    auto* pathLayout = new QHBoxLayout();
    pathLayout->addWidget(new QLabel(tr("Image File:"), this));

    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setPlaceholderText(tr("Select or drop a disc image file..."));
    pathLayout->addWidget(m_pathEdit, 1);

    m_browseButton = new QPushButton(tr("Browse..."), this);
    pathLayout->addWidget(m_browseButton);

    m_clearButton = new QPushButton(tr("Clear"), this);
    m_clearButton->setEnabled(false);
    pathLayout->addWidget(m_clearButton);

    mainLayout->addLayout(pathLayout);

    // Image info group
    m_infoGroup = new QGroupBox(tr("Image Information"), this);
    m_infoGroup->setVisible(false);

    auto* infoLayout = new QGridLayout(m_infoGroup);
    infoLayout->setColumnStretch(1, 1);

    int row = 0;

    infoLayout->addWidget(new QLabel(tr("Volume Label:"), this), row, 0);
    m_volumeLabel = new QLabel(this);
    m_volumeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    infoLayout->addWidget(m_volumeLabel, row++, 1);

    infoLayout->addWidget(new QLabel(tr("Size:"), this), row, 0);
    m_sizeLabel = new QLabel(this);
    infoLayout->addWidget(m_sizeLabel, row++, 1);

    infoLayout->addWidget(new QLabel(tr("Type:"), this), row, 0);
    m_typeLabel = new QLabel(this);
    infoLayout->addWidget(m_typeLabel, row++, 1);

    infoLayout->addWidget(new QLabel(tr("Tracks:"), this), row, 0);
    m_trackListLabel = new QLabel(this);
    m_trackListLabel->setWordWrap(true);
    m_trackListLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_trackListLabel->setVisible(false);
    infoLayout->addWidget(m_trackListLabel, row++, 1);

    infoLayout->addWidget(new QLabel(tr("Recommended Disc:"), this), row, 0);
    m_discRecommendation = new QLabel(this);
    m_discRecommendation->setStyleSheet("font-weight: bold;");
    infoLayout->addWidget(m_discRecommendation, row++, 1);

    // Checksums section
    QFont monoFont("monospace");
    monoFont.setStyleHint(QFont::Monospace);

    infoLayout->addWidget(new QLabel(tr("MD5:"), this), row, 0);
    m_md5Label = new QLabel(this);
    m_md5Label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_md5Label->setFont(monoFont);
    infoLayout->addWidget(m_md5Label, row++, 1);

    infoLayout->addWidget(new QLabel(tr("SHA-256:"), this), row, 0);
    m_sha256Label = new QLabel(this);
    m_sha256Label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_sha256Label->setFont(monoFont);
    m_sha256Label->setWordWrap(true);
    infoLayout->addWidget(m_sha256Label, row++, 1);

    m_checksumProgress = new QProgressBar(this);
    m_checksumProgress->setVisible(false);
    m_checksumProgress->setMaximumHeight(16);
    infoLayout->addWidget(m_checksumProgress, row++, 0, 1, 2);

    auto* verifyLayout = new QHBoxLayout();
    m_verifyInput = new QLineEdit(this);
    m_verifyInput->setPlaceholderText(tr("Paste expected checksum to verify..."));
    m_verifyInput->setFont(monoFont);
    verifyLayout->addWidget(m_verifyInput, 1);

    m_verifyButton = new QPushButton(tr("Verify"), this);
    verifyLayout->addWidget(m_verifyButton);

    m_verifyResult = new QLabel(this);
    verifyLayout->addWidget(m_verifyResult);

    infoLayout->addLayout(verifyLayout, row++, 0, 1, 2);

    connect(m_verifyButton, &QPushButton::clicked, this, &IsoWriterPage::verifyChecksum);

    mainLayout->addWidget(m_infoGroup);

    mainLayout->addStretch();

    // Connections
    connect(m_browseButton, &QPushButton::clicked, this, &IsoWriterPage::browseIso);
    connect(m_clearButton, &QPushButton::clicked, this, &IsoWriterPage::clearSelection);
    connect(m_pathEdit, &QLineEdit::textChanged, this, &IsoWriterPage::updateIsoInfo);
}

void IsoWriterPage::browseIso() {
    QString file = QFileDialog::getOpenFileName(
        this,
        tr("Select Disc Image"),
        QString(),
        tr("Disc Images (*.iso *.img *.bin *.cue *.nrg);;CUE Sheets (*.cue);;ISO Images (*.iso *.img);;All Files (*)")
    );

    if (!file.isEmpty()) {
        setIsoPath(file);
    }
}

void IsoWriterPage::setIsoPath(const QString& path) {
    m_pathEdit->setText(path);
}

void IsoWriterPage::clearSelection() {
    m_pathEdit->clear();
    m_infoGroup->setVisible(false);
    m_dropHint->setVisible(true);
    m_isoSize = 0;
    m_isValidIso = false;
    m_isCueImage = false;
    m_clearButton->setEnabled(false);
    cleanupChecksumWorker();
    m_cachedMd5.clear();
    m_cachedSha256.clear();
    m_checksumFilePath.clear();
    emit isoCleared();
}

void IsoWriterPage::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        const auto urls = event->mimeData()->urls();
        for (const QUrl& url : urls) {
            if (url.isLocalFile()) {
                QString path = url.toLocalFile().toLower();
                if (path.endsWith(".iso") || path.endsWith(".img") ||
                    path.endsWith(".bin") || path.endsWith(".cue") ||
                    path.endsWith(".nrg")) {
                    event->acceptProposedAction();
                    m_dropHint->setStyleSheet(
                        "QLabel {"
                        "  border: 2px dashed #4CAF50;"
                        "  border-radius: 8px;"
                        "  padding: 32px;"
                        "  background-color: rgba(76, 175, 80, 0.1);"
                        "  color: #4CAF50;"
                        "}"
                    );
                    return;
                }
            }
        }
    }
    event->ignore();
}

void IsoWriterPage::dropEvent(QDropEvent* event) {
    // Reset drop hint style
    m_dropHint->setStyleSheet(
        "QLabel {"
        "  border: 2px dashed palette(mid);"
        "  border-radius: 8px;"
        "  padding: 32px;"
        "  background-color: palette(alternate-base);"
        "  color: palette(mid);"
        "}"
    );

    const QMimeData* mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        for (const QUrl& url : mimeData->urls()) {
            if (url.isLocalFile()) {
                QString path = url.toLocalFile();
                QString lower = path.toLower();
                if (lower.endsWith(".iso") || lower.endsWith(".img") ||
                    lower.endsWith(".bin") || lower.endsWith(".cue") ||
                    lower.endsWith(".nrg")) {
                    setIsoPath(path);
                    event->acceptProposedAction();
                    return;
                }
            }
        }
    }
}

void IsoWriterPage::updateIsoInfo(const QString& path) {
    m_isCueImage = false;
    m_trackListLabel->setVisible(false);

    if (path.isEmpty()) {
        m_infoGroup->setVisible(false);
        m_dropHint->setVisible(true);
        m_isoSize = 0;
        m_isValidIso = false;
        m_clearButton->setEnabled(false);
        return;
    }

    QFileInfo info(path);
    if (!info.exists()) {
        m_infoGroup->setVisible(false);
        m_dropHint->setVisible(true);
        m_dropHint->setText(tr("File not found: %1").arg(path));
        m_isoSize = 0;
        m_isValidIso = false;
        m_clearButton->setEnabled(false);
        return;
    }

    m_clearButton->setEnabled(true);

    // CUE files get special handling
    if (path.toLower().endsWith(".cue")) {
        updateCueInfo(path);
        return;
    }

    m_isoSize = info.size();

    // Validate and read ISO info
    m_isValidIso = validateIsoFile(path);

    if (m_isValidIso) {
        m_dropHint->setVisible(false);
        m_infoGroup->setVisible(true);

        // Volume label
        QString volumeLabel = readIsoVolumeLabel(path);
        m_volumeLabel->setText(volumeLabel.isEmpty() ? tr("(none)") : volumeLabel);

        // Size
        m_sizeLabel->setText(formatSize(m_isoSize));

        // Determine type
        QString type;
        if (path.toLower().endsWith(".nrg")) {
            type = tr("Nero Image");
        } else if (path.toLower().endsWith(".bin")) {
            type = tr("BIN/CUE Image");
        } else {
            type = tr("ISO 9660 Image");
        }
        m_typeLabel->setText(type);

        // Disc recommendation
        QString recommendation;
        if (m_isoSize <= 700LL * 1024 * 1024) {
            recommendation = tr("CD (700 MB)");
        } else if (m_isoSize <= 4700LL * 1024 * 1024) {
            recommendation = tr("DVD (4.7 GB)");
        } else if (m_isoSize <= 8500LL * 1024 * 1024) {
            recommendation = tr("DVD DL (8.5 GB)");
        } else if (m_isoSize <= 25LL * 1024 * 1024 * 1024) {
            recommendation = tr("BD (25 GB)");
        } else if (m_isoSize <= 50LL * 1024 * 1024 * 1024) {
            recommendation = tr("BD DL (50 GB)");
        } else {
            recommendation = tr("BD XL (100+ GB)");
        }
        m_discRecommendation->setText(recommendation);

        emit isoSelected(path);
        startChecksumCalculation(path);
    } else {
        m_infoGroup->setVisible(false);
        m_dropHint->setVisible(true);
        m_dropHint->setText(tr("Invalid or unsupported image format"));
    }
}

void IsoWriterPage::updateCueInfo(const QString& path) {
    Core::CueSheet cueSheet;
    if (!cueSheet.parse(path.toStdString())) {
        m_infoGroup->setVisible(false);
        m_dropHint->setVisible(true);
        m_dropHint->setText(tr("Failed to parse CUE file: %1")
                           .arg(QString::fromStdString(cueSheet.lastError())));
        m_isoSize = 0;
        m_isValidIso = false;
        m_isCueImage = false;
        return;
    }

    if (!cueSheet.validate()) {
        m_infoGroup->setVisible(false);
        m_dropHint->setVisible(true);
        m_dropHint->setText(tr("CUE validation failed: %1")
                           .arg(QString::fromStdString(cueSheet.lastError())));
        m_isoSize = 0;
        m_isValidIso = false;
        m_isCueImage = false;
        return;
    }

    m_isCueImage = true;
    m_isValidIso = true;
    m_isoSize = static_cast<qint64>(cueSheet.totalDataSize());

    m_dropHint->setVisible(false);
    m_infoGroup->setVisible(true);

    // Title from CUE
    QString title = QString::fromStdString(cueSheet.title());
    m_volumeLabel->setText(title.isEmpty() ? tr("(none)") : title);

    // Size
    m_sizeLabel->setText(formatSize(m_isoSize));

    // Determine type description
    QString typeDesc;
    if (cueSheet.isAudioDisc()) {
        typeDesc = tr("Audio CD (%1 tracks)").arg(cueSheet.trackCount());
    } else if (cueSheet.isMixedMode()) {
        typeDesc = tr("Mixed Mode CD (%1 tracks)").arg(cueSheet.trackCount());
    } else if (cueSheet.isSingleDataTrack()) {
        typeDesc = tr("Data Disc (BIN/CUE)");
    } else {
        typeDesc = tr("BIN/CUE Image (%1 tracks)").arg(cueSheet.trackCount());
    }
    m_typeLabel->setText(typeDesc);

    // Build track listing
    if (cueSheet.trackCount() > 1) {
        QStringList trackDescs;
        for (const auto& track : cueSheet.tracks()) {
            QString trackType;
            switch (track.type) {
                case Core::CueTrackType::Audio:      trackType = tr("Audio"); break;
                case Core::CueTrackType::Mode1_2048: trackType = tr("Data (MODE1/2048)"); break;
                case Core::CueTrackType::Mode1_2352: trackType = tr("Data (MODE1/2352)"); break;
                case Core::CueTrackType::Mode2_2336: trackType = tr("Data (MODE2/2336)"); break;
                case Core::CueTrackType::Mode2_2352: trackType = tr("Data (MODE2/2352)"); break;
                default: trackType = tr("Unknown"); break;
            }
            QString desc = tr("Track %1: %2").arg(track.number).arg(trackType);
            if (!track.title.empty()) {
                desc += " - " + QString::fromStdString(track.title);
            }
            trackDescs.append(desc);
        }
        m_trackListLabel->setText(trackDescs.join("\n"));
        m_trackListLabel->setVisible(true);
    } else {
        m_trackListLabel->setVisible(false);
    }

    // Disc recommendation (same logic as ISO)
    QString recommendation;
    if (m_isoSize <= 700LL * 1024 * 1024) {
        recommendation = tr("CD (700 MB)");
    } else if (m_isoSize <= 4700LL * 1024 * 1024) {
        recommendation = tr("DVD (4.7 GB)");
    } else if (m_isoSize <= 8500LL * 1024 * 1024) {
        recommendation = tr("DVD DL (8.5 GB)");
    } else if (m_isoSize <= 25LL * 1024 * 1024 * 1024) {
        recommendation = tr("BD (25 GB)");
    } else if (m_isoSize <= 50LL * 1024 * 1024 * 1024) {
        recommendation = tr("BD DL (50 GB)");
    } else {
        recommendation = tr("BD XL (100+ GB)");
    }
    m_discRecommendation->setText(recommendation);

    emit isoSelected(path);

    // For CUE images, checksum the BIN data file
    QString binPath = QString::fromStdString(cueSheet.binPath().string());
    startChecksumCalculation(binPath);
}

bool IsoWriterPage::validateIsoFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    // Check file size (minimum for ISO with PVD)
    if (file.size() < ISO_PVD_OFFSET + ISO_SECTOR_SIZE) {
        // Small file - might still be valid, allow it
        return file.size() > 0;
    }

    // Try to read ISO 9660 Primary Volume Descriptor
    if (!file.seek(ISO_PVD_OFFSET)) {
        return false;
    }

    char buffer[8];
    if (file.read(buffer, 8) != 8) {
        return false;
    }

    // Check for ISO 9660 signature: type byte (0x01) + "CD001"
    if (buffer[0] == 0x01 &&
        buffer[1] == 'C' && buffer[2] == 'D' &&
        buffer[3] == '0' && buffer[4] == '0' && buffer[5] == '1') {
        return true;
    }

    // Check for UDF signature at different location
    // BEA01 at sector 16
    file.seek(ISO_PVD_OFFSET);
    if (file.read(buffer, 5) == 5) {
        if (buffer[0] == 'B' && buffer[1] == 'E' && buffer[2] == 'A' &&
            buffer[3] == '0' && buffer[4] == '1') {
            return true;
        }
    }

    // Allow any file with image extension (user knows what they're doing)
    QString lower = path.toLower();
    return lower.endsWith(".iso") || lower.endsWith(".img") ||
           lower.endsWith(".bin") || lower.endsWith(".cue") ||
           lower.endsWith(".nrg");
}

QString IsoWriterPage::readIsoVolumeLabel(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    if (file.size() < ISO_PVD_OFFSET + ISO_SECTOR_SIZE) {
        return {};
    }

    // Seek to Primary Volume Descriptor
    if (!file.seek(ISO_PVD_OFFSET)) {
        return {};
    }

    // Read the PVD
    QByteArray pvd = file.read(ISO_SECTOR_SIZE);
    if (pvd.size() < 190) {
        return {};
    }

    // Verify signature
    if (pvd[0] != 0x01 || pvd[1] != 'C' || pvd[2] != 'D' ||
        pvd[3] != '0' || pvd[4] != '0' || pvd[5] != '1') {
        return {};
    }

    // Volume identifier is at offset 40, 32 bytes, space-padded
    QByteArray volumeId = pvd.mid(40, 32);
    QString label = QString::fromLatin1(volumeId).trimmed();

    return label;
}

QString IsoWriterPage::formatSize(qint64 bytes) const {
    if (bytes >= 1024LL * 1024 * 1024 * 1024) {
        return QString("%1 TB (%2 bytes)")
            .arg(static_cast<double>(bytes) / (1024.0 * 1024 * 1024 * 1024), 0, 'f', 2)
            .arg(QLocale().toString(bytes));
    } else if (bytes >= 1024LL * 1024 * 1024) {
        return QString("%1 GB (%2 bytes)")
            .arg(static_cast<double>(bytes) / (1024.0 * 1024 * 1024), 0, 'f', 2)
            .arg(QLocale().toString(bytes));
    } else if (bytes >= 1024 * 1024) {
        return QString("%1 MB (%2 bytes)")
            .arg(static_cast<double>(bytes) / (1024.0 * 1024), 0, 'f', 1)
            .arg(QLocale().toString(bytes));
    } else if (bytes >= 1024) {
        return QString("%1 KB").arg(bytes / 1024);
    } else {
        return QString("%1 bytes").arg(bytes);
    }
}

void IsoWriterPage::startChecksumCalculation(const QString& path) {
    // Don't recalculate for the same file
    if (path == m_checksumFilePath && !m_cachedMd5.isEmpty()) {
        return;
    }

    cleanupChecksumWorker();

    m_checksumFilePath = path;
    m_cachedMd5.clear();
    m_cachedSha256.clear();
    m_md5Label->setText(tr("Calculating..."));
    m_sha256Label->setText(tr("Calculating..."));
    m_checksumProgress->setVisible(true);
    m_checksumProgress->setValue(0);
    m_verifyResult->clear();

    m_checksumThread = new QThread(this);
    m_checksumWorker = new Engine::ChecksumWorker();
    m_checksumWorker->moveToThread(m_checksumThread);

    connect(m_checksumThread, &QThread::finished,
            m_checksumWorker, &QObject::deleteLater);
    connect(m_checksumWorker, &Engine::ChecksumWorker::progressUpdated,
            this, [this](double percent, quint64, quint64) {
                m_checksumProgress->setValue(static_cast<int>(percent));
            });
    connect(m_checksumWorker, &Engine::ChecksumWorker::checksumReady,
            this, &IsoWriterPage::onChecksumReady);
    connect(m_checksumWorker, &Engine::ChecksumWorker::finished,
            this, &IsoWriterPage::onChecksumFinished);

    m_checksumThread->start();

    QMetaObject::invokeMethod(m_checksumWorker, "calculateChecksums",
                               Qt::QueuedConnection,
                               Q_ARG(QString, path));
}

void IsoWriterPage::cleanupChecksumWorker() {
    if (m_checksumThread) {
        if (m_checksumWorker) {
            m_checksumWorker->cancel();
        }
        m_checksumThread->quit();
        m_checksumThread->wait(3000);
        m_checksumThread->deleteLater();
        m_checksumThread = nullptr;
        m_checksumWorker = nullptr;
    }
}

void IsoWriterPage::onChecksumReady(const QString& algorithm, const QString& hexDigest) {
    if (algorithm == "MD5") {
        m_cachedMd5 = hexDigest;
        m_md5Label->setText(hexDigest);
    } else if (algorithm == "SHA-256") {
        m_cachedSha256 = hexDigest;
        m_sha256Label->setText(hexDigest);
    }
}

void IsoWriterPage::onChecksumFinished(bool success, const QString& message) {
    m_checksumProgress->setVisible(false);
    if (!success) {
        if (m_cachedMd5.isEmpty()) {
            m_md5Label->setText(message);
        }
        if (m_cachedSha256.isEmpty()) {
            m_sha256Label->setText(message);
        }
    }
}

void IsoWriterPage::verifyChecksum() {
    QString input = m_verifyInput->text().trimmed().toLower();
    if (input.isEmpty()) {
        m_verifyResult->setText(tr("Enter a checksum to verify"));
        m_verifyResult->setStyleSheet("");
        return;
    }

    bool match = false;
    QString matchedAlgo;

    if (input.length() == 32 && !m_cachedMd5.isEmpty()) {
        match = (input == m_cachedMd5.toLower());
        matchedAlgo = "MD5";
    } else if (input.length() == 64 && !m_cachedSha256.isEmpty()) {
        match = (input == m_cachedSha256.toLower());
        matchedAlgo = "SHA-256";
    } else {
        m_verifyResult->setText(tr("Unrecognized checksum length"));
        m_verifyResult->setStyleSheet("color: orange;");
        return;
    }

    if (match) {
        m_verifyResult->setText(tr("%1 MATCH").arg(matchedAlgo));
        m_verifyResult->setStyleSheet("color: green; font-weight: bold;");
    } else {
        m_verifyResult->setText(tr("%1 MISMATCH").arg(matchedAlgo));
        m_verifyResult->setStyleSheet("color: red; font-weight: bold;");
    }
}

} // namespace Burner::UI
