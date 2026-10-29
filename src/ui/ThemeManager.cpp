#include "ThemeManager.hpp"

#include <QApplication>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>
#include <QSettings>
#include <QProcess>
#include <QWidget>

#include <algorithm>
#include <utility>
#include <vector>

namespace Burner::UI {

namespace {

// Light themes first; System resolves to "light" or "dark"
const ThemeInfo kThemes[] = {
    {"light", QT_TRANSLATE_NOOP("Burner::UI::ThemeManager", "Light"), false, {
        .window = 0xeff1f5, .base = 0xffffff, .control = 0xffffff, .alternate = 0xe6e9ef,
        .text = 0x4c4f69, .mutedText = 0x64677d, .border = 0xccd0da,
        .accent = 0x1e66f5, .accentText = 0xffffff,
        .success = 0x2b8a3e, .warning = 0xb45309, .error = 0xd20f39,
        .burn = 0xe65100, .disabled = 0xbcc0cc, .disabledText = 0x9ca0b0}},
    {"paper", QT_TRANSLATE_NOOP("Burner::UI::ThemeManager", "Paper"), false, {
        .window = 0xf4eee3, .base = 0xfffcf6, .control = 0xfffcf6, .alternate = 0xebe3d4,
        .text = 0x3b3128, .mutedText = 0x72645a, .border = 0xd6c9b4,
        .accent = 0x2d5f8b, .accentText = 0xffffff,
        .success = 0x4d7a26, .warning = 0xa8650f, .error = 0xb8322a,
        .burn = 0xd84315, .disabled = 0xdcd2c1, .disabledText = 0xa39580}},
    {"frost", QT_TRANSLATE_NOOP("Burner::UI::ThemeManager", "Frost"), false, {
        .window = 0xedf3f7, .base = 0xffffff, .control = 0xffffff, .alternate = 0xdfe9f0,
        .text = 0x1d3140, .mutedText = 0x566d7d, .border = 0xc5d5e0,
        .accent = 0x0b7480, .accentText = 0xffffff,
        .success = 0x23824a, .warning = 0xa96400, .error = 0xc0303b,
        .burn = 0xe65100, .disabled = 0xd0dde6, .disabledText = 0x93a8b6}},
    {"sage", QT_TRANSLATE_NOOP("Burner::UI::ThemeManager", "Sage"), false, {
        .window = 0xedf1ea, .base = 0xfbfcf9, .control = 0xffffff, .alternate = 0xe0e7dc,
        .text = 0x2a382c, .mutedText = 0x5d6e5c, .border = 0xc6d2c1,
        .accent = 0x37734a, .accentText = 0xffffff,
        .success = 0x2f7a3a, .warning = 0xa35f00, .error = 0xb93a2f,
        .burn = 0xe65100, .disabled = 0xd3dccf, .disabledText = 0x98a695}},
    {"blossom", QT_TRANSLATE_NOOP("Burner::UI::ThemeManager", "Blossom"), false, {
        .window = 0xfaf0f2, .base = 0xfffafb, .control = 0xffffff, .alternate = 0xf2e2e6,
        .text = 0x43303a, .mutedText = 0x7b5e6e, .border = 0xe6ccd3,
        .accent = 0xb83a63, .accentText = 0xffffff,
        .success = 0x3a8050, .warning = 0xab6a08, .error = 0xc22b2b,
        .burn = 0xe65100, .disabled = 0xead8dd, .disabledText = 0xad95a0}},
    {"dark", QT_TRANSLATE_NOOP("Burner::UI::ThemeManager", "Dark"), true, {
        .window = 0x1e1e2e, .base = 0x181825, .control = 0x313244, .alternate = 0x313244,
        .text = 0xcdd6f4, .mutedText = 0x9ca2ba, .border = 0x45475a,
        .accent = 0x89b4fa, .accentText = 0x1e1e2e,
        .success = 0xa6e3a1, .warning = 0xf9e2af, .error = 0xf38ba8,
        .burn = 0xe65100, .disabled = 0x45475a, .disabledText = 0x6c7086}},
    {"ember", QT_TRANSLATE_NOOP("Burner::UI::ThemeManager", "Ember"), true, {
        .window = 0x1c1917, .base = 0x151311, .control = 0x2a2522, .alternate = 0x2a2522,
        .text = 0xeee5dc, .mutedText = 0x9d9189, .border = 0x433b36,
        .accent = 0xff9248, .accentText = 0x1c1917,
        .success = 0x9bd67f, .warning = 0xf5d06f, .error = 0xff6b7f,
        .burn = 0xe65100, .disabled = 0x433b36, .disabledText = 0x6f655e}},
    {"graphite", QT_TRANSLATE_NOOP("Burner::UI::ThemeManager", "Graphite"), true, {
        .window = 0x1f2023, .base = 0x17181a, .control = 0x2c2d31, .alternate = 0x2c2d31,
        .text = 0xe4e4e7, .mutedText = 0x9b9da4, .border = 0x42444a,
        .accent = 0x7eaaff, .accentText = 0x141518,
        .success = 0x7fd99a, .warning = 0xf0c463, .error = 0xff7d7d,
        .burn = 0xe65100, .disabled = 0x42444a, .disabledText = 0x696b72}},
    {"ocean", QT_TRANSLATE_NOOP("Burner::UI::ThemeManager", "Ocean"), true, {
        .window = 0x0e1a24, .base = 0x0a131b, .control = 0x172736, .alternate = 0x172736,
        .text = 0xd5e5f1, .mutedText = 0x7d95a8, .border = 0x24394c,
        .accent = 0x4fd3c4, .accentText = 0x0a131b,
        .success = 0x72e3a5, .warning = 0xf4c27a, .error = 0xff7f8c,
        .burn = 0xe65100, .disabled = 0x24394c, .disabledText = 0x51687a}},
    {"amethyst", QT_TRANSLATE_NOOP("Burner::UI::ThemeManager", "Amethyst"), true, {
        .window = 0x1c1726, .base = 0x15111d, .control = 0x282038, .alternate = 0x282038,
        .text = 0xe9e1f6, .mutedText = 0x9b90b2, .border = 0x3b3252,
        .accent = 0xc4a0ff, .accentText = 0x1c1726,
        .success = 0x8fe3b2, .warning = 0xf4c87d, .error = 0xff7fa6,
        .burn = 0xe65100, .disabled = 0x3b3252, .disabledText = 0x665b7c}},
};

const ThemeColors* s_activeColors = &kThemes[0].colors;

const ThemeInfo* findTheme(const QString& id) {
    for (const ThemeInfo& theme : kThemes) {
        if (id == QLatin1String(theme.id)) {
            return &theme;
        }
    }
    return nullptr;
}

// Blends from a toward b; t = 0 gives a
QColor mix(const QColor& a, const QColor& b, float t) {
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                            a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t);
}

} // namespace

