/*
 * RadioControlWindow.cpp
 *
 * Radio Control Window implementation
 * Part of HamMixer CT7BAC
 */

#include "ui/RadioControlWindow.h"
#include "ui/Styles.h"
#include "serial/CIVProtocol.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QKeyEvent>
#include <QMessageBox>
#include <QDebug>
#include <QDateTime>

#ifdef Q_OS_WIN
#include <Windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <functiondiscoverykeys_devpkey.h>
#endif

RadioControlWindow::RadioControlWindow(RadioController* controller, QWidget* parent)
    : QDialog(parent)
    , m_radioController(controller)
    , m_isConnected(controller && controller->isConnected())
{
    setupUI();

    // Dial inactive timer - fires after dial stops, resumes accepting radio frequency updates
    m_dialInactiveTimer = new QTimer(this);
    m_dialInactiveTimer->setInterval(500);  // Resume radio feedback 500ms after last dial input
    m_dialInactiveTimer->setSingleShot(true);
    connect(m_dialInactiveTimer, &QTimer::timeout, this, &RadioControlWindow::onDialInactive);

    // Set initial state from controller
    if (m_radioController && m_isConnected) {
        updateFrequency(m_radioController->currentFrequency());
        updateMode(m_radioController->currentMode());
        setControlsEnabled(true);
    } else {
        setControlsEnabled(false);
    }
}

RadioControlWindow::~RadioControlWindow()
{
    removeVolumeHook();
}

void RadioControlWindow::setupUI()
{
    setWindowTitle("Radio Control");
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setMinimumSize(650, 480);
    resize(680, 500);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(12, 12, 12, 12);

    // Top section: Frequency LCD + S-Meter
    mainLayout->addWidget(createTopSection());

    // Band selection
    mainLayout->addWidget(createBandSection());

    // Mode (55%) + Tuner (45%) section - side by side
    QWidget* modeTunerRow = new QWidget(this);
    QHBoxLayout* modeTunerLayout = new QHBoxLayout(modeTunerRow);
    modeTunerLayout->setContentsMargins(0, 0, 0, 0);
    modeTunerLayout->setSpacing(10);
    modeTunerLayout->addWidget(createModeSection(), 55);  // 55%
    modeTunerLayout->addWidget(createTunerSection(), 45); // 45%
    mainLayout->addWidget(modeTunerRow);

    // Voice Memory section (full width, left-aligned)
    mainLayout->addWidget(createVoiceMemorySection());

    // Connection status bar
    m_connectionStatus = new QLabel(this);
    m_connectionStatus->setAlignment(Qt::AlignCenter);
    m_connectionStatus->setStyleSheet(
        "QLabel { "
        "  background-color: #2A2A2A; "
        "  border: 1px solid #444; "
        "  border-radius: 4px; "
        "  padding: 4px; "
        "  font-size: 10pt; "
        "}"
    );
    updateConnectionState(m_isConnected);
    mainLayout->addWidget(m_connectionStatus);
}

