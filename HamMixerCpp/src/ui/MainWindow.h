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
#include "ui/RadioControlWindow.h"
#include "serial/RadioController.h"
#include "websdr/WebSdrManager.h"

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

    // Radio Control window
    void onShowRadioControl();

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

    // Radio Control window
    RadioControlWindow* m_radioControlWindow;

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
