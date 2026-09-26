#include "TitleBar.hpp"

#include <QApplication>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QWindow>

#include <algorithm>

namespace Burner::UI {

// ------------------------------------------------------------------ WindowButton

WindowButton::WindowButton(Kind kind, QWidget* parent)
    : QAbstractButton(parent)
    , m_kind(kind) {
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::ArrowCursor);
}

void WindowButton::setKind(Kind kind) {
    m_kind = kind;
    update();
}

void WindowButton::enterEvent(QEnterEvent* event) {
    QAbstractButton::enterEvent(event);
    update();
}

void WindowButton::leaveEvent(QEvent* event) {
    QAbstractButton::leaveEvent(event);
    update();
}

void WindowButton::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const bool hot = underMouse();
    const bool down = isDown();
    const QColor text = palette().color(QPalette::WindowText);

    // Round hover disc (red for close), like GNOME/Windows window buttons
    const double d = std::min(width(), height()) - 4.0;
    const QRectF disc((width() - d) / 2.0, (height() - d) / 2.0, d, d);
    if (hot || down) {
        QColor bg;
        if (m_kind == Kind::Close) {
            bg = down ? QColor(0xe5, 0x48, 0x4d).darker(115) : QColor(0xe5, 0x48, 0x4d);
        } else {
            bg = text;
            bg.setAlphaF(down ? 0.20 : 0.12);
        }
        p.setPen(Qt::NoPen);
        p.setBrush(bg);
        p.drawEllipse(disc);
    }

    QColor fg = text;
    if (m_kind == Kind::Close && (hot || down)) {
        fg = Qt::white;
    } else if (!window()->isActiveWindow() && !hot) {
        fg = palette().color(QPalette::PlaceholderText);
    }
    QPen pen(fg, 1.25);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    const QPointF c = disc.center();
    const double g = d * 0.18;  // Glyph half-size
    switch (m_kind) {
        case Kind::Minimize:
            p.drawLine(QPointF(c.x() - g, c.y() + 0.5), QPointF(c.x() + g, c.y() + 0.5));
            break;
        case Kind::Maximize:
            p.drawRoundedRect(QRectF(c.x() - g, c.y() - g, 2 * g, 2 * g), 1.5, 1.5);
            break;
        case Kind::Restore:
            p.drawRoundedRect(QRectF(c.x() - g, c.y() - g + 2, 2 * g - 2, 2 * g - 2), 1.2, 1.2);
            p.drawPolyline(QPolygonF{QPointF(c.x() - g + 2, c.y() - g),
                                     QPointF(c.x() + g, c.y() - g),
                                     QPointF(c.x() + g, c.y() + g - 2)});
            break;
        case Kind::Close:
            p.drawLine(QPointF(c.x() - g, c.y() - g), QPointF(c.x() + g, c.y() + g));
            p.drawLine(QPointF(c.x() - g, c.y() + g), QPointF(c.x() + g, c.y() - g));
            break;
    }
}

// ------------------------------------------------------------------ TitleBar

TitleBar::TitleBar(QWidget* window, int buttons, QWidget* parent)
    : QWidget(parent)
    , m_window(window) {
    m_layout = new QHBoxLayout(this);

    m_icon = new QLabel;
    m_icon->setFixedSize(20, 20);
    m_layout->addWidget(m_icon);

    m_widgets = new QHBoxLayout;
    m_widgets->setSpacing(0);
    m_layout->addLayout(m_widgets);

    // The title sits between two stretches; they collapse when not integrated
    // so the added widgets (the menu bar) span the full width
    m_layout->addStretch(1);
    m_title = new QLabel;
    QFont font = m_title->font();
    font.setWeight(QFont::DemiBold);
    m_title->setFont(font);
    m_layout->addWidget(m_title);
    m_layout->addStretch(1);

    if (buttons & Minimize) {
        m_minimize = new WindowButton(WindowButton::Kind::Minimize);
        m_minimize->setToolTip(tr("Minimize"));
        connect(m_minimize, &QAbstractButton::clicked, m_window, &QWidget::showMinimized);
        m_layout->addWidget(m_minimize);
    }
    if (buttons & Maximize) {
        m_maximize = new WindowButton(WindowButton::Kind::Maximize);
        connect(m_maximize, &QAbstractButton::clicked, this, &TitleBar::toggleMaximized);
        m_layout->addWidget(m_maximize);
    }
    if (buttons & Close) {
        m_close = new WindowButton(WindowButton::Kind::Close);
        m_close->setToolTip(tr("Close"));
        connect(m_close, &QAbstractButton::clicked, m_window, &QWidget::close);
        m_layout->addWidget(m_close);
    }

    m_window->installEventFilter(this);
    setIntegrated(true);
}

