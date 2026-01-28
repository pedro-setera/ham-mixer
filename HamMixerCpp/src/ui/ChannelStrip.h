#ifndef CHANNELSTRIP_H
#define CHANNELSTRIP_H

#include <QWidget>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include "ui/LevelMeter.h"

/**
 * @brief Single channel control strip with volume slider
 *
 * Features:
 * - Channel label + volume percentage at top
 * - Vertical volume slider + level meter side by side
 * - Mute button at bottom
 */
class ChannelStrip : public QWidget {
    Q_OBJECT

public:
    explicit ChannelStrip(const QString& title, QWidget* parent = nullptr);
    ~ChannelStrip() override = default;

    /**
     * @brief Update the level meter
     * @param db Level in dB
     */
    void updateLevel(float db);

    /**
     * @brief Reset the level meter
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

    /**
     * @brief Set volume (0-100)
     */
    void setVolume(int volume);

    /**
     * @brief Get current volume (0-100)
     */
    int getVolume() const;

signals:
    void muteChanged(bool muted);
    void volumeChanged(int volume);

private:
    QString m_title;
    QLabel* m_titleLabel;
    QLabel* m_volumeLabel;
    QSlider* m_volumeSlider;
    LevelMeter* m_levelMeter;
    QPushButton* m_muteButton;

    void setupUI();
    void updateVolumeLabel(int value);
};

#endif // CHANNELSTRIP_H
