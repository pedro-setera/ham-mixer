/*
 * RadioControlPanel.cpp
 *
 * Compact single-row UI panel for CI-V serial port and WebSDR site controls
 * Part of HamMixer CT7BAC
 */

#include "RadioControlPanel.h"
#include "Styles.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QDebug>

RadioControlPanel::RadioControlPanel(QWidget* parent)
    : QWidget(parent)
    , m_isConnected(false)
{
    setupUI();
    connectSignals();

    // Populate with default sites
    setSiteList(WebSdrSite::defaultSites());

    // Initial port refresh
    refreshPorts();
}

void RadioControlPanel::setupUI()
{
    // Main horizontal layout - single row, equally distributed
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(10);

    // ===== CI-V Serial Connection Section =====
    QGroupBox* serialGroup = new QGroupBox("CI-V Connection", this);
    QHBoxLayout* serialLayout = new QHBoxLayout(serialGroup);
    serialLayout->setContentsMargins(10, 5, 10, 5);
    serialLayout->setSpacing(8);

    QLabel* portLabel = new QLabel("Port:", serialGroup);
    m_portCombo = new QComboBox(serialGroup);
    m_portCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_portCombo->setStyleSheet("QComboBox { min-width: 60px; }");  // Override global min-width
    m_portCombo->setToolTip("Select the COM port connected to the IC-7300");

    m_connectButton = new QPushButton("Connect", serialGroup);
    m_connectButton->setFixedWidth(85);  // Fixed width to fit "Disconnect"
    m_connectButton->setCheckable(true);
    m_connectButton->setToolTip("Connect to or disconnect from the radio");
    updateConnectButtonStyle();

    serialLayout->addWidget(portLabel);
    serialLayout->addWidget(m_portCombo, 1);  // Stretch factor 1
    serialLayout->addWidget(m_connectButton);

    mainLayout->addWidget(serialGroup, 1);  // Equal stretch

    // ===== WebSDR Site Section =====
    QGroupBox* webSdrGroup = new QGroupBox("WebSDR Site", this);
    QHBoxLayout* webSdrLayout = new QHBoxLayout(webSdrGroup);
    webSdrLayout->setContentsMargins(10, 5, 10, 5);
    webSdrLayout->setSpacing(8);

    QLabel* siteLabel = new QLabel("Site:", webSdrGroup);
    m_siteCombo = new QComboBox(webSdrGroup);
    m_siteCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_siteCombo->setStyleSheet("QComboBox { min-width: 80px; }");  // Override global min-width
    m_siteCombo->setToolTip("Select WebSDR site to use");

    m_webSdrStatusLabel = new QLabel("Not loaded", webSdrGroup);
    m_webSdrStatusLabel->setFixedWidth(70);
    m_webSdrStatusLabel->setStyleSheet("QLabel { color: #808080; }");

    webSdrLayout->addWidget(siteLabel);
    webSdrLayout->addWidget(m_siteCombo, 1);  // Stretch factor 1
    webSdrLayout->addWidget(m_webSdrStatusLabel);

    mainLayout->addWidget(webSdrGroup, 1);  // Equal stretch

    // ===== Radio Info Section =====
    QGroupBox* infoGroup = new QGroupBox("Radio Info", this);
    QHBoxLayout* infoLayout = new QHBoxLayout(infoGroup);
    infoLayout->setContentsMargins(10, 5, 10, 5);
    infoLayout->setSpacing(15);

    // Frequency
    QLabel* freqLabel = new QLabel("Freq:", infoGroup);
    m_frequencyLabel = new QLabel("---.--- MHz", infoGroup);
    m_frequencyLabel->setStyleSheet(
        "QLabel { font-family: 'Consolas'; font-size: 11pt; font-weight: bold; color: #00BCD4; }");

    // Mode
    QLabel* modeLabel = new QLabel("Mode:", infoGroup);
    m_modeLabel = new QLabel("---", infoGroup);
    m_modeLabel->setStyleSheet(
        "QLabel { font-family: 'Consolas'; font-size: 11pt; font-weight: bold; color: #E8E8E8; }");

    infoLayout->addWidget(freqLabel);
    infoLayout->addWidget(m_frequencyLabel);
    infoLayout->addStretch();
    infoLayout->addWidget(modeLabel);
    infoLayout->addWidget(m_modeLabel);

    mainLayout->addWidget(infoGroup, 1);  // Equal stretch
}

void RadioControlPanel::connectSignals()
{
    connect(m_connectButton, &QPushButton::clicked,
            this, &RadioControlPanel::onConnectButtonClicked);

    connect(m_portCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RadioControlPanel::onPortComboChanged);

    connect(m_siteCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RadioControlPanel::onSiteComboChanged);
}

void RadioControlPanel::onConnectButtonClicked()
{
    if (m_isConnected) {
        emit serialDisconnectClicked();
    } else {
        emit serialConnectClicked();
    }
}

void RadioControlPanel::onPortComboChanged(int index)
{
    Q_UNUSED(index)
    emit portChanged(selectedPort());
}