ThemeManager::ThemeManager(QObject* parent)
    : QObject(parent) {
}

std::span<const ThemeInfo> ThemeManager::themes() {
    return kThemes;
}

QString ThemeManager::displayName(const ThemeInfo& theme) {
    return tr(theme.name);
}

const ThemeColors& ThemeManager::colors() {
    return *s_activeColors;
}

void ThemeManager::setTone(QWidget* label, Tone tone) {
    QString name;
    switch (tone) {
        case Tone::Normal:  break;
        case Tone::Accent:  name = QStringLiteral("accent");  break;
        case Tone::Success: name = QStringLiteral("success"); break;
        case Tone::Warning: name = QStringLiteral("warning"); break;
        case Tone::Error:   name = QStringLiteral("error");   break;
    }
    label->setProperty("tone", name);
    // Stylesheet property selectors are only re-evaluated on polish
    label->style()->unpolish(label);
    label->style()->polish(label);
}

bool ThemeManager::isDark() const {
    return resolvedTheme().dark;
}

void ThemeManager::setTheme(const QString& id) {
    m_theme = findTheme(id) ? id : QString(SystemTheme);
    applyToApplication();
    emit themeChanged(m_theme);
}

void ThemeManager::loadFromSettings() {
    QSettings settings("Burner", "Burner");
    QString id = settings.value("appearance/theme", SystemTheme).toString();
    m_theme = findTheme(id) ? id : QString(SystemTheme);
}

void ThemeManager::saveToSettings() const {
    QSettings settings("Burner", "Burner");
    settings.setValue("appearance/theme", m_theme);
}

