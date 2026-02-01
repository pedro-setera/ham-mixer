/*
 * RadioControlPanel.cpp
 *
 * Compact single-row UI panel for CI-V, WebSDR, Radio Info, and Tools
 * Part of HamMixer CT7BAC
 */

#include "RadioControlPanel.h"
#include "serial/CIVController.h"  // For static availablePorts()
#include "Styles.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QDebug>

RadioControlPanel::RadioControlPanel(QWidget* parent)
    : QWidget(parent)
    , m_isConnected(false)
    , m_recording(false)
    , m_blinkState(false)
    , m_transmitting(false)
    , m_webSdrViewVisible(true)
    , m_radioInfoGroup(nullptr)
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
    // Main horizontal layout - single row with specific proportions
    // CI-V 30%, WebSDR 28%, Radio Info 25%, Tools 17%
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(10);

    // ===== CI-V Serial Connection Section (30%) =====
    QGroupBox* serialGroup = new QGroupBox("CI-V Connection", this);
    QHBoxLayout* serialLayout = new QHBoxLayout(serialGroup);
    serialLayout->setContentsMargins(10, 5, 10, 5);
    serialLayout->setSpacing(8);

    QLabel* portLabel = new QLabel("Port:", serialGroup);
    m_portCombo = new QComboBox(serialGroup);
    m_portCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_portCombo->setStyleSheet("QComboBox { min-width: 60px; }");
    m_portCombo->setToolTip("Select the COM port connected to the transceiver");

    m_connectButton = new QPushButton("Connect", serialGroup);
    m_connectButton->setFixedWidth(85);
    m_connectButton->setCheckable(true);
    m_connectButton->setToolTip("Connect to or disconnect from the radio");
    updateConnectButtonStyle();

    serialLayout->addWidget(portLabel);
    serialLayout->addWidget(m_portCombo, 1);
    serialLayout->addWidget(m_connectButton);

    mainLayout->addWidget(serialGroup, 30);

    // ===== WebSDR Site Section (28%) =====
    QGroupBox* webSdrGroup = new QGroupBox("WebSDR", this);
    QHBoxLayout* webSdrLayout = new QHBoxLayout(webSdrGroup);
    webSdrLayout->setContentsMargins(10, 5, 10, 5);
    webSdrLayout->setSpacing(8);

    QLabel* siteLabel = new QLabel("Site:", webSdrGroup);
    m_siteCombo = new QComboBox(webSdrGroup);
    m_siteCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_siteCombo->setStyleSheet("QComboBox { min-width: 46px; }");  // Reduced by 34px for toggle button
    m_siteCombo->setToolTip("Select WebSDR or KiwiSDR site to use");

    // Cogwheel button to open site manager
    m_manageButton = new QPushButton(webSdrGroup);
    m_manageButton->setFixedSize(26, 26);
    m_manageButton->setToolTip("Manage SDR sites");
    m_manageButton->setStyleSheet(
        "QPushButton { "
        "  background-color: #3C3C3C; "
        "  border: 1px solid #555555; "
        "  border-radius: 4px; "
        "  font-size: 14px; "
        "  padding: 0px; "
        "  text-align: center; "
        "}"
        "QPushButton:hover { background-color: #4A4A4A; }"
        "QPushButton:pressed { background-color: #2A2A2A; }"
    );
    m_manageButton->setText(QString::fromUtf8("\u2699"));  // Unicode cogwheel

    // Toggle button to show/hide WebSDR browser view
    m_toggleViewButton = new QPushButton(webSdrGroup);
    m_toggleViewButton->setFixedSize(26, 26);
    m_toggleViewButton->setToolTip("Show/Hide WebSDR browser view");
    m_toggleViewButton->setStyleSheet(
        "QPushButton { "
        "  background-color: #3C3C3C; "
        "  border: 1px solid #555555; "
        "  border-radius: 4px; "
        "  font-size: 12px; "
        "  padding: 0px; "
        "  text-align: center; "
        "}"
        "QPushButton:hover { background-color: #4A4A4A; }"
        "QPushButton:pressed { background-color: #2A2A2A; }"
    );
    m_toggleViewButton->setText(QString::fromUtf8("\u25C9"));  // Fisheye (visible state)

    webSdrLayout->addWidget(siteLabel);
    webSdrLayout->addWidget(m_siteCombo, 1);
    webSdrLayout->addWidget(m_manageButton);
    webSdrLayout->addWidget(m_toggleViewButton);

    mainLayout->addWidget(webSdrGroup, 23);  // Reduced from 28% to give space for Radio Control button

    // ===== Radio Info Section (25%) =====
    m_radioInfoGroup = new QGroupBox("Radio Info", this);
    QHBoxLayout* infoLayout = new QHBoxLayout(m_radioInfoGroup);
    infoLayout->setContentsMargins(10, 5, 10, 5);
    infoLayout->setSpacing(15);

    // Frequency
    QLabel* freqLabel = new QLabel("Freq:", m_radioInfoGroup);
    m_frequencyLabel = new QLabel("---.--- MHz", m_radioInfoGroup);
    m_frequencyLabel->setStyleSheet(
        "QLabel { font-family: 'Consolas'; font-size: 11pt; font-weight: bold; color: #00BCD4; }");

    // Mode
    QLabel* modeLabel = new QLabel("Mode:", m_radioInfoGroup);
    m_modeLabel = new QLabel("---", m_radioInfoGroup);
    m_modeLabel->setStyleSheet(
        "QLabel { font-family: 'Consolas'; font-size: 11pt; font-weight: bold; color: #E8E8E8; }");

    infoLayout->addWidget(freqLabel);
    infoLayout->addWidget(m_frequencyLabel);
    infoLayout->addStretch();
    infoLayout->addWidget(modeLabel);
    infoLayout->addWidget(m_modeLabel);

    mainLayout->addWidget(m_radioInfoGroup, 25);

    // ===== Tools Section (17%) =====
    QGroupBox* toolsGroup = new QGroupBox("Tools", this);
    QHBoxLayout* toolsLayout = new QHBoxLayout(toolsGroup);
    toolsLayout->setContentsMargins(10, 5, 10, 5);
    toolsLayout->setSpacing(8);

    // TX indicator (label + LED) - shows when radio is transmitting
    m_txLabel = new QLabel("Tx", toolsGroup);
    m_txLabel->setStyleSheet("QLabel { font-weight: bold; color: #808080; }");

    m_txIndicator = new QLabel(toolsGroup);
    m_txIndicator->setFixedSize(12, 12);
    m_txIndicator->setStyleSheet(
        "background-color: #404040; "
        "border: 1px solid #606060; "
        "border-radius: 6px;"
    );

    toolsLayout->addWidget(m_txLabel);
    toolsLayout->addWidget(m_txIndicator);
    toolsLayout->addSpacing(8);

    // Record button
    m_recordButton = new QPushButton("REC", toolsGroup);
    m_recordButton->setProperty("buttonType", "record");
    m_recordButton->setCheckable(true);
    m_recordButton->setFixedWidth(60);
    m_recordButton->setEnabled(false);  // Disabled until connected

    // Recording indicator (12px red circle, blinks when recording)
    m_recordIndicator = new QLabel(toolsGroup);
    m_recordIndicator->setFixedSize(12, 12);
    m_recordIndicator->setStyleSheet("background-color: transparent; border-radius: 6px;");
    m_recordIndicator->hide();

    toolsLayout->addWidget(m_recordButton);
    toolsLayout->addWidget(m_recordIndicator);
    toolsLayout->addSpacing(8);

    // Radio Control button - opens the radio control window
    m_radioControlButton = new QPushButton(toolsGroup);
    m_radioControlButton->setFixedSize(26, 26);
    m_radioControlButton->setToolTip("Open Radio Control window");
    m_radioControlButton->setStyleSheet(
        "QPushButton { "
        "  background-color: #3C3C3C; "
        "  border: 1px solid #555555; "
        "  border-radius: 4px; "
        "  font-size: 14px; "
        "  padding: 0px; "
        "  text-align: center; "
        "}"
        "QPushButton:hover { background-color: #4A4A4A; }"
        "QPushButton:pressed { background-color: #2A2A2A; }"
    );
    m_radioControlButton->setText(QString::fromUtf8("\U0001F4FB"));  // Radio emoji

    toolsLayout->addWidget(m_radioControlButton);
    toolsLayout->addStretch();

    mainLayout->addWidget(toolsGroup, 22);  // Increased from 17% for Radio Control button

    // Blink timer for recording indicator
    m_blinkTimer = new QTimer(this);
    connect(m_blinkTimer, &QTimer::timeout, this, [this]() {
        m_blinkState = !m_blinkState;
        m_recordIndicator->setStyleSheet(
            m_blinkState ?
            "background-color: #F44336; border-radius: 6px;" :
            "background-color: #800000; border-radius: 6px;"
        );
    });
}