QWidget* RadioControlWindow::createTopSection()
{
    QWidget* container = new QWidget(this);
    QHBoxLayout* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(15);

    // Left side: Frequency LCD with TX indicator and step display below
    QWidget* freqContainer = new QWidget(container);
    QVBoxLayout* freqLayout = new QVBoxLayout(freqContainer);
    freqLayout->setContentsMargins(0, 0, 0, 0);
    freqLayout->setSpacing(4);

    m_frequencyLCD = new FrequencyLCD(freqContainer);
    freqLayout->addWidget(m_frequencyLCD);

    // Status bar: TX indicator + Dial step indicator
    QHBoxLayout* statusLayout = new QHBoxLayout();
    statusLayout->setSpacing(8);

    // TX indicator (label + LED)
    m_txLabel = new QLabel("Tx", freqContainer);
    m_txLabel->setStyleSheet("QLabel { color: #888; font-size: 9pt; font-weight: bold; }");

    m_txLed = new QLabel(freqContainer);
    m_txLed->setFixedSize(14, 14);
    m_txLed->setStyleSheet(
        "QLabel { "
        "  background-color: #400; "
        "  border: 1px solid #600; "
        "  border-radius: 7px; "
        "}"
    );
    m_txLed->setToolTip("TX indicator - lights red when transmitting");

    // Dial step indicator (for USB jog dial: F9=up, F10=down, +=cycle step)
    QLabel* stepLabel = new QLabel("Step:", freqContainer);
    stepLabel->setStyleSheet("QLabel { color: #888; font-size: 9pt; }");

    m_dialStepLabel = new QLabel(freqContainer);
    m_dialStepLabel->setStyleSheet(
        "QLabel { "
        "  color: #0FF; "
        "  font-size: 9pt; "
        "  font-weight: bold; "
        "  font-family: Consolas; "
        "}"
    );
    m_dialStepLabel->setToolTip("Dial step size (press + to cycle: 10Hz, 100Hz, 1kHz, 10kHz, 100kHz)");
    updateDialStepDisplay();

    statusLayout->addWidget(m_txLabel);
    statusLayout->addWidget(m_txLed);
    statusLayout->addSpacing(20);
    statusLayout->addWidget(stepLabel);
    statusLayout->addWidget(m_dialStepLabel);
    statusLayout->addStretch();
    freqLayout->addLayout(statusLayout);

    layout->addWidget(freqContainer, 1);

    // Right side: S-Meter
    m_smeter = new SMeter("Signal", container);
    layout->addWidget(m_smeter);

    return container;
}

QWidget* RadioControlWindow::createBandSection()
{
    QGroupBox* group = new QGroupBox("Band", this);
    QHBoxLayout* layout = new QHBoxLayout(group);
    layout->setSpacing(4);
    layout->setContentsMargins(8, 8, 8, 8);

    m_bandGroup = new QButtonGroup(this);
    m_bandGroup->setExclusive(true);

    // Two-line labels: wavelength on top, frequency below
    const char* bandWavelengths[] = {"160m", "80m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m"};
    const char* bandFreqs[] = {"1.8", "3.5", "7", "10", "14", "18", "21", "24", "28", "50"};

    QString buttonStyle =
        "QPushButton { "
        "  background-color: #3A3A3A; "
        "  border: 1px solid #555; "
        "  border-radius: 4px; "
        "  padding: 4px 2px; "
        "  font-weight: bold; "
        "  font-size: 9pt; "
        "  color: #DDD; "
        "}"
        "QPushButton:hover { background-color: #4A4A4A; }"
        "QPushButton:pressed { background-color: #2A2A2A; }"
        "QPushButton:checked { "
        "  background-color: #2196F3; "
        "  color: white; "
        "  border-color: #42A5F5; "
        "}"
        "QPushButton:disabled { "
        "  background-color: #2A2A2A; "
        "  color: #555; "
        "}";

    for (int i = 0; i < BAND_COUNT; i++) {
        // Two-line text: wavelength on top, frequency (MHz) below
        QString label = QString("%1\n%2").arg(bandWavelengths[i]).arg(bandFreqs[i]);
        QPushButton* btn = new QPushButton(label, group);
        btn->setCheckable(true);
        btn->setAutoDefault(false);  // Prevent Enter key from triggering this button
        btn->setDefault(false);
        btn->setMinimumWidth(48);
        btn->setMinimumHeight(42);
        btn->setStyleSheet(buttonStyle);
        btn->setToolTip(QString("%1 band (%2 MHz)").arg(bandWavelengths[i]).arg(bandFreqs[i]));
        m_bandButtons[i] = btn;
        m_bandGroup->addButton(btn, i);
        layout->addWidget(btn);
    }

    connect(m_bandGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, &RadioControlWindow::onBandSelected);

    return group;
}

