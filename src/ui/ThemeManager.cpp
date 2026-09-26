#include "ThemeManager.hpp"

#include <QApplication>
#include <QStyleFactory>
#include <QStyleHints>
#include <QSettings>
#include <QProcess>

namespace Burner::UI {

ThemeManager::ThemeManager(QObject* parent)
    : QObject(parent) {
}

bool ThemeManager::isDark() const {
    switch (m_theme) {
        case Theme::Dark:  return true;
        case Theme::Light: return false;
        case Theme::System:
        default:           return detectSystemDarkMode();
    }
}

void ThemeManager::setTheme(Theme theme) {
    m_theme = theme;
    applyToApplication();
    emit themeChanged(theme);
}

void ThemeManager::loadFromSettings() {
    QSettings settings("Burner", "Burner");
    QString val = settings.value("appearance/theme", "system").toString();
    if (val == "dark")       m_theme = Theme::Dark;
    else if (val == "light") m_theme = Theme::Light;
    else                     m_theme = Theme::System;
}

void ThemeManager::saveToSettings() const {
    QSettings settings("Burner", "Burner");
    switch (m_theme) {
        case Theme::Dark:  settings.setValue("appearance/theme", "dark");   break;
        case Theme::Light: settings.setValue("appearance/theme", "light");  break;
        default:           settings.setValue("appearance/theme", "system"); break;
    }
}

bool ThemeManager::detectSystemDarkMode() const {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    // Qt 6.5+ native color scheme detection
    auto scheme = QGuiApplication::styleHints()->colorScheme();
    if (scheme == Qt::ColorScheme::Dark) {
        return true;
    }
#endif

    // Fallback: GNOME color-scheme setting
    QProcess process;
    process.start("gsettings", {"get", "org.gnome.desktop.interface", "color-scheme"});
    if (process.waitForFinished(500)) {
        QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        if (output.contains("dark", Qt::CaseInsensitive)) {
            return true;
        }
    }

    // Fallback: GTK theme name
    process.start("gsettings", {"get", "org.gnome.desktop.interface", "gtk-theme"});
    if (process.waitForFinished(500)) {
        QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        if (output.contains("dark", Qt::CaseInsensitive)) {
            return true;
        }
    }

    return false;
}

QPalette ThemeManager::buildDarkPalette() const {
    QPalette p;
    p.setColor(QPalette::Window,          QColor(30, 30, 46));
    p.setColor(QPalette::WindowText,      QColor(205, 214, 244));
    p.setColor(QPalette::Base,            QColor(24, 24, 37));
    p.setColor(QPalette::AlternateBase,   QColor(49, 50, 68));
    p.setColor(QPalette::ToolTipBase,     QColor(49, 50, 68));
    p.setColor(QPalette::ToolTipText,     QColor(205, 214, 244));
    p.setColor(QPalette::Text,            QColor(205, 214, 244));
    p.setColor(QPalette::Button,          QColor(49, 50, 68));
    p.setColor(QPalette::ButtonText,      QColor(205, 214, 244));
    p.setColor(QPalette::BrightText,      QColor(243, 139, 168));
    p.setColor(QPalette::Link,            QColor(137, 180, 250));
    p.setColor(QPalette::Highlight,       QColor(137, 180, 250));
    p.setColor(QPalette::HighlightedText, QColor(30, 30, 46));
    p.setColor(QPalette::PlaceholderText, QColor(108, 112, 134));
    p.setColor(QPalette::Mid,             QColor(69, 71, 90));

    p.setColor(QPalette::Disabled, QPalette::Text,       QColor(88, 91, 112));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(88, 91, 112));
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(88, 91, 112));
    return p;
}

