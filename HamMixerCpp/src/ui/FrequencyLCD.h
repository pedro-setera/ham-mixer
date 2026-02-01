/*
 * FrequencyLCD.h
 *
 * Custom 7-segment LCD display widget for frequency display
 * Part of HamMixer CT7BAC Radio Control Module
 */

#ifndef FREQUENCYLCD_H
#define FREQUENCYLCD_H

#include <QWidget>
#include <QColor>

/**
 * @brief 7-segment LCD frequency display widget
 *
 * Displays frequency in classic radio LCD style with:
 * - Green-on-black LCD appearance
 * - Format: XXXXX.XX kHz (e.g., 14205.67 kHz)
 * - Dimmed background segments for unlit appearance
 * - Large, readable digits
 */
class FrequencyLCD : public QWidget {
    Q_OBJECT

public:
    explicit FrequencyLCD(QWidget* parent = nullptr);
    ~FrequencyLCD() override = default;

    /**
     * @brief Set the frequency to display
     * @param frequencyHz Frequency in Hz
     */
    void setFrequency(uint64_t frequencyHz);

    /**
     * @brief Get the current frequency
     * @return Frequency in Hz
     */
    uint64_t frequency() const { return m_frequencyHz; }

    /**
     * @brief Set LCD background color
     */
    void setBackgroundColor(const QColor& color);

    /**
     * @brief Set lit digit color
     */
    void setDigitColor(const QColor& color);

    /**
     * @brief Set dim (unlit) segment color
     */
    void setDimDigitColor(const QColor& color);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    uint64_t m_frequencyHz = 14205000;  // Default 14.205 MHz

    QColor m_backgroundColor = QColor(15, 20, 25);       // Dark blue-gray
    QColor m_digitColor = QColor(0, 255, 100);           // Bright green LCD
    QColor m_dimDigitColor = QColor(0, 22, 12);          // Dim segments (darker for better contrast)

    // 7-segment rendering
    void drawDigit(QPainter& painter, int x, int y, int digit, int height, bool dim = false);
    void drawSegment(QPainter& painter, int x, int y, int segment, int height, bool lit);
    void drawDecimalPoint(QPainter& painter, int x, int y, int height, bool lit = true);
    void drawMHzLabel(QPainter& painter, int x, int y);

    // 7-segment encoding: segments are numbered 0-6 (a-g)
    //     aaa
    //    f   b
    //     ggg
    //    e   c
    //     ddd
    static constexpr uint8_t SEGMENT_PATTERNS[10] = {
        0b0111111,  // 0: a,b,c,d,e,f
        0b0000110,  // 1: b,c
        0b1011011,  // 2: a,b,d,e,g
        0b1001111,  // 3: a,b,c,d,g
        0b1100110,  // 4: b,c,f,g
        0b1101101,  // 5: a,c,d,f,g
        0b1111101,  // 6: a,c,d,e,f,g
        0b0000111,  // 7: a,b,c
        0b1111111,  // 8: all
        0b1101111   // 9: a,b,c,d,f,g
    };

    // Dimensions - sized for visibility
    static constexpr int DIGIT_HEIGHT = 48;
    static constexpr int DIGIT_WIDTH = 28;
    static constexpr int DIGIT_SPACING = 4;
    static constexpr int GROUP_SPACING = 8;    // Extra space around decimal points
    static constexpr int DOT_SIZE = 6;
    static constexpr int PADDING = 12;
    static constexpr int SEGMENT_THICKNESS = 5;
};

#endif // FREQUENCYLCD_H