QWidget* RadioControlWindow::createModeSection()
{
    QGroupBox* group = new QGroupBox("Mode", this);
    QHBoxLayout* layout = new QHBoxLayout(group);
    layout->setSpacing(6);
    layout->setContentsMargins(8, 8, 8, 8);

    m_modeGroup = new QButtonGroup(this);
    m_modeGroup->setExclusive(true);

    // Only 5 modes: LSB, USB, CW, AM, FM
    const char* modeLabels[] = {"LSB", "USB", "CW", "AM", "FM"};

    QString buttonStyle =
        "QPushButton { "
        "  background-color: #3A3A3A; "
        "  border: 1px solid #555; "
        "  border-radius: 4px; "
        "  padding: 10px 12px; "
        "  font-weight: bold; "
        "  font-size: 11pt; "
        "  color: #DDD; "
        "}"
        "QPushButton:hover { background-color: #4A4A4A; }"
        "QPushButton:pressed { background-color: #2A2A2A; }"
        "QPushButton:checked { "
        "  background-color: #4CAF50; "
        "  color: white; "
        "  border-color: #66BB6A; "
        "}"
        "QPushButton:disabled { "
        "  background-color: #2A2A2A; "
        "  color: #555; "
        "}";

    for (int i = 0; i < MODE_COUNT; i++) {
        QPushButton* btn = new QPushButton(modeLabels[i], group);
        btn->setCheckable(true);
        btn->setAutoDefault(false);  // Prevent Enter key from triggering this button
        btn->setDefault(false);
        btn->setMinimumWidth(55);
        btn->setStyleSheet(buttonStyle);
        m_modeButtons[i] = btn;
        m_modeGroup->addButton(btn, i);
        layout->addWidget(btn);
    }

    connect(m_modeGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, &RadioControlWindow::onModeSelected);

    return group;
}

QWidget* RadioControlWindow::createTunerSection()
{
    QGroupBox* tunerGroup = new QGroupBox("Tuner", this);
    QHBoxLayout* tunerLayout = new QHBoxLayout(tunerGroup);
    tunerLayout->setSpacing(8);
    tunerLayout->setContentsMargins(8, 8, 8, 8);

    m_tuneButton = new QPushButton("TUNE", tunerGroup);
    m_tuneButton->setAutoDefault(false);  // Prevent Enter key from triggering
    m_tuneButton->setDefault(false);
    m_tuneButton->setMinimumWidth(70);
    m_tuneButton->setStyleSheet(
        "QPushButton { "
        "  background-color: #FF9800; "
        "  border: 1px solid #FFB74D; "
        "  border-radius: 4px; "
        "  padding: 10px 15px; "
        "  font-weight: bold; "
        "  font-size: 11pt; "
        "  color: #222; "
        "}"
        "QPushButton:hover { background-color: #FFB74D; }"
        "QPushButton:pressed { background-color: #E65100; }"
        "QPushButton:disabled { background-color: #5A4A20; color: #888; }"
    );
    m_tuneButton->setToolTip("Start antenna tuner");
    connect(m_tuneButton, &QPushButton::clicked, this, &RadioControlWindow::onTuneClicked);

    m_tunerToggle = new QPushButton("ATU OFF", tunerGroup);
    m_tunerToggle->setAutoDefault(false);  // Prevent Enter key from triggering
    m_tunerToggle->setDefault(false);
    m_tunerToggle->setCheckable(true);
    m_tunerToggle->setMinimumWidth(95);  // Wider to fit "ATU OFF" without cutting
    m_tunerToggle->setStyleSheet(
        "QPushButton { "
        "  background-color: #424242; "
        "  border: 1px solid #555; "
        "  border-radius: 4px; "
        "  padding: 10px 15px; "
        "  font-weight: bold; "
        "  font-size: 10pt; "
        "  color: #AAA; "
        "}"
        "QPushButton:hover { background-color: #4A4A4A; }"
        "QPushButton:checked { "
        "  background-color: #388E3C; "
        "  color: white; "
        "  border-color: #4CAF50; "
        "}"
        "QPushButton:disabled { background-color: #2A2A2A; color: #555; }"
    );
    m_tunerToggle->setToolTip("Toggle antenna tuner on/off");
    connect(m_tunerToggle, &QPushButton::clicked, this, &RadioControlWindow::onTunerToggled);

    tunerLayout->addWidget(m_tuneButton);
    tunerLayout->addWidget(m_tunerToggle);
    tunerLayout->addStretch();

    return tunerGroup;
}