QPalette ThemeManager::buildLightPalette() const {
    QPalette p;
    p.setColor(QPalette::Window,          QColor(239, 241, 245));
    p.setColor(QPalette::WindowText,      QColor(76, 79, 105));
    p.setColor(QPalette::Base,            QColor(255, 255, 255));
    p.setColor(QPalette::AlternateBase,   QColor(230, 233, 239));
    p.setColor(QPalette::ToolTipBase,     QColor(230, 233, 239));
    p.setColor(QPalette::ToolTipText,     QColor(76, 79, 105));
    p.setColor(QPalette::Text,            QColor(76, 79, 105));
    p.setColor(QPalette::Button,          QColor(230, 233, 239));
    p.setColor(QPalette::ButtonText,      QColor(76, 79, 105));
    p.setColor(QPalette::BrightText,      QColor(210, 15, 57));
    p.setColor(QPalette::Link,            QColor(30, 102, 245));
    p.setColor(QPalette::Highlight,       QColor(30, 102, 245));
    p.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    p.setColor(QPalette::PlaceholderText, QColor(140, 143, 161));
    p.setColor(QPalette::Mid,             QColor(188, 192, 204));

    p.setColor(QPalette::Disabled, QPalette::Text,       QColor(172, 176, 190));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(172, 176, 190));
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(172, 176, 190));
    return p;
}

