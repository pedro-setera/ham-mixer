#include "ui/LevelMeter.h"
#include "ui/Styles.h"
#include <QPainter>
#include <cmath>

LevelMeter::LevelMeter(bool stereo, QWidget* parent)
    : QWidget(parent)
    , m_stereo(stereo)
    , m_leftLevel(MIN_DB)
    , m_rightLevel(MIN_DB)
{
    // Initialize colors from style
    m_greenColor = Styles::Colors::MeterGreen;
    m_yellowColor = Styles::Colors::MeterYellow;
    m_redColor = Styles::Colors::MeterRed;
    m_offColor = Styles::Colors::MeterSegmentOff;
    m_bgColor = Styles::Colors::MeterBackground;

    // Set size policy
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

    // Enable transparent background
    setAttribute(Qt::WA_TranslucentBackground);
}

void LevelMeter::setLevel(float db)
{
    m_leftLevel = std::clamp(db, MIN_DB, MAX_DB);
    if (!m_stereo) {
        m_rightLevel = m_leftLevel;
    }
    update();
}

void LevelMeter::setLevels(float leftDb, float rightDb)
{
    m_leftLevel = std::clamp(leftDb, MIN_DB, MAX_DB);
    m_rightLevel = std::clamp(rightDb, MIN_DB, MAX_DB);
    update();
}

void LevelMeter::reset()
{
    m_leftLevel = MIN_DB;
    m_rightLevel = MIN_DB;
    update();
}

QSize LevelMeter::sizeHint() const
{
    int width = m_stereo ? (CHANNEL_WIDTH * 2 + CHANNEL_GAP) : CHANNEL_WIDTH;
    // Use actual height if set, otherwise default
    int h = height() > 0 ? height() : 130;
    return QSize(width + 4, h);
}

QSize LevelMeter::minimumSizeHint() const
{
    int width = m_stereo ? (CHANNEL_WIDTH * 2 + CHANNEL_GAP) : CHANNEL_WIDTH;
    return QSize(width + 4, 50);  // Minimum height
}

int LevelMeter::dbToSegment(float db, int totalSegments) const
{
    // Map dB to segment index (0 at bottom, totalSegments-1 at top)
    float normalized = (db - MIN_DB) / (MAX_DB - MIN_DB);
    return static_cast<int>(normalized * totalSegments);
}

QColor LevelMeter::getSegmentColor(int segment, int totalSegments) const
{
    // Calculate dB for this segment
    float db = MIN_DB + (static_cast<float>(segment) / totalSegments) * (MAX_DB - MIN_DB);

    if (db >= RED_DB) {
        return m_redColor;
    } else if (db >= YELLOW_DB) {
        return m_yellowColor;
    } else {
        return m_greenColor;
    }
}

void LevelMeter::drawChannel(QPainter& painter, int x, int channelWidth, float db)
{
    // Calculate dynamic segment sizing based on available height
    int availableHeight = height() - 4;  // Subtract padding

    // Calculate segment dimensions to fit the available height
    // Keep segment gap at 1px for compact look, adjust segment height
    int segmentGap = 1;
    int segmentCount = SEGMENT_COUNT;

    // Calculate segment height: (availableHeight - gaps) / segments
    int totalGaps = segmentCount - 1;
    int segmentHeight = (availableHeight - totalGaps * segmentGap) / segmentCount;

    // Ensure minimum segment height of 1px
    if (segmentHeight < 1) {
        segmentHeight = 1;
        segmentCount = availableHeight / (segmentHeight + segmentGap);
    }

    int activeSegments = dbToSegment(db, segmentCount);
    int y = height() - 2;  // Start from bottom

    for (int i = 0; i < segmentCount; i++) {
        QRect segmentRect(x, y - segmentHeight, channelWidth, segmentHeight);

        if (i < activeSegments) {
            // Active segment
            painter.fillRect(segmentRect, getSegmentColor(i, segmentCount));
        } else {
            // Inactive segment
            painter.fillRect(segmentRect, m_offColor);
        }

        y -= (segmentHeight + segmentGap);
    }
}

void LevelMeter::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    // Transparent background - don't fill, let parent show through

    if (m_stereo) {
        // Draw left channel
        int leftX = 2;
        drawChannel(painter, leftX, CHANNEL_WIDTH, m_leftLevel);

        // Draw right channel
        int rightX = leftX + CHANNEL_WIDTH + CHANNEL_GAP;
        drawChannel(painter, rightX, CHANNEL_WIDTH, m_rightLevel);
    } else {
        // Draw single channel (centered)
        int x = (width() - CHANNEL_WIDTH) / 2;
        drawChannel(painter, x, CHANNEL_WIDTH, m_leftLevel);
    }
}
