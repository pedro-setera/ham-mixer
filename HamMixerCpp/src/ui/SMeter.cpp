#include "ui/SMeter.h"
#include "ui/Styles.h"
#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

SMeter::SMeter(const QString& title, QWidget* parent)
    : QWidget(parent)
    , m_title(title)
    , m_targetDb(-80.0f)
    , m_displayDb(-80.0f)
    , m_needleAngle(ANGLE_MIN)
    , m_targetAngle(ANGLE_MIN)
    , m_needleVelocity(0.0f)
{
    setFixedSize(METER_WIDTH, METER_HEIGHT);

    // Load background image from resources
    m_backgroundImage = QPixmap(":/images/Smeter_background.jpg");
    if (m_backgroundImage.isNull()) {
        qWarning("Failed to load S-Meter background image");
    }

    // Start physics timer
    m_physicsTimer = new QTimer(this);
    connect(m_physicsTimer, &QTimer::timeout, this, &SMeter::updatePhysics);
    m_physicsTimer->start(static_cast<int>(1000.0f / PHYSICS_FPS));
}

SMeter::~SMeter()
{
    m_physicsTimer->stop();
}

void SMeter::setLevel(float db)
{
    // Extended range to -80dB to allow needle to reach S0 and below
    m_targetDb = std::clamp(db, -80.0f, 0.0f);
    m_targetAngle = dbToAngle(m_targetDb);
}

void SMeter::setTitle(const QString& title)
{
    m_title = title;
    update();
}

QSize SMeter::sizeHint() const
{
    return QSize(METER_WIDTH, METER_HEIGHT);
}

QSize SMeter::minimumSizeHint() const
{
    return sizeHint();
}

float SMeter::dbToAngle(float db) const
{
    // Linear mapping from input dB range to angle range
    // For now, we map dBFS input (-80 to 0) linearly to the full scale
    // Later this will be replaced with proper dBm signal level calculation

    // Input range (dBFS from audio metering)
    constexpr float INPUT_MIN = -80.0f;  // Maps to S0
    constexpr float INPUT_MAX = 0.0f;    // Maps to S9+60

    // Normalize input to 0.0 - 1.0 range
    float normalized = (db - INPUT_MIN) / (INPUT_MAX - INPUT_MIN);
    normalized = std::clamp(normalized, 0.0f, 1.0f);

    // Map to angle range (ANGLE_MIN to ANGLE_MAX)
    return ANGLE_MIN + normalized * (ANGLE_MAX - ANGLE_MIN);
}

float SMeter::dbToSUnit(float db) const
{
    // Calculate S-unit based on the angle, which matches the visual scale
    // The scale is non-linear: S0-S9 spans ANGLE_MIN to 0°, S9-S9+60 spans 0° to ANGLE_MAX
    float angle = dbToAngle(db);

    if (angle <= 0.0f) {
        // S0 to S9 range: angle from ANGLE_MIN (-36.5°) to 0°
        float progress = (angle - ANGLE_MIN) / (0.0f - ANGLE_MIN);
        return progress * 9.0f;
    } else {
        // S9+ range: angle from 0° to ANGLE_MAX (37°)
        float progress = angle / ANGLE_MAX;
        float dbOver = progress * 60.0f;  // 0 to 60 dB over S9
        return 9.0f + dbOver / 10.0f;     // Convert to S-unit equivalent for display
    }
}

QString SMeter::formatSUnit(float db) const
{
    float sUnit = dbToSUnit(db);

    if (sUnit <= 9.0f) {
        // Show one decimal place, e.g., "S4.5"
        return QString("S%1").arg(sUnit, 0, 'f', 1);
    } else {
        // Show dB over S9, e.g., "S9+30"
        int dbOver = static_cast<int>((sUnit - 9.0f) * 10.0f);
        return QString("S9+%1").arg(dbOver);
    }
}

QString SMeter::formatDbm(float db) const
{
    // Map input dBFS to dBm display
    // Input: -80 dBFS = S0 = -127 dBm
    // Input: 0 dBFS = S9+60 = -13 dBm
    constexpr float INPUT_MIN = -80.0f;
    constexpr float INPUT_MAX = 0.0f;

    float normalized = (db - INPUT_MIN) / (INPUT_MAX - INPUT_MIN);
    normalized = std::clamp(normalized, 0.0f, 1.0f);

    float dbm = DBM_S0 + normalized * (DBM_S9_PLUS_60 - DBM_S0);
    return QString("%1dBm").arg(static_cast<int>(dbm));
}

