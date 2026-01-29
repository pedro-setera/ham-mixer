/*
 * RadioControlPanel.h
 *
 * Compact single-row UI panel for CI-V serial port, WebSDR site, Radio Info, and Tools
 * Part of HamMixer CT7BAC
 */

#ifndef RADIOCONTROLPANEL_H
#define RADIOCONTROLPANEL_H

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QTimer>

#include "serial/RadioController.h"
#include "websdr/WebSdrSite.h"
#include "websdr/WebSdrController.h"

/**
 * @brief Compact panel containing CI-V, WebSDR, Radio Info, and Tools sections
 *
 * Layout: [CI-V Connection 30%] | [WebSDR 20%] | [Radio Info 25%] | [Tools 25%]
 * All in a single row with minimal height
 */
class RadioControlPanel : public QWidget
{
    Q_OBJECT

public:
    // Audio source mode enum (same as TransportControls)
    enum AudioSourceMode {
        Both,       // Both channels unmuted
        RadioOnly,  // WebSDR muted
        WebSdrOnly  // Radio muted
    };
    Q_ENUM(AudioSourceMode)

    explicit RadioControlPanel(QWidget* parent = nullptr);
    ~RadioControlPanel() override = default;

    // Serial port settings
    QString selectedPort() const;
    void setPortList(const QStringList& ports);
    void setSelectedPort(const QString& port);

    // WebSDR site settings
    WebSdrSite selectedSite() const;
    void setSiteList(const QList<WebSdrSite>& sites);
    void setSelectedSite(const QString& siteId);

    // Status updates
    void setSerialConnectionState(RadioController::ConnectionState state);
    void setWebSdrState(WebSdrController::State state);

    // Frequency display
    void setFrequencyDisplay(uint64_t frequencyHz);
    void setModeDisplay(const QString& mode);
    void clearRadioInfo();

    // Refresh available ports
    void refreshPorts();

    // Tools section
    AudioSourceMode audioSourceMode() const { return m_audioSourceMode; }
    void setAudioSourceMode(AudioSourceMode mode);
    void setRecordingActive(bool recording);
    void setRecordEnabled(bool enabled);

signals:
    void serialConnectClicked();
    void serialDisconnectClicked();
    void portChanged(const QString& port);
    void webSdrSiteChanged(const WebSdrSite& site);

    // Tools signals
    void audioSourceModeChanged(AudioSourceMode mode);
    void recordClicked(bool checked);

private slots:
    void onConnectButtonClicked();
    void onPortComboChanged(int index);
    void onSiteComboChanged(int index);
    void onSourceToggleClicked();

private:
    void setupUI();
    void connectSignals();
    void updateConnectButtonStyle();
    void updateSourceButtonText();
    QString formatFrequency(uint64_t frequencyHz) const;

    // Serial controls
    QComboBox* m_portCombo;
    QPushButton* m_connectButton;

    // WebSDR controls
    QComboBox* m_siteCombo;

    // Info display
    QLabel* m_frequencyLabel;
    QLabel* m_modeLabel;

    // Tools controls
    QPushButton* m_sourceToggleButton;
    QPushButton* m_recordButton;
    QLabel* m_recordIndicator;
    QTimer* m_blinkTimer;
    AudioSourceMode m_audioSourceMode;
    bool m_recording;
    bool m_blinkState;

    // State
    bool m_isConnected;
    QList<WebSdrSite> m_sites;
};

#endif // RADIOCONTROLPANEL_H