const ThemeInfo& ThemeManager::resolvedTheme() const {
    QString id = m_theme;
    if (id == SystemTheme) {
        id = detectSystemDarkMode() ? QStringLiteral("dark") : QStringLiteral("light");
    }
    const ThemeInfo* theme = findTheme(id);
    return theme ? *theme : kThemes[0];
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

QPalette ThemeManager::buildPalette(const ThemeColors& c) {
    QPalette p;
    p.setColor(QPalette::Window,          c.window);
    p.setColor(QPalette::WindowText,      c.text);
    p.setColor(QPalette::Base,            c.base);
    p.setColor(QPalette::AlternateBase,   c.alternate);
    p.setColor(QPalette::ToolTipBase,     c.alternate);
    p.setColor(QPalette::ToolTipText,     c.text);
    p.setColor(QPalette::Text,            c.text);
    p.setColor(QPalette::Button,          c.alternate);
    p.setColor(QPalette::ButtonText,      c.text);
    p.setColor(QPalette::BrightText,      c.error);
    p.setColor(QPalette::Link,            c.accent);
    p.setColor(QPalette::Highlight,       c.accent);
    p.setColor(QPalette::HighlightedText, c.accentText);
    p.setColor(QPalette::PlaceholderText, c.mutedText);
    p.setColor(QPalette::Mid,             c.border);

    p.setColor(QPalette::Disabled, QPalette::Text,       c.disabledText);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, c.disabledText);
    p.setColor(QPalette::Disabled, QPalette::WindowText, c.disabledText);
    return p;
}

