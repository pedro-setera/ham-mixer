/*
 * RadioControlPanel.h
 *
 * Compact single-row UI panel for CI-V serial port and WebSDR site controls
 * Part of HamMixer CT7BAC
 */

#ifndef RADIOCONTROLPANEL_H
#define RADIOCONTROLPANEL_H

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>

#include "serial/CIVController.h"
#include "websdr/WebSdrSite.h"
#include "websdr/WebSdrController.h"

/**
 * @brief Compact panel containing CI-V serial port controls and WebSDR site selector
 *
 * Layout: [CI-V Connection] | [WebSDR Site] | [Radio Info]
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
    void setSerialConnectionState(CIVController::ConnectionState state);
    void setWebSdrState(WebSdrController::State state);

    // Frequency display
    void setFrequencyDisplay(uint64_t frequencyHz);
    void setModeDisplay(const QString& mode);
    void clearRadioInfo();

    // Refresh available ports
    void refreshPorts();

signals:
    void serialConnectClicked();
    void serialDisconnectClicked();
    void portChanged(const QString& port);
    void webSdrSiteChanged(const WebSdrSite& site);

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
    QLabel* m_webSdrStatusLabel;

    // Info display
    QLabel* m_frequencyLabel;
    QLabel* m_modeLabel;

    // State
    bool m_isConnected;
    QList<WebSdrSite> m_sites;
};

#endif // RADIOCONTROLPANEL_H
