#include "BurnOptionsDialog.hpp"

#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QLabel>
#include <QFont>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QGroupBox>

namespace Burner::UI {

BurnOptionsDialog::BurnOptionsDialog(QWidget* parent)
    : ChromeDialog(parent, tr("Burn Options")) {
    setupUi();
}

BurnOptionsDialog::~BurnOptionsDialog() = default;

BurnOptionsDialog::Options BurnOptionsDialog::options() const {
    Options opts;
    opts.volumeLabel = m_volumeLabelEdit->text();
    opts.speed = m_speedCombo->currentData().toInt();
    opts.simulate = m_simulateCheck->isChecked();
    opts.verify = m_verifyCheck->isChecked();
    opts.ejectAfter = m_ejectCheck->isChecked();
    opts.burnProof = m_burnProofCheck->isChecked();
    opts.closeDisc = !m_leaveOpenCheck->isChecked();  // Inverted: leave open = don't close
    opts.blankBeforeBurn = m_blankFirstCheck->isChecked();
    return opts;
}

void BurnOptionsDialog::setDiscType(const QString& discType) {
    m_discTypeLabel->setText(discType);
}

void BurnOptionsDialog::setSummary(const QString& summary) {
    m_summaryLabel->setText(summary);
}

void BurnOptionsDialog::setVolumeLabel(const QString& label) {
    m_volumeLabelEdit->setText(label);
}

void BurnOptionsDialog::setVolumeLabelVisible(bool visible) {
    m_volumeLabelWidget->setVisible(visible);
}

void BurnOptionsDialog::setMultiSessionInfo(int sessions) {
    if (sessions > 0) {
        m_multiSessionLabel->setText(
            tr("Appending to existing disc (Session %1)").arg(sessions + 1));
        m_multiSessionLabel->setVisible(true);
        // Default to leave open for multi-session discs
        m_leaveOpenCheck->setChecked(true);
        m_leaveOpenCheck->setToolTip(
            tr("Recommended: Keep disc open for future sessions.\n"
               "Uncheck to finalize the disc (no more data can be added)."));
    } else {
        m_multiSessionLabel->setVisible(false);
    }
}

void BurnOptionsDialog::setWriteSpeeds(const QList<int>& speeds) {
    m_speedCombo->clear();
    m_speedCombo->addItem(tr("Maximum"), 0);

    for (int speed : speeds) {
        // Convert KB/s to display speed (1x CD = 150 KB/s, 1x DVD = 1385 KB/s)
        QString label;
        if (speed < 2000) {
            int cdSpeed = speed / 150;
            label = QString("%1x CD (%2 KB/s)").arg(cdSpeed).arg(speed);
        } else {
            int dvdSpeed = speed / 1385;
            label = QString("%1x DVD (%2 KB/s)").arg(dvdSpeed).arg(speed);
        }
        m_speedCombo->addItem(label, speed);
    }
}

void BurnOptionsDialog::setupUi() {
    auto* mainLayout = body();

    // Summary group - shows what will be burned
    m_summaryGroup = new QGroupBox(tr("Burn Summary"), this);
    auto* summaryLayout = new QVBoxLayout(m_summaryGroup);

    m_discTypeLabel = new QLabel(this);
    QFont typeFont = m_discTypeLabel->font();
    typeFont.setBold(true);
    typeFont.setPointSize(typeFont.pointSize() + 1);
    m_discTypeLabel->setFont(typeFont);
    summaryLayout->addWidget(m_discTypeLabel);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setWordWrap(true);
    summaryLayout->addWidget(m_summaryLabel);

    mainLayout->addWidget(m_summaryGroup);

    // Volume label (in a container so we can hide it for Audio CD, etc.)
    m_volumeLabelWidget = new QWidget(this);
    auto* volumeLayout = new QHBoxLayout(m_volumeLabelWidget);
    volumeLayout->setContentsMargins(0, 0, 0, 0);
    volumeLayout->addWidget(new QLabel(tr("Volume Label:"), this));
    m_volumeLabelEdit = new QLineEdit(this);
    m_volumeLabelEdit->setPlaceholderText(tr("DISC"));
    m_volumeLabelEdit->setMaxLength(32);
    volumeLayout->addWidget(m_volumeLabelEdit, 1);
    mainLayout->addWidget(m_volumeLabelWidget);

    // Speed selection
    auto* formLayout = new QFormLayout();
    m_speedCombo = new QComboBox(this);
    m_speedCombo->addItem(tr("Maximum"), 0);
    formLayout->addRow(tr("Write Speed:"), m_speedCombo);
    mainLayout->addLayout(formLayout);

    // Options group
    auto* optionsGroup = new QGroupBox(tr("Options"), this);
    auto* optionsLayout = new QVBoxLayout(optionsGroup);

    m_simulateCheck = new QCheckBox(tr("Simulate (dry run - don't actually burn)"), this);
    optionsLayout->addWidget(m_simulateCheck);

    m_verifyCheck = new QCheckBox(tr("Verify disc after burning"), this);
    m_verifyCheck->setChecked(true);
    optionsLayout->addWidget(m_verifyCheck);

    m_ejectCheck = new QCheckBox(tr("Eject disc when finished"), this);
    m_ejectCheck->setChecked(true);
    optionsLayout->addWidget(m_ejectCheck);

    m_burnProofCheck = new QCheckBox(tr("Enable buffer underrun protection"), this);
    m_burnProofCheck->setChecked(true);
    optionsLayout->addWidget(m_burnProofCheck);

    m_multiSessionLabel = new QLabel(this);
    m_multiSessionLabel->setStyleSheet("QLabel { color: #0066cc; font-style: italic; }");
    m_multiSessionLabel->setVisible(false);
    optionsLayout->addWidget(m_multiSessionLabel);

    m_leaveOpenCheck = new QCheckBox(tr("Leave disc open for additional sessions"), this);
    m_leaveOpenCheck->setChecked(false);  // Default: finalize the disc
    m_leaveOpenCheck->setToolTip(tr("If checked, the disc can have more data added later.\n"
                                     "If unchecked (default), the disc will be finalized and closed."));
    optionsLayout->addWidget(m_leaveOpenCheck);

    m_blankFirstCheck = new QCheckBox(tr("Blank rewritable disc before burning"), this);
    m_blankFirstCheck->setChecked(false);
    m_blankFirstCheck->setToolTip(tr("For CD-RW, DVD-RW, DVD+RW, or BD-RE discs:\n"
                                      "Erase existing data before writing new content."));
    optionsLayout->addWidget(m_blankFirstCheck);

    mainLayout->addWidget(optionsGroup);

    // Buttons
    auto* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        this
    );
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    setMinimumWidth(350);
}

} // namespace Burner::UI
