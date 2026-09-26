#pragma once

#include <QWidget>
#include <memory>

class QListView;
class QLabel;
class QPushButton;

namespace Burner::Models {
class TrackListModel;
}

namespace Burner::UI {

class FileDropArea;

/// Page for creating audio CDs.
/// Allows adding and reordering audio tracks.
class AudioCdPage : public QWidget {
    Q_OBJECT

public:
    explicit AudioCdPage(QWidget* parent = nullptr);
    ~AudioCdPage() override;

    /// Get the track list model
    [[nodiscard]] Models::TrackListModel* model() const { return m_model; }

    /// Get all audio file paths in order
    [[nodiscard]] QStringList audioFiles() const;

    /// Get total duration in seconds
    [[nodiscard]] int totalDurationSeconds() const;

    /// Check if tracks fit on CD
    [[nodiscard]] bool fitsOnCD() const;

    /// Set audio files (for loading projects)
    void setAudioFiles(const QStringList& files);

public slots:
    void addTracks();
    void removeSelected();
    void moveUp();
    void moveDown();
    void clearAll();

signals:
    void tracksChanged();
    void burnRequested();
    void capacityExceeded(bool exceeded);

private:
    void setupUi();
    void connectSignals();
    void updateDurationLabel();

    QListView* m_listView{nullptr};
    QLabel* m_durationLabel{nullptr};
    QLabel* m_capacityWarning{nullptr};
    FileDropArea* m_dropArea{nullptr};
    QPushButton* m_addButton{nullptr};
    QPushButton* m_removeButton{nullptr};
    QPushButton* m_upButton{nullptr};
    QPushButton* m_downButton{nullptr};
    QPushButton* m_clearButton{nullptr};
    Models::TrackListModel* m_model{nullptr};
};

} // namespace Burner::UI
