/*
 * FrequencyLCD.cpp
 *
 * Custom 7-segment LCD display widget implementation
 * Part of HamMixer CT7BAC Radio Control Module
 */

#include "ui/FrequencyLCD.h"
#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <algorithm>

FrequencyLCD::FrequencyLCD(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(minimumSizeHint());
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void FrequencyLCD::setFrequency(uint64_t frequencyHz)
{
    if (m_frequencyHz != frequencyHz) {
        m_frequencyHz = frequencyHz;
        update();
    }
}

void FrequencyLCD::setBackgroundColor(const QColor& color)
{
    m_backgroundColor = color;
    update();
}

void FrequencyLCD::setDigitColor(const QColor& color)
{
    m_digitColor = color;
    update();
}

void FrequencyLCD::setDimDigitColor(const QColor& color)
{
    m_dimDigitColor = color;
    update();
}

void FrequencyLCD::setShowUnit(bool show)
{
    m_showUnit = show;
    updateGeometry();
    update();
}

void FrequencyLCD::setDisplayHeight(int height)
{
    m_displayHeight = std::max(20, height);  // Minimum 20 pixels
    updateGeometry();
    update();
}

void FrequencyLCD::setHighlightDigit(int positionFromRight)
{
    m_highlightDigit = positionFromRight;
    update();
}

QSize FrequencyLCD::sizeHint() const
{
    // 7 digits + 1 decimal point + optional kHz label + padding
    // Format: XXXXX.XX [kHz]
    int unitWidth = m_showUnit ? 40 : 0;
    int width = padding() * 2 + 7 * digitWidth() + 6 * digitSpacing() + groupSpacing() + unitWidth;
    int height = padding() * 2 + digitHeight();
    return QSize(width, height);
}

QSize FrequencyLCD::minimumSizeHint() const
{
    return sizeHint();
}

void FrequencyLCD::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Get scaled dimensions
    int dHeight = digitHeight();
    int dWidth = digitWidth();
    int dSpacing = digitSpacing();
    int gSpacing = groupSpacing();
    int dSize = dotSize();
    int pad = padding();

    // Draw background with rounded corners and slight border
    painter.setPen(QPen(QColor(40, 50, 60), 2));
    painter.setBrush(m_backgroundColor);
    painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 6, 6);

    // Add subtle inner shadow/glow effect
    QLinearGradient innerGlow(0, 0, 0, height());
    innerGlow.setColorAt(0, QColor(0, 0, 0, 60));
    innerGlow.setColorAt(0.1, QColor(0, 0, 0, 0));
    innerGlow.setColorAt(0.9, QColor(0, 0, 0, 0));
    innerGlow.setColorAt(1, QColor(0, 0, 0, 40));
    painter.fillRect(rect().adjusted(3, 3, -3, -3), innerGlow);

    // Convert Hz to kHz with 2 decimal places
    // Format: XXXXX.XX [kHz] (e.g., 14205.67 kHz)
    // We need 7 digits total: 5 before decimal, 2 after
    uint64_t freqIn10Hz = m_frequencyHz / 10;  // Now in units of 0.01 kHz
    int digits[7];

    // Extract from right to left
    for (int i = 6; i >= 0; i--) {
        digits[i] = freqIn10Hz % 10;
        freqIn10Hz /= 10;
    }

    // Calculate starting X position (center the display)
    int unitWidth = m_showUnit ? (dHeight * 35 / 48) : 0;  // Scale unit label width
    int totalWidth = 7 * dWidth + 6 * dSpacing + gSpacing + dSize;
    int startX = (width() - totalWidth - unitWidth) / 2;
    int y = (height() - dHeight) / 2;

    int x = startX;

    // Draw all 7 digits with decimal after position 4 (indices 0-4 before, 5-6 after)
    // Format: XXXXX.XX [kHz]
    // Skip leading zeros before the decimal point (positions 0-4)
    // Always show digits after decimal point (positions 5-6)
    bool foundNonZero = false;
    for (int i = 0; i < 7; i++) {
        // Check if this is a leading zero (before decimal, before first non-zero digit)
        bool isLeadingZero = (i < 4) && !foundNonZero && (digits[i] == 0);

        // Track when we find the first non-zero digit
        if (digits[i] != 0 && i < 5) {
            foundNonZero = true;
        }

        // Always show the digit just before decimal (position 4) even if it's 0
        // Also always show decimal digits (positions 5-6)
        if (i == 4) {
            foundNonZero = true;  // Force showing the units digit
        }

        // Calculate which digit position from right this is (for highlighting)
        // Position 6 (i=6) = rightmost = position 0 from right
        // Position 5 (i=5) = position 1 from right
        // Position 4 (i=4, before decimal) = position 3 from right (skip decimal)
        // Position 3 (i=3) = position 4 from right
        // etc.
        int posFromRight = (i >= 5) ? (6 - i) : (6 - i + 1);  // Account for decimal point
        bool isHighlighted = (m_highlightDigit >= 0 && posFromRight == m_highlightDigit);

        // First draw dim "8" as background (all segments dim)
        drawDigit(painter, x, y, 8, dHeight, true);

        // Then draw actual digit on top (skip if leading zero)
        if (!isLeadingZero) {
            drawDigit(painter, x, y, digits[i], dHeight, false, isHighlighted);
        }

        x += dWidth + dSpacing;

        // Add decimal point after position 4 (before the last 2 digits)
        if (i == 4) {
            x += gSpacing / 2;
            drawDecimalPoint(painter, x, y + dHeight - dSize - 2, dHeight);
            x += dSize + gSpacing / 2;
        }
    }

    // Draw kHz label (optional)
    if (m_showUnit) {
        drawMHzLabel(painter, x + 4, y);
    }
}

