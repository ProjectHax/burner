#pragma once

#include <QWidget>

class QLineEdit;
class QLabel;
class QComboBox;
class QPushButton;
class QGroupBox;
class QRadioButton;
class QCheckBox;

namespace Burner::Models {
class DriveListModel;
}

namespace Burner::UI {

/// Page for cloning discs to image files.
class DiscClonerPage : public QWidget {
    Q_OBJECT

public:
    /// Output format for disc cloning
    enum CloneFormat { ISO, BinCue };

    explicit DiscClonerPage(QWidget* parent = nullptr);
    ~DiscClonerPage() override;

    /// Set the drive list model
    void setDriveModel(Models::DriveListModel* model);

    /// Get selected source device
    [[nodiscard]] QString sourceDevice() const;

    /// Get image file path
    [[nodiscard]] QString imagePath() const;

    /// Get selected clone format
    [[nodiscard]] CloneFormat cloneFormat() const;

    /// Check if full blank is selected
    [[nodiscard]] bool isFullBlank() const;

    /// Check if stop on first error is selected
    [[nodiscard]] bool stopOnFirstError() const;

public slots:
    void browseOutputFile();

signals:
    void cloneRequested();
    void blankRequested(bool fullBlank);
    void integrityCheckRequested(bool stopOnFirstError);

private:
    void setupUi();
    void updateCloneButton();
    void updateBlankButton();
    void updateIntegrityButton();
    void onFormatChanged();

    // Clone section
    QComboBox* m_sourceDriveCombo{nullptr};
    QComboBox* m_formatCombo{nullptr};
    QLineEdit* m_imagePathEdit{nullptr};
    QPushButton* m_browseButton{nullptr};
    QPushButton* m_cloneButton{nullptr};
    QLabel* m_infoLabel{nullptr};

    // Blank section
    QGroupBox* m_blankGroup{nullptr};
    QRadioButton* m_quickBlankRadio{nullptr};
    QRadioButton* m_fullBlankRadio{nullptr};
    QPushButton* m_blankButton{nullptr};

    // Integrity check section
    QGroupBox* m_integrityGroup{nullptr};
    QCheckBox* m_stopOnErrorCheck{nullptr};
    QPushButton* m_integrityButton{nullptr};

    Models::DriveListModel* m_driveModel{nullptr};
};

} // namespace Burner::UI
