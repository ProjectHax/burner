#pragma once

#include "widgets/WindowChrome.hpp"

#include <QMainWindow>
#include <memory>

class QPushButton;
class QMenuBar;
class QTabWidget;
class QStatusBar;
class QJsonObject;
class QActionGroup;
class QMenu;

namespace Burner::Core {
struct BurnOptions;
struct DriveInfo;
}

namespace Burner::Engine {
class QtBurnEngine;
class DriveMonitor;
}

namespace Burner::Models {
class DriveListModel;
}

namespace Burner::UI {

class ThemeManager;
class TitleBar;
class DriveSelector;
class CapacityBar;
class DataDiscPage;
class AudioCdPage;
class VideoCdPage;
class IsoWriterPage;
class DiscClonerPage;
class CdRipperPage;
class IsoBrowserPage;

/// Main application window.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(ThemeManager* themeManager, QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void changeEvent(QEvent* event) override;

private slots:
    void onDriveSelectionChanged(const QString& device);
    void onDrivesChanged();
    void onBurnClicked();
    void onSaveIsoRequested(const QString& outputPath);
    void onCloneRequested();
    void onBlankRequested(bool fullBlank);
    void onIntegrityCheckRequested(bool stopOnFirstError);
    void onPageChanged(int index);
    void refreshDrives();
    void ejectDrive(const QString& device);
    void showAbout();

    // Project management
    void newProject();
    void openProject();
    bool saveProject();
    bool saveProjectAs();

private:
    void setupUi();
    void setupMenuBar();
    void applyChrome();
    void setupToolBar();
    void connectSignals();
    void updateCapacityBar();
    void loadSettings();
    void saveSettings();

    // Project management helpers
    bool maybeSave();
    QJsonObject projectToJson() const;
    bool loadProjectFromJson(const QJsonObject& json);
    void setProjectDirty(bool dirty);
    void updateWindowTitle();

    // Project type conflict handling
    bool hasFilesOnOtherTabs(int currentTab) const;
    bool confirmProjectTypeChange(int newTab);
    void clearOtherTabs(int keepTab);

    // Engine
    std::unique_ptr<Engine::QtBurnEngine> m_engine;
    std::unique_ptr<Engine::DriveMonitor> m_driveMonitor;

    // Models
    Models::DriveListModel* m_driveModel{nullptr};

    // Widgets
    DriveSelector* m_driveSelector{nullptr};
    CapacityBar* m_capacityBar{nullptr};
    QTabWidget* m_tabWidget{nullptr};

    // Pages
    DataDiscPage* m_dataPage{nullptr};
    AudioCdPage* m_audioPage{nullptr};
    VideoCdPage* m_videoPage{nullptr};
    IsoWriterPage* m_isoPage{nullptr};
    DiscClonerPage* m_clonePage{nullptr};
    CdRipperPage* m_ripPage{nullptr};
    IsoBrowserPage* m_browserPage{nullptr};

    // Toolbar buttons
    QPushButton* m_burnButton{nullptr};

    // Theme
    ThemeManager* m_themeManager{nullptr};
    QActionGroup* m_themeActions{nullptr};

    // Title bar: the menu bar lives inside it, and it shows the window
    // controls when Burner draws its own title bar
    QMenuBar* m_menuBar{nullptr};
    TitleBar* m_titleBar{nullptr};
    WindowChrome* m_chrome{nullptr};
    QActionGroup* m_titleBarActions{nullptr};
    TitleBarMode m_titleBarMode{TitleBarMode::Auto};

    // Recent projects
    QMenu* m_recentProjectsMenu{nullptr};
    static constexpr int MaxRecentProjects = 10;
    void addToRecentProjects(const QString& path);
    void updateRecentProjectsMenu();
    void openRecentProject(const QString& path);
    void clearRecentProjects();

    // Project state
    QString m_currentProjectPath;
    bool m_projectDirty{false};
    bool m_clearingTabs{false};  // Prevent recursive project type checks
};

} // namespace Burner::UI
