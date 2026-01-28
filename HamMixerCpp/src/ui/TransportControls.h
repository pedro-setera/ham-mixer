#ifndef TRANSPORTCONTROLS_H
#define TRANSPORTCONTROLS_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QTimer>

/**
 * @brief Tools panel with audio source toggle and record button
 *
 * The audio source toggle cycles through: BOTH -> RADIO -> WEBSDR -> BOTH
 * - BOTH: Both Radio and WebSDR channels are unmuted
 * - RADIO: Only Radio channel is audible (WebSDR muted)
 * - WEBSDR: Only WebSDR channel is audible (Radio muted)
 */
class TransportControls : public QWidget {
    Q_OBJECT

public:
    enum AudioSourceMode {
        Both,       // Both channels unmuted
        RadioOnly,  // WebSDR muted, Radio unmuted
        WebSdrOnly  // Radio muted, WebSDR unmuted
    };
    Q_ENUM(AudioSourceMode)

    explicit TransportControls(QWidget* parent = nullptr);
    ~TransportControls() override = default;

    /**
     * @brief Get current audio source mode
     */
    AudioSourceMode audioSourceMode() const { return m_audioSourceMode; }

    /**
     * @brief Set audio source mode
     */
    void setAudioSourceMode(AudioSourceMode mode);

    /**
     * @brief Set recording state
     * @param recording True if recording
     */
    void setRecordingActive(bool recording);

    /**
     * @brief Check if recording is active
     */
    bool isRecordingActive() const { return m_recording; }

    /**
     * @brief Enable/disable record button
     */
    void setRecordEnabled(bool enabled);

signals:
    void audioSourceModeChanged(AudioSourceMode mode);
    void recordClicked(bool checked);

private slots:
    void onSourceToggleClicked();

private:
    void setupUI();
    void updateSourceButtonText();

    QPushButton* m_sourceToggleButton;
    QPushButton* m_recordButton;
    QLabel* m_recordIndicator;
    QTimer* m_blinkTimer;

    AudioSourceMode m_audioSourceMode;
    bool m_recording;
    bool m_blinkState;
};

#endif // TRANSPORTCONTROLS_H
