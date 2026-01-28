#ifndef LEVELMETER_H
#define LEVELMETER_H

#include <QWidget>
#include <QColor>

/**
 * @brief Vertical LED-style audio level meter widget
 *
 * Displays audio levels with color-coded segments:
 * - Green: -60 to -12 dB
 * - Yellow: -12 to -3 dB
 * - Red: -3 to 0 dB
 */
class LevelMeter : public QWidget {
    Q_OBJECT

public:
    explicit LevelMeter(bool stereo = false, QWidget* parent = nullptr);
    ~LevelMeter() override = default;

    /**
     * @brief Set the level (mono mode)
     * @param db Level in dB (-60 to 0)
     */
    void setLevel(float db);

    /**
     * @brief Set stereo levels
     * @param leftDb Left channel level in dB
     * @param rightDb Right channel level in dB
     */
    void setLevels(float leftDb, float rightDb);

    /**
     * @brief Reset the meter to minimum
     */
    void reset();

    /**
     * @brief Check if stereo mode
     */
    bool isStereo() const { return m_stereo; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    bool m_stereo;
    float m_leftLevel;   // dB
    float m_rightLevel;  // dB

    // Visual parameters
    static constexpr int SEGMENT_COUNT = 40;
    static constexpr int SEGMENT_HEIGHT = 4;
    static constexpr int SEGMENT_GAP = 2;
    static constexpr int CHANNEL_WIDTH = 14;
    static constexpr int CHANNEL_GAP = 4;

    // Level thresholds
    static constexpr float MIN_DB = -60.0f;
    static constexpr float MAX_DB = 0.0f;
    static constexpr float YELLOW_DB = -12.0f;
    static constexpr float RED_DB = -3.0f;

    // Colors
    QColor m_greenColor;
    QColor m_yellowColor;
    QColor m_redColor;
    QColor m_offColor;
    QColor m_bgColor;

    // Helper methods
    int dbToSegment(float db, int totalSegments) const;
    QColor getSegmentColor(int segment, int totalSegments) const;
    void drawChannel(QPainter& painter, int x, int channelWidth, float db);
};

#endif // LEVELMETER_H