QWidget* RadioControlWindow::createVoiceMemorySection()
{
    QGroupBox* voiceGroup = new QGroupBox("Voice Memory", this);
    QHBoxLayout* voiceLayout = new QHBoxLayout(voiceGroup);
    voiceLayout->setSpacing(4);
    voiceLayout->setContentsMargins(8, 8, 8, 8);

    QString voiceButtonStyle =
        "QPushButton { "
        "  background-color: #3A3A3A; "
        "  border: 1px solid #555; "
        "  border-radius: 4px; "
        "  padding: 8px 4px; "
        "  font-weight: bold; "
        "  font-size: 10pt; "
        "  color: #DDD; "
        "  min-width: 38px; "
        "}"
        "QPushButton:hover { background-color: #5A5A5A; }"
        "QPushButton:pressed { background-color: #9C27B0; color: white; }"
        "QPushButton:disabled { background-color: #2A2A2A; color: #555; }";

    for (int i = 0; i < 8; i++) {
        QPushButton* btn = new QPushButton(QString("M%1").arg(i + 1), voiceGroup);
        btn->setAutoDefault(false);  // Prevent Enter key from triggering
        btn->setDefault(false);
        btn->setStyleSheet(voiceButtonStyle);
        btn->setToolTip(QString("Play voice memory %1").arg(i + 1));
        btn->setProperty("memoryNumber", i + 1);
        m_voiceButtons[i] = btn;
        connect(btn, &QPushButton::clicked, this, &RadioControlWindow::onVoiceMemoryClicked);
        voiceLayout->addWidget(btn);
    }

    // Add stretch at end to left-align buttons
    voiceLayout->addStretch();

    return voiceGroup;
}

void RadioControlWindow::updateFrequency(uint64_t frequencyHz)
{
    // Don't update LCD while actively dialing - prevents jumping back to old values
    // The dial updates the LCD locally for instant feedback
    if (!m_dialActive) {
        m_frequencyLCD->setFrequency(frequencyHz);
    }
    updateBandSelection(frequencyHz);
}

void RadioControlWindow::updateMode(uint8_t mode)
{
    updateModeSelection(mode);
}

void RadioControlWindow::updateSMeter(float dBm)
{
    m_smeter->setLevel(dBm);
}

void RadioControlWindow::updateTunerState(bool enabled)
{
    m_tunerEnabled = enabled;
    m_tunerToggle->setChecked(enabled);
    m_tunerToggle->setText(enabled ? "ATU ON" : "ATU OFF");
}

void RadioControlWindow::updateTxStatus(bool transmitting)
{
    m_isTransmitting = transmitting;

    // Update TX LED
    if (transmitting) {
        m_txLed->setStyleSheet(
            "QLabel { "
            "  background-color: #F00; "
            "  border: 1px solid #F44; "
            "  border-radius: 7px; "
            "}"
        );
    } else {
        m_txLed->setStyleSheet(
            "QLabel { "
            "  background-color: #400; "
            "  border: 1px solid #600; "
            "  border-radius: 7px; "
            "}"
        );

        // TX ended - reset active voice memory
        if (m_activeVoiceMemory > 0) {
            m_activeVoiceMemory = 0;
            updateVoiceButtonStates();
        }
    }
}

void RadioControlWindow::updateConnectionState(bool connected)
{
    m_isConnected = connected;
    setControlsEnabled(connected);

    if (connected && m_radioController) {
        QString model = m_radioController->radioModel();
        if (model.isEmpty()) {
            m_connectionStatus->setText("Connected");
        } else {
            m_connectionStatus->setText(QString("Connected - %1").arg(model));
        }
        m_connectionStatus->setStyleSheet(
            "QLabel { "
            "  background-color: #1B5E20; "
            "  border: 1px solid #4CAF50; "
            "  border-radius: 4px; "
            "  padding: 4px; "
            "  font-size: 10pt; "
            "  color: #A5D6A7; "
            "}"
        );
    } else {
        m_connectionStatus->setText("Not Connected - Connect via CI-V to control radio");
        m_connectionStatus->setStyleSheet(
            "QLabel { "
            "  background-color: #2A2A2A; "
            "  border: 1px solid #444; "
            "  border-radius: 4px; "
            "  padding: 4px; "
            "  font-size: 10pt; "
            "  color: #888; "
            "}"
        );
    }
}

