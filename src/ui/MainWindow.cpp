#include "MainWindow.hpp"
#include "Dialogs.hpp"
#include "ThemeManager.hpp"

#include "widgets/DriveSelector.hpp"
#include "widgets/TitleBar.hpp"
#include "widgets/CapacityBar.hpp"
#include "pages/DataDiscPage.hpp"
#include "pages/AudioCdPage.hpp"
#include "pages/VideoCdPage.hpp"
#include "pages/IsoWriterPage.hpp"
#include "pages/DiscClonerPage.hpp"
#include "pages/CdRipperPage.hpp"
#include "pages/IsoBrowserPage.hpp"
#include "dialogs/BurnOptionsDialog.hpp"
#include "dialogs/BurnProgressDialog.hpp"
#include "dialogs/ChecksumDialog.hpp"

#include "../engine/QtBurnEngine.hpp"
#include "../engine/DriveMonitor.hpp"
#include "../models/DriveListModel.hpp"
#include "../models/FileTreeModel.hpp"
#include "../models/TrackListModel.hpp"
#include "../core/DriveInfo.hpp"
#include "../core/libburnia/BurniaInit.hpp"

#include <QTabWidget>
#include <QTabBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QMenu>
#include <QActionGroup>
#include <QToolBar>
#include <QPushButton>
#include <QStatusBar>
#include <QMessageBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QFileInfo>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QThread>
#include <QTimer>
#include <QMimeData>
#include <QSettings>
#include <QApplication>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileDialog>

