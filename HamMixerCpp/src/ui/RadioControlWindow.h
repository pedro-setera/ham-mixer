/*
 * RadioControlWindow.h
 *
 * Radio Control Window - comprehensive transceiver control panel
 * Part of HamMixer CT7BAC
 */

#ifndef RADIOCONTROLWINDOW_H
#define RADIOCONTROLWINDOW_H

#include <QDialog>
#include <QPushButton>
#include <QButtonGroup>
#include <QLabel>
#include <QTimer>
#include <QGroupBox>

#include "ui/FrequencyLCD.h"
#include "ui/SMeter.h"
#include "serial/RadioController.h"

/**
 * @brief Radio Control Window with frequency display, band/mode selectors,
 *        tuner control, voice memories, and USB dial support
 */
class RadioControlWindow : public QDialog {
    Q_OBJECT

public:
    explicit RadioControlWindow(RadioController* controller, QWidget* parent = nullptr);
    ~RadioControlWindow() override;

    // Live updates from radio
    void updateFrequency(uint64_t frequencyHz);
    void updateMode(uint8_t mode);
    void updateSMeter(float dBm);
    void updateTunerState(bool enabled);
    void updateTxStatus(bool transmitting);
    void updateConnectionState(bool connected);

    // Voice memory configuration
    void setVoiceMemoryLabels(const QStringList& labels);

protected:
    bool event(QEvent* event) override;  // Intercept keys before child widgets
    void keyPressEvent(QKeyEvent* event) override;
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    void onBandSelected(int bandIndex);
    void onModeSelected(int modeIndex);
    void onTuneClicked();
    void onTunerToggled();
    void onVoiceMemoryClicked();
    void onDialInactive();

private:
    RadioController* m_radioController;
    bool m_isConnected = false;

    // UI Components
    FrequencyLCD* m_frequencyLCD;
    SMeter* m_smeter;

    QButtonGroup* m_bandGroup;
    QButtonGroup* m_modeGroup;

    QPushButton* m_tuneButton;
    QPushButton* m_tunerToggle;
    bool m_tunerEnabled = false;

    QPushButton* m_voiceButtons[8];
    QPushButton* m_bandButtons[10];
    QPushButton* m_modeButtons[5];

    // Voice memory labels (configurable tips)
    QStringList m_voiceMemoryLabels;

    // TX status indicator
    QLabel* m_txLabel;
    QLabel* m_txLed;
    bool m_isTransmitting = false;
    int m_activeVoiceMemory = 0;  // 0 = none, 1-8 = active memory

    // Connection status indicator
    QLabel* m_connectionStatus;

    // USB dial support (F9=up, F10=down, +=toggle step)
    int m_dialStepIndex = 1;  // Index into DIAL_STEPS array (default 100 Hz)
    QLabel* m_dialStepLabel;  // Shows current step size

    // Dial rate limiting and feedback suppression
    QTimer* m_dialInactiveTimer;      // Fires when dial stops (resumes radio feedback)
    qint64 m_lastDialCommandTime = 0; // Rate limit: max ~10 commands/sec
    bool m_dialActive = false;        // True while actively dialing (suppresses radio feedback)

    // Volume-based dial support (alternative to F9/F10)
    bool m_volumeHookInstalled = false;
    int m_lastVolume = -1;

    // Available dial step sizes
    static constexpr int DIAL_STEPS[] = {10, 100, 1000, 10000, 100000};
    static constexpr int DIAL_STEP_COUNT = 5;

    // Band definitions (MHz)
    static constexpr double BANDS[10] = {1.8, 3.5, 7.0, 10.0, 14.0, 18.0, 21.0, 24.0, 28.0, 50.0};
    static constexpr int BAND_COUNT = 10;

    // Band frequency defaults (Hz) - center frequencies
    static constexpr uint64_t BAND_FREQS[10] = {
        1840000,   // 160m
        3573000,   // 80m (FT8)
        7074000,   // 40m (FT8)
        10136000,  // 30m (FT8)
        14074000,  // 20m (FT8)
        18100000,  // 17m
        21074000,  // 15m (FT8)
        24915000,  // 12m
        28074000,  // 10m (FT8)
        50313000   // 6m (FT8)
    };

    // Mode definitions (CI-V codes) - simplified: LSB, USB, CW, AM, FM
    static constexpr uint8_t MODE_CODES[5] = {
        0x00,  // LSB
        0x01,  // USB
        0x03,  // CW
        0x02,  // AM
        0x05   // FM
    };
    static constexpr int MODE_COUNT = 5;

    void setupUI();
    QWidget* createTopSection();
    QWidget* createBandSection();
    QWidget* createModeSection();
    QWidget* createTunerSection();
    QWidget* createVoiceMemorySection();

    void updateBandSelection(uint64_t freqHz);
    void updateModeSelection(uint8_t mode);
    int frequencyToBandIndex(uint64_t freqHz) const;
    int modeToIndex(uint8_t mode) const;

    void setControlsEnabled(bool enabled);
    void updateVoiceButtonStates();
    void cycleDialStep();
    void updateDialStepDisplay();
    QString formatStepSize(int hz) const;
    void handleDialInput(int direction);  // +1 = up, -1 = down

    // USB dial (volume-based) helpers
    void installVolumeHook();
    void removeVolumeHook();
    int getCurrentVolume();
};

#endif // RADIOCONTROLWINDOW_H