void RadioControlWindow::setControlsEnabled(bool enabled)
{
    m_tuneButton->setEnabled(enabled);
    m_tunerToggle->setEnabled(enabled);

    for (int i = 0; i < BAND_COUNT; i++) {
        m_bandButtons[i]->setEnabled(enabled);
    }
    for (int i = 0; i < MODE_COUNT; i++) {
        m_modeButtons[i]->setEnabled(enabled);
    }
    for (int i = 0; i < 8; i++) {
        m_voiceButtons[i]->setEnabled(enabled);
    }
}

void RadioControlWindow::updateVoiceButtonStates()
{
    // Update voice button visual states based on TX status
    QString activeStyle =
        "QPushButton { "
        "  background-color: #D32F2F; "
        "  border: 1px solid #F44336; "
        "  border-radius: 4px; "
        "  padding: 8px 4px; "
        "  font-weight: bold; "
        "  font-size: 10pt; "
        "  color: white; "
        "  min-width: 38px; "
        "}"
        "QPushButton:hover { background-color: #E53935; }"
        "QPushButton:pressed { background-color: #B71C1C; }";

    QString normalStyle =
        "QPushButton { "
        "  background-color: #3A3A3A; "
        "  border: 1px solid #555; "
        "  border-radius: 4px; "
        "  padding: 8px 4px; "
        "  font-weight: bold; "
        "  font-size: 10pt; "
        "  color: #DDD; "
        "  min-width: 38px; "
        "}"
        "QPushButton:hover { background-color: #5A5A5A; }"
        "QPushButton:pressed { background-color: #9C27B0; color: white; }"
        "QPushButton:disabled { background-color: #2A2A2A; color: #555; }";

    for (int i = 0; i < 8; i++) {
        int memNum = i + 1;
        if (m_isTransmitting && m_activeVoiceMemory == memNum) {
            // This button is actively transmitting - show red
            m_voiceButtons[i]->setStyleSheet(activeStyle);
        } else {
            // Normal state
            m_voiceButtons[i]->setStyleSheet(normalStyle);
        }
    }
}

void RadioControlWindow::cycleDialStep()
{
    // Cycle through step sizes: 10Hz -> 100Hz -> 1kHz -> 10kHz -> 100kHz -> 10Hz...
    m_dialStepIndex = (m_dialStepIndex + 1) % DIAL_STEP_COUNT;
    updateDialStepDisplay();
    qDebug() << "RadioControlWindow: Dial step changed to" << DIAL_STEPS[m_dialStepIndex] << "Hz";
}

void RadioControlWindow::updateDialStepDisplay()
{
    m_dialStepLabel->setText(formatStepSize(DIAL_STEPS[m_dialStepIndex]));
}

QString RadioControlWindow::formatStepSize(int hz) const
{
    if (hz >= 1000) {
        return QString("%1 kHz").arg(hz / 1000);
    } else {
        return QString("%1 Hz").arg(hz);
    }
}

