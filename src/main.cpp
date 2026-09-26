#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QLibraryInfo>
#include <QDir>
#include <QIcon>

#include "ui/MainWindow.hpp"
#include "ui/ThemeManager.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // Set application metadata
    // Note: applicationName must be lowercase to match StartupWMClass in .desktop file
    app.setApplicationName("burner");
    app.setApplicationVersion(BURNER_VERSION);
    app.setOrganizationName("Burner");
    app.setOrganizationDomain("burner.app");
    app.setDesktopFileName("burner");  // For GNOME/Wayland taskbar icon

    // Set application icon (uses Qt resource system)
    QIcon appIcon;
    appIcon.addFile(":/icons/burner-16.png", QSize(16, 16));
    appIcon.addFile(":/icons/burner-22.png", QSize(22, 22));
    appIcon.addFile(":/icons/burner-24.png", QSize(24, 24));
    appIcon.addFile(":/icons/burner-32.png", QSize(32, 32));
    appIcon.addFile(":/icons/burner-48.png", QSize(48, 48));
    appIcon.addFile(":/icons/burner-64.png", QSize(64, 64));
    appIcon.addFile(":/icons/burner-128.png", QSize(128, 128));
    appIcon.addFile(":/icons/burner-256.png", QSize(256, 256));
    appIcon.addFile(":/icons/burner-512.png", QSize(512, 512));
    app.setWindowIcon(appIcon);

    // Load Qt's built-in translations for standard dialogs
    QTranslator qtTranslator;
    if (qtTranslator.load(QLocale::system(), "qt", "_",
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        app.installTranslator(&qtTranslator);
    }

    // Load application translations
    QTranslator appTranslator;
    QString locale = QLocale::system().name(); // e.g., "es_ES", "fr_FR"

    // Try to load from embedded resources first
    if (appTranslator.load(":/translations/burner_" + locale)) {
        app.installTranslator(&appTranslator);
    }
    // Try with just language code (e.g., "es" instead of "es_ES")
    else if (appTranslator.load(":/translations/burner_" + locale.left(2))) {
        app.installTranslator(&appTranslator);
    }
    // Try from translations directory (development/installed)
    else if (appTranslator.load("burner_" + locale, "translations")) {
        app.installTranslator(&appTranslator);
    }
    else if (appTranslator.load("burner_" + locale.left(2), "translations")) {
        app.installTranslator(&appTranslator);
    }
    // Try system-wide installation path
    else if (appTranslator.load("burner_" + locale, "/usr/share/burner/translations")) {
        app.installTranslator(&appTranslator);
    }

    // Apply theme before creating any widgets
    Burner::UI::ThemeManager themeManager;
    themeManager.loadFromSettings();
    themeManager.setTheme(themeManager.currentTheme());

    // Create and show main window
    Burner::UI::MainWindow mainWindow(&themeManager);
    mainWindow.show();

    return app.exec();
}