QString ThemeManager::buildStyleSheet(bool dark) const {
    // %1=border     %2=hoverBg    %3=inputBg    %4=inputFocus  %5=accent
    // %6=surfaceBg  %7=tabSelBg   %8=disabledBg %9=disabledFg
    QString border     = dark ? "#45475a" : "#ccd0da";
    QString hoverBg    = dark ? "rgba(255,255,255,0.06)" : "rgba(0,0,0,0.04)";
    QString inputBg    = dark ? "#313244" : "#ffffff";
    QString inputFocus = dark ? "#89b4fa" : "#1e66f5";
    QString accent     = dark ? "#89b4fa" : "#1e66f5";
    QString surfaceBg  = dark ? "#1e1e2e" : "#eff1f5";
    QString tabSelBg   = dark ? "#313244" : "#ffffff";
    QString disabledBg = dark ? "#45475a" : "#bcc0cc";
    QString disabledFg = dark ? "#6c7086" : "#9ca0b0";

    return QStringLiteral(
        // --- Tabs ---
        "QTabWidget::pane {"
        "  border: 1px solid %1;"
        "  border-radius: 6px;"
        "  top: -1px;"
        "}"
        "QTabBar::tab {"
        "  padding: 8px 16px;"
        "  margin-right: 2px;"
        "  border: 1px solid transparent;"
        "  border-bottom: none;"
        "  border-top-left-radius: 6px;"
        "  border-top-right-radius: 6px;"
        "}"
        "QTabBar::tab:selected {"
        "  background: %7;"
        "  border-color: %1;"
        "}"
        "QTabBar::tab:!selected:hover {"
        "  background: %2;"
        "}"

        // --- Inputs ---
        "QLineEdit, QSpinBox, QComboBox {"
        "  padding: 5px 8px;"
        "  border: 1px solid %1;"
        "  border-radius: 6px;"
        "  background: %3;"
        "}"
        "QLineEdit:focus, QSpinBox:focus, QComboBox:on {"
        "  border-color: %4;"
        "}"

        // --- Combo dropdown ---
        "QComboBox::drop-down {"
        "  border: none;"
        "  padding-right: 6px;"
        "}"
        "QComboBox QAbstractItemView {"
        "  border: 1px solid %1;"
        "  border-radius: 6px;"
        "  background: %3;"
        "  selection-background-color: %5;"
        "  selection-color: white;"
        "}"

        // --- Buttons ---
        "QPushButton {"
        "  padding: 5px 14px;"
        "  border: 1px solid %1;"
        "  border-radius: 6px;"
        "  background: %3;"
        "}"
        "QPushButton:hover {"
        "  background: %2;"
        "  border-color: %5;"
        "}"
        "QPushButton:pressed {"
        "  background: %1;"
        "}"
        "QPushButton:disabled {"
        "  background: %8;"
        "  color: %9;"
        "  border-color: %8;"
        "}"

        // --- Burn button (special accent) ---
        "QPushButton#burnButton {"
        "  background-color: #e65100;"
        "  color: white;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 12px 24px;"
        "  font-size: 15px;"
        "  font-weight: bold;"
        "}"
        "QPushButton#burnButton:hover {"
        "  background-color: #ff6d00;"
        "}"
        "QPushButton#burnButton:pressed {"
        "  background-color: #bf360c;"
        "}"
        "QPushButton#burnButton:disabled {"
        "  background-color: %8;"
        "  color: %9;"
        "}"

        // --- GroupBox ---
        "QGroupBox {"
        "  border: 1px solid %1;"
        "  border-radius: 8px;"
        "  margin-top: 12px;"
        "  padding-top: 16px;"
        "  font-weight: bold;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  left: 12px;"
        "  padding: 0 6px;"
        "}"

        // --- ProgressBar ---
        "QProgressBar {"
        "  border: 1px solid %1;"
        "  border-radius: 6px;"
        "  text-align: center;"
        "  background: %3;"
        "  min-height: 18px;"
        "}"
        "QProgressBar::chunk {"
        "  background: %5;"
        "  border-radius: 5px;"
        "}"

        // --- ScrollBars ---
        "QScrollBar:vertical {"
        "  width: 10px;"
        "  background: transparent;"
        "  margin: 0;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: %1;"
        "  border-radius: 5px;"
        "  min-height: 30px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "  background: %5;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "  background: transparent;"
        "}"
        "QScrollBar:horizontal {"
        "  height: 10px;"
        "  background: transparent;"
        "  margin: 0;"
        "}"
        "QScrollBar::handle:horizontal {"
        "  background: %1;"
        "  border-radius: 5px;"
        "  min-width: 30px;"
        "}"
        "QScrollBar::handle:horizontal:hover {"
        "  background: %5;"
        "}"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
        "  width: 0;"
        "}"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {"
        "  background: transparent;"
        "}"

        // --- Toolbar ---
        "QToolBar {"
        "  border: none;"
        "  spacing: 0;"
        "  padding: 4px;"
        "}"

        // --- MenuBar ---
        "QMenuBar {"
        "  border-bottom: 1px solid %1;"
        "  padding: 2px;"
        "}"
        "QMenuBar::item {"
        "  padding: 6px 12px;"
        "  border-radius: 4px;"
        "}"
        "QMenuBar::item:selected {"
        "  background: %2;"
        "}"

        // --- Menu ---
        "QMenu {"
        "  border: 1px solid %1;"
        "  border-radius: 8px;"
        "  padding: 4px;"
        "  background: %6;"
        "}"
        "QMenu::item {"
        "  padding: 6px 24px;"
        "  border-radius: 4px;"
        "}"
        "QMenu::item:selected {"
        "  background: %5;"
        "  color: white;"
        "}"
        "QMenu::separator {"
        "  height: 1px;"
        "  background: %1;"
        "  margin: 4px 8px;"
        "}"

        // --- StatusBar ---
        "QStatusBar {"
        "  border-top: 1px solid %1;"
        "}"

        // --- TreeView / TableView / ListView ---
        "QTreeView, QTableView, QListView {"
        "  border: 1px solid %1;"
        "  border-radius: 6px;"
        "  background: %3;"
        "}"
        "QHeaderView::section {"
        "  background: %6;"
        "  border: none;"
        "  border-bottom: 1px solid %1;"
        "  border-right: 1px solid %1;"
        "  padding: 6px 8px;"
        "  font-weight: bold;"
        "}"

        // --- CheckBox / RadioButton ---
        "QCheckBox::indicator, QRadioButton::indicator {"
        "  width: 16px;"
        "  height: 16px;"
        "}"

        // --- ToolTip ---
        "QToolTip {"
        "  border: 1px solid %1;"
        "  border-radius: 4px;"
        "  padding: 4px 8px;"
        "  background: %6;"
        "}"

        // --- TextEdit (BurnLog etc.) ---
        "QTextEdit {"
        "  border: 1px solid %1;"
        "  border-radius: 6px;"
        "  background: %3;"
        "}"
    ).arg(border, hoverBg, inputBg, inputFocus, accent, surfaceBg,
          tabSelBg, disabledBg, disabledFg);
}

void ThemeManager::applyToApplication() {
    auto* app = qApp;
    app->setStyle(QStyleFactory::create("Fusion"));

    bool dark = isDark();
    app->setPalette(dark ? buildDarkPalette() : buildLightPalette());
    app->setStyleSheet(buildStyleSheet(dark));
}

} // namespace Burner::UI