void TitleBar::addWidget(QWidget* widget) {
    m_widgets->addWidget(widget);
}

void TitleBar::setIntegrated(bool integrated) {
    m_integrated = integrated;
    m_icon->setVisible(integrated);
    m_title->setVisible(integrated);
    for (auto* button : {m_minimize, m_maximize, m_close}) {
        if (button) {
            button->setVisible(integrated);
        }
    }

    m_layout->setContentsMargins(integrated ? QMargins(10, 4, 6, 4) : QMargins());
    m_layout->setSpacing(integrated ? 4 : 0);
    for (int i = 0; i < m_layout->count(); ++i) {
        if (QSpacerItem* spacer = m_layout->itemAt(i)->spacerItem()) {
            spacer->changeSize(0, 0, integrated ? QSizePolicy::Expanding : QSizePolicy::Fixed,
                               QSizePolicy::Minimum);
        }
    }
    m_layout->setStretchFactor(m_widgets, integrated ? 0 : 1);
    m_layout->invalidate();

    syncState();
    updateGeometry();
}

QSize TitleBar::sizeHint() const {
    QSize size = QWidget::sizeHint();
    if (m_integrated) {
        size.setHeight(std::max(size.height(), 40));
    }
    return size;
}

void TitleBar::syncState() {
    m_title->setText(m_window->windowTitle());
    const QIcon icon = m_window->windowIcon().isNull() ? QApplication::windowIcon() : m_window->windowIcon();
    m_icon->setPixmap(icon.pixmap(QSize(20, 20), devicePixelRatioF()));

    if (m_maximize) {
        const bool maximized = m_window->windowState() & Qt::WindowMaximized;
        m_maximize->setKind(maximized ? WindowButton::Kind::Restore : WindowButton::Kind::Maximize);
        m_maximize->setToolTip(maximized ? tr("Restore") : tr("Maximize"));
    }

    QPalette titlePalette = m_title->palette();
    titlePalette.setColor(QPalette::WindowText,
        palette().color(m_window->isActiveWindow() ? QPalette::WindowText : QPalette::PlaceholderText));
    m_title->setPalette(titlePalette);
    update();
}

bool TitleBar::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_window) {
        switch (event->type()) {
            case QEvent::WindowStateChange:
            case QEvent::WindowTitleChange:
            case QEvent::WindowIconChange:
            case QEvent::ActivationChange:
                syncState();
                break;
            default:
                break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void TitleBar::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    // Follow theme changes
    if (event->type() == QEvent::PaletteChange) {
        syncState();
    }
}

void TitleBar::paintEvent(QPaintEvent*) {
    if (!m_integrated) {
        return;
    }
    QPainter p(this);
    p.fillRect(rect(), palette().color(QPalette::Window));
    p.setPen(palette().color(QPalette::Mid));
    p.drawLine(QPointF(0, height() - 0.5), QPointF(width(), height() - 0.5));
}

void TitleBar::toggleMaximized() {
    if (m_window->windowState() & Qt::WindowMaximized) {
        m_window->showNormal();
    } else {
        m_window->showMaximized();
    }
}

void TitleBar::mousePressEvent(QMouseEvent* event) {
    // Empty title bar space moves the window (the compositor handles snapping)
    if (m_integrated && event->button() == Qt::LeftButton && m_window->windowHandle()) {
        m_window->windowHandle()->startSystemMove();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent* event) {
    if (m_integrated && event->button() == Qt::LeftButton && m_maximize) {
        toggleMaximized();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void TitleBar::contextMenuEvent(QContextMenuEvent* event) {
    if (!m_integrated) {
        event->ignore();
        return;
    }
    QMenu menu(this);
    if (m_minimize) {
        menu.addAction(tr("Minimize"), m_window, &QWidget::showMinimized);
    }
    if (m_maximize) {
        const bool maximized = m_window->windowState() & Qt::WindowMaximized;
        menu.addAction(maximized ? tr("Restore") : tr("Maximize"), this, &TitleBar::toggleMaximized);
    }
    menu.addSeparator();
    menu.addAction(tr("Close"), m_window, &QWidget::close);
    menu.exec(event->globalPos());
}

} // namespace Burner::UI