void RadioControlWindow::onBandSelected(int bandIndex)
{
    if (!m_radioController || !m_isConnected) return;
    if (bandIndex < 0 || bandIndex >= BAND_COUNT) return;

    qDebug() << "RadioControlWindow: Band button" << bandIndex << "clicked";

    // Send frequency to change to the target band
    // NOTE: This uses our default frequency for each band. The radio's band stacking
    // registers cannot be recalled via CI-V - that only works with physical band buttons.
    uint64_t freq = BAND_FREQS[bandIndex];
    qDebug() << "RadioControlWindow: Setting frequency to" << freq << "Hz for band" << bandIndex;
    m_radioController->setFrequency(freq);

    // Set mode based on band (commands are queued with proper timing):
    // 160m, 80m, 40m, 30m (bands 0, 1, 2, 3) -> LSB
    // All others (20m, 17m, 15m, 12m, 10m, 6m) -> USB
    uint8_t mode;
    if (bandIndex <= 3) {
        mode = 0x00;  // LSB
        qDebug() << "RadioControlWindow: Setting mode to LSB for band" << bandIndex;
    } else {
        mode = 0x01;  // USB
        qDebug() << "RadioControlWindow: Setting mode to USB for band" << bandIndex;
    }
    m_radioController->setMode(mode);

    // Poll frequency and mode after a delay to get the actual values from radio
    QTimer::singleShot(200, this, [this]() {
        if (m_radioController && m_radioController->isConnected()) {
            m_radioController->requestFrequency();
            m_radioController->requestMode();
        }
    });
}

void RadioControlWindow::onModeSelected(int modeIndex)
{
    if (!m_radioController || !m_isConnected) return;
    if (modeIndex < 0 || modeIndex >= MODE_COUNT) return;

    uint8_t mode = MODE_CODES[modeIndex];
    qDebug() << "RadioControlWindow: Mode selected:" << modeIndex << "-> mode" << mode;
    m_radioController->setMode(mode);

    // Poll frequency after a delay (mode change shouldn't affect frequency, but confirm)
    QTimer::singleShot(200, this, [this]() {
        if (m_radioController && m_radioController->isConnected()) {
            m_radioController->requestFrequency();
        }
    });
}

void RadioControlWindow::onTuneClicked()
{
    if (!m_radioController || !m_isConnected) return;

    qDebug() << "RadioControlWindow: Starting tune";
    m_radioController->startTune();
}

void RadioControlWindow::onTunerToggled()
{
    if (!m_radioController || !m_isConnected) return;

    bool enable = m_tunerToggle->isChecked();
    qDebug() << "RadioControlWindow: Tuner toggle ->" << (enable ? "ON" : "OFF");
    m_radioController->setTunerState(enable);

    // Update button text immediately (will be confirmed by radio response)
    m_tunerToggle->setText(enable ? "ATU ON" : "ATU OFF");
}

void RadioControlWindow::onVoiceMemoryClicked()
{
    if (!m_radioController || !m_isConnected) return;

    QPushButton* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;

    int memNum = btn->property("memoryNumber").toInt();
    if (memNum < 1 || memNum > 8) return;

    // If currently transmitting the same memory, stop it
    if (m_isTransmitting && m_activeVoiceMemory == memNum) {
        qDebug() << "RadioControlWindow: Stopping voice memory" << memNum;
        m_radioController->stopVoiceMemory();
        m_activeVoiceMemory = 0;
        updateVoiceButtonStates();
        return;
    }

    // If transmitting a different memory, stop first then play new one
    if (m_isTransmitting && m_activeVoiceMemory > 0) {
        qDebug() << "RadioControlWindow: Stopping voice memory" << m_activeVoiceMemory << "to play" << memNum;
        m_radioController->stopVoiceMemory();
    }

    // Play the requested memory
    qDebug() << "RadioControlWindow: Playing voice memory" << memNum;
    m_radioController->playVoiceMemory(memNum);
    m_activeVoiceMemory = memNum;
    updateVoiceButtonStates();
}

void RadioControlWindow::updateBandSelection(uint64_t freqHz)
{
    int bandIndex = frequencyToBandIndex(freqHz);

    // Block signals to prevent triggering onBandSelected
    m_bandGroup->blockSignals(true);

    // Uncheck all first
    for (int i = 0; i < BAND_COUNT; i++) {
        m_bandButtons[i]->setChecked(false);
    }

    // Check the matching band
    if (bandIndex >= 0 && bandIndex < BAND_COUNT) {
        m_bandButtons[bandIndex]->setChecked(true);
    }

    m_bandGroup->blockSignals(false);
}

