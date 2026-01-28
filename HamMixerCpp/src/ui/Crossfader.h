#ifndef CROSSFADER_H
#define CROSSFADER_H

#include <QWidget>
#include <QSlider>
#include <QLabel>

/**
 * @brief Crossfader widget for balancing between Radio and WebSDR channels
 *
 * Position mapping:
 * - -100: Radio full volume, WebSDR silent
 * - 0: Both channels at full volume (centered)
 * - +100: WebSDR full volume, Radio silent
 */
class Crossfader : public QWidget {
    Q_OBJECT

public:
    explicit Crossfader(QWidget* parent = nullptr);
    ~Crossfader() override = default;

    /**
     * @brief Set crossfader position
     * @param position Position from -100 to +100
     */
    void setPosition(int position);

    /**
     * @brief Get current position
     */
    int getPosition() const;

    /**
     * @brief Calculate channel parameters from position
     * @param position Crossfader position (-100 to +100)
     * @param radioVol Output radio volume (0.0 to 1.0)
     * @param radioPan Output radio pan (-1.0 to +1.0)
     * @param websdrVol Output WebSDR volume (0.0 to 1.0)
     * @param websdrPan Output WebSDR pan (-1.0 to +1.0)
     */
    static void calculateChannelParams(int position,
                                        float& radioVol, float& radioPan,
                                        float& websdrVol, float& websdrPan);

signals:
    /**
     * @brief Emitted when crossfader position changes
     * @param radioVol Radio volume (0.0 to 1.0)
     * @param radioPan Radio pan (-1.0 to +1.0)
     * @param websdrVol WebSDR volume (0.0 to 1.0)
     * @param websdrPan WebSDR pan (-1.0 to +1.0)
     */
    void crossfaderChanged(float radioVol, float radioPan,
                           float websdrVol, float websdrPan);

private:
    QSlider* m_slider;
    QLabel* m_positionLabel;
    QLabel* m_radioLabel;
    QLabel* m_websdrLabel;

    void setupUI();
    void connectSignals();
    void updatePositionLabel(int position);
    void emitCrossfaderChanged(int position);
};

#endif // CROSSFADER_H
