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
 * Layout: [CI-V Connection 30%] | [WebSDR 32%] | [Radio Info 25%] | [Tools 13%]
 * All in a single row with minimal height
 */
class RadioControlPanel : public QWidget
{
    Q_OBJECT

public:
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
    void setRadioModel(const QString& modelName);

    // Refresh available ports
    void refreshPorts();

    // Tools section
    void setRecordingActive(bool recording);
    void setRecordEnabled(bool enabled);
    void setTransmitting(bool transmitting);

    // WebSDR view toggle
    void setWebSdrViewVisible(bool visible);
    bool isWebSdrViewVisible() const { return m_webSdrViewVisible; }

signals:
    void serialConnectClicked();
    void serialDisconnectClicked();
    void portChanged(const QString& port);
    void webSdrSiteChanged(const WebSdrSite& site);
    void manageSitesClicked();
    void webSdrViewToggled(bool visible);

    // Tools signals
    void recordClicked(bool checked);
    void radioControlClicked();

private slots:
    void onConnectButtonClicked();
    void onPortComboChanged(int index);
    void onSiteComboChanged(int index);

private:
    void setupUI();
    void connectSignals();
    void updateConnectButtonStyle();
    QString formatFrequency(uint64_t frequencyHz) const;

    // Serial controls
    QComboBox* m_portCombo;
    QPushButton* m_connectButton;

    // WebSDR controls
    QComboBox* m_siteCombo;
    QPushButton* m_manageButton;
    QPushButton* m_toggleViewButton;
    bool m_webSdrViewVisible;

    // Info display
    QLabel* m_frequencyLabel;
    QLabel* m_modeLabel;

    // Tools controls
    QPushButton* m_recordButton;
    QLabel* m_recordIndicator;
    QLabel* m_txLabel;
    QLabel* m_txIndicator;
    QPushButton* m_radioControlButton;
    QTimer* m_blinkTimer;
    bool m_recording;
    bool m_blinkState;
    bool m_transmitting;

    // State
    bool m_isConnected;
    QList<WebSdrSite> m_sites;

    // Radio Info group box (for title updates)
    QGroupBox* m_radioInfoGroup;
};

#endif // RADIOCONTROLPANEL_H
