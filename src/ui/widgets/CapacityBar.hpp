#pragma once

#include <QWidget>
#include <cstdint>

namespace Burner::UI {

/// Visual capacity bar showing disc usage.
/// Displays used/free space with color-coded indicator.
class CapacityBar : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qint64 capacity READ capacity WRITE setCapacity)
    Q_PROPERTY(qint64 used READ used WRITE setUsed)
    Q_PROPERTY(bool overCapacity READ isOverCapacity NOTIFY overCapacityChanged)

public:
    /// Standard media capacities in bytes
    enum MediaCapacity : qint64 {
        CD_650MB = 681574400LL,
        CD_700MB = 737280000LL,
        CD_800MB = 838860800LL,
        CD_900MB = 912261120LL,
        DVD_SL = 4707319808LL,
        DVD_DL = 8543666176LL,
        BD_SL = 25025314816LL,
        BD_DL = 50050629632LL
    };

    explicit CapacityBar(QWidget* parent = nullptr);

    /// Get current capacity
    [[nodiscard]] qint64 capacity() const { return m_capacity; }

    /// Get used space
    [[nodiscard]] qint64 used() const { return m_used; }

    /// Check if used exceeds capacity
    [[nodiscard]] bool isOverCapacity() const { return m_used > m_capacity; }

    /// Get usage percentage (can exceed 100%)
    [[nodiscard]] double percentUsed() const;

public slots:
    /// Set the capacity in bytes
    void setCapacity(qint64 bytes);

    /// Set the used space in bytes
    void setUsed(qint64 bytes);

    /// Set capacity based on media type
    void setMediaCapacity(MediaCapacity capacity);

signals:
    void overCapacityChanged(bool over);

protected:
    void paintEvent(QPaintEvent* event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    QString formatSize(qint64 bytes) const;

    qint64 m_capacity{CD_700MB};
    qint64 m_used{0};
};

} // namespace Burner::UI
