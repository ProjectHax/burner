#pragma once

#include <QWidget>
#include <QStringList>

namespace Burner::UI {

/// Widget that accepts file drag-and-drop.
/// Displays a visual drop zone indicator.
class FileDropArea : public QWidget {
    Q_OBJECT
    Q_PROPERTY(bool dragActive READ isDragActive NOTIFY dragActiveChanged)
    Q_PROPERTY(QString hint READ hint WRITE setHint)

public:
    explicit FileDropArea(QWidget* parent = nullptr);

    /// Check if a drag operation is in progress over this widget
    [[nodiscard]] bool isDragActive() const { return m_dragActive; }

    /// Get the hint text shown in the drop area
    [[nodiscard]] QString hint() const { return m_hint; }

    /// Set the hint text
    void setHint(const QString& hint);

    /// Set file filter (list of extensions like "mp3", "wav")
    void setFileFilter(const QStringList& extensions);

    /// Get file filter
    [[nodiscard]] QStringList fileFilter() const { return m_fileFilter; }

signals:
    void filesDropped(const QStringList& filePaths);
    void dragActiveChanged(bool active);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    bool acceptsFile(const QString& filePath) const;

    bool m_dragActive{false};
    QString m_hint;
    QStringList m_fileFilter;
};

} // namespace Burner::UI
