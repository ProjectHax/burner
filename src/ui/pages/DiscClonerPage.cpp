#include "DiscClonerPage.hpp"
#include "../../models/DriveListModel.hpp"

#include <QLineEdit>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QGroupBox>
#include <QRadioButton>
#include <QCheckBox>

namespace Burner::UI {

DiscClonerPage::DiscClonerPage(QWidget* parent)
    : QWidget(parent) {
    setupUi();
}

DiscClonerPage::~DiscClonerPage() = default;

void DiscClonerPage::setDriveModel(Models::DriveListModel* model) {
    m_driveModel = model;
    m_sourceDriveCombo->setModel(model);

    connect(m_sourceDriveCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DiscClonerPage::updateCloneButton);
    connect(m_sourceDriveCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DiscClonerPage::updateBlankButton);
    connect(m_sourceDriveCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DiscClonerPage::updateIntegrityButton);
}

QString DiscClonerPage::sourceDevice() const {
    if (!m_driveModel || m_sourceDriveCombo->currentIndex() < 0) {
        return {};
    }
    return m_driveModel->devicePathAt(m_sourceDriveCombo->currentIndex());
}

QString DiscClonerPage::imagePath() const {
    return m_imagePathEdit->text();
}

DiscClonerPage::CloneFormat DiscClonerPage::cloneFormat() const {
    return m_formatCombo && m_formatCombo->currentIndex() == 1 ? BinCue : ISO;
}

bool DiscClonerPage::isFullBlank() const {
    return m_fullBlankRadio && m_fullBlankRadio->isChecked();
}

bool DiscClonerPage::stopOnFirstError() const {
    return m_stopOnErrorCheck && m_stopOnErrorCheck->isChecked();
}

void DiscClonerPage::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);

    // ========== Clone Disc to Image Section ==========
    auto* cloneGroup = new QGroupBox(tr("Clone Disc to Image"), this);
    auto* cloneLayout = new QVBoxLayout(cloneGroup);

    // Description
    m_infoLabel = new QLabel(
        tr("Create an exact copy of a disc as an image file.\n"
           "ISO format stores data sectors. BIN/CUE preserves raw sectors and audio tracks.\n"
           "The disc must be unmounted before cloning."),
        cloneGroup);
    m_infoLabel->setWordWrap(true);
    cloneLayout->addWidget(m_infoLabel);

    // Form layout for inputs
    auto* formLayout = new QFormLayout();
    formLayout->setSpacing(12);

    // Source drive
    m_sourceDriveCombo = new QComboBox(cloneGroup);
    m_sourceDriveCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    formLayout->addRow(tr("Source Drive:"), m_sourceDriveCombo);

    // Output format
    m_formatCombo = new QComboBox(cloneGroup);
    m_formatCombo->addItem(tr("ISO Image (.iso)"));
    m_formatCombo->addItem(tr("BIN/CUE Image (.bin/.cue)"));
    m_formatCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    formLayout->addRow(tr("Format:"), m_formatCombo);

    // Output file path
    auto* pathLayout = new QHBoxLayout();
    m_imagePathEdit = new QLineEdit(cloneGroup);
    m_imagePathEdit->setPlaceholderText(tr("Select output file location..."));
    pathLayout->addWidget(m_imagePathEdit, 1);
    m_browseButton = new QPushButton(tr("Browse..."), cloneGroup);
    pathLayout->addWidget(m_browseButton);
    formLayout->addRow(tr("Output File:"), pathLayout);

    cloneLayout->addLayout(formLayout);

    // Clone button
    auto* cloneButtonLayout = new QHBoxLayout();
    cloneButtonLayout->addStretch();
    m_cloneButton = new QPushButton(tr("Clone Disc"), cloneGroup);
    m_cloneButton->setMinimumWidth(120);
    m_cloneButton->setEnabled(false);
    cloneButtonLayout->addWidget(m_cloneButton);
    cloneButtonLayout->addStretch();
    cloneLayout->addLayout(cloneButtonLayout);

    mainLayout->addWidget(cloneGroup);

    // ========== Blank Rewritable Disc Section ==========
    m_blankGroup = new QGroupBox(tr("Blank Rewritable Disc"), this);
    auto* blankLayout = new QVBoxLayout(m_blankGroup);

    auto* blankDescription = new QLabel(
        tr("Erase a CD-RW, DVD-RW, DVD+RW, or BD-RE disc to prepare it for new data."),
        m_blankGroup);
    blankDescription->setWordWrap(true);
    blankLayout->addWidget(blankDescription);

    // Blank type radio buttons
    m_quickBlankRadio = new QRadioButton(tr("Quick blank (fast, erases table of contents only)"), m_blankGroup);
    m_fullBlankRadio = new QRadioButton(tr("Full blank (slow, erases entire disc)"), m_blankGroup);
    m_quickBlankRadio->setChecked(true);
    blankLayout->addWidget(m_quickBlankRadio);
    blankLayout->addWidget(m_fullBlankRadio);

    // Blank button
    auto* blankButtonLayout = new QHBoxLayout();
    blankButtonLayout->addStretch();
    m_blankButton = new QPushButton(tr("Blank Disc"), m_blankGroup);
    m_blankButton->setMinimumWidth(120);
    m_blankButton->setEnabled(false);
    blankButtonLayout->addWidget(m_blankButton);
    blankButtonLayout->addStretch();
    blankLayout->addLayout(blankButtonLayout);

    mainLayout->addWidget(m_blankGroup);

    // ========== Check Disc Integrity Section ==========
    m_integrityGroup = new QGroupBox(tr("Check Disc Integrity"), this);
    auto* integrityLayout = new QVBoxLayout(m_integrityGroup);

    auto* integrityDescription = new QLabel(
        tr("Verify that all sectors on the disc are readable.\n"
           "This can help detect scratches or degraded media before data loss occurs."),
        m_integrityGroup);
    integrityDescription->setWordWrap(true);
    integrityLayout->addWidget(integrityDescription);

    // Stop on first error checkbox
    m_stopOnErrorCheck = new QCheckBox(tr("Stop on first error"), m_integrityGroup);
    integrityLayout->addWidget(m_stopOnErrorCheck);

    // Integrity button
    auto* integrityButtonLayout = new QHBoxLayout();
    integrityButtonLayout->addStretch();
    m_integrityButton = new QPushButton(tr("Check Integrity"), m_integrityGroup);
    m_integrityButton->setMinimumWidth(120);
    m_integrityButton->setEnabled(false);
    integrityButtonLayout->addWidget(m_integrityButton);
    integrityButtonLayout->addStretch();
    integrityLayout->addLayout(integrityButtonLayout);

    mainLayout->addWidget(m_integrityGroup);

    mainLayout->addStretch();

    // Connections
    connect(m_browseButton, &QPushButton::clicked, this, &DiscClonerPage::browseOutputFile);
    connect(m_imagePathEdit, &QLineEdit::textChanged, this, &DiscClonerPage::updateCloneButton);
    connect(m_cloneButton, &QPushButton::clicked, this, &DiscClonerPage::cloneRequested);
    connect(m_formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DiscClonerPage::onFormatChanged);
    connect(m_blankButton, &QPushButton::clicked, this, [this]() {
        emit blankRequested(isFullBlank());
    });
    connect(m_integrityButton, &QPushButton::clicked, this, [this]() {
        emit integrityCheckRequested(stopOnFirstError());
    });
}