void RadioControlPanel::onSiteComboChanged(int index)
{
    if (index >= 0 && index < m_sites.size()) {
        emit webSdrSiteChanged(m_sites[index]);
    }
}

QString RadioControlPanel::selectedPort() const
{
    return m_portCombo->currentText();
}

void RadioControlPanel::setPortList(const QStringList& ports)
{
    QString currentPort = m_portCombo->currentText();
    m_portCombo->clear();

    if (ports.isEmpty()) {
        m_portCombo->addItem("---");
        m_portCombo->setEnabled(false);
    } else {
        m_portCombo->setEnabled(true);
        for (const QString& port : ports) {
            m_portCombo->addItem(port);
        }

        // Restore previous selection if possible
        int idx = m_portCombo->findText(currentPort);
        if (idx >= 0) {
            m_portCombo->setCurrentIndex(idx);
        }
    }
}

void RadioControlPanel::setSelectedPort(const QString& port)
{
    int idx = m_portCombo->findText(port);
    if (idx >= 0) {
        m_portCombo->setCurrentIndex(idx);
    }
}

WebSdrSite RadioControlPanel::selectedSite() const
{
    int idx = m_siteCombo->currentIndex();
    if (idx >= 0 && idx < m_sites.size()) {
        return m_sites[idx];
    }
    return WebSdrSite();
}

void RadioControlPanel::setSiteList(const QList<WebSdrSite>& sites)
{
    m_sites = sites;
    m_siteCombo->clear();

    for (const WebSdrSite& site : sites) {
        m_siteCombo->addItem(site.name, site.id);
    }

    if (!sites.isEmpty()) {
        m_siteCombo->setCurrentIndex(0);
    }
}

void RadioControlPanel::setSelectedSite(const QString& siteId)
{
    for (int i = 0; i < m_sites.size(); i++) {
        if (m_sites[i].id == siteId) {
            m_siteCombo->setCurrentIndex(i);
            return;
        }
    }
}

void RadioControlPanel::updateConnectButtonStyle()
{
    if (m_isConnected) {
        m_connectButton->setText("Disconnect");
        m_connectButton->setStyleSheet(
            "QPushButton { background-color: #4CAF50; color: white; font-weight: bold; "
            "min-width: 85px; max-width: 85px; }"
            "QPushButton:hover { background-color: #66BB6A; }");
        m_connectButton->setChecked(true);
    } else {
        m_connectButton->setText("Connect");
        m_connectButton->setStyleSheet(
            "QPushButton { background-color: #424242; color: #E0E0E0; "
            "min-width: 85px; max-width: 85px; }"
            "QPushButton:hover { background-color: #616161; }");
        m_connectButton->setChecked(false);
    }
}

void RadioControlPanel::setSerialConnectionState(CIVController::ConnectionState state)
{
    switch (state) {
        case CIVController::Disconnected:
            m_isConnected = false;
            m_portCombo->setEnabled(true);
            break;

        case CIVController::Connecting:
            // Keep current state, button shows "Connecting..."
            m_connectButton->setText("...");
            return;

        case CIVController::Connected:
            m_isConnected = true;
            m_portCombo->setEnabled(false);
            break;

        case CIVController::Error:
            m_isConnected = false;
            m_portCombo->setEnabled(true);
            break;
    }

    updateConnectButtonStyle();
}

void RadioControlPanel::setWebSdrState(WebSdrController::State state)
{
    switch (state) {
        case WebSdrController::Unloaded:
            m_webSdrStatusLabel->setText("Not loaded");
            m_webSdrStatusLabel->setStyleSheet("QLabel { color: #808080; }");
            break;

        case WebSdrController::Loading:
            m_webSdrStatusLabel->setText("Loading...");
            m_webSdrStatusLabel->setStyleSheet("QLabel { color: #FFEB3B; }");
            break;

        case WebSdrController::Ready:
            m_webSdrStatusLabel->setText("Ready");
            m_webSdrStatusLabel->setStyleSheet("QLabel { color: #4CAF50; }");
            break;

        case WebSdrController::Error:
            m_webSdrStatusLabel->setText("Error");
            m_webSdrStatusLabel->setStyleSheet("QLabel { color: #F44336; }");
            break;
    }
}

void RadioControlPanel::setFrequencyDisplay(uint64_t frequencyHz)
{
    m_frequencyLabel->setText(formatFrequency(frequencyHz));
}

void RadioControlPanel::setModeDisplay(const QString& mode)
{
    m_modeLabel->setText(mode);
}

void RadioControlPanel::clearRadioInfo()
{
    m_frequencyLabel->setText("--- kHz");
    m_modeLabel->setText("---");
}

void RadioControlPanel::refreshPorts()
{
    QStringList ports = CIVController::availablePorts();
    setPortList(ports);
    qDebug() << "RadioControlPanel: Found" << ports.size() << "serial ports";
}

QString RadioControlPanel::formatFrequency(uint64_t frequencyHz) const
{
    // Format as XXXXX.XX kHz (2 decimal places)
    double freqKHz = frequencyHz / 1000.0;
    return QString("%1 kHz").arg(freqKHz, 0, 'f', 2);
}
