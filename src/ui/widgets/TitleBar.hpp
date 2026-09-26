#pragma once

#include <QAbstractButton>
#include <QWidget>

class QHBoxLayout;
class QLabel;

namespace Burner::UI {

/// Minimize / maximize / close button, drawn with vector glyphs so it stays
/// crisp at any scale factor.
class WindowButton : public QAbstractButton {
    Q_OBJECT

public:
    enum class Kind { Minimize, Maximize, Restore, Close };

    explicit WindowButton(Kind kind, QWidget* parent = nullptr);
    void setKind(Kind kind);
    [[nodiscard]] QSize sizeHint() const override { return {34, 28}; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    Kind m_kind;
};

/// Burner's own title bar: icon, extra widgets (the main window's menu bar),
/// title and window buttons. Drag to move, double-click to maximize, right-click
/// for the window menu. It's built from normal widgets, so it scales with the
/// rest of the UI.
///
/// When not integrated, only the extra widgets are shown, so the main window
/// can keep its menu bar in here whichever title bar is in use.
class TitleBar : public QWidget {
    Q_OBJECT

public:
    enum Button { Minimize = 1, Maximize = 2, Close = 4, All = Minimize | Maximize | Close };

    explicit TitleBar(QWidget* window, int buttons = All, QWidget* parent = nullptr);

    /// Add a widget after the icon (e.g. a menu bar)
    void addWidget(QWidget* widget);

    /// Show the icon, title and window buttons, or only the added widgets
    void setIntegrated(bool integrated);
    [[nodiscard]] bool isIntegrated() const { return m_integrated; }

    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void changeEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void toggleMaximized();
    void syncState();

    QWidget* m_window;
    QHBoxLayout* m_layout{nullptr};
    QHBoxLayout* m_widgets{nullptr};
    QLabel* m_icon{nullptr};
    QLabel* m_title{nullptr};
    WindowButton* m_minimize{nullptr};
    WindowButton* m_maximize{nullptr};
    WindowButton* m_close{nullptr};
    bool m_integrated{true};
};

} // namespace Burner::UI
