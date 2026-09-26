#include "WindowChrome.hpp"
#include "TitleBar.hpp"

#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QVBoxLayout>
#include <QWindow>

#include <optional>

#ifdef BURNER_HAVE_WAYLAND_CLIENT
#include <wayland-client.h>
#include <cstring>
#endif

namespace Burner::UI {

namespace {

constexpr int EdgeSize = 6;     // Resize band, logical pixels
constexpr int CornerSize = 14;  // Larger grab area at the corners

bool g_integrated = false;

/// Does the Wayland compositor offer server-side decorations (xdg-decoration)?
/// Asked once on a separate, short-lived connection so Qt's own is untouched.
std::optional<bool> compositorDrawsDecorations() {
#ifdef BURNER_HAVE_WAYLAND_CLIENT
    static std::optional<bool> cached;
    static bool asked = false;
    if (asked) {
        return cached;
    }
    asked = true;

    wl_display* display = wl_display_connect(nullptr);
    if (!display) {
        return cached;
    }
    bool serverSide = false;
    static const wl_registry_listener listener = {
        [](void* data, wl_registry*, uint32_t, const char* interface, uint32_t) {
            if (!std::strcmp(interface, "zxdg_decoration_manager_v1") ||
                !std::strcmp(interface, "org_kde_kwin_server_decoration_manager")) {
                *static_cast<bool*>(data) = true;
            }
        },
        [](void*, wl_registry*, uint32_t) {}};
    wl_registry* registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &listener, &serverSide);
    if (wl_display_roundtrip(display) >= 0) {
        cached = serverSide;
    }
    wl_registry_destroy(registry);
    wl_display_disconnect(display);
    return cached;
#else
    return std::nullopt;
#endif
}

} // namespace

TitleBarDecision decideTitleBar(TitleBarMode mode) {
    if (mode == TitleBarMode::Integrated) {
        return {true, QObject::tr("Burner's own title bar (chosen in View > Title Bar)")};
    }
    if (mode == TitleBarMode::System) {
        return {false, QObject::tr("the system's title bar (chosen in View > Title Bar)")};
    }

    const QString platform = QGuiApplication::platformName();
    if (!platform.startsWith(QLatin1String("wayland"))) {
        return {false, QObject::tr("the window manager's title bar (%1 session)").arg(platform)};
    }
    if (qEnvironmentVariableIntValue("QT_WAYLAND_DISABLE_WINDOWDECORATION") > 0) {
        return {true, QObject::tr("Burner's own title bar (Qt window decorations are disabled)")};
    }
    std::optional<bool> serverSide = compositorDrawsDecorations();
    if (!serverSide) {
        // No way to ask: GNOME is the common compositor without server-side decorations
        serverSide = !qEnvironmentVariable("XDG_CURRENT_DESKTOP").contains(QLatin1String("GNOME"), Qt::CaseInsensitive);
    }
    if (*serverSide) {
        return {false, QObject::tr("the compositor's title bar")};
    }
    return {true, QObject::tr("Burner's own title bar: this compositor doesn't draw title bars for Qt apps, "
                              "and Qt's fallback ignores QT_SCALE_FACTOR")};
}

// ------------------------------------------------------------------ WindowChrome

bool WindowChrome::integratedTitleBars() {
    return g_integrated;
}

void WindowChrome::setIntegratedTitleBars(bool on) {
    g_integrated = on;
}

WindowChrome::WindowChrome(QWidget* window)
    : QObject(window)
    , m_window(window) {
    m_window->installEventFilter(this);
}

WindowChrome::~WindowChrome() {
    // The window can close with the pointer on its edge (e.g. a dialog
    // dismissed with Enter); don't leave the resize cursor behind
    setEdgeCursor({});
}

void WindowChrome::setActive(bool active) {
    m_active = active;
    if (!active) {
        setEdgeCursor({});
    }
    attach();
    m_window->update();
}

void WindowChrome::attach() {
    // The native window is recreated when window flags change: follow it
    QWindow* handle = m_window->windowHandle();
    if (handle == m_handle) {
        return;
    }
    if (m_handle) {
        m_handle->removeEventFilter(this);
    }
    m_handle = handle;
    if (m_handle) {
        m_handle->installEventFilter(this);
    }
}

Qt::Edges WindowChrome::edgesAt(const QPointF& pos) const {
    if (!m_handle) {
        return {};
    }
    const double w = m_handle->width();
    const double h = m_handle->height();
    const double x = pos.x();
    const double y = pos.y();
    const bool nearLeft = x < EdgeSize;
    const bool nearRight = x >= w - EdgeSize;
    const bool nearTop = y < EdgeSize;
    const bool nearBottom = y >= h - EdgeSize;

    Qt::Edges edges;
    if (nearLeft || (x < CornerSize && (nearTop || nearBottom))) {
        edges |= Qt::LeftEdge;
    }
    if (nearRight || (x >= w - CornerSize && (nearTop || nearBottom))) {
        edges |= Qt::RightEdge;
    }
    if (nearTop || (y < CornerSize && (nearLeft || nearRight))) {
        edges |= Qt::TopEdge;
    }
    if (nearBottom || (y >= h - CornerSize && (nearLeft || nearRight))) {
        edges |= Qt::BottomEdge;
    }
    return edges;
}