namespace Burner::UI {

namespace {
BurnProgressDialog::SpeedMedia mediaToSpeedMedia(Core::MediaType mt) {
    switch (mt) {
        case Core::MediaType::CD_ROM:
        case Core::MediaType::CD_R:
        case Core::MediaType::CD_RW:
            return BurnProgressDialog::CD;
        case Core::MediaType::BD_ROM:
        case Core::MediaType::BD_R:
        case Core::MediaType::BD_RE:
            return BurnProgressDialog::BluRay;
        default:
            return BurnProgressDialog::DVD;
    }
}
} // anonymous namespace

MainWindow::MainWindow(ThemeManager* themeManager, QWidget* parent)
    : QMainWindow(parent)
    , m_themeManager(themeManager) {

    setWindowTitle(tr("Burner - CD/DVD Burning Application"));
    setMinimumSize(900, 700);
    resize(1000, 750);
    setAcceptDrops(true);
    setContextMenuPolicy(Qt::NoContextMenu);  // Disable toolbar right-click menu

    // Decide the title bar before anything can show a dialog
    m_titleBarMode = static_cast<TitleBarMode>(
        qBound(0, QSettings("Burner", "Burner").value("appearance/titleBar", 0).toInt(), 2));
    WindowChrome::setIntegratedTitleBars(decideTitleBar(m_titleBarMode).integrated);

    // Initialize engine
    m_engine = std::make_unique<Engine::QtBurnEngine>(this);

    if (!m_engine->isInitialized()) {
        Dialogs::critical(this, tr("Initialization Error"),
            tr("Failed to initialize burning engine:\n%1\n\n"
               "Please ensure libburn and libisofs are properly installed.")
            .arg(m_engine->initError()));
    }

    // Initialize drive monitor
    m_driveMonitor = std::make_unique<Engine::DriveMonitor>(
        m_engine->driveManager(), this);

    // Initialize model
    m_driveModel = new Models::DriveListModel(this);

    setupUi();
    setupMenuBar();
    setupToolBar();
    connectSignals();
    applyChrome();
    loadSettings();

    // Start monitoring drives (10 second interval for quick scans)
    m_driveMonitor->start(10000);
    refreshDrives();
}

MainWindow::~MainWindow() {
    saveSettings();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (!maybeSave()) {
        event->ignore();
        return;
    }
    saveSettings();
    event->accept();
}

void MainWindow::applyChrome() {
    const TitleBarDecision decision = decideTitleBar(m_titleBarMode);
    WindowChrome::setIntegratedTitleBars(decision.integrated);
    if (qEnvironmentVariableIsSet("BURNER_DEBUG_TITLEBAR")) {
        qInfo().noquote() << "Title bar:" << decision.reason;
    }

    const bool wasVisible = isVisible();
    const bool maximized = isMaximized();
    const bool frameless = windowFlags() & Qt::FramelessWindowHint;

    m_titleBar->setIntegrated(decision.integrated);
    // The integrated title bar draws the separator line across the full width
    m_menuBar->setStyleSheet(decision.integrated
        ? QStringLiteral("QMenuBar { border: none; background: transparent; }")
        : QString());
    m_chrome->setActive(decision.integrated);
    setContentsMargins(decision.integrated && !maximized ? QMargins(1, 1, 1, 1) : QMargins());

    if (frameless != decision.integrated) {
        // Changing frame flags recreates the native window; show it again as it was
        setWindowFlag(Qt::FramelessWindowHint, decision.integrated);
        if (wasVisible) {
            maximized ? showMaximized() : show();
        }
    }
}

void MainWindow::paintEvent(QPaintEvent* event) {
    QMainWindow::paintEvent(event);
    if (m_chrome && m_chrome->isActive()) {
        WindowChrome::paintOutline(this);
    }
}

void MainWindow::changeEvent(QEvent* event) {
    QMainWindow::changeEvent(event);
    // No outline (and so no margin for it) while maximized
    if (event->type() == QEvent::WindowStateChange && m_chrome && m_chrome->isActive()) {
        setContentsMargins(windowState() & (Qt::WindowMaximized | Qt::WindowFullScreen)
            ? QMargins() : QMargins(1, 1, 1, 1));
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    const QMimeData* mimeData = event->mimeData();
    if (!mimeData->hasUrls()) {
        return;
    }

    QStringList paths;
    for (const QUrl& url : mimeData->urls()) {
        if (url.isLocalFile()) {
            paths.append(url.toLocalFile());
        }
    }

    if (paths.isEmpty()) {
        return;
    }

    // Add to current page based on tab
    int currentTab = m_tabWidget->currentIndex();
    switch (currentTab) {
        case 0: // Data
            m_dataPage->model()->addFiles(paths);
            break;
        case 1: // Audio
            m_audioPage->model()->addTracks(paths);
            break;
        default:
            break;
    }

    event->acceptProposedAction();
}

void MainWindow::setupUi() {
    auto* centralWidget = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    // Drive selector row
    m_driveSelector = new DriveSelector(this);
    m_driveSelector->setModel(m_driveModel);
    mainLayout->addWidget(m_driveSelector);

    // Tab widget
    m_tabWidget = new QTabWidget(this);

    m_dataPage = new DataDiscPage(this);
    m_tabWidget->addTab(m_dataPage, tr("💿 Data CD/DVD"));

    m_audioPage = new AudioCdPage(this);
    m_tabWidget->addTab(m_audioPage, tr("🎵 Audio CD"));

    m_videoPage = new VideoCdPage(this);
    m_tabWidget->addTab(m_videoPage, tr("🎬 Video CD"));

    m_isoPage = new IsoWriterPage(this);
    m_tabWidget->addTab(m_isoPage, tr("🔥 Burn ISO"));

    m_clonePage = new DiscClonerPage(this);
    m_clonePage->setDriveModel(m_driveModel);
    m_tabWidget->addTab(m_clonePage, tr("📋 Clone Disc"));

    m_ripPage = new CdRipperPage(this);
    m_tabWidget->addTab(m_ripPage, tr("📥 Rip CD"));

    m_browserPage = new IsoBrowserPage(this);
    m_tabWidget->addTab(m_browserPage, tr("📂 Browse ISO"));

    mainLayout->addWidget(m_tabWidget, 1);

    // Capacity bar
    m_capacityBar = new CapacityBar(this);
    mainLayout->addWidget(m_capacityBar);

    setCentralWidget(centralWidget);

    // Status bar
    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::setupMenuBar() {
    // The menu bar sits in the title bar, which also shows the window controls
    // when Burner draws its own title bar (see applyChrome)
    m_menuBar = new QMenuBar;
    m_titleBar = new TitleBar(this);
    m_titleBar->addWidget(m_menuBar);
    setMenuWidget(m_titleBar);
    m_chrome = new WindowChrome(this);

    // File menu
    QMenu* fileMenu = m_menuBar->addMenu(tr("&File"));

    // Shortcuts are set separately: the addAction(text, shortcut, receiver, slot)
    // overload requires Qt 6.3
    fileMenu->addAction(tr("&New Project"), this, &MainWindow::newProject)
        ->setShortcut(QKeySequence::New);
    fileMenu->addAction(tr("&Open Project..."), this, &MainWindow::openProject)
        ->setShortcut(QKeySequence::Open);

    fileMenu->addSeparator();

    fileMenu->addAction(tr("&Save Project"), this, &MainWindow::saveProject)
        ->setShortcut(QKeySequence::Save);
    fileMenu->addAction(tr("Save Project &As..."), this, &MainWindow::saveProjectAs)
        ->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));

    fileMenu->addSeparator();

    m_recentProjectsMenu = fileMenu->addMenu(tr("&Recent Projects"));
    updateRecentProjectsMenu();

    fileMenu->addSeparator();

    fileMenu->addAction(tr("E&xit"), this, &QMainWindow::close)
        ->setShortcut(QKeySequence::Quit);

    // View menu
    QMenu* viewMenu = m_menuBar->addMenu(tr("&View"));
    QMenu* themeMenu = viewMenu->addMenu(tr("&Theme"));

    m_themeActions = new QActionGroup(this);
    m_themeActions->setExclusive(true);

    auto* systemTheme = themeMenu->addAction(tr("&System"));
    systemTheme->setCheckable(true);
    systemTheme->setData(static_cast<int>(ThemeManager::Theme::System));

    auto* lightTheme = themeMenu->addAction(tr("&Light"));
    lightTheme->setCheckable(true);
    lightTheme->setData(static_cast<int>(ThemeManager::Theme::Light));

    auto* darkTheme = themeMenu->addAction(tr("&Dark"));
    darkTheme->setCheckable(true);
    darkTheme->setData(static_cast<int>(ThemeManager::Theme::Dark));

    m_themeActions->addAction(systemTheme);
    m_themeActions->addAction(lightTheme);
    m_themeActions->addAction(darkTheme);

    // Set initial check based on current theme
    auto current = m_themeManager->currentTheme();
    for (auto* action : m_themeActions->actions()) {
        if (action->data().toInt() == static_cast<int>(current)) {
            action->setChecked(true);
            break;
        }
    }

    connect(m_themeActions, &QActionGroup::triggered, this, [this](QAction* action) {
        auto theme = static_cast<ThemeManager::Theme>(action->data().toInt());
        m_themeManager->setTheme(theme);
        m_themeManager->saveToSettings();
    });

    // Title bar submenu: Qt's fallback title bar on Wayland ignores
    // QT_SCALE_FACTOR, so Burner can draw its own
    QMenu* titleBarMenu = viewMenu->addMenu(tr("Title &Bar"));
    titleBarMenu->setToolTipsVisible(true);
    m_titleBarActions = new QActionGroup(this);
    m_titleBarActions->setExclusive(true);

    const TitleBarDecision automatic = decideTitleBar(TitleBarMode::Auto);
    auto* autoTitleBar = titleBarMenu->addAction(
        automatic.integrated ? tr("&Automatic (Burner's Own)") : tr("&Automatic (System)"));
    autoTitleBar->setToolTip(tr("Automatic uses %1.").arg(automatic.reason));
    autoTitleBar->setData(static_cast<int>(TitleBarMode::Auto));
    auto* ownTitleBar = titleBarMenu->addAction(tr("&Burner's Own"));
    ownTitleBar->setData(static_cast<int>(TitleBarMode::Integrated));
    auto* systemTitleBar = titleBarMenu->addAction(tr("&System"));
    systemTitleBar->setData(static_cast<int>(TitleBarMode::System));

    for (auto* action : {autoTitleBar, ownTitleBar, systemTitleBar}) {
        action->setCheckable(true);
        action->setChecked(action->data().toInt() == static_cast<int>(m_titleBarMode));
        m_titleBarActions->addAction(action);
    }

    connect(m_titleBarActions, &QActionGroup::triggered, this, [this](QAction* action) {
        m_titleBarMode = static_cast<TitleBarMode>(action->data().toInt());
        QSettings("Burner", "Burner").setValue("appearance/titleBar", static_cast<int>(m_titleBarMode));
        applyChrome();
    });

    // Tools menu
    QMenu* toolsMenu = m_menuBar->addMenu(tr("&Tools"));

    toolsMenu->addAction(tr("&Refresh Drives"), this, &MainWindow::refreshDrives)
        ->setShortcut(QKeySequence::Refresh);

    toolsMenu->addAction(tr("&Eject"), this, [this]() {
        ejectDrive(m_driveSelector->selectedDevice());
    });

    toolsMenu->addSeparator();

    toolsMenu->addAction(tr("&Checksum Calculator..."), this, [this]() {
        ChecksumDialog dialog(this);
        dialog.exec();
    });

    // Help menu
    QMenu* helpMenu = m_menuBar->addMenu(tr("&Help"));

    helpMenu->addAction(tr("&About"), this, &MainWindow::showAbout);

    helpMenu->addAction(tr("About &Qt"), qApp, &QApplication::aboutQt);
}

void MainWindow::setupToolBar() {
    QToolBar* toolBar = addToolBar(tr("Main"));
    toolBar->setObjectName("MainToolBar");
    toolBar->setMovable(false);
    toolBar->setContextMenuPolicy(Qt::PreventContextMenu);
    toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    // Create prominent burn button that spans full width
    m_burnButton = new QPushButton(tr("🔥 Burn Disc"), this);
    m_burnButton->setObjectName("burnButton");
    m_burnButton->setToolTip(tr("Start burning disc"));
    m_burnButton->setCursor(Qt::PointingHandCursor);
    m_burnButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    connect(m_burnButton, &QPushButton::clicked, this, &MainWindow::onBurnClicked);
    toolBar->addWidget(m_burnButton);
}

void MainWindow::connectSignals() {
    connect(m_driveSelector, &DriveSelector::selectionChanged,
            this, &MainWindow::onDriveSelectionChanged);
    connect(m_driveSelector, &DriveSelector::refreshRequested,
            this, &MainWindow::refreshDrives);
    connect(m_driveSelector, &DriveSelector::ejectRequested,
            this, &MainWindow::ejectDrive);

    connect(m_driveMonitor.get(), &Engine::DriveMonitor::drivesChanged,
            this, &MainWindow::onDrivesChanged);

    // Re-enable controls when scan completes (even if nothing changed)
    connect(m_driveMonitor.get(), &Engine::DriveMonitor::scanCompleted,
            this, [this]() {
                m_driveSelector->setControlsEnabled(true);
                statusBar()->showMessage(tr("Ready"));
            });

    // When media is inserted, trigger a delayed full refresh to get capacity info
    // (libburn may not have complete info immediately)
    connect(m_driveMonitor.get(), &Engine::DriveMonitor::mediaInserted,
            this, [this](const QString& device, const Core::MediaInfo& media) {
                QTimer::singleShot(1000, m_driveMonitor.get(), &Engine::DriveMonitor::refreshFull);

                // If an audio CD is inserted, notify the rip page
                if (media.isAudioDisc || media.audioTracks > 0) {
                    m_ripPage->onAudioCdInserted(device);
                }

                // Update multi-session banner on Data page
                if (media.status == Core::MediaStatus::Appendable) {
                    m_dataPage->setMultiSessionInfo(media.sessions, true);
                } else {
                    m_dataPage->setMultiSessionInfo(0, false);
                }
            });

    // When media is removed, notify the rip page and clear multi-session banner
    connect(m_driveMonitor.get(), &Engine::DriveMonitor::mediaRemoved,
            this, [this](const QString& device) {
                m_ripPage->onMediaRemoved(device);
                m_dataPage->setMultiSessionInfo(0, false);
            });

    connect(m_tabWidget, &QTabWidget::currentChanged,
            this, &MainWindow::onPageChanged);

    connect(m_dataPage, &DataDiscPage::filesChanged,
            this, &MainWindow::updateCapacityBar);
    connect(m_dataPage, &DataDiscPage::saveIsoRequested,
            this, &MainWindow::onSaveIsoRequested);
    connect(m_audioPage, &AudioCdPage::tracksChanged,
            this, &MainWindow::updateCapacityBar);

    // Track dirty state and handle project type changes
    connect(m_dataPage, &DataDiscPage::filesChanged,
            this, [this]() {
                if (!m_clearingTabs && m_dataPage->model()->rowCount() > 0 && hasFilesOnOtherTabs(0)) {
                    confirmProjectTypeChange(0);
                }
                if (!m_clearingTabs) {
                    setProjectDirty(true);
                }
            });
    connect(m_audioPage, &AudioCdPage::tracksChanged,
            this, [this]() {
                if (!m_clearingTabs && m_audioPage->model()->trackCount() > 0 && hasFilesOnOtherTabs(1)) {
                    confirmProjectTypeChange(1);
                }
                if (!m_clearingTabs) {
                    setProjectDirty(true);
                }
            });
    connect(m_videoPage, &VideoCdPage::videosChanged,
            this, [this]() {
                if (!m_clearingTabs && !m_videoPage->videoFiles().isEmpty() && hasFilesOnOtherTabs(2)) {
                    confirmProjectTypeChange(2);
                }
                if (!m_clearingTabs) {
                    setProjectDirty(true);
                }
            });

    connect(m_clonePage, &DiscClonerPage::cloneRequested,
            this, &MainWindow::onCloneRequested);
    connect(m_clonePage, &DiscClonerPage::blankRequested,
            this, &MainWindow::onBlankRequested);
    connect(m_clonePage, &DiscClonerPage::integrityCheckRequested,
            this, &MainWindow::onIntegrityCheckRequested);

    // Disable tab switching during ripping
    connect(m_ripPage, &CdRipperPage::ripStarted,
            this, [this]() {
                m_tabWidget->tabBar()->setEnabled(false);
                m_burnButton->setEnabled(false);
            });
    connect(m_ripPage, &CdRipperPage::ripComplete,
            this, [this](bool, const QString&) {
                m_tabWidget->tabBar()->setEnabled(true);
                // Re-enable burn button based on current tab
                m_burnButton->setEnabled(m_tabWidget->currentIndex() < 4);
            });

    connect(m_engine.get(), &Engine::QtBurnEngine::statusChanged,
            this, [this](const QString& message) {
                statusBar()->showMessage(message);
            });
}

void MainWindow::onDriveSelectionChanged(const QString& device) {
    qDebug() << "MainWindow::onDriveSelectionChanged -" << device << "tab:" << m_tabWidget->currentIndex();
    updateCapacityBar();

    // Update Rip CD page if we're on that tab
    if (m_tabWidget->currentIndex() == 5) {
        m_ripPage->setCurrentDevice(device);
    }
}

void MainWindow::onDrivesChanged() {
    auto drives = m_driveMonitor->currentDrives();
    m_driveModel->updateDrives(drives);
    updateCapacityBar();

    // Update multi-session banner based on current drive's media
    const Core::DriveInfo* drive = m_driveModel->driveAt(
        m_driveModel->indexOf(m_driveSelector->selectedDevice()));
    if (drive && drive->media.status == Core::MediaStatus::Appendable) {
        m_dataPage->setMultiSessionInfo(drive->media.sessions, true);
    } else {
        m_dataPage->setMultiSessionInfo(0, false);
    }
}

void MainWindow::onBurnClicked() {
    QString device = m_driveSelector->selectedDevice();
    if (device.isEmpty()) {
        Dialogs::warning(this, tr("No Drive Selected"),
            tr("Please select a drive to burn to."));
        return;
    }

    // Show options dialog
    BurnOptionsDialog optionsDialog(this);

    // Configure based on current tab
    int currentTab = m_tabWidget->currentIndex();
    bool showVolumeLabel = (currentTab == 0 || currentTab == 2);  // Data or Video
    optionsDialog.setVolumeLabelVisible(showVolumeLabel);

    // Set disc type and summary based on current tab
    QString discType;
    QString summary;
    switch (currentTab) {
        case 0: { // Data
            discType = tr("Data CD/DVD");
            int fileCount = m_dataPage->fileMappings().size();
            qint64 size = m_dataPage->totalSize();
            summary = tr("%1 files, %2 MB total")
                .arg(fileCount)
                .arg(size / (1024 * 1024));
            break;
        }
        case 1: { // Audio
            discType = tr("Audio CD");
            int trackCount = m_audioPage->audioFiles().size();
            int duration = m_audioPage->totalDurationSeconds();
            int mins = duration / 60;
            int secs = duration % 60;
            summary = tr("%1 tracks, %2:%3 total duration")
                .arg(trackCount)
                .arg(mins)
                .arg(secs, 2, 10, QChar('0'));
            break;
        }
        case 2: { // Video
            discType = tr("Video CD (%1)").arg(m_videoPage->vcdType());
            int videoCount = m_videoPage->videoFiles().size();
            summary = tr("%1 videos").arg(videoCount);
            break;
        }
        case 3: { // ISO / CUE
            discType = m_isoPage->isCueImage() ? tr("BIN/CUE Image") : tr("ISO Image");
            qint64 size = m_isoPage->isoSize();
            summary = tr("Image: %1\nSize: %2 MB")
                .arg(m_isoPage->isoPath())
                .arg(size / (1024 * 1024));
            break;
        }
        default:
            discType = tr("Unknown");
            break;
    }
    optionsDialog.setDiscType(discType);
    optionsDialog.setSummary(summary);

    // Set available write speeds from selected drive
    const Core::DriveInfo* drive = m_driveModel->driveAt(
        m_driveModel->indexOf(device));

    // Set multi-session info if burning data to an appendable disc
    if (currentTab == 0 && drive &&
        drive->media.status == Core::MediaStatus::Appendable) {
        optionsDialog.setMultiSessionInfo(drive->media.sessions);
    }

    // Check if drive is mounted/busy
    if (drive && drive->media.status == Core::MediaStatus::Busy) {
        Dialogs::warning(this, tr("Drive is Mounted"),
            tr("The disc in %1 is currently mounted by the system.\n\n"
               "Please unmount the disc before burning:\n"
               "  sudo umount %1\n\n"
               "Or use your file manager to safely eject/unmount the disc.")
            .arg(device));
        return;
    }

    if (drive && !drive->writeSpeeds.empty()) {
        QList<int> speeds;
        for (int s : drive->writeSpeeds) {
            speeds.append(s);
        }
        optionsDialog.setWriteSpeeds(speeds);
    }

    if (optionsDialog.exec() != QDialog::Accepted) {
        return;
    }

    auto dialogOptions = optionsDialog.options();

    // Convert dialog options to core BurnOptions
    Core::BurnOptions options;
    options.speed = dialogOptions.speed;
    options.simulate = dialogOptions.simulate;
    options.verify = dialogOptions.verify;
    options.ejectAfter = dialogOptions.ejectAfter;
    options.burnProof = dialogOptions.burnProof;
    options.closeDisc = dialogOptions.closeDisc;
    options.blankBeforeBurn = dialogOptions.blankBeforeBurn;

    // Warn about simulation mode limitations
    if (options.simulate) {
        QMessageBox::StandardButton result = Dialogs::warning(this,
            tr("Simulation Mode Warning"),
            tr("Simulated burns may not work correctly on all disc types, "
               "especially write-once media like DVD-R and BD-R.\n\n"
               "After a simulated burn, you may need to eject and re-insert "
               "the disc to restore correct capacity reporting.\n\n"
               "Do you want to continue with simulation?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes);

        if (result != QMessageBox::Yes) {
            return;
        }
    }

    // Stop drive monitoring during burn to release the drive
    qDebug() << "Stopping drive monitor...";
    m_driveMonitor->stop();

    // Wait for any ongoing scan to complete
    qDebug() << "Waiting for scan to complete...";
    bool scanDone = m_driveMonitor->waitForScan(5000);
    qDebug() << "Scan done:" << scanDone;

    // Give libburn a moment to fully release the drive
    QThread::msleep(500);

    // Check if data fits on disc before starting burn
    qint64 dataSize = 0;
    switch (currentTab) {
        case 0: // Data
            dataSize = m_dataPage->totalSize();
            break;
        case 1: // Audio
            dataSize = m_audioPage->totalDurationSeconds() * 176 * 1024;
            break;
        case 3: // ISO
            dataSize = m_isoPage->isoSize();
            break;
    }

    if (dataSize > 0 && drive) {
        qint64 discCapacity = static_cast<qint64>(drive->media.freeBytes);
        if (discCapacity > 0 && dataSize > discCapacity) {
            Dialogs::critical(this, tr("Data Too Large"),
                tr("The data (%1 MB) is too large for the disc (%2 MB available).\n\n"
                   "Please remove some files or use a larger capacity disc.")
                .arg(dataSize / (1024 * 1024))
                .arg(discCapacity / (1024 * 1024)));
            m_driveMonitor->start(3000);
            return;
        }
    }

    // Show progress dialog
    BurnProgressDialog progressDialog(this);

    // Set media type for correct speed multiplier display
    if (drive) {
        progressDialog.setSpeedMedia(mediaToSpeedMedia(drive->media.type));
    }

    // Connect signals before starting burn
    connect(m_engine.get(), &Engine::QtBurnEngine::progressChanged,
            &progressDialog, &BurnProgressDialog::setProgress);
    connect(m_engine.get(), &Engine::QtBurnEngine::statusChanged,
            &progressDialog, &BurnProgressDialog::setStatus);
    connect(m_engine.get(), &Engine::QtBurnEngine::progressDetail,
            &progressDialog, [&progressDialog](quint64 written, quint64 total, int buffer, int speed) {
                progressDialog.setBufferFill(buffer);
                progressDialog.setWriteSpeed(speed);
            });
    connect(m_engine.get(), &Engine::QtBurnEngine::timeUpdate,
            &progressDialog, &BurnProgressDialog::setTimes);
    connect(m_engine.get(), &Engine::QtBurnEngine::burnComplete,
            &progressDialog, &BurnProgressDialog::onComplete);
    connect(&progressDialog, &BurnProgressDialog::cancelRequested,
            m_engine.get(), &Engine::QtBurnEngine::cancel);

    switch (currentTab) {
        case 0: // Data
            m_engine->burnDataDiscWithStructure(device,
                                                m_dataPage->fileMappings(),
                                                dialogOptions.volumeLabel,
                                                options);
            break;
        case 1: // Audio
            m_engine->burnAudioCD(device,
                                  m_audioPage->audioFiles(),
                                  options);
            break;
        case 2: // Video
            if (m_videoPage->videoFiles().isEmpty()) {
                Dialogs::warning(this, tr("No Videos"),
                    tr("Please add video files to burn."));
                return;
            }
            m_engine->burnVideoCd(device,
                                  m_videoPage->videoFiles(),
                                  m_videoPage->vcdType(),
                                  options);
            break;
        case 3: // ISO / CUE
            if (m_isoPage->isCueImage()) {
                m_engine->burnCueImage(device,
                                       m_isoPage->isoPath(),
                                       options);
            } else {
                m_engine->burnIsoFile(device,
                                      m_isoPage->isoPath(),
                                      options);
            }
            break;
        case 4: // Clone - handled by its own button
            Dialogs::information(this, tr("Clone Disc"),
                tr("Use the 'Clone Disc' button on the Clone tab to start cloning."));
            m_driveMonitor->start(3000);
            return;
        default:
            Dialogs::information(this, tr("Not Implemented"),
                tr("This burn mode is not yet implemented."));
            return;
    }

    progressDialog.exec();

    // Resume drive monitoring after burn
    m_driveMonitor->start(3000);
}

void MainWindow::onSaveIsoRequested(const QString& outputPath) {
    auto fileMappings = m_dataPage->fileMappings();
    if (fileMappings.isEmpty()) {
        Dialogs::warning(this, tr("No Files"),
            tr("Please add files to create an ISO image."));
        return;
    }

    // Get volume label from user
    QFileInfo fileInfo(outputPath);
    QString defaultLabel = fileInfo.baseName().toUpper().left(32);
    if (defaultLabel.isEmpty()) {
        defaultLabel = "DISC";
    }

    bool ok;
    QString volumeLabel = Dialogs::getText(this, tr("Volume Label"),
        tr("Enter volume label for the ISO:"), QLineEdit::Normal,
        defaultLabel, &ok);

    if (!ok) {
        return;  // User cancelled
    }

    if (volumeLabel.isEmpty()) {
        volumeLabel = "DISC";
    }

    // Show progress dialog
    BurnProgressDialog progressDialog(this);
    progressDialog.setWindowTitle(tr("Creating ISO Image"));

    // Connect signals
    connect(m_engine.get(), &Engine::QtBurnEngine::progressChanged,
            &progressDialog, &BurnProgressDialog::setProgress);
    connect(m_engine.get(), &Engine::QtBurnEngine::statusChanged,
            &progressDialog, &BurnProgressDialog::setStatus);
    connect(m_engine.get(), &Engine::QtBurnEngine::burnComplete,
            &progressDialog, &BurnProgressDialog::onComplete);
    connect(&progressDialog, &BurnProgressDialog::cancelRequested,
            m_engine.get(), &Engine::QtBurnEngine::cancel);

    // Start ISO creation with directory structure preserved
    m_engine->createIsoFileWithStructure(outputPath, fileMappings, volumeLabel);

    progressDialog.exec();
}

void MainWindow::onCloneRequested() {
    QString sourceDevice = m_clonePage->sourceDevice();
    QString outputPath = m_clonePage->imagePath();

    if (sourceDevice.isEmpty()) {
        Dialogs::warning(this, tr("No Source Drive"),
            tr("Please select a source drive to clone."));
        return;
    }

    if (outputPath.isEmpty()) {
        Dialogs::warning(this, tr("No Output File"),
            tr("Please specify an output file path for the disc image."));
        return;
    }

    // Stop drive monitoring during clone
    m_driveMonitor->stop();
    m_driveMonitor->waitForScan(5000);
    QThread::msleep(500);

    // Show progress dialog
    BurnProgressDialog progressDialog(this);
    progressDialog.setWindowTitle(tr("Cloning Disc"));
    progressDialog.setMode(BurnProgressDialog::CloneMode);

    // Set media type for correct speed multiplier display
    {
        int idx = m_driveModel->indexOf(sourceDevice);
        if (idx >= 0) {
            const auto* di = m_driveModel->driveAt(idx);
            if (di) {
                progressDialog.setSpeedMedia(mediaToSpeedMedia(di->media.type));
            }
        }
    }

    // Connect signals
    connect(m_engine.get(), &Engine::QtBurnEngine::progressChanged,
            &progressDialog, &BurnProgressDialog::setProgress);
    connect(m_engine.get(), &Engine::QtBurnEngine::statusChanged,
            &progressDialog, &BurnProgressDialog::setStatus);
    connect(m_engine.get(), &Engine::QtBurnEngine::burnComplete,
            &progressDialog, &BurnProgressDialog::onComplete);
    connect(&progressDialog, &BurnProgressDialog::cancelRequested,
            m_engine.get(), &Engine::QtBurnEngine::cancel);

    // Connect speed signal for clone operations (speed comes as the last parameter)
    connect(m_engine.get(), &Engine::QtBurnEngine::progressDetail,
            &progressDialog, [&progressDialog](quint64, quint64, int, int speed) {
                progressDialog.setWriteSpeed(speed);
            });

    // Auto-switch to BIN/CUE for audio CDs (they have no readable filesystem)
    bool useBinCue = (m_clonePage->cloneFormat() == DiscClonerPage::BinCue);
    if (!useBinCue) {
        int driveIdx = m_driveModel->indexOf(sourceDevice);
        if (driveIdx >= 0) {
            const auto* info = m_driveModel->driveAt(driveIdx);
            if (info && info->media.isAudioDisc) {
                useBinCue = true;
                // Fix output extension from .iso to .bin
                if (outputPath.endsWith(QStringLiteral(".iso"), Qt::CaseInsensitive)) {
                    outputPath.chop(4);
                    outputPath += QStringLiteral(".bin");
                }
            }
        }
    }

    // Start clone operation based on format
    if (useBinCue) {
        m_engine->cloneToBinCue(sourceDevice, outputPath);
    } else {
        m_engine->cloneToImage(sourceDevice, outputPath);
    }

    progressDialog.exec();

    // Resume drive monitoring
    m_driveMonitor->start(3000);
}

void MainWindow::onBlankRequested(bool fullBlank) {
    QString device = m_clonePage->sourceDevice();

    if (device.isEmpty()) {
        Dialogs::warning(this, tr("No Drive Selected"),
            tr("Please select a drive to blank."));
        return;
    }

    // Confirm blanking - this is destructive
    QString blankType = fullBlank ? tr("full") : tr("quick");
    QMessageBox::StandardButton result = Dialogs::warning(this,
        tr("Confirm Blank Disc"),
        tr("This will erase all data on the disc in %1.\n\n"
           "Blank type: %2\n\n"
           "This operation cannot be undone. Continue?")
        .arg(device)
        .arg(blankType),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (result != QMessageBox::Yes) {
        return;
    }

    // Stop drive monitoring during blank
    m_driveMonitor->stop();
    m_driveMonitor->waitForScan(5000);
    QThread::msleep(500);

    // Show progress dialog
    BurnProgressDialog progressDialog(this);
    progressDialog.setWindowTitle(tr("Blanking Disc"));
    progressDialog.setMode(BurnProgressDialog::BlankMode);

    // Connect signals
    connect(m_engine.get(), &Engine::QtBurnEngine::progressChanged,
            &progressDialog, &BurnProgressDialog::setProgress);
    connect(m_engine.get(), &Engine::QtBurnEngine::statusChanged,
            &progressDialog, &BurnProgressDialog::setStatus);
    connect(m_engine.get(), &Engine::QtBurnEngine::burnComplete,
            &progressDialog, &BurnProgressDialog::onComplete);
    connect(&progressDialog, &BurnProgressDialog::cancelRequested,
            m_engine.get(), &Engine::QtBurnEngine::cancel);

    // Start blank operation
    m_engine->blankDisc(device, fullBlank);

    progressDialog.exec();

    // Resume drive monitoring
    m_driveMonitor->start(3000);
}

void MainWindow::onIntegrityCheckRequested(bool stopOnFirstError) {
    QString device = m_clonePage->sourceDevice();

    if (device.isEmpty()) {
        Dialogs::warning(this, tr("No Drive Selected"),
            tr("Please select a drive to check."));
        return;
    }

    // Stop drive monitoring during integrity check
    m_driveMonitor->stop();
    m_driveMonitor->waitForScan(5000);
    QThread::msleep(500);

    // Show progress dialog
    BurnProgressDialog progressDialog(this);
    progressDialog.setWindowTitle(tr("Checking Disc Integrity"));
    progressDialog.setMode(BurnProgressDialog::IntegrityMode);

    // Connect signals
    connect(m_engine.get(), &Engine::QtBurnEngine::progressChanged,
            &progressDialog, &BurnProgressDialog::setProgress);
    connect(m_engine.get(), &Engine::QtBurnEngine::statusChanged,
            &progressDialog, &BurnProgressDialog::setStatus);
    connect(m_engine.get(), &Engine::QtBurnEngine::burnComplete,
            &progressDialog, &BurnProgressDialog::onComplete);
    connect(&progressDialog, &BurnProgressDialog::cancelRequested,
            m_engine.get(), &Engine::QtBurnEngine::cancel);

    // Start integrity check
    m_engine->checkIntegrity(device, stopOnFirstError);

    progressDialog.exec();

    // Resume drive monitoring
    m_driveMonitor->start(3000);
}

void MainWindow::onPageChanged(int index) {
    updateCapacityBar();

    // Disable burn button on Clone Disc and Rip CD tabs (they have their own action buttons)
    m_burnButton->setEnabled(index < 4);

    // Update Rip CD page with current drive selection when switching to that tab
    if (index == 5) {
        QString device = m_driveSelector->selectedDevice();
        if (!device.isEmpty() && m_ripPage->currentDevice().isEmpty()) {
            m_ripPage->setCurrentDevice(device);
        }
    }

    // Update status bar hint
    switch (index) {
        case 0:
            statusBar()->showMessage(tr("Data CD/DVD - Add files and folders to burn"));
            break;
        case 1:
            statusBar()->showMessage(tr("Audio CD - Add audio tracks (MP3, FLAC, WAV)"));
            break;
        case 2:
            statusBar()->showMessage(tr("Video CD - Add videos for VCD/SVCD"));
            break;
        case 3:
            statusBar()->showMessage(tr("Burn ISO - Burn an ISO image file"));
            break;
        case 4:
            statusBar()->showMessage(tr("Clone Disc - Copy disc to image or image to disc"));
            break;
        case 5:
            statusBar()->showMessage(tr("Rip CD - Extract audio tracks from CD to files"));
            break;
        case 6:
            statusBar()->showMessage(tr("Browse ISO - Open and extract files from ISO images"));
            break;
    }
}

void MainWindow::refreshDrives() {
    m_driveSelector->setControlsEnabled(false);
    statusBar()->showMessage(tr("Scanning for drives..."));
    m_driveMonitor->refreshFull();  // Full scan to get media info
    // Controls will be re-enabled in onDrivesChanged()
}

void MainWindow::ejectDrive(const QString& device) {
    if (device.isEmpty()) {
        return;
    }

    m_driveSelector->setControlsEnabled(false);
    statusBar()->showMessage(tr("Ejecting disc..."));

    // Stop monitoring to release the drive
    m_driveMonitor->stop();
    m_driveMonitor->waitForScan(2000);

    m_engine->ejectDrive(device);

    // Resume monitoring (this will trigger a scan and re-enable controls via onDrivesChanged)
    m_driveMonitor->start(10000);
}

void MainWindow::showAbout() {
    Dialogs::about(this, tr("About Burner"),
        tr("<h2>Burner</h2>"
           "<p>CD/DVD/Blu-ray Burning Application</p>"
           "<p>Version %3</p>"
           "<p>Built with Qt6 and libburnia (libburn, libisofs)</p>"
           "<p>libburn version: %1</p>"
           "<p>libisofs version: %2</p>")
        .arg(QString::fromStdString(Core::BurniaInit::libburnVersion()))
        .arg(QString::fromStdString(Core::BurniaInit::libisofsVersion()))
        .arg(QApplication::applicationVersion()));
}

void MainWindow::updateCapacityBar() {
    int currentTab = m_tabWidget->currentIndex();

    // Get media info from selected drive
    const Core::DriveInfo* drive = m_driveModel->driveAt(
        m_driveModel->indexOf(m_driveSelector->selectedDevice()));

    qint64 discCapacity = 0;
    qint64 discUsed = 0;
    bool isFullOrReadOnly = false;

    if (drive) {
        discCapacity = static_cast<qint64>(drive->media.capacityBytes);
        qint64 freeBytes = static_cast<qint64>(drive->media.freeBytes);

        // Calculate used space on disc
        if (discCapacity > 0) {
            discUsed = discCapacity - freeBytes;
        }

        // Check if disc is full or read-only (no free space to write)
        isFullOrReadOnly = (drive->media.status == Core::MediaStatus::Full ||
                           freeBytes == 0) && discCapacity > 0;
    }

    // For Clone/Integrity tab or full/read-only discs, show disc's used capacity
    if (currentTab == 4 || currentTab == 5 || isFullOrReadOnly) {
        if (discCapacity > 0) {
            m_capacityBar->setCapacity(discCapacity);
            m_capacityBar->setUsed(discUsed);
        } else {
            // No disc or unknown capacity
            m_capacityBar->setMediaCapacity(CapacityBar::CD_700MB);
            m_capacityBar->setUsed(0);
        }
        return;
    }

    // For burn tabs, show free space as capacity and user's selection as used
    if (drive && drive->media.freeBytes > 0) {
        m_capacityBar->setCapacity(drive->media.freeBytes);
    } else if (discCapacity > 0) {
        // Disc is full but we're on a burn tab - show total capacity
        m_capacityBar->setCapacity(discCapacity);
    } else {
        // Default to CD-700MB
        m_capacityBar->setMediaCapacity(CapacityBar::CD_700MB);
    }

    // Get used size based on current page (what user wants to burn)
    qint64 used = 0;
    switch (currentTab) {
        case 0: // Data
            used = m_dataPage->totalSize();
            break;
        case 1: // Audio
            // Estimate: 176 KB/s for CD-DA audio
            used = m_audioPage->totalDurationSeconds() * 176 * 1024;
            break;
        case 3: // ISO
            used = m_isoPage->isoSize();
            break;
    }

    m_capacityBar->setUsed(used);
}

void MainWindow::loadSettings() {
    QSettings settings("Burner", "Burner");
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
}

void MainWindow::saveSettings() {
    QSettings settings("Burner", "Burner");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
}

// Project management

void MainWindow::newProject() {
    if (!maybeSave()) {
        return;
    }

    // Clear all pages
    m_dataPage->model()->clear();
    m_audioPage->model()->clear();
    m_videoPage->clearAll();

    // Reset to first tab
    m_tabWidget->setCurrentIndex(0);

    m_currentProjectPath.clear();
    setProjectDirty(false);
}

void MainWindow::openProject() {
    if (!maybeSave()) {
        return;
    }

    QString path = QFileDialog::getOpenFileName(
        this,
        tr("Open Project"),
        QString(),
        tr("Burner Project (*.burn);;All Files (*)")
    );

    if (path.isEmpty()) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        Dialogs::critical(this, tr("Error"),
            tr("Could not open file: %1").arg(file.errorString()));
        return;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (parseError.error != QJsonParseError::NoError) {
        Dialogs::critical(this, tr("Error"),
            tr("Invalid project file: %1").arg(parseError.errorString()));
        return;
    }

    if (!loadProjectFromJson(doc.object())) {
        Dialogs::critical(this, tr("Error"),
            tr("Failed to load project data."));
        return;
    }

    m_currentProjectPath = path;
    setProjectDirty(false);
    addToRecentProjects(path);
}

bool MainWindow::saveProject() {
    if (m_currentProjectPath.isEmpty()) {
        return saveProjectAs();
    }

    QFile file(m_currentProjectPath);
    if (!file.open(QIODevice::WriteOnly)) {
        Dialogs::critical(this, tr("Error"),
            tr("Could not save file: %1").arg(file.errorString()));
        return false;
    }

    QJsonDocument doc(projectToJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    setProjectDirty(false);
    addToRecentProjects(m_currentProjectPath);
    statusBar()->showMessage(tr("Project saved"), 3000);
    return true;
}

bool MainWindow::saveProjectAs() {
    QString path = QFileDialog::getSaveFileName(
        this,
        tr("Save Project As"),
        QString(),
        tr("Burner Project (*.burn);;All Files (*)")
    );

    if (path.isEmpty()) {
        return false;
    }

    if (!path.endsWith(".burn", Qt::CaseInsensitive)) {
        path += ".burn";
    }

    m_currentProjectPath = path;
    return saveProject();
}

bool MainWindow::maybeSave() {
    if (!m_projectDirty) {
        return true;
    }

    QMessageBox::StandardButton result = Dialogs::question(
        this,
        tr("Unsaved Changes"),
        tr("The project has unsaved changes. Do you want to save them?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save
    );

    switch (result) {
        case QMessageBox::Save:
            return saveProject();
        case QMessageBox::Discard:
            return true;
        case QMessageBox::Cancel:
        default:
            return false;
    }
}

QJsonObject MainWindow::projectToJson() const {
    QJsonObject json;
    json["version"] = 1;

    int currentTab = m_tabWidget->currentIndex();

    // Determine project type based on current tab
    QString projectType;
    switch (currentTab) {
        case 0: projectType = "data"; break;
        case 1: projectType = "audio"; break;
        case 2: projectType = "video"; break;
        default: projectType = "data"; break;
    }
    json["projectType"] = projectType;

    // Save data based on type
    QJsonArray filesArray;

    if (projectType == "data") {
        json["volumeLabel"] = m_dataPage->model()->volumeLabel();

        auto mappings = m_dataPage->fileMappings();
        for (const auto& mapping : mappings) {
            QJsonObject fileObj;
            fileObj["localPath"] = mapping.first;
            fileObj["isoPath"] = mapping.second;
            filesArray.append(fileObj);
        }
    } else if (projectType == "audio") {
        QStringList files = m_audioPage->audioFiles();
        for (const QString& path : files) {
            QJsonObject fileObj;
            fileObj["localPath"] = path;
            filesArray.append(fileObj);
        }
    } else if (projectType == "video") {
        json["vcdType"] = m_videoPage->vcdType();

        QStringList files = m_videoPage->videoFiles();
        for (const QString& path : files) {
            QJsonObject fileObj;
            fileObj["localPath"] = path;
            filesArray.append(fileObj);
        }
    }

    json["files"] = filesArray;
    return json;
}

bool MainWindow::loadProjectFromJson(const QJsonObject& json) {
    int version = json["version"].toInt();
    if (version != 1) {
        return false;
    }

    QString projectType = json["projectType"].toString();

    // Clear all pages first
    m_dataPage->model()->clear();
    m_audioPage->model()->clear();
    m_videoPage->clearAll();

    QJsonArray filesArray = json["files"].toArray();

    if (projectType == "data") {
        m_tabWidget->setCurrentIndex(0);

        QString volumeLabel = json["volumeLabel"].toString();
        m_dataPage->model()->setVolumeLabel(volumeLabel);

        QList<QPair<QString, QString>> mappings;
        for (const QJsonValue& val : filesArray) {
            QJsonObject fileObj = val.toObject();
            QString localPath = fileObj["localPath"].toString();
            QString isoPath = fileObj["isoPath"].toString();
            mappings.append({localPath, isoPath});
        }
        m_dataPage->setFileMappings(mappings);

    } else if (projectType == "audio") {
        m_tabWidget->setCurrentIndex(1);

        QStringList files;
        for (const QJsonValue& val : filesArray) {
            QJsonObject fileObj = val.toObject();
            files.append(fileObj["localPath"].toString());
        }
        m_audioPage->setAudioFiles(files);

    } else if (projectType == "video") {
        m_tabWidget->setCurrentIndex(2);

        QString vcdType = json["vcdType"].toString();
        m_videoPage->setVcdType(vcdType);

        QStringList files;
        for (const QJsonValue& val : filesArray) {
            QJsonObject fileObj = val.toObject();
            files.append(fileObj["localPath"].toString());
        }
        m_videoPage->setVideoFiles(files);
    }

    return true;
}

void MainWindow::setProjectDirty(bool dirty) {
    if (m_projectDirty != dirty) {
        m_projectDirty = dirty;
        updateWindowTitle();
    }
}

void MainWindow::updateWindowTitle() {
    QString title = tr("Burner");

    if (!m_currentProjectPath.isEmpty()) {
        title = QFileInfo(m_currentProjectPath).fileName() + " - " + title;
    }

    if (m_projectDirty) {
        title = "* " + title;
    }

    setWindowTitle(title);
}

bool MainWindow::hasFilesOnOtherTabs(int currentTab) const {
    // Check Data CD (tab 0)
    if (currentTab != 0 && m_dataPage->model()->rowCount() > 0) {
        return true;
    }

    // Check Audio CD (tab 1)
    if (currentTab != 1 && m_audioPage->model()->trackCount() > 0) {
        return true;
    }

    // Check Video CD (tab 2)
    if (currentTab != 2 && !m_videoPage->videoFiles().isEmpty()) {
        return true;
    }

    return false;
}

bool MainWindow::confirmProjectTypeChange(int newTab) {
    if (!hasFilesOnOtherTabs(newTab)) {
        return true;
    }

    QString newType;
    switch (newTab) {
        case 0: newType = tr("Data CD/DVD"); break;
        case 1: newType = tr("Audio CD"); break;
        case 2: newType = tr("Video CD"); break;
        default: return true;
    }

    QMessageBox::StandardButton result = Dialogs::question(
        this,
        tr("Change Project Type"),
        tr("You are adding files to create a %1.\n\n"
           "Files on other tabs will be cleared.\n\n"
           "Do you want to continue?").arg(newType),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (result == QMessageBox::Yes) {
        clearOtherTabs(newTab);
        return true;
    }

    return false;
}

void MainWindow::clearOtherTabs(int keepTab) {
    m_clearingTabs = true;

    if (keepTab != 0) {
        m_dataPage->model()->clear();
    }
    if (keepTab != 1) {
        m_audioPage->model()->clear();
    }
    if (keepTab != 2) {
        m_videoPage->clearAll();
    }

    m_clearingTabs = false;
}

// Recent projects

void MainWindow::addToRecentProjects(const QString& path) {
    QSettings settings("Burner", "Burner");
    QStringList projects = settings.value("recentProjects/paths").toStringList();

    projects.removeAll(path);
    projects.prepend(path);

    while (projects.size() > MaxRecentProjects) {
        projects.removeLast();
    }

    settings.setValue("recentProjects/paths", projects);
    updateRecentProjectsMenu();
}

void MainWindow::updateRecentProjectsMenu() {
    m_recentProjectsMenu->clear();

    QSettings settings("Burner", "Burner");
    QStringList projects = settings.value("recentProjects/paths").toStringList();

    // Filter out files that no longer exist
    QStringList valid;
    for (const QString& path : projects) {
        if (QFileInfo::exists(path)) {
            valid.append(path);
        }
    }

    if (valid.size() != projects.size()) {
        settings.setValue("recentProjects/paths", valid);
    }

    m_recentProjectsMenu->setEnabled(!valid.isEmpty());

    for (int i = 0; i < valid.size(); ++i) {
        const QString& path = valid.at(i);
        QString fileName = QFileInfo(path).fileName();
        QString text = tr("&%1. %2").arg(i + 1).arg(fileName);

        QAction* action = m_recentProjectsMenu->addAction(text);
        action->setToolTip(path);
        action->setData(path);

        connect(action, &QAction::triggered, this, [this, path]() {
            openRecentProject(path);
        });
    }

    if (!valid.isEmpty()) {
        m_recentProjectsMenu->addSeparator();
        m_recentProjectsMenu->addAction(tr("&Clear Recent Projects"), this,
            &MainWindow::clearRecentProjects);
    }
}

void MainWindow::openRecentProject(const QString& path) {
    if (!QFileInfo::exists(path)) {
        Dialogs::warning(this, tr("File Not Found"),
            tr("The project file no longer exists:\n%1").arg(path));
        updateRecentProjectsMenu();
        return;
    }

    if (!maybeSave()) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        Dialogs::critical(this, tr("Error"),
            tr("Could not open file: %1").arg(file.errorString()));
        return;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (parseError.error != QJsonParseError::NoError) {
        Dialogs::critical(this, tr("Error"),
            tr("Invalid project file: %1").arg(parseError.errorString()));
        return;
    }

    if (!loadProjectFromJson(doc.object())) {
        Dialogs::critical(this, tr("Error"),
            tr("Failed to load project data."));
        return;
    }

    m_currentProjectPath = path;
    setProjectDirty(false);
    addToRecentProjects(path);
}

void MainWindow::clearRecentProjects() {
    QSettings settings("Burner", "Burner");
    settings.setValue("recentProjects/paths", QStringList());
    updateRecentProjectsMenu();
}

} // namespace Burner::UI
