#pragma once

#include <QWidget>
#include <memory>

class QTableView;
class QLabel;
class QPushButton;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QCheckBox;
class QProgressBar;

namespace Burner::Engine {
class QtRipEngine;
}

namespace Burner::Models {
class CdTrackModel;
}

namespace Burner::UI {

/// Page for ripping audio CDs to various formats.
class CdRipperPage : public QWidget {
    Q_OBJECT

public:
    explicit CdRipperPage(QWidget* parent = nullptr);
    ~CdRipperPage() override;

    /// Get the track model
    [[nodiscard]] Models::CdTrackModel* model() const { return m_model; }

    /// Get the rip engine
    [[nodiscard]] Engine::QtRipEngine* engine() const { return m_engine; }

    /// Check if a disc is loaded
    [[nodiscard]] bool hasDisc() const;

    /// Get current device path
    [[nodiscard]] QString currentDevice() const { return m_currentDevice; }

public slots:
    /// Notify that an audio CD was inserted
    void onAudioCdInserted(const QString& device);

    /// Notify that media was removed
    void onMediaRemoved(const QString& device);

    /// Set the current device (used when main drive selector changes)
    void setCurrentDevice(const QString& device);

    /// Refresh/scan the current disc
    void refresh();

    /// Start ripping selected tracks
    void startRip();

    /// Cancel ripping
    void cancelRip();

signals:
    void ripRequested();
    void ripStarted();
    void ripComplete(bool success, const QString& message);

private slots:
    void onScanComplete(bool success, const QString& discId, int trackCount,
                         const QList<int>& trackNumbers, const QList<int>& durations,
                         const QStringList& titles, const QStringList& artists,
                         const QString& albumTitle, const QString& albumArtist);

    void onMetadataComplete(bool success, const QString& albumTitle,
                             const QString& albumArtist, int year,
                             const QString& genre, const QString& releaseId,
                             const QStringList& trackTitles,
                             const QStringList& trackArtists);

    void onCoverArtFetched(bool success, const QByteArray& imageData,
                            const QString& mimeType);

    void onRipProgress(double overallPercent, int currentTrack, int totalTracks,
                        double trackPercent, const QString& operation);

    void onRipComplete(bool success, int tracksRipped, int tracksFailed,
                        const QString& message);

    void onFormatChanged(int index);
    void onPatternChanged();
    void browseOutputDir();
    void updateSelectionInfo();
    void selectAllTracks();
    void deselectAllTracks();

private:
    void setupUi();
    void connectSignals();
    void updatePreview();
    void setRippingState(bool ripping);
    void loadSettings();
    void saveSettings();

    // UI elements
    QLabel* m_coverLabel{nullptr};
    QLineEdit* m_albumEdit{nullptr};
    QLineEdit* m_artistEdit{nullptr};
    QSpinBox* m_yearSpin{nullptr};
    QLineEdit* m_genreEdit{nullptr};
    QTableView* m_trackView{nullptr};
    QPushButton* m_selectAllButton{nullptr};
    QPushButton* m_deselectAllButton{nullptr};
    QLabel* m_selectionLabel{nullptr};
    QComboBox* m_formatCombo{nullptr};
    QComboBox* m_qualityCombo{nullptr};
    QLineEdit* m_outputEdit{nullptr};
    QPushButton* m_browseButton{nullptr};
    QLineEdit* m_patternEdit{nullptr};
    QComboBox* m_presetCombo{nullptr};
    QLabel* m_previewLabel{nullptr};
    QCheckBox* m_embedCoverCheck{nullptr};
    QCheckBox* m_saveCoverCheck{nullptr};
    QCheckBox* m_ejectCheck{nullptr};
    QPushButton* m_refreshButton{nullptr};
    QPushButton* m_ripButton{nullptr};
    QProgressBar* m_progressBar{nullptr};
    QLabel* m_statusLabel{nullptr};
    QWidget* m_progressWidget{nullptr};
    QPushButton* m_cancelButton{nullptr};

    // Model and engine
    Models::CdTrackModel* m_model{nullptr};
    Engine::QtRipEngine* m_engine{nullptr};

    // State
    QString m_currentDevice;
    QString m_discId;
    QString m_releaseId;
    QByteArray m_coverArtData;
    QString m_coverArtMimeType;
    bool m_hasDisc{false};
    bool m_isRipping{false};
};

} // namespace Burner::UI
