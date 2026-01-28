#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QElapsedTimer>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <memory>
#include <deque>

#include "audio/AudioManager.h"
#include "config/Settings.h"
#include "ui/DevicePanel.h"
#include "ui/ChannelStrip.h"
#include "ui/MasterStrip.h"
#include "ui/Crossfader.h"
#include "ui/TransportControls.h"
#include "ui/SMeter.h"
#include "ui/RadioControlPanel.h"
#include "serial/CIVController.h"
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
    void onAutoSyncClicked();
    void onCrossfaderChanged(float radioVol, float radioPan, float websdrVol, float websdrPan);
    void updateMeters();
    void checkSyncResult();

    // Audio source mode (BOTH/RADIO/WEBSDR toggle)
    void onAudioSourceModeChanged(TransportControls::AudioSourceMode mode);

    // CI-V serial connection slots
    void onSerialConnectClicked();
    void onSerialDisconnectClicked();
    void onCIVConnectionStateChanged(CIVController::ConnectionState state);
    void onCIVFrequencyChanged(uint64_t frequencyHz);
    void onCIVModeChanged(uint8_t mode, const QString& modeName);
    void onCIVSMeterChanged(int value);
    void onCIVError(const QString& error);

    // WebSDR slots
    void onWebSdrSiteChanged(const WebSdrSite& site);
    void onWebSdrStateChanged(WebSdrController::State state);
    void onWebSdrSmeterChanged(int value);
    void onManageWebSdr();

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
    TransportControls* m_transport;

    // CI-V Controller
    CIVController* m_civController;

    // WebSDR Manager (manages multiple WebSDR sites)
    WebSdrManager* m_webSdrManager;

    // CI-V S-meter data with delay buffer for sync with audio
    float m_civSMeterDb;
    bool m_civConnected;

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
    QPushButton* m_autoSyncButton;

    // Timers
    QTimer* m_meterTimer;
    QTimer* m_syncTimer;

    void setupWindow();
    void setupUI();
    void setupMenuBar();
    void connectSignals();
    void loadSettings();
    void saveSettings();
    void refreshDevices();
    void applyPanningWithMuteOverride();
    void onMuteChanged();
    void applyAudioSourceMode(TransportControls::AudioSourceMode mode);

    // Peak detection and auto-level adjustment before unmuting WebSDR
    void checkAndUnmuteWebSdrChannel(const QString& siteId);
    static constexpr float WEBSDR_PEAK_THRESHOLD = 0.65f;  // 65% max level threshold
};

#endif // MAINWINDOW_H