void RadioControlWindow::updateModeSelection(uint8_t mode)
{
    int modeIndex = modeToIndex(mode);

    // Block signals to prevent triggering onModeSelected
    m_modeGroup->blockSignals(true);

    // Uncheck all first
    for (int i = 0; i < MODE_COUNT; i++) {
        m_modeButtons[i]->setChecked(false);
    }

    // Check the matching mode
    if (modeIndex >= 0 && modeIndex < MODE_COUNT) {
        m_modeButtons[modeIndex]->setChecked(true);
    }

    m_modeGroup->blockSignals(false);
}

int RadioControlWindow::frequencyToBandIndex(uint64_t freqHz) const
{
    // Band ranges in Hz (approximate)
    struct BandRange {
        uint64_t low;
        uint64_t high;
        int index;
    };

    static const BandRange ranges[] = {
        {1800000,  2000000,  0},  // 160m
        {3500000,  4000000,  1},  // 80m
        {7000000,  7300000,  2},  // 40m
        {10100000, 10150000, 3},  // 30m
        {14000000, 14350000, 4},  // 20m
        {18068000, 18168000, 5},  // 17m
        {21000000, 21450000, 6},  // 15m
        {24890000, 24990000, 7},  // 12m
        {28000000, 29700000, 8},  // 10m
        {50000000, 54000000, 9},  // 6m
    };

    for (const auto& range : ranges) {
        if (freqHz >= range.low && freqHz <= range.high) {
            return range.index;
        }
    }

    return -1;  // Not in any amateur band
}

int RadioControlWindow::modeToIndex(uint8_t mode) const
{
    for (int i = 0; i < MODE_COUNT; i++) {
        if (MODE_CODES[i] == mode) {
            return i;
        }
    }
    return -1;
}

bool RadioControlWindow::event(QEvent* e)
{
    // Intercept key press events before child widgets get them
    if (e->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(e);
        int key = keyEvent->key();

        // Handle "+" key to cycle dial step
        if (key == Qt::Key_Plus || key == Qt::Key_Equal) {
            cycleDialStep();
            return true;
        }

        // Handle F9/F10 for USB dial
        // F9 = dial UP = frequency INCREASE
        // F10 = dial DOWN = frequency DECREASE
        if (key == Qt::Key_F9) {
            handleDialInput(+1);
            return true;
        }
        if (key == Qt::Key_F10) {
            handleDialInput(-1);
            return true;
        }
    }

    return QDialog::event(e);
}

void RadioControlWindow::keyPressEvent(QKeyEvent* event)
{
    // Handle "+" key to cycle dial step
    if (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal) {
        cycleDialStep();
        event->accept();
        return;
    }

    // Handle frequency tuning keys (requires connection)
    if (!m_radioController || !m_isConnected) {
        QDialog::keyPressEvent(event);
        return;
    }

    switch (event->key()) {
        // F9/F10 handled by event() - this is fallback
        case Qt::Key_F9:
            handleDialInput(+1);
            event->accept();
            return;

        case Qt::Key_F10:
            handleDialInput(-1);
            event->accept();
            return;

        // Arrow keys also work for tuning
        case Qt::Key_Up:
        case Qt::Key_Right:
            handleDialInput(+1);
            event->accept();
            return;

        case Qt::Key_Down:
        case Qt::Key_Left:
            handleDialInput(-1);
            event->accept();
            return;

        default:
            break;
    }

    QDialog::keyPressEvent(event);
}

void RadioControlWindow::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    installVolumeHook();

    // Poll initial state from radio when window is shown
    if (m_radioController && m_radioController->isConnected()) {
        qDebug() << "RadioControlWindow: Polling initial state from radio";
        m_radioController->requestFrequency();
        m_radioController->requestMode();
        m_radioController->requestTunerState();
    }
}

void RadioControlWindow::hideEvent(QHideEvent* event)
{
    QDialog::hideEvent(event);
    removeVolumeHook();
}

