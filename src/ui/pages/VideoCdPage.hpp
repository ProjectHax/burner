#pragma once

#include <QWidget>

class QListWidget;
class QLabel;
class QComboBox;
class QPushButton;

namespace Burner::UI {

class FileDropArea;

/// Page for creating Video CDs (VCD/SVCD).
class VideoCdPage : public QWidget {
    Q_OBJECT

public:
    explicit VideoCdPage(QWidget* parent = nullptr);
    ~VideoCdPage() override;

    /// Get all video file paths
    [[nodiscard]] QStringList videoFiles() const;

    /// Get selected VCD type (VCD, SVCD)
    [[nodiscard]] QString vcdType() const;

    /// Set video files (for loading projects)
    void setVideoFiles(const QStringList& files);

    /// Set VCD type (for loading projects)
    void setVcdType(const QString& type);

    /// Refresh file existence status
    void refreshFileExistence();

    /// Check if all files exist
    [[nodiscard]] bool allFilesExist() const;

public slots:
    void addVideos();
    void addVideos(const QStringList& files);
    void removeSelected();
    void clearAll();

signals:
    void videosChanged();
    void burnRequested();

private:
    void setupUi();
    void connectSignals();

    QListWidget* m_listWidget{nullptr};
    QLabel* m_infoLabel{nullptr};
    QComboBox* m_typeCombo{nullptr};
    FileDropArea* m_dropArea{nullptr};
    QPushButton* m_addButton{nullptr};
    QPushButton* m_removeButton{nullptr};
    QPushButton* m_clearButton{nullptr};
};

} // namespace Burner::UI