void WindowChrome::setEdgeCursor(Qt::Edges edges) {
    if (edges == m_cursorEdges) {
        return;
    }
    m_cursorEdges = edges;

    Qt::CursorShape shape = Qt::ArrowCursor;
    if (edges == (Qt::LeftEdge | Qt::TopEdge) || edges == (Qt::RightEdge | Qt::BottomEdge)) {
        shape = Qt::SizeFDiagCursor;
    } else if (edges == (Qt::RightEdge | Qt::TopEdge) || edges == (Qt::LeftEdge | Qt::BottomEdge)) {
        shape = Qt::SizeBDiagCursor;
    } else if (edges & (Qt::LeftEdge | Qt::RightEdge)) {
        shape = Qt::SizeHorCursor;
    } else if (edges & (Qt::TopEdge | Qt::BottomEdge)) {
        shape = Qt::SizeVerCursor;
    }

    // An override cursor wins over whatever child widget is under the mouse
    if (edges) {
        if (m_overrideCursor) {
            QGuiApplication::changeOverrideCursor(shape);
        } else {
            QGuiApplication::setOverrideCursor(shape);
        }
        m_overrideCursor = true;
    } else if (m_overrideCursor) {
        QGuiApplication::restoreOverrideCursor();
        m_overrideCursor = false;
    }
}

bool WindowChrome::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_window) {
        if (event->type() == QEvent::Show || event->type() == QEvent::WinIdChange) {
            attach();
        }
        return false;
    }
    if (watched != m_handle || !m_active) {
        return false;
    }

    const bool resizable = !(m_window->windowState() & (Qt::WindowMaximized | Qt::WindowFullScreen)) &&
                           m_window->minimumSize() != m_window->maximumSize();
    switch (event->type()) {
        case QEvent::MouseMove: {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->buttons() == Qt::NoButton) {
                setEdgeCursor(resizable ? edgesAt(mouseEvent->position()) : Qt::Edges{});
            }
            break;
        }
        case QEvent::MouseButtonPress: {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (resizable && mouseEvent->button() == Qt::LeftButton) {
                const Qt::Edges edges = edgesAt(mouseEvent->position());
                if (edges && m_handle->startSystemResize(edges)) {
                    setEdgeCursor({});
                    return true;  // The compositor owns this drag now
                }
            }
            break;
        }
        case QEvent::Leave:
            setEdgeCursor({});
            break;
        default:
            break;
    }
    return false;
}

void WindowChrome::paintOutline(QWidget* widget) {
    if (widget->windowState() & (Qt::WindowMaximized | Qt::WindowFullScreen)) {
        return;
    }
    QPainter p(widget);
    p.setPen(QPen(widget->palette().color(QPalette::Mid), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF(widget->rect()).adjusted(0.5, 0.5, -0.5, -0.5));
}

// ------------------------------------------------------------------ ChromeDialog

ChromeDialog::ChromeDialog(QWidget* parent, const QString& title)
    : QDialog(parent) {
    setWindowTitle(title);
    m_integrated = WindowChrome::integratedTitleBars();

    auto* outer = new QVBoxLayout(this);
    outer->setSpacing(0);
    auto* content = new QWidget;
    m_body = new QVBoxLayout(content);
    // Keep the margins the dialog's own top-level layout would have had
    m_body->setContentsMargins(style()->pixelMetric(QStyle::PM_LayoutLeftMargin, nullptr, this),
                               style()->pixelMetric(QStyle::PM_LayoutTopMargin, nullptr, this),
                               style()->pixelMetric(QStyle::PM_LayoutRightMargin, nullptr, this),
                               style()->pixelMetric(QStyle::PM_LayoutBottomMargin, nullptr, this));

    if (m_integrated) {
        setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        outer->setContentsMargins(1, 1, 1, 1);  // Room for the outline
        outer->addWidget(new TitleBar(this, TitleBar::Close));
        m_chrome = new WindowChrome(this);
        m_chrome->setActive(true);
    } else {
        outer->setContentsMargins(0, 0, 0, 0);
    }
    outer->addWidget(content, 1);
}

void ChromeDialog::paintEvent(QPaintEvent* event) {
    QDialog::paintEvent(event);
    if (m_integrated) {
        WindowChrome::paintOutline(this);
    }
}

} // namespace Burner::UI
