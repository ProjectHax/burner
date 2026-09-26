#pragma once

#include <QWidget>
#include <QDateTime>

class QTextEdit;
class QPushButton;
class QVBoxLayout;

namespace Burner::UI {

/// Log entry severity level for emoji selection
enum class LogLevel {
    Info,       ///< General information (ℹ️)
    Drive,      ///< Drive/disc operations (📀)
    Prepare,    ///< Preparation phase (⏳)
    Blank,      ///< Blanking operation (🧹)
    Write,      ///< Writing data (🔥)
    Close,      ///< Finalizing/closing disc (📝)
    Verify,     ///< Verification (🔍)
    Success,    ///< Successful completion (✅)
    Warning,    ///< Warning message (⚠️)
    Error       ///< Error occurred (❌)
};

/// Collapsible burn log widget showing timestamped events with emoji icons.
class BurnLog : public QWidget {
    Q_OBJECT

public:
    explicit BurnLog(QWidget* parent = nullptr);
    ~BurnLog() override;

    /// Add a log entry with automatic timestamp and emoji
    void addEntry(LogLevel level, const QString& message);

    /// Clear all log entries
    void clear();

    /// Get all log text (for saving/copying)
    QString logText() const;

    /// Check if log is expanded
    bool isExpanded() const { return m_expanded; }

public slots:
    /// Toggle expanded/collapsed state
    void setExpanded(bool expanded);

    /// Toggle current state
    void toggle();

signals:
    void expandedChanged(bool expanded);

private:
    void setupUi();
    QString emojiForLevel(LogLevel level) const;
    QString colorForLevel(LogLevel level) const;
    QString formatTimestamp() const;
    void updateToggleButton();

    QTextEdit* m_logText{nullptr};
    QPushButton* m_toggleButton{nullptr};
    QVBoxLayout* m_layout{nullptr};
    bool m_expanded{false};
    QString m_lastEntry;
};

} // namespace Burner::UI
