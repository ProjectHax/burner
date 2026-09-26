#pragma once

#include <QDialog>
#include <QPointer>
#include <QString>

class QVBoxLayout;
class QWindow;

namespace Burner::UI {

/// Title bar preference (View > Title Bar)
enum class TitleBarMode { Auto = 0, Integrated = 1, System = 2 };

/// Whether Burner should draw its own title bar, and why.
///
/// On Wayland compositors that don't offer server-side decorations (GNOME/Mutter,
/// Weston), Qt falls back to drawing a title bar itself, and that fallback is
/// drawn in physical pixels: it ignores QT_SCALE_FACTOR and looks too small next
/// to a scaled UI. There Burner draws its own, which scales with the rest.
struct TitleBarDecision {
    bool integrated{false};
    QString reason;
};
[[nodiscard]] TitleBarDecision decideTitleBar(TitleBarMode mode);

/// Frameless-window support for a top-level widget: moving and resizing from
/// the window edges are handed to the compositor (startSystemMove/Resize), so
/// snapping, tiling and multi-monitor behavior stay native.
class WindowChrome : public QObject {
    Q_OBJECT

public:
    explicit WindowChrome(QWidget* window);
    ~WindowChrome() override;

    void setActive(bool active);
    [[nodiscard]] bool isActive() const { return m_active; }

    /// Global switch read by dialogs created later
    [[nodiscard]] static bool integratedTitleBars();
    static void setIntegratedTitleBars(bool on);

    /// 1px outline around a frameless window (skipped when maximized)
    static void paintOutline(QWidget* widget);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void attach();
    [[nodiscard]] Qt::Edges edgesAt(const QPointF& pos) const;
    void setEdgeCursor(Qt::Edges edges);

    QWidget* m_window;
    QPointer<QWindow> m_handle;
    Qt::Edges m_cursorEdges;
    bool m_overrideCursor{false};
    bool m_active{false};
};

/// A QDialog that gets Burner's title bar when integrated title bars are on.
/// Put the dialog's content into body().
class ChromeDialog : public QDialog {
    Q_OBJECT

public:
    explicit ChromeDialog(QWidget* parent, const QString& title);

    [[nodiscard]] QVBoxLayout* body() const { return m_body; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QVBoxLayout* m_body{nullptr};
    WindowChrome* m_chrome{nullptr};
    bool m_integrated{false};
};

} // namespace Burner::UI
