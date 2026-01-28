#ifndef SMETER_H
#define SMETER_H

#include <QWidget>
#include <QTimer>
#include <QString>
#include <QPixmap>

/**
 * @brief Vintage analog S-Meter widget with smooth needle animation
 *
 * Based on WebSDR S-Meter design with background image and CSS-style needle.
 * Displays audio level in classic S-meter style with:
 * - S1 to S9 scale (6dB per S-unit)
 * - dB over S9 (+10, +20, +40, +60)
 * - Spring-damper needle physics for smooth animation
 */
class SMeter : public QWidget {
    Q_OBJECT

public:
    explicit SMeter(const QString& title = "Signal", QWidget* parent = nullptr);
    ~SMeter() override;

    /**
     * @brief Set signal level in dBFS
     * @param db Level in dB (-60 to 0)
     */
    void setLevel(float db);

    /**
     * @brief Get current level
     */
    float getLevel() const { return m_targetDb; }

    /**
     * @brief Set meter title
     */
    void setTitle(const QString& title);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void updatePhysics();

private:
    QString m_title;
    float m_targetDb;        // Target level in dBFS
    float m_displayDb;       // Smoothed level for readout display (heavily averaged)
    float m_needleAngle;     // Current needle angle (degrees)
    float m_targetAngle;     // Target needle angle (degrees)
    float m_needleVelocity;  // Needle angular velocity

    QTimer* m_physicsTimer;
    QPixmap m_backgroundImage;

    // Readout smoothing constant (lower = slower/smoother updates)
    static constexpr float READOUT_SMOOTHING = 0.02f;

    // Physical constants (tuned for responsive movement with minimal overshoot)
    static constexpr float NEEDLE_STIFFNESS = 60.0f;  // Higher = faster response
    static constexpr float NEEDLE_DAMPING = 0.55f;    // Higher = less spring bounce
    static constexpr float PHYSICS_FPS = 60.0f;

    // Angle range (calculated from scale coordinates on background image)
    // Pivot at (117.5, 193.5) in pixel coords, radius 156.5px
    static constexpr float ANGLE_MIN = -36.5f;  // S0 position (left) - fine-tuned
    static constexpr float ANGLE_MAX = 37.0f;   // S9+60 position (right)

    // dBm reference values for S-meter scale
    static constexpr float DBM_S0 = -127.0f;        // S0 = -127 dBm
    static constexpr float DBM_S9_PLUS_60 = -13.0f; // S9+60 = -13 dBm

    // Widget dimensions (matches WebSDR: 250x115)
    static constexpr int METER_WIDTH = 250;
    static constexpr int METER_HEIGHT = 115;

    // Needle parameters (calculated from background image scale coordinates)
    // The pivot is BELOW the visible widget area at (117.5, 193.5) in image coords
    static constexpr float NEEDLE_PIVOT_X_PERCENT = 0.47f;  // 117.5/250 = 47%
    static constexpr int NEEDLE_LENGTH = 157;  // Radius of arc ≈ 156.5px
    static constexpr int NEEDLE_PIVOT_Y_OFFSET = 79;  // 193.5 - 115 ≈ 79px below widget bottom
    static constexpr int NEEDLE_WIDTH = 2;

    // Helper methods
    float dbToAngle(float db) const;
    float dbToSUnit(float db) const;
    QString formatSUnit(float db) const;
    QString formatDbm(float db) const;

    void drawBackground(QPainter& painter);
    void drawNeedle(QPainter& painter);
    void drawTitle(QPainter& painter);
    void drawReadouts(QPainter& painter);
};

#endif // SMETER_H