void FrequencyLCD::drawDigit(QPainter& painter, int x, int y, int digit, int height, bool dim, bool highlighted)
{
    if (digit < 0 || digit > 9) return;

    uint8_t pattern = SEGMENT_PATTERNS[digit];

    // Draw each of the 7 segments
    for (int seg = 0; seg < 7; seg++) {
        bool lit = (pattern >> seg) & 1;
        if (dim) {
            // For dim mode, draw all segments as dim
            drawSegment(painter, x, y, seg, height, false, false);
        } else if (lit) {
            drawSegment(painter, x, y, seg, height, true, highlighted);
        }
    }
}

void FrequencyLCD::drawSegment(QPainter& painter, int x, int y, int segment, int height, bool lit, bool highlighted)
{
    // Segment positions for a 7-segment display:
    //     aaa      (segment 0)
    //    f   b     (segments 5, 1)
    //     ggg      (segment 6)
    //    e   c     (segments 4, 2)
    //     ddd      (segment 3)

    int w = digitWidth();
    int h = height;
    int t = segmentThickness();
    int gap = std::max(1, t / 3);  // Small gap between segments, scaled

    // Choose color: highlighted yellow, normal green, or dim
    QColor color;
    if (!lit) {
        color = m_dimDigitColor;
    } else if (highlighted) {
        color = m_highlightColor;  // Yellow for highlighted digit
    } else {
        color = m_digitColor;
    }
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);

    QPainterPath path;

    switch (segment) {
        case 0:  // a - top horizontal
            path.moveTo(x + gap + t/2, y);
            path.lineTo(x + w - gap - t/2, y);
            path.lineTo(x + w - gap - t, y + t);
            path.lineTo(x + gap + t, y + t);
            path.closeSubpath();
            break;

        case 1:  // b - top right vertical
            path.moveTo(x + w, y + gap + t/2);
            path.lineTo(x + w, y + h/2 - gap);
            path.lineTo(x + w - t/2, y + h/2);
            path.lineTo(x + w - t, y + h/2 - gap);
            path.lineTo(x + w - t, y + gap + t);
            path.closeSubpath();
            break;

        case 2:  // c - bottom right vertical
            path.moveTo(x + w, y + h/2 + gap);
            path.lineTo(x + w, y + h - gap - t/2);
            path.lineTo(x + w - t, y + h - gap - t);
            path.lineTo(x + w - t, y + h/2 + gap);
            path.lineTo(x + w - t/2, y + h/2);
            path.closeSubpath();
            break;

        case 3:  // d - bottom horizontal
            path.moveTo(x + gap + t/2, y + h);
            path.lineTo(x + gap + t, y + h - t);
            path.lineTo(x + w - gap - t, y + h - t);
            path.lineTo(x + w - gap - t/2, y + h);
            path.closeSubpath();
            break;

        case 4:  // e - bottom left vertical
            path.moveTo(x, y + h/2 + gap);
            path.lineTo(x + t/2, y + h/2);
            path.lineTo(x + t, y + h/2 + gap);
            path.lineTo(x + t, y + h - gap - t);
            path.lineTo(x, y + h - gap - t/2);
            path.closeSubpath();
            break;

        case 5:  // f - top left vertical
            path.moveTo(x, y + gap + t/2);
            path.lineTo(x + t, y + gap + t);
            path.lineTo(x + t, y + h/2 - gap);
            path.lineTo(x + t/2, y + h/2);
            path.lineTo(x, y + h/2 - gap);
            path.closeSubpath();
            break;

        case 6:  // g - middle horizontal
            path.moveTo(x + gap + t/2, y + h/2);
            path.lineTo(x + gap + t, y + h/2 - t/2);
            path.lineTo(x + w - gap - t, y + h/2 - t/2);
            path.lineTo(x + w - gap - t/2, y + h/2);
            path.lineTo(x + w - gap - t, y + h/2 + t/2);
            path.lineTo(x + gap + t, y + h/2 + t/2);
            path.closeSubpath();
            break;
    }

    painter.drawPath(path);

    // Add glow effect for lit segments
    if (lit) {
        painter.setOpacity(0.3);
        QColor glowColor = highlighted ? m_highlightColor.lighter(150) : m_digitColor.lighter(150);
        painter.setBrush(glowColor);
        painter.drawPath(path);
        painter.setOpacity(1.0);
    }
}

void FrequencyLCD::drawDecimalPoint(QPainter& painter, int x, int y, int height, bool lit)
{
    Q_UNUSED(height)

    int dSize = dotSize();
    QColor color = lit ? m_digitColor : m_dimDigitColor;
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawEllipse(x, y, dSize, dSize);

    // Add glow for lit dot
    if (lit) {
        painter.setOpacity(0.3);
        painter.setBrush(m_digitColor.lighter(150));
        painter.drawEllipse(x - 1, y - 1, dSize + 2, dSize + 2);
        painter.setOpacity(1.0);
    }
}

void FrequencyLCD::drawMHzLabel(QPainter& painter, int x, int y)
{
    // Scale font size based on display height
    int fontSize = std::max(8, m_displayHeight * 12 / 48);
    QFont font("Segoe UI", fontSize, QFont::Bold);
    painter.setFont(font);
    painter.setPen(m_digitColor);

    // Center vertically
    QFontMetrics fm(font);
    int textY = y + (digitHeight() + fm.ascent()) / 2 - 2;

    painter.drawText(x, textY, "kHz");
}
