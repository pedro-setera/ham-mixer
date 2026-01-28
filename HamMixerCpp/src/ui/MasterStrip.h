#ifndef MASTERSTRIP_H
#define MASTERSTRIP_H

#include <QWidget>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include "ui/LevelMeter.h"

/**
 * @brief Master output control strip
 *
 * Features:
 * - "Master" label at top
 * - Vertical volume slider
 * - Volume percentage label
 * - Vertical stereo level meter
 * - Mute button at bottom
 */
class MasterStrip : public QWidget {
    Q_OBJECT

public:
    explicit MasterStrip(QWidget* parent = nullptr);
    ~MasterStrip() override = default;

    /**
     * @brief Set volume (0-100)
     * @param volume Volume percentage
     */
    void setVolume(int volume);

    /**
     * @brief Get current volume (0-100)
     */
    int getVolume() const;

    /**
     * @brief Update level meter
     * @param leftDb Left channel level in dB
     * @param rightDb Right channel level in dB
     */
    void updateLevel(float leftDb, float rightDb);

    /**
     * @brief Reset level meter
     */
    void resetMeter();

    /**
     * @brief Set mute state
     * @param muted True to mute
     */
    void setMuted(bool muted);

    /**
     * @brief Check if muted
     */
    bool isMuted() const;

signals:
    void volumeChanged(int volume);
    void muteChanged(bool muted);

private:
    QLabel* m_titleLabel;
    QSlider* m_volumeSlider;
    QLabel* m_volumeLabel;
    LevelMeter* m_levelMeter;
    QPushButton* m_muteButton;

    void setupUI();
    void connectSignals();
    void updateVolumeLabel(int volume);
};

#endif // MASTERSTRIP_H
