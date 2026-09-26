#include "FileDropArea.hpp"
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QPainter>
#include <QFileInfo>
#include <QUrl>

namespace Burner::UI {

FileDropArea::FileDropArea(QWidget* parent)
    : QWidget(parent)
    , m_hint(tr("Drop files here\nor use Add button")) {
    setAcceptDrops(true);
    setMinimumSize(200, 100);
}

void FileDropArea::setHint(const QString& hint) {
    if (m_hint != hint) {
        m_hint = hint;
        update();
    }
}

void FileDropArea::setFileFilter(const QStringList& extensions) {
    m_fileFilter = extensions;
}

void FileDropArea::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        // Check if any of the URLs are acceptable
        bool hasAcceptable = false;
        for (const QUrl& url : event->mimeData()->urls()) {
            if (url.isLocalFile()) {
                if (m_fileFilter.isEmpty() || acceptsFile(url.toLocalFile())) {
                    hasAcceptable = true;
                    break;
                }
            }
        }

        if (hasAcceptable) {
            event->acceptProposedAction();
            m_dragActive = true;
            emit dragActiveChanged(true);
            update();
        }
    }
}

void FileDropArea::dragMoveEvent(QDragMoveEvent* event) {
    if (m_dragActive) {
        event->acceptProposedAction();
    }
}

void FileDropArea::dragLeaveEvent(QDragLeaveEvent* event) {
    Q_UNUSED(event);
    m_dragActive = false;
    emit dragActiveChanged(false);
    update();
}

void FileDropArea::dropEvent(QDropEvent* event) {
    m_dragActive = false;
    emit dragActiveChanged(false);
    update();

    const QMimeData* mimeData = event->mimeData();

    if (mimeData->hasUrls()) {
        QStringList pathList;
        for (const QUrl& url : mimeData->urls()) {
            if (url.isLocalFile()) {
                QString path = url.toLocalFile();
                if (m_fileFilter.isEmpty() || acceptsFile(path)) {
                    pathList.append(path);
                }
            }
        }

        if (!pathList.isEmpty()) {
            emit filesDropped(pathList);
            event->acceptProposedAction();
        }
    }
}

void FileDropArea::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Background
    if (m_dragActive) {
        painter.fillRect(rect(), QColor(100, 150, 220, 40));
        painter.setPen(QPen(QColor(100, 150, 220), 2, Qt::DashLine));
    } else {
        painter.setPen(QPen(palette().mid().color(), 1, Qt::DashLine));
    }

    // Border
    QRect borderRect = rect().adjusted(4, 4, -4, -4);
    painter.drawRoundedRect(borderRect, 8, 8);

    // Hint text
    if (!m_hint.isEmpty()) {
        painter.setPen(m_dragActive ? QColor(100, 150, 220) : palette().mid().color());
        QFont font = painter.font();
        font.setPointSize(font.pointSize() + 1);
        painter.setFont(font);
        painter.drawText(borderRect, Qt::AlignCenter, m_hint);
    }
}

bool FileDropArea::acceptsFile(const QString& filePath) const {
    if (m_fileFilter.isEmpty()) {
        return true;
    }

    QFileInfo fileInfo(filePath);
    if (fileInfo.isDir()) {
        return true; // Always accept directories
    }

    QString ext = fileInfo.suffix().toLower();
    for (const QString& filter : m_fileFilter) {
        if (ext == filter.toLower()) {
            return true;
        }
    }
    return false;
}

} // namespace Burner::UI
