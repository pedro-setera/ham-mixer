#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QElapsedTimer>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QMenu>
#include <QGroupBox>
#include <memory>
#include <deque>

#include "audio/AudioManager.h"
#include "config/Settings.h"
#include "ui/DevicePanel.h"
#include "ui/ChannelStrip.h"
#include "ui/MasterStrip.h"
#include "ui/Crossfader.h"
#include "ui/SMeter.h"
#include "ui/RadioControlPanel.h"
#include "serial/RadioController.h"
#include "websdr/WebSdrManager.h"

#include <QButtonGroup>

/**
 * @brief Main application window
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    bool event(QEvent* event) override;

private slots:
    void onRecordClicked();
    void onDelayChanged(int value);
    void onSyncClicked();
    void onAutoSyncToggled(bool enabled);
    void onAutoSyncTimerTick();
    void onCountdownTick();
    void onCrossfaderChanged(float radioVol, float radioPan, float websdrVol, float websdrPan);
    void updateMeters();
    void checkSyncResult();

    // CI-V serial connection slots
    void onSerialConnectClicked();
    void onSerialDisconnectClicked();
    void onRadioConnectionStateChanged(RadioController::ConnectionState state);
    void onCIVFrequencyChanged(uint64_t frequencyHz);
    void onCIVModeChanged(uint8_t mode, const QString& modeName);
    void onCIVSMeterChanged(int value);
    void onCIVError(const QString& error);
    void onTxStatusChanged(bool transmitting);

    // WebSDR slots
    void onWebSdrSiteChanged(const WebSdrSite& site);
    void onWebSdrStateChanged(WebSdrController::State state);
    void onWebSdrSmeterChanged(int value);
    void onManageWebSdr();

    // Settings dialogs
    void onAudioDevicesClicked();

    // View toggle
    void onToggleWebSdrView(bool checked);

    // Band/Mode/Tuner/Voice controls
    void onBandSelected(int bandIndex);
    void onModeSelected(int modeIndex);
    void onTuneClicked();
    void onTunerToggled();
    void onVoiceMemoryClicked();
    void onTunerStateChanged(bool enabled);

    // Config file management
    void onSaveConfig();
    void onOpenConfig();
    void onOpenRecentConfig();
    void updateRecentConfigsMenu();

private:
    // Audio components
    std::unique_ptr<AudioManager> m_audioManager;
    Settings m_settings;

    // UI components
    RadioControlPanel* m_radioControlPanel;
    DevicePanel* m_devicePanel;
    SMeter* m_radioSMeter;
    SMeter* m_websdrSMeter;
    ChannelStrip* m_radioStrip;
    ChannelStrip* m_websdrStrip;
    MasterStrip* m_masterStrip;
    Crossfader* m_crossfader;

    // Radio Controller (Icom CI-V or Kenwood/Elecraft CAT)
    RadioController* m_radioController;

    // WebSDR Manager (manages multiple WebSDR sites)
    WebSdrManager* m_webSdrManager;

    // CI-V S-meter data with delay buffer for sync with audio
    float m_civSMeterDb;
    bool m_civConnected;

    // TX mute state - mutes master during transmission to prevent hearing own voice
    bool m_txMuteActive;        // true when master is muted due to TX
    bool m_masterMuteBeforeTx;  // master mute state before TX started

    // WebSDR S-meter data (from page's smeter variable)
    float m_websdrSMeterDb;
    bool m_websdrSmeterValid;

    // S-Meter sample structure for delay buffers
    struct SMeterSample {
        qint64 timestamp;  // ms since app start
        float valueDb;
    };
    QElapsedTimer m_smeterTimer;

    // Radio S-Meter delay buffer (syncs S-meter display with delayed audio)
    std::deque<SMeterSample> m_smeterBuffer;
    static constexpr size_t SMETER_BUFFER_MAX = 64;  // ~6.4 sec at 10Hz
    float getDelayedSMeterValue() const;

    // WebSDR S-Meter delay buffer (compensates for browser audio buffering)
    std::deque<SMeterSample> m_websdrSmeterBuffer;
    static constexpr int WEBSDR_SMETER_DELAY_MS = 200;  // Browser audio latency compensation
    static constexpr size_t WEBSDR_SMETER_BUFFER_MAX = 32;  // ~3.2 sec at 10Hz
    float getDelayedWebSdrSMeterValue() const;

    // Delay controls
    QSlider* m_delaySlider;
    QLabel* m_delayLabel;
    QPushButton* m_syncButton;
    QPushButton* m_autoSyncToggle;
    QLabel* m_autoSyncCountdown;

    // Auto-sync timer and settings
    QTimer* m_autoSyncTimer;
    QTimer* m_countdownTimer;
    int m_countdownSeconds = 0;
    bool m_isAutoTriggeredSync = false;
    static constexpr int AUTO_SYNC_INTERVAL_SEC = 15;        // 15 seconds between auto-syncs
    static constexpr float AUTO_SYNC_THRESHOLD_MS = 200.0f;  // Max allowed delta from current delay

    // Timers
    QTimer* m_meterTimer;
    QTimer* m_syncTimer;

    // Config menu
    QMenu* m_recentConfigsMenu;

    // WebSDR browser view
    QGroupBox* m_browserGroup;
    QPoint m_preExpandPos;  // Window position before full-screen expand

    // Band/Mode/Tuner/Voice controls
    QButtonGroup* m_bandGroup;
    QButtonGroup* m_modeGroup;
    QPushButton* m_bandButtons[10];
    QPushButton* m_modeButtons[5];
    QPushButton* m_tuneButton;
    QPushButton* m_tunerToggle;
    bool m_tunerEnabled = false;
    QPushButton* m_voiceButtons[8];
    int m_activeVoiceMemory = 0;  // 0 = none, 1-8 = active memory
    QStringList m_voiceMemoryLabels;

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

    // USB jog wheel dial handling
    bool m_dialActive = false;         // True while actively dialing (suppresses radio feedback)
    qint64 m_lastDialCommandTime = 0;  // Rate limit: max ~10 commands/sec
    QTimer* m_dialInactiveTimer;       // Fires when dial stops (resumes radio feedback)
    uint64_t m_localFrequency = 0;     // Local frequency for dial feedback

    void handleDialInput(int direction);  // +1 = up, -1 = down
    void onDialInactive();

    // Radio controls helpers
    QWidget* createRadioControlsSection();
    void updateBandSelection(uint64_t freqHz);
    void updateModeSelection(uint8_t mode);
    int frequencyToBandIndex(uint64_t freqHz) const;
    int modeToIndex(uint8_t mode) const;
    void setRadioControlsEnabled(bool enabled);
    void updateVoiceButtonStates();

    void setupWindow();
    void setupUI();
    void setupMenuBar();
    void connectSignals();
    void loadSettings();
    void applySettingsToUI();  // Apply current m_settings to UI without reloading from file
    void saveSettings();
    void refreshDevices();
    void applyPanningWithMuteOverride();
    void onMuteChanged();

    // Peak detection and auto-level adjustment before unmuting WebSDR
    void checkAndUnmuteWebSdrChannel(const QString& siteId);
    static constexpr float WEBSDR_PEAK_THRESHOLD = 0.65f;  // 65% max level threshold
};

#endif // MAINWINDOW_H