void SMeter::updatePhysics()
{
    float dt = 1.0f / PHYSICS_FPS;
    dt = std::min(dt, 0.05f);  // Clamp for stability

    // Spring-damper model for realistic needle movement
    float displacement = m_needleAngle - m_targetAngle;
    float springForce = -NEEDLE_STIFFNESS * displacement;
    float dampingForce = -NEEDLE_DAMPING * m_needleVelocity * 10.0f;

    float acceleration = springForce + dampingForce;
    m_needleVelocity += acceleration * dt;
    m_needleAngle += m_needleVelocity * dt;

    // Hard limits like physical pins - needle cannot go beyond S0 or S9+60
    if (m_needleAngle < ANGLE_MIN) {
        m_needleAngle = ANGLE_MIN;
        m_needleVelocity = 0.0f;  // Stop at the pin
    } else if (m_needleAngle > ANGLE_MAX) {
        m_needleAngle = ANGLE_MAX;
        m_needleVelocity = 0.0f;  // Stop at the pin
    }

    // Smooth the display value for readable readouts (heavy exponential smoothing)
    m_displayDb += (m_targetDb - m_displayDb) * READOUT_SMOOTHING;

    update();
}

void SMeter::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    drawBackground(painter);
    drawNeedle(painter);
    drawTitle(painter);
    drawReadouts(painter);
}

void SMeter::drawBackground(QPainter& painter)
{
    if (!m_backgroundImage.isNull()) {
        // Scale image to fit widget while maintaining aspect ratio
        QPixmap scaled = m_backgroundImage.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);

        // Center the image
        int x = (width() - scaled.width()) / 2;
        int y = (height() - scaled.height()) / 2;
        painter.drawPixmap(x, y, scaled);
    } else {
        // Fallback: dark background
        painter.fillRect(rect(), QColor(40, 40, 40));
    }
}

void SMeter::drawNeedle(QPainter& painter)
{
    // Needle pivot point (47% from left, BELOW the widget bottom)
    // This matches the WebSDR CSS where the needle pivot is outside the visible area
    int pivotX = static_cast<int>(width() * NEEDLE_PIVOT_X_PERCENT);
    int pivotY = height() + NEEDLE_PIVOT_Y_OFFSET;  // Below widget bottom

    painter.save();

    // Enable clipping to widget bounds so needle is cut off at edges
    painter.setClipRect(rect());

    // Move origin to pivot point
    painter.translate(pivotX, pivotY);

    // Rotate needle (Qt uses clockwise positive, so our angle works directly)
    painter.rotate(m_needleAngle);

    // Draw needle (pointing up from pivot, so negative Y)
    // Color: #444444 as per WebSDR CSS
    painter.setPen(QPen(QColor(0x44, 0x44, 0x44), NEEDLE_WIDTH, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(0, 0, 0, -NEEDLE_LENGTH);

    painter.restore();
}

void SMeter::drawTitle(QPainter& painter)
{
    // Draw title below the scale arc, aligned with S9 position (117px from left)
    QFont titleFont("Segoe UI", 9, QFont::Bold);
    painter.setFont(titleFont);

    QFontMetrics fm(titleFont);
    int textWidth = fm.horizontalAdvance(m_title);
    int textHeight = fm.height();

    // Align horizontally with S9 position (117px), position vertically halfway between center and bottom
    int x = 117 - textWidth / 2;  // Center text on S9 position
    int centerY = (height() - textHeight) / 2;
    int bottomY = height() - textHeight - 5;
    int y = (centerY + bottomY) / 2;

    // No background - just dark grey text
    painter.setPen(QColor(100, 100, 100));
    painter.drawText(x, y + fm.ascent() - 1, m_title);
}

void SMeter::drawReadouts(QPainter& painter)
{
    QFont readoutFont("Consolas", 10, QFont::Bold);
    painter.setFont(readoutFont);

    QFontMetrics fm(readoutFont);

    // Fixed widths to prevent jumping (based on max content: "-127dBm" and "S9+60")
    int dbmFixedWidth = fm.horizontalAdvance("-127dBm") + 8;
    int sFixedWidth = fm.horizontalAdvance("S9+60") + 10;  // Extra space for decimal values like "S8.5"

    // Dark grey color for readouts
    painter.setPen(QColor(100, 100, 100));

    // dBm readout (bottom left) - fixed width, right-aligned text
    // Use smoothed display value for readable readouts
    QString dbmText = formatDbm(m_displayDb);
    int dbmTextWidth = fm.horizontalAdvance(dbmText);

    // Right-align text within fixed width area (no background)
    int dbmTextX = 3 + dbmFixedWidth - dbmTextWidth - 4;
    painter.drawText(dbmTextX, height() - 6, dbmText);

    // S-unit readout (bottom right) - fixed width, right-aligned text
    // Use smoothed display value for readable readouts
    QString sText = formatSUnit(m_displayDb);
    int sTextWidth = fm.horizontalAdvance(sText);

    // Right-align text within fixed width area (no background)
    int sTextX = width() - sTextWidth - 7;
    painter.drawText(sTextX, height() - 6, sText);
}
