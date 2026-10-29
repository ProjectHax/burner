#pragma once

#include <QColor>
#include <QObject>
#include <QPalette>
#include <QString>

#include <span>

class QWidget;

namespace Burner::UI {

/// Colors that define a theme; the palette and stylesheet are derived from these.
struct ThemeColors {
    QColor window;        // Window, menu and header background
    QColor base;          // Text area background
    QColor control;       // Inputs, buttons, item views and the selected tab
    QColor alternate;     // Alternate rows and tooltips
    QColor text;
    QColor mutedText;     // Placeholders and secondary text
    QColor border;
    QColor accent;        // Selection, focus and progress
    QColor accentText;    // Text drawn on the accent color
    QColor success;
    QColor warning;
    QColor error;
    QColor burn;          // Burn Disc button
    QColor disabled;      // Disabled button background
    QColor disabledText;
};

struct ThemeInfo {
    const char* id;       // Stored in settings
    const char* name;     // Untranslated; display with ThemeManager::displayName()
    bool dark;
    ThemeColors colors;
};

/// Manages the application theme (System or a built-in theme) with palette, stylesheet, and persistence.
class ThemeManager : public QObject {
    Q_OBJECT

public:
    /// Text color for status labels, applied through the application stylesheet
    enum class Tone { Normal, Accent, Success, Warning, Error };

    /// Theme id that follows the desktop's light/dark preference
    static constexpr const char* SystemTheme = "system";

    explicit ThemeManager(QObject* parent = nullptr);

    /// Built-in themes, light themes first
    [[nodiscard]] static std::span<const ThemeInfo> themes();
    [[nodiscard]] static QString displayName(const ThemeInfo& theme);
    /// Colors of the theme currently applied to the application
    [[nodiscard]] static const ThemeColors& colors();
    /// Colors a label by status; it follows later theme changes
    static void setTone(QWidget* label, Tone tone);

    /// SystemTheme or the id of a built-in theme
    [[nodiscard]] QString currentTheme() const { return m_theme; }
    [[nodiscard]] bool isDark() const;

    void setTheme(const QString& id);
    void loadFromSettings();
    void saveToSettings() const;

signals:
    void themeChanged(const QString& id);

private:
    [[nodiscard]] bool detectSystemDarkMode() const;
    [[nodiscard]] const ThemeInfo& resolvedTheme() const;
    [[nodiscard]] static QPalette buildPalette(const ThemeColors& c);
    [[nodiscard]] static QString buildStyleSheet(const ThemeInfo& theme);

    void applyToApplication();

    QString m_theme{SystemTheme};
};

} // namespace Burner::UI
