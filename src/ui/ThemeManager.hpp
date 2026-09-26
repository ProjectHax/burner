#pragma once

#include <QObject>
#include <QPalette>
#include <QString>

namespace Burner::UI {

/// Manages application theme (System/Light/Dark) with palette, stylesheet, and persistence.
class ThemeManager : public QObject {
    Q_OBJECT

public:
    enum class Theme { System, Light, Dark };
    Q_ENUM(Theme)

    explicit ThemeManager(QObject* parent = nullptr);

    [[nodiscard]] Theme currentTheme() const { return m_theme; }
    [[nodiscard]] bool isDark() const;

    void setTheme(Theme theme);
    void loadFromSettings();
    void saveToSettings() const;

signals:
    void themeChanged(Theme theme);

private:
    [[nodiscard]] bool detectSystemDarkMode() const;
    [[nodiscard]] QPalette buildDarkPalette() const;
    [[nodiscard]] QPalette buildLightPalette() const;
    [[nodiscard]] QString buildStyleSheet(bool dark) const;

    void applyToApplication();

    Theme m_theme{Theme::System};
};

} // namespace Burner::UI