QString ThemeManager::buildStyleSheet(const ThemeInfo& theme) {
    QString css = QStringLiteral(
        // --- Tabs ---
        "QTabWidget::pane {"
        "  border: 1px solid @border;"
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
        "  background: @control;"
        "  border-color: @border;"
        "}"
        "QTabBar::tab:!selected:hover {"
        "  background: @hover;"
        "}"

        // --- Inputs ---
        "QLineEdit, QSpinBox, QComboBox {"
        "  padding: 5px 8px;"
        "  border: 1px solid @border;"
        "  border-radius: 6px;"
        "  background: @control;"
        "}"
        "QLineEdit:focus, QSpinBox:focus, QComboBox:on {"
        "  border-color: @accent;"
        "}"

        // --- Combo dropdown ---
        "QComboBox::drop-down {"
        "  border: none;"
        "  padding-right: 6px;"
        "}"
        "QComboBox QAbstractItemView {"
        "  border: 1px solid @border;"
        "  border-radius: 6px;"
        "  background: @control;"
        "  selection-background-color: @accent;"
        "  selection-color: @accentText;"
        "}"

        // --- Buttons ---
        "QPushButton {"
        "  padding: 5px 14px;"
        "  border: 1px solid @border;"
        "  border-radius: 6px;"
        "  background: @control;"
        "}"
        "QPushButton:hover {"
        "  background: @hover;"
        "  border-color: @accent;"
        "}"
        "QPushButton:pressed {"
        "  background: @border;"
        "}"
        "QPushButton:disabled {"
        "  background: @disabled;"
        "  color: @disabledText;"
        "  border-color: @disabled;"
        "}"

        // --- Burn button (special accent) ---
        "QPushButton#burnButton {"
        "  background-color: @burn;"
        "  color: white;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 12px 24px;"
        "  font-size: 15px;"
        "  font-weight: bold;"
        "}"
        "QPushButton#burnButton:hover {"
        "  background-color: @burnHover;"
        "}"
        "QPushButton#burnButton:pressed {"
        "  background-color: @burnPressed;"
        "}"
        "QPushButton#burnButton:disabled {"
        "  background-color: @disabled;"
        "  color: @disabledText;"
        "}"

        // --- GroupBox ---
        "QGroupBox {"
        "  border: 1px solid @border;"
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
        "  border: 1px solid @border;"
        "  border-radius: 6px;"
        "  text-align: center;"
        "  background: @control;"
        "  min-height: 18px;"
        "}"
        "QProgressBar::chunk {"
        "  background: @accent;"
        "  border-radius: 5px;"
        "}"

        // --- ScrollBars ---
        "QScrollBar:vertical {"
        "  width: 10px;"
        "  background: transparent;"
        "  margin: 0;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: @border;"
        "  border-radius: 5px;"
        "  min-height: 30px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "  background: @accent;"
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
        "  background: @border;"
        "  border-radius: 5px;"
        "  min-width: 30px;"
        "}"
        "QScrollBar::handle:horizontal:hover {"
        "  background: @accent;"
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
        "  border-bottom: 1px solid @border;"
        "  padding: 2px;"
        "}"
        "QMenuBar::item {"
        "  padding: 6px 12px;"
        "  border-radius: 4px;"
        "}"
        "QMenuBar::item:selected {"
        "  background: @hover;"
        "}"

        // --- Menu ---
        "QMenu {"
        "  border: 1px solid @border;"
        "  border-radius: 8px;"
        "  padding: 4px;"
        "  background: @window;"
        "}"
        "QMenu::item {"
        "  padding: 6px 24px;"
        "  border-radius: 4px;"
        "}"
        "QMenu::item:selected {"
        "  background: @accent;"
        "  color: @accentText;"
        "}"
        "QMenu::separator {"
        "  height: 1px;"
        "  background: @border;"
        "  margin: 4px 8px;"
        "}"

        // --- StatusBar ---
        "QStatusBar {"
        "  border-top: 1px solid @border;"
        "}"

        // --- TreeView / TableView / ListView ---
        "QTreeView, QTableView, QListView {"
        "  border: 1px solid @border;"
        "  border-radius: 6px;"
        "  background: @control;"
        "}"
        "QHeaderView::section {"
        "  background: @window;"
        "  border: none;"
        "  border-bottom: 1px solid @border;"
        "  border-right: 1px solid @border;"
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
        "  border: 1px solid @border;"
        "  border-radius: 4px;"
        "  padding: 4px 8px;"
        "  background: @window;"
        "}"

        // --- TextEdit (BurnLog etc.) ---
        "QTextEdit {"
        "  border: 1px solid @border;"
        "  border-radius: 6px;"
        "  background: @control;"
        "}"

        // --- Status labels (ThemeManager::setTone) ---
        "QLabel[tone=\"accent\"] { color: @accent; }"
        "QLabel[tone=\"success\"] { color: @success; }"
        "QLabel[tone=\"warning\"] { color: @warning; }"
        "QLabel[tone=\"error\"] { color: @error; }"

        // --- Multi-session banner (DataDiscPage) ---
        "QFrame#sessionBanner {"
        "  background: @accentTint;"
        "  border: 1px solid @accentBorder;"
        "  border-radius: 4px;"
        "  padding: 6px;"
        "}"
    );

    const ThemeColors& c = theme.colors;
    std::vector<std::pair<QString, QString>> vars = {
        {"@window",       c.window.name()},
        {"@control",      c.control.name()},
        {"@border",       c.border.name()},
        {"@hover",        theme.dark ? "rgba(255,255,255,0.06)" : "rgba(0,0,0,0.04)"},
        {"@accent",       c.accent.name()},
        {"@accentText",   c.accentText.name()},
        {"@accentTint",   mix(c.window, c.accent, 0.12f).name()},
        {"@accentBorder", mix(c.window, c.accent, 0.45f).name()},
        {"@success",      c.success.name()},
        {"@warning",      c.warning.name()},
        {"@error",        c.error.name()},
        {"@burn",         c.burn.name()},
        {"@burnHover",    c.burn.lighter(115).name()},
        {"@burnPressed",  c.burn.darker(120).name()},
        {"@disabled",     c.disabled.name()},
        {"@disabledText", c.disabledText.name()},
    };
    // Longest names first so "@accent" doesn't replace the start of "@accentText"
    std::sort(vars.begin(), vars.end(), [](const auto& a, const auto& b) {
        return a.first.size() > b.first.size();
    });
    for (const auto& [name, value] : vars) {
        css.replace(name, value);
    }
    return css;
}

void ThemeManager::applyToApplication() {
    const ThemeInfo& theme = resolvedTheme();
    s_activeColors = &theme.colors;

    auto* app = qApp;
    app->setStyle(QStyleFactory::create("Fusion"));
    app->setPalette(buildPalette(theme.colors));
    app->setStyleSheet(buildStyleSheet(theme));
}

} // namespace Burner::UI
