#include "ChecksumDialog.hpp"
#include "../../engine/ChecksumWorker.hpp"

#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QClipboard>
#include <QApplication>
#include <QThread>
#include <QFont>

namespace Burner::UI {

ChecksumDialog::ChecksumDialog(QWidget* parent)
    : ChromeDialog(parent, tr("Checksum Calculator")) {
    setMinimumWidth(550);
    setupUi();
}

ChecksumDialog::~ChecksumDialog() {
    cleanupWorker();
}

void ChecksumDialog::setupUi() {
    auto* mainLayout = body();
    mainLayout->setSpacing(12);

    // File selection
    auto* fileGroup = new QGroupBox(tr("File"), this);
    auto* fileLayout = new QHBoxLayout(fileGroup);

    m_filePathEdit = new QLineEdit(this);
    m_filePathEdit->setPlaceholderText(tr("Select a file..."));
    fileLayout->addWidget(m_filePathEdit, 1);

    m_browseButton = new QPushButton(tr("Browse..."), this);
    fileLayout->addWidget(m_browseButton);

    mainLayout->addWidget(fileGroup);

    // Calculate button
    m_calculateButton = new QPushButton(tr("Calculate Checksums"), this);
    mainLayout->addWidget(m_calculateButton);

    // Progress
    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    mainLayout->addWidget(m_progressBar);

    m_statusLabel = new QLabel(this);
    mainLayout->addWidget(m_statusLabel);

    // Results
    auto* resultsGroup = new QGroupBox(tr("Results"), this);
    auto* resultsLayout = new QGridLayout(resultsGroup);

    QFont monoFont("monospace");
    monoFont.setStyleHint(QFont::Monospace);

    resultsLayout->addWidget(new QLabel(tr("MD5:"), this), 0, 0);
    m_md5Result = new QLineEdit(this);
    m_md5Result->setReadOnly(true);
    m_md5Result->setFont(monoFont);
    resultsLayout->addWidget(m_md5Result, 0, 1);

    m_copyMd5Button = new QPushButton(tr("Copy"), this);
    resultsLayout->addWidget(m_copyMd5Button, 0, 2);

    resultsLayout->addWidget(new QLabel(tr("SHA-256:"), this), 1, 0);
    m_sha256Result = new QLineEdit(this);
    m_sha256Result->setReadOnly(true);
    m_sha256Result->setFont(monoFont);
    resultsLayout->addWidget(m_sha256Result, 1, 1);

    m_copySha256Button = new QPushButton(tr("Copy"), this);
    resultsLayout->addWidget(m_copySha256Button, 1, 2);

    resultsLayout->setColumnStretch(1, 1);
    mainLayout->addWidget(resultsGroup);

    // Verify
    auto* verifyGroup = new QGroupBox(tr("Verify"), this);
    auto* verifyLayout = new QHBoxLayout(verifyGroup);

    m_verifyInput = new QLineEdit(this);
    m_verifyInput->setPlaceholderText(tr("Paste expected checksum..."));
    m_verifyInput->setFont(monoFont);
    verifyLayout->addWidget(m_verifyInput, 1);

    m_verifyButton = new QPushButton(tr("Verify"), this);
    verifyLayout->addWidget(m_verifyButton);

    m_verifyResult = new QLabel(this);
    verifyLayout->addWidget(m_verifyResult);

    mainLayout->addWidget(verifyGroup);

    // Close button
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    auto* closeButton = new QPushButton(tr("Close"), this);
    buttonLayout->addWidget(closeButton);
    mainLayout->addLayout(buttonLayout);

    // Connections
    connect(m_browseButton, &QPushButton::clicked, this, &ChecksumDialog::browseFile);
    connect(m_calculateButton, &QPushButton::clicked, this, &ChecksumDialog::calculateChecksums);
    connect(m_verifyButton, &QPushButton::clicked, this, &ChecksumDialog::verifyChecksum);
    connect(m_copyMd5Button, &QPushButton::clicked, this, &ChecksumDialog::copyMd5);
    connect(m_copySha256Button, &QPushButton::clicked, this, &ChecksumDialog::copySha256);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
}

void ChecksumDialog::browseFile() {
    QString file = QFileDialog::getOpenFileName(this, tr("Select File"));
    if (!file.isEmpty()) {
        m_filePathEdit->setText(file);
    }
}

void ChecksumDialog::calculateChecksums() {
    QString path = m_filePathEdit->text().trimmed();
    if (path.isEmpty()) {
        m_statusLabel->setText(tr("Please select a file first."));
        return;
    }

    cleanupWorker();

    m_cachedMd5.clear();
    m_cachedSha256.clear();
    m_md5Result->clear();
    m_sha256Result->clear();
    m_verifyResult->clear();
    m_statusLabel->setText(tr("Calculating..."));
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    m_calculateButton->setEnabled(false);

    m_workerThread = new QThread(this);
    m_worker = new Engine::ChecksumWorker();
    m_worker->moveToThread(m_workerThread);

    connect(m_workerThread, &QThread::finished,
            m_worker, &QObject::deleteLater);
    connect(m_worker, &Engine::ChecksumWorker::progressUpdated,
            this, [this](double percent, quint64, quint64) {
                m_progressBar->setValue(static_cast<int>(percent));
            });
    connect(m_worker, &Engine::ChecksumWorker::checksumReady,
            this, &ChecksumDialog::onChecksumReady);
    connect(m_worker, &Engine::ChecksumWorker::finished,
            this, &ChecksumDialog::onChecksumFinished);

    m_workerThread->start();

    QMetaObject::invokeMethod(m_worker, "calculateChecksums",
                               Qt::QueuedConnection,
                               Q_ARG(QString, path));
}

void ChecksumDialog::verifyChecksum() {
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

void ChecksumDialog::onChecksumReady(const QString& algorithm, const QString& hexDigest) {
    if (algorithm == "MD5") {
        m_cachedMd5 = hexDigest;
        m_md5Result->setText(hexDigest);
    } else if (algorithm == "SHA-256") {
        m_cachedSha256 = hexDigest;
        m_sha256Result->setText(hexDigest);
    }
}

void ChecksumDialog::onChecksumFinished(bool success, const QString& message) {
    m_progressBar->setVisible(false);
    m_calculateButton->setEnabled(true);
    m_statusLabel->setText(success ? tr("Done") : message);
}

void ChecksumDialog::copyMd5() {
    if (!m_cachedMd5.isEmpty()) {
        QApplication::clipboard()->setText(m_cachedMd5);
        m_statusLabel->setText(tr("MD5 copied to clipboard"));
    }
}

void ChecksumDialog::copySha256() {
    if (!m_cachedSha256.isEmpty()) {
        QApplication::clipboard()->setText(m_cachedSha256);
        m_statusLabel->setText(tr("SHA-256 copied to clipboard"));
    }
}

void ChecksumDialog::cleanupWorker() {
    if (m_workerThread) {
        if (m_worker) {
            m_worker->cancel();
        }
        m_workerThread->quit();
        m_workerThread->wait(3000);
        m_workerThread->deleteLater();
        m_workerThread = nullptr;
        m_worker = nullptr;
    }
}

} // namespace Burner::UI
