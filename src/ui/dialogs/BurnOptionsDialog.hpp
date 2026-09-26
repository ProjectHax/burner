#pragma once

#include "../widgets/WindowChrome.hpp"

#include <QDialog>
#include <QString>

class QComboBox;
class QCheckBox;
class QSpinBox;
class QLineEdit;
class QWidget;
class QLabel;
class QGroupBox;

namespace Burner::UI {

/// Dialog for configuring burn options.
class BurnOptionsDialog : public ChromeDialog {
    Q_OBJECT

public:
    struct Options {
        QString volumeLabel;        ///< Disc volume label
        int speed{0};               ///< 0 = max speed
        bool simulate{false};       ///< Dry run
        bool verify{false};         ///< Verify after burn
        bool ejectAfter{true};      ///< Eject when done
        bool burnProof{true};       ///< Buffer underrun protection
        bool closeDisc{true};       ///< Finalize disc (default: true)
        bool blankBeforeBurn{false}; ///< Blank rewritable disc before burning
    };

    explicit BurnOptionsDialog(QWidget* parent = nullptr);
    ~BurnOptionsDialog() override;

    /// Get the configured options
    [[nodiscard]] Options options() const;

    /// Set the disc type being burned (shown in summary)
    void setDiscType(const QString& discType);

    /// Set the summary text (file count, size, etc.)
    void setSummary(const QString& summary);

    /// Set the available write speeds
    void setWriteSpeeds(const QList<int>& speeds);

    /// Set the volume label (and optionally show/hide the field)
    void setVolumeLabel(const QString& label);

    /// Show or hide the volume label field (e.g., hide for Audio CD)
    void setVolumeLabelVisible(bool visible);

    /// Set multi-session information to display
    /// @param sessions Number of existing sessions on the disc
    void setMultiSessionInfo(int sessions);

private:
    void setupUi();

    QGroupBox* m_summaryGroup{nullptr};
    QLabel* m_discTypeLabel{nullptr};
    QLabel* m_summaryLabel{nullptr};
    QWidget* m_volumeLabelWidget{nullptr};
    QLineEdit* m_volumeLabelEdit{nullptr};
    QComboBox* m_speedCombo{nullptr};
    QCheckBox* m_simulateCheck{nullptr};
    QCheckBox* m_verifyCheck{nullptr};
    QCheckBox* m_ejectCheck{nullptr};
    QCheckBox* m_burnProofCheck{nullptr};
    QCheckBox* m_leaveOpenCheck{nullptr};
    QCheckBox* m_blankFirstCheck{nullptr};
    QLabel* m_multiSessionLabel{nullptr};
};

} // namespace Burner::UI
