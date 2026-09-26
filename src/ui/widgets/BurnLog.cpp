#include "BurnLog.hpp"

#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QTime>
#include <QScrollBar>

namespace Burner::UI {

BurnLog::BurnLog(QWidget* parent)
    : QWidget(parent) {
    setupUi();
}

BurnLog::~BurnLog() = default;

void BurnLog::setupUi() {
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 8, 0, 0);
    m_layout->setSpacing(4);

    // Toggle button
    m_toggleButton = new QPushButton(this);
    m_toggleButton->setFlat(true);
    m_toggleButton->setCursor(Qt::PointingHandCursor);
    m_toggleButton->setStyleSheet(
        "QPushButton { text-align: left; padding: 4px 8px; }"
        "QPushButton:hover { background-color: rgba(128, 128, 128, 0.1); }"
    );
    connect(m_toggleButton, &QPushButton::clicked, this, &BurnLog::toggle);
    m_layout->addWidget(m_toggleButton);

    // Log text area
    m_logText = new QTextEdit(this);
    m_logText->setReadOnly(true);
    m_logText->setMinimumHeight(150);
    m_logText->setMaximumHeight(250);
    m_logText->setStyleSheet(
        "QTextEdit {"
        "  background-color: palette(base);"
        "  border: 1px solid palette(mid);"
        "  border-radius: 4px;"
        "  font-family: monospace;"
        "  font-size: 11px;"
        "}"
    );
    m_logText->hide();  // Start collapsed
    m_layout->addWidget(m_logText);

    updateToggleButton();
}

void BurnLog::addEntry(LogLevel level, const QString& message) {
    QString timestamp = formatTimestamp();
    QString emoji = emojiForLevel(level);
    QString color = colorForLevel(level);

    // Format as HTML for colored output
    QString html = QString("<span style=\"color: #888;\">%1</span> %2 <span style=\"color: %3;\">%4</span><br>")
        .arg(timestamp)
        .arg(emoji)
        .arg(color)
        .arg(message.toHtmlEscaped());

    m_logText->insertHtml(html);

    // Auto-scroll to bottom
    QScrollBar* scrollBar = m_logText->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());

    // Store last entry for collapsed view
    m_lastEntry = QString("%1 %2 %3").arg(timestamp, emoji, message);
    updateToggleButton();
}

void BurnLog::clear() {
    m_logText->clear();
    m_lastEntry.clear();
    updateToggleButton();
}

QString BurnLog::logText() const {
    return m_logText->toPlainText();
}

void BurnLog::setExpanded(bool expanded) {
    if (m_expanded == expanded) {
        return;
    }

    m_expanded = expanded;
    m_logText->setVisible(expanded);
    updateToggleButton();
    emit expandedChanged(expanded);
}

void BurnLog::toggle() {
    setExpanded(!m_expanded);
}

QString BurnLog::emojiForLevel(LogLevel level) const {
    switch (level) {
        case LogLevel::Info:    return "ℹ️";
        case LogLevel::Drive:   return "📀";
        case LogLevel::Prepare: return "⏳";
        case LogLevel::Blank:   return "🧹";
        case LogLevel::Write:   return "🔥";
        case LogLevel::Close:   return "📝";
        case LogLevel::Verify:  return "🔍";
        case LogLevel::Success: return "✅";
        case LogLevel::Warning: return "⚠️";
        case LogLevel::Error:   return "❌";
    }
    return "•";
}

QString BurnLog::colorForLevel(LogLevel level) const {
    switch (level) {
        case LogLevel::Success: return "#2e7d32";  // Green
        case LogLevel::Error:   return "#c62828";  // Red
        case LogLevel::Warning: return "#f57c00";  // Orange
        default:                return "palette(text)";
    }
}

QString BurnLog::formatTimestamp() const {
    return QTime::currentTime().toString("HH:mm:ss");
}

void BurnLog::updateToggleButton() {
    QString arrow = m_expanded ? "▼" : "▶";
    QString label = m_expanded ? tr("Hide Log") : tr("Show Log");

    if (!m_expanded && !m_lastEntry.isEmpty()) {
        // Show last entry when collapsed
        m_toggleButton->setText(QString("%1 %2  —  %3").arg(arrow, label, m_lastEntry));
    } else {
        m_toggleButton->setText(QString("%1 %2").arg(arrow, label));
    }
}

} // namespace Burner::UI