void DiscClonerPage::browseOutputFile() {
    QString filter;
    QString defaultSuffix;

    if (cloneFormat() == BinCue) {
        filter = tr("BIN Images (*.bin);;All Files (*)");
        defaultSuffix = QStringLiteral(".bin");
    } else {
        filter = tr("ISO Images (*.iso);;All Files (*)");
        defaultSuffix = QStringLiteral(".iso");
    }

    QString file = QFileDialog::getSaveFileName(
        this,
        tr("Save Disc Image"),
        QString(),
        filter
    );

    if (!file.isEmpty()) {
        if (!file.endsWith(defaultSuffix, Qt::CaseInsensitive)) {
            file += defaultSuffix;
        }
        m_imagePathEdit->setText(file);
    }
}

void DiscClonerPage::onFormatChanged() {
    // Update file extension if a path is already entered
    QString path = m_imagePathEdit->text().trimmed();
    if (path.isEmpty()) return;

    if (cloneFormat() == BinCue) {
        if (path.endsWith(QStringLiteral(".iso"), Qt::CaseInsensitive)) {
            path.chop(4);
            path += QStringLiteral(".bin");
            m_imagePathEdit->setText(path);
        }
    } else {
        if (path.endsWith(QStringLiteral(".bin"), Qt::CaseInsensitive)) {
            path.chop(4);
            path += QStringLiteral(".iso");
            m_imagePathEdit->setText(path);
        }
    }
}

void DiscClonerPage::updateCloneButton() {
    bool hasDevice = !sourceDevice().isEmpty();
    bool hasPath = !m_imagePathEdit->text().trimmed().isEmpty();
    m_cloneButton->setEnabled(hasDevice && hasPath);
}

void DiscClonerPage::updateBlankButton() {
    bool hasDevice = !sourceDevice().isEmpty();
    m_blankButton->setEnabled(hasDevice);
}

void DiscClonerPage::updateIntegrityButton() {
    bool hasDevice = !sourceDevice().isEmpty();
    m_integrityButton->setEnabled(hasDevice);
}

} // namespace Burner::UI