bool RadioControlWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG") {
        MSG* msg = static_cast<MSG*>(message);

        // Check for volume change notifications (WM_APPCOMMAND)
        // This provides an alternative to F9/F10 for USB dials that send volume commands
        if (msg->message == WM_APPCOMMAND) {
            int cmd = GET_APPCOMMAND_LPARAM(msg->lParam);
            if (cmd == APPCOMMAND_VOLUME_UP || cmd == APPCOMMAND_VOLUME_DOWN) {
                int direction = (cmd == APPCOMMAND_VOLUME_UP) ? 1 : -1;
                handleDialInput(direction);

                // Block the volume change from affecting system
                *result = 1;
                return true;
            }
        }
    }
#else
    Q_UNUSED(eventType)
    Q_UNUSED(message)
    Q_UNUSED(result)
#endif
    return QDialog::nativeEvent(eventType, message, result);
}

void RadioControlWindow::onDialInactive()
{
    // Dial has been inactive for 500ms - resume accepting frequency updates from radio
    m_dialActive = false;
    qDebug() << "RadioControlWindow: Dial inactive, resuming radio feedback";
}

void RadioControlWindow::handleDialInput(int direction)
{
    // direction: +1 = frequency up, -1 = frequency down
    if (!m_radioController || !m_isConnected) return;

    int dialStep = DIAL_STEPS[m_dialStepIndex];
    int64_t currentFreq = static_cast<int64_t>(m_frequencyLCD->frequency());
    int64_t newFreq = currentFreq + (direction * dialStep);

    // Clamp to valid range
    if (newFreq < 100000) newFreq = 100000;
    if (newFreq > 60000000) newFreq = 60000000;

    // Update LCD immediately for instant visual feedback
    m_frequencyLCD->setFrequency(static_cast<uint64_t>(newFreq));

    // Mark dial as active - this suppresses incoming frequency updates from radio
    // to prevent the display from jumping back to old values
    m_dialActive = true;
    m_dialInactiveTimer->start();  // Reset the 500ms inactivity timeout

    // Rate-limit commands to the radio: max ~10 per second (100ms between commands)
    // This prevents overwhelming the serial buffer and causing delayed execution
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastDialCommandTime >= 100) {
        m_radioController->setFrequency(static_cast<uint64_t>(newFreq));
        m_lastDialCommandTime = now;
    }
}

void RadioControlWindow::installVolumeHook()
{
    if (m_volumeHookInstalled) return;

    m_lastVolume = getCurrentVolume();
    m_volumeHookInstalled = true;
    qDebug() << "RadioControlWindow: Volume hook installed, initial volume:" << m_lastVolume;
}

void RadioControlWindow::removeVolumeHook()
{
    if (!m_volumeHookInstalled) return;

    m_volumeHookInstalled = false;
    m_dialInactiveTimer->stop();
    m_dialActive = false;
    qDebug() << "RadioControlWindow: Volume hook removed";
}

int RadioControlWindow::getCurrentVolume()
{
#ifdef Q_OS_WIN
    HRESULT hr;
    IMMDeviceEnumerator* deviceEnumerator = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
                         __uuidof(IMMDeviceEnumerator), (void**)&deviceEnumerator);
    if (FAILED(hr)) return -1;

    IMMDevice* defaultDevice = nullptr;
    hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultDevice);
    deviceEnumerator->Release();
    if (FAILED(hr)) return -1;

    IAudioEndpointVolume* endpointVolume = nullptr;
    hr = defaultDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER,
                                nullptr, (void**)&endpointVolume);
    defaultDevice->Release();
    if (FAILED(hr)) return -1;

    float volume = 0.0f;
    endpointVolume->GetMasterVolumeLevelScalar(&volume);
    endpointVolume->Release();

    return static_cast<int>(volume * 100.0f);
#else
    return -1;
#endif
}

void RadioControlWindow::setVoiceMemoryLabels(const QStringList& labels)
{
    m_voiceMemoryLabels = labels;

    // Update button tooltips with the configured labels
    for (int i = 0; i < 8; i++) {
        QString label = (i < labels.size() && !labels[i].isEmpty())
                        ? labels[i]
                        : QString("Memory %1").arg(i + 1);
        m_voiceButtons[i]->setToolTip(label);
    }
}
