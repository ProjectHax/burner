#include "CapacityBar.hpp"
#include <QPainter>
#include <QPainterPath>

namespace Burner::UI {

CapacityBar::CapacityBar(QWidget* parent)
    : QWidget(parent) {
    setMinimumHeight(24);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

double CapacityBar::percentUsed() const {
    if (m_capacity <= 0) {
        return 0.0;
    }
    return (static_cast<double>(m_used) / m_capacity) * 100.0;
}

void CapacityBar::setCapacity(qint64 bytes) {
    if (m_capacity != bytes) {
        bool wasOver = isOverCapacity();
        m_capacity = bytes;
        update();
        if (wasOver != isOverCapacity()) {
            emit overCapacityChanged(isOverCapacity());
        }
    }
}

void CapacityBar::setUsed(qint64 bytes) {
    if (m_used != bytes) {
        bool wasOver = isOverCapacity();
        m_used = bytes;
        update();
        if (wasOver != isOverCapacity()) {
            emit overCapacityChanged(isOverCapacity());
        }
    }
}

void CapacityBar::setMediaCapacity(MediaCapacity capacity) {
    setCapacity(static_cast<qint64>(capacity));
}

void CapacityBar::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int barHeight = 16;
    const int barY = (height() - barHeight) / 2;
    const int radius = 4;

    QRect barRect(0, barY, width(), barHeight);

    // Background
    painter.setPen(Qt::NoPen);
    painter.setBrush(palette().window());
    painter.drawRoundedRect(barRect, radius, radius);

    // Border
    painter.setPen(QPen(palette().mid().color(), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(barRect, radius, radius);

    // Calculate fill width
    double percent = percentUsed();
    int fillWidth = static_cast<int>(barRect.width() * std::min(percent, 100.0) / 100.0);

    if (fillWidth > 0) {
        QRect fillRect = barRect.adjusted(1, 1, -1, -1);
        fillRect.setWidth(std::min(fillWidth, fillRect.width()));

        // Color based on usage
        QColor fillColor;
        if (isOverCapacity()) {
            fillColor = QColor(220, 50, 50);  // Red for over capacity
        } else if (percent > 90) {
            fillColor = QColor(220, 150, 50); // Orange for near capacity
        } else {
            fillColor = QColor(70, 150, 220); // Blue for normal
        }

        // Gradient fill
        QLinearGradient gradient(fillRect.topLeft(), fillRect.bottomLeft());
        gradient.setColorAt(0, fillColor.lighter(110));
        gradient.setColorAt(1, fillColor);

        QPainterPath fillPath;
        fillPath.addRoundedRect(fillRect, radius - 1, radius - 1);
        painter.fillPath(fillPath, gradient);
    }

    // Text
    QString text;
    if (m_capacity > 0) {
        text = QString("%1 / %2 (%3%)")
            .arg(formatSize(m_used))
            .arg(formatSize(m_capacity))
            .arg(QString::number(percent, 'f', 1));
    } else {
        text = formatSize(m_used);
    }

    QFont font = painter.font();
    font.setPointSize(font.pointSize() - 1);
    font.setBold(true);
    painter.setFont(font);

    // Draw text shadow/outline for better readability on colored backgrounds
    QColor shadowColor = palette().window().color();
    painter.setPen(shadowColor);
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            if (dx != 0 || dy != 0) {
                painter.drawText(barRect.translated(dx, dy), Qt::AlignCenter, text);
            }
        }
    }

    // Draw main text
    painter.setPen(palette().text().color());
    painter.drawText(barRect, Qt::AlignCenter, text);
}

QSize CapacityBar::sizeHint() const {
    return QSize(300, 24);
}

QSize CapacityBar::minimumSizeHint() const {
    return QSize(150, 24);
}

QString CapacityBar::formatSize(qint64 bytes) const {
    constexpr qint64 KB = 1024;
    constexpr qint64 MB = KB * 1024;
    constexpr qint64 GB = MB * 1024;

    if (bytes >= GB) {
        return QString::number(static_cast<double>(bytes) / GB, 'f', 2) + " GB";
    } else if (bytes >= MB) {
        return QString::number(static_cast<double>(bytes) / MB, 'f', 1) + " MB";
    } else if (bytes >= KB) {
        return QString::number(static_cast<double>(bytes) / KB, 'f', 0) + " KB";
    } else {
        return QString::number(bytes) + " B";
    }
}

} // namespace Burner::UI