void RadioControlPanel::connectSignals()
{
    connect(m_connectButton, &QPushButton::clicked,
            this, &RadioControlPanel::onConnectButtonClicked);

    connect(m_portCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RadioControlPanel::onPortComboChanged);

    connect(m_siteCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RadioControlPanel::onSiteComboChanged);

    connect(m_recordButton, &QPushButton::clicked,
            this, &RadioControlPanel::recordClicked);

    connect(m_radioControlButton, &QPushButton::clicked,
            this, &RadioControlPanel::radioControlClicked);

    connect(m_manageButton, &QPushButton::clicked,
            this, &RadioControlPanel::manageSitesClicked);

    connect(m_toggleViewButton, &QPushButton::clicked, this, [this]() {
        m_webSdrViewVisible = !m_webSdrViewVisible;
        // Update icon: fisheye for visible, circle for hidden
        m_toggleViewButton->setText(m_webSdrViewVisible ?
            QString::fromUtf8("\u25C9") :   // Fisheye (visible)
            QString::fromUtf8("\u25CE"));   // Bullseye (hidden)
        emit webSdrViewToggled(m_webSdrViewVisible);
    });
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

void RadioControlPanel::setRecordingActive(bool recording)
{
    m_recording = recording;
    m_recordButton->setChecked(recording);

    if (recording) {
        m_recordIndicator->show();
        m_blinkState = true;
        m_recordIndicator->setStyleSheet("background-color: #F44336; border-radius: 6px;");
        m_blinkTimer->start(500);  // Blink every 500ms
    } else {
        m_recordIndicator->hide();
        m_blinkTimer->stop();
    }
}

void RadioControlPanel::setRecordEnabled(bool enabled)
{
    m_recordButton->setEnabled(enabled);
}

void RadioControlPanel::setTransmitting(bool transmitting)
{
    m_transmitting = transmitting;

    if (transmitting) {
        // TX active - bright red LED with glow effect
        m_txLabel->setStyleSheet("QLabel { font-weight: bold; color: #FF5252; }");
        m_txIndicator->setStyleSheet(
            "background-color: #FF1744; "
            "border: 1px solid #FF5252; "
            "border-radius: 6px;"
        );
    } else {
        // RX mode - dim gray LED
        m_txLabel->setStyleSheet("QLabel { font-weight: bold; color: #808080; }");
        m_txIndicator->setStyleSheet(
            "background-color: #404040; "
            "border: 1px solid #606060; "
            "border-radius: 6px;"
        );
    }
}

void RadioControlPanel::setWebSdrViewVisible(bool visible)
{
    m_webSdrViewVisible = visible;
    // Update icon: fisheye for visible, bullseye for hidden
    m_toggleViewButton->setText(visible ?
        QString::fromUtf8("\u25C9") :   // Fisheye (visible)
        QString::fromUtf8("\u25CE"));   // Bullseye (hidden)
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

    // Block signals to prevent triggering site change during programmatic update
    m_siteCombo->blockSignals(true);
    m_siteCombo->clear();

    for (const WebSdrSite& site : sites) {
        m_siteCombo->addItem(site.name, site.id);
    }

    if (!sites.isEmpty()) {
        m_siteCombo->setCurrentIndex(0);
    }
    m_siteCombo->blockSignals(false);
}

void RadioControlPanel::setSelectedSite(const QString& siteId)
{
    // Block signals to prevent triggering site change during programmatic update
    m_siteCombo->blockSignals(true);
    for (int i = 0; i < m_sites.size(); i++) {
        if (m_sites[i].id == siteId) {
            m_siteCombo->setCurrentIndex(i);
            break;
        }
    }
    m_siteCombo->blockSignals(false);
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

void RadioControlPanel::setSerialConnectionState(RadioController::ConnectionState state)
{
    switch (state) {
        case RadioController::Disconnected:
            m_isConnected = false;
            m_portCombo->setEnabled(true);
            break;

        case RadioController::Connecting:
            // Keep current state, button shows "..."
            m_connectButton->setText("...");
            return;

        case RadioController::Connected:
            m_isConnected = true;
            m_portCombo->setEnabled(false);
            break;

        case RadioController::Error:
            m_isConnected = false;
            m_portCombo->setEnabled(true);
            break;
    }

    updateConnectButtonStyle();
}

void RadioControlPanel::setWebSdrState(WebSdrController::State state)
{
    // No longer showing status label, but we can change combo box style if needed
    Q_UNUSED(state)
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
    // Reset title to default
    if (m_radioInfoGroup) {
        m_radioInfoGroup->setTitle("Radio Info");
    }
}

void RadioControlPanel::setRadioModel(const QString& modelName)
{
    if (m_radioInfoGroup) {
        if (modelName.isEmpty()) {
            m_radioInfoGroup->setTitle("Radio Info");
        } else {
            m_radioInfoGroup->setTitle(QString("Radio Info - %1").arg(modelName));
        }
    }
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
