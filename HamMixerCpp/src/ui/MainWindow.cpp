#include "ui/MainWindow.h"
#include "ui/Styles.h"
#include "ui/WebSdrManagerDialog.h"
#include "ui/AudioDevicesDialog.h"
#include "ui/VoiceMemoryDialog.h"
#include "audio/MixerCore.h"
#include "audio/AudioSync.h"
#include "serial/CIVProtocol.h"
#include "HamMixer/Version.h"
#include <algorithm>
#include <cmath>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFrame>
#include <QMenuBar>
#include <QMessageBox>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QDateTime>
#include <QSplitter>
#include <QIcon>
#include <QDebug>
#include <QWebEngineView>
#include <QFileDialog>
#include <QDir>
#include <QGridLayout>
#include <QScreen>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_civSMeterDb(-80.0f)
    , m_civConnected(false)
    , m_txMuteActive(false)
    , m_masterMuteBeforeTx(false)
    , m_websdrSMeterDb(-80.0f)
    , m_websdrSmeterValid(false)
    , m_recentConfigsMenu(nullptr)
    , m_browserGroup(nullptr)
{
    // Start S-meter delay timer
    m_smeterTimer.start();

    // Initialize audio manager
    m_audioManager = std::make_unique<AudioManager>(this);
    if (!m_audioManager->initialize()) {
        QMessageBox::critical(this, "Error", "Failed to initialize audio system");
    }

    // Radio controller initialized on connect (auto-detects protocol)
    m_radioController = nullptr;

    setupWindow();
    setupUI();
    setupMenuBar();
    connectSignals();
    loadSettings();
    refreshDevices();

    // Start meter update timer
    m_meterTimer = new QTimer(this);
    connect(m_meterTimer, &QTimer::timeout, this, &MainWindow::updateMeters);
    m_meterTimer->start(1000 / 60);  // 60 Hz

    // Sync check timer
    m_syncTimer = new QTimer(this);
    connect(m_syncTimer, &QTimer::timeout, this, &MainWindow::checkSyncResult);

    // Auto-sync periodic timer (triggers sync every N seconds)
    m_autoSyncTimer = new QTimer(this);
    connect(m_autoSyncTimer, &QTimer::timeout, this, &MainWindow::onAutoSyncTimerTick);

    // Countdown display timer (updates every second)
    m_countdownTimer = new QTimer(this);
    connect(m_countdownTimer, &QTimer::timeout, this, &MainWindow::onCountdownTick);

    // Dial inactive timer - resumes radio frequency feedback after dial stops
    m_dialInactiveTimer = new QTimer(this);
    m_dialInactiveTimer->setInterval(500);  // 500ms after last dial input
    m_dialInactiveTimer->setSingleShot(true);
    connect(m_dialInactiveTimer, &QTimer::timeout, this, &MainWindow::onDialInactive);

    // Marquee timer for scrolling long voice memory labels
    m_marqueeTimer = new QTimer(this);
    m_marqueeTimer->setInterval(150);  // Scroll speed: 150ms per character
    connect(m_marqueeTimer, &QTimer::timeout, this, &MainWindow::updateMarqueeLabels);

    qDebug() << "MainWindow created";
}

MainWindow::~MainWindow()
{
    m_meterTimer->stop();
    m_syncTimer->stop();
    m_autoSyncTimer->stop();
    m_countdownTimer->stop();
    m_dialInactiveTimer->stop();
    m_marqueeTimer->stop();

    // Disconnect radio if connected
    if (m_radioController && m_radioController->isConnected()) {
        m_radioController->disconnect();
    }

    // Unload WebSDR site
    if (m_webSdrManager) {
        m_webSdrManager->unloadCurrent();
    }

    if (m_audioManager->isRunning()) {
        m_audioManager->stopStreams();
    }

    saveSettings();

    qDebug() << "MainWindow destroyed";
}

void MainWindow::setupWindow()
{
    setWindowTitle(QString("%1 v%2 (%3)").arg(HAMMIXER_APP_NAME).arg(HAMMIXER_VERSION_STRING).arg(HAMMIXER_VERSION_DATE));
    setWindowIcon(QIcon(":/icons/icons/antenna.png"));
    setMinimumSize(1200, 916);  // Full view with WebSDR browser
    resize(m_settings.window().size.width(), m_settings.window().size.height());
    move(m_settings.window().position);

    // Apply stylesheet
    setStyleSheet(Styles::getStylesheet());
}

void MainWindow::setupUI()
{
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // ========== Radio Control Panel (compact single row at top) ==========
    // Now includes: CI-V 30%, WebSDR 20%, Radio Info 25%, Tools 25%
    m_radioControlPanel = new RadioControlPanel(this);
    mainLayout->addWidget(m_radioControlPanel);

    // ========== Device Panel (not in layout - shown via dialog) ==========
    m_devicePanel = new DevicePanel(this);
    m_devicePanel->hide();  // Hidden - accessed via File > Audio Devices dialog

    // ========== Main content area (controls + S-Meters + levels) ==========
    QHBoxLayout* contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(10);
    contentLayout->setAlignment(Qt::AlignTop);

    // Left side: Controls in a widget container (Delay + Crossfader stack)
    QWidget* controlsWidget = new QWidget(this);
    controlsWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QVBoxLayout* controlsLayout = new QVBoxLayout(controlsWidget);
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    controlsLayout->setSpacing(10);

    // Delay controls - two rows: top row has buttons and labels, bottom row has slider
    QGroupBox* delayGroup = new QGroupBox("Delay (Radio)", this);
    delayGroup->setFixedHeight(145);
    QVBoxLayout* delayMainLayout = new QVBoxLayout(delayGroup);
    delayMainLayout->setSpacing(10);

    // Top row: Sync button | Auto-Sync toggle + countdown | delay value
    QHBoxLayout* delayTopRow = new QHBoxLayout();
    delayTopRow->setSpacing(8);

    // Manual Sync button
    m_syncButton = new QPushButton("Sync", this);
    m_syncButton->setToolTip("Detect and apply optimal delay once");
    m_syncButton->setFixedSize(120, 28);
    delayTopRow->addWidget(m_syncButton);

    delayTopRow->addStretch();

    // Auto-Sync toggle button (checkable/toggle style)
    m_autoSyncToggle = new QPushButton("Auto-Sync", this);
    m_autoSyncToggle->setCheckable(true);
    m_autoSyncToggle->setToolTip("Enable continuous automatic sync every 15 seconds");
    m_autoSyncToggle->setFixedSize(100, 28);
    m_autoSyncToggle->setStyleSheet(
        "QPushButton {"
        "  background-color: #3a3a3a;"
        "  border: 1px solid #555;"
        "  border-radius: 4px;"
        "  color: #aaa;"
        "  font-weight: bold;"
        "  padding: 4px 8px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #454545;"
        "  border-color: #666;"
        "}"
        "QPushButton:checked {"
        "  background-color: #2d5a2d;"
        "  border: 1px solid #4a8;"
        "  color: #8f8;"
        "}"
        "QPushButton:checked:hover {"
        "  background-color: #3a6a3a;"
        "}"
    );
    delayTopRow->addWidget(m_autoSyncToggle);

    // Countdown timer label (shows "15s", "14s", ...)
    m_autoSyncCountdown = new QLabel("", this);
    m_autoSyncCountdown->setFixedWidth(35);
    m_autoSyncCountdown->setAlignment(Qt::AlignCenter);
    m_autoSyncCountdown->setStyleSheet(
        "QLabel { font-family: 'Consolas'; font-size: 11pt; font-weight: bold; color: #666; }");
    m_autoSyncCountdown->setToolTip("Time until next auto-sync");
    delayTopRow->addWidget(m_autoSyncCountdown);

    delayTopRow->addStretch();

    // Delay value label
    m_delayLabel = new QLabel("300 ms", this);
    m_delayLabel->setFixedWidth(70);
    m_delayLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_delayLabel->setStyleSheet(
        "QLabel { font-family: 'Consolas'; font-size: 11pt; font-weight: bold; color: #00BCD4; }");
    delayTopRow->addWidget(m_delayLabel);

    delayMainLayout->addLayout(delayTopRow);

    // Bottom row: Slider (full width)
    m_delaySlider = new QSlider(Qt::Horizontal, this);
    m_delaySlider->setRange(0, 2000);  // Extended for distant KiwiSDR sites (AU/NZ)
    m_delaySlider->setValue(300);
    m_delaySlider->setTickPosition(QSlider::TicksBelow);
    m_delaySlider->setTickInterval(200);  // 10 ticks for 2000ms
    delayMainLayout->addWidget(m_delaySlider);

    controlsLayout->addWidget(delayGroup);

    // Crossfader
    QGroupBox* crossfaderGroup = new QGroupBox("Crossfader", this);
    crossfaderGroup->setFixedHeight(125);  // +10px for taller row
    QVBoxLayout* crossfaderLayout = new QVBoxLayout(crossfaderGroup);
    m_crossfader = new Crossfader(this);
    crossfaderLayout->addWidget(m_crossfader);
    controlsLayout->addWidget(crossfaderGroup);

    contentLayout->addWidget(controlsWidget, 1);  // stretch factor 1 = flexible width

    // Middle: S-Meters stack (Radio on top, WebSDR on bottom)
    QGroupBox* smeterGroup = new QGroupBox("S-Meters", this);
    smeterGroup->setFixedWidth(292);  // 250px meter + padding + group box margins
    QVBoxLayout* smeterLayout = new QVBoxLayout(smeterGroup);
    smeterLayout->setContentsMargins(10, 5, 10, 5);
    smeterLayout->setSpacing(5);

    // Radio S-Meter (top)
    m_radioSMeter = new SMeter("Radio", this);
    smeterLayout->addWidget(m_radioSMeter);

    // WebSDR S-Meter (bottom)
    m_websdrSMeter = new SMeter("WebSDR", this);
    smeterLayout->addWidget(m_websdrSMeter);

    contentLayout->addWidget(smeterGroup, 0);  // stretch factor 0 = fixed width

    // Right side: Levels (in a group box)
    QGroupBox* levelsGroup = new QGroupBox("Levels", this);
    levelsGroup->setFixedHeight(280);  // +20px for taller row
    QHBoxLayout* metersLayout = new QHBoxLayout(levelsGroup);
    metersLayout->setContentsMargins(15, 0, 15, 0);
    metersLayout->setSpacing(20);
    metersLayout->setAlignment(Qt::AlignCenter);

    // Radio channel
    m_radioStrip = new ChannelStrip("Radio", this);
    metersLayout->addWidget(m_radioStrip, 0, Qt::AlignTop);

    // WebSDR channel
    m_websdrStrip = new ChannelStrip("WebSDR", this);
    metersLayout->addWidget(m_websdrStrip, 0, Qt::AlignTop);

    // Vertical separator
    QFrame* separator = new QFrame(this);
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);
    metersLayout->addWidget(separator);

    // Master strip
    m_masterStrip = new MasterStrip(this);
    metersLayout->addWidget(m_masterStrip, 0, Qt::AlignTop);

    contentLayout->addWidget(levelsGroup, 0);  // stretch factor 0 = fixed width

    mainLayout->addLayout(contentLayout);

    // ========== Radio Controls Section (Band, Mode, Tuner, Voice) ==========
    mainLayout->addWidget(createRadioControlsSection());

    // ========== Embedded WebSDR Browser (bottom section) ==========
    m_browserGroup = new QGroupBox("WebSDR Browser", this);
    m_browserGroup->setMinimumHeight(350);  // Minimum height, can expand
    m_browserGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);  // Allow vertical expansion
    m_browserGroup->setContentsMargins(0, 0, 0, 0);  // Remove group box margins
    QVBoxLayout* browserLayout = new QVBoxLayout(m_browserGroup);
    browserLayout->setContentsMargins(5, 0, 5, 5);  // Reduced top margin to avoid double spacing

    // Create a container widget for the browser with its own layout
    QWidget* browserContainer = new QWidget(m_browserGroup);
    browserContainer->setMinimumHeight(260);  // Minimum height
    browserContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);  // Allow expansion
    QVBoxLayout* containerLayout = new QVBoxLayout(browserContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);
    browserLayout->addWidget(browserContainer, 1);  // Stretch factor 1

    mainLayout->addWidget(m_browserGroup, 1);  // Stretch factor 1 - expands to fill space

    // Create WebSDR manager with the browser container for embedded mode
    m_webSdrManager = new WebSdrManager(browserContainer, this);

    // Set site list in manager and populate dropdown (no sites loaded yet)
    m_webSdrManager->setSiteList(m_settings.webSdrSites());
    m_radioControlPanel->setSiteList(m_settings.webSdrSites());

    // Pre-initialize Chromium engine at startup to avoid visual blink on first connect
    m_webSdrManager->preInitialize();
}

void MainWindow::setupMenuBar()
{
    // ===== File Menu =====
    QMenu* fileMenu = menuBar()->addMenu("&File");

    fileMenu->addAction("&Open Config...", this, &MainWindow::onOpenConfig, QKeySequence::Open);
    fileMenu->addAction("&Save Config", this, &MainWindow::onSaveConfig, QKeySequence::Save);

    m_recentConfigsMenu = fileMenu->addMenu("Open &Recent");
    updateRecentConfigsMenu();

    fileMenu->addSeparator();
    fileMenu->addAction("E&xit", this, &QMainWindow::close, QKeySequence::Quit);

    // ===== Tools Menu =====
    QMenu* toolsMenu = menuBar()->addMenu("&Tools");

    toolsMenu->addAction("&Audio Devices...", this, &MainWindow::onAudioDevicesClicked);
    toolsMenu->addAction("Manage &SDR Sites...", this, &MainWindow::onManageWebSdr);
    toolsMenu->addAction("&Voice Memory...", this, &MainWindow::onVoiceMemoryConfig);

    // ===== Help Menu =====
    QMenu* helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("&About", this, [this]() {
        QMessageBox::about(this, "About HamMixer",
            QString("<h3>HamMixer :: Transceiver + WebSDR Mixer v%1</h3>"
                    "<p>Mixes transceiver audio with multiple WebSDR audio.</p>"
                    "<p>Architecture and management by Pedro Silva CT7BAC, Portugal.</p>"
                    "<p>Coding 100% by Claude Code AI.</p>"
                    "<p>Built with C++ and Qt %2.</p>"
                    "<p>Contacts by email: <a href='mailto:ct7bac@gmail.com'>ct7bac@gmail.com</a></p>"
                    "<p><b>Good DX and 73!</b></p>")
                .arg(HAMMIXER_VERSION_STRING)
                .arg(QT_VERSION_STR));
    });
}

QWidget* MainWindow::createRadioControlsSection()
{
    QWidget* container = new QWidget(this);
    QHBoxLayout* mainLayout = new QHBoxLayout(container);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(10);

    // Common button dimensions
    static constexpr int BUTTON_WIDTH = 46;
    static constexpr int BUTTON_HEIGHT = 40;
    static constexpr int BUTTON_SPACING = 4;

    // ===== Band Section (2 rows x 5 cols) - Fixed width =====
    QGroupBox* bandGroup = new QGroupBox("Band", container);
    bandGroup->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    QGridLayout* bandLayout = new QGridLayout(bandGroup);
    bandLayout->setSpacing(BUTTON_SPACING);
    bandLayout->setContentsMargins(8, 8, 8, 8);

    m_bandGroup = new QButtonGroup(this);
    m_bandGroup->setExclusive(true);

    // Two-line labels: wavelength on top, frequency below
    const char* bandWavelengths[] = {"160m", "80m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m"};
    const char* bandFreqs[] = {"1.8", "3.5", "7", "10", "14", "18", "21", "24", "28", "50"};

    QString bandButtonStyle =
        "QPushButton { "
        "  background-color: #3A3A3A; "
        "  border: 1px solid #555; "
        "  border-radius: 4px; "
        "  padding: 2px 2px; "
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

    // Row 0: 160m, 80m, 40m, 30m, 20m (indices 0-4)
    // Row 1: 17m, 15m, 12m, 10m, 6m (indices 5-9)
    for (int i = 0; i < BAND_COUNT; i++) {
        QString label = QString("%1\n%2").arg(bandWavelengths[i]).arg(bandFreqs[i]);
        QPushButton* btn = new QPushButton(label, bandGroup);
        btn->setCheckable(true);
        btn->setAutoDefault(false);
        btn->setDefault(false);
        btn->setFixedSize(BUTTON_WIDTH, BUTTON_HEIGHT);
        btn->setStyleSheet(bandButtonStyle);
        btn->setToolTip(QString("%1 band (%2 MHz)").arg(bandWavelengths[i]).arg(bandFreqs[i]));
        btn->setEnabled(false);  // Disabled until connected
        m_bandButtons[i] = btn;
        m_bandGroup->addButton(btn, i);

        int row = (i < 5) ? 0 : 1;
        int col = (i < 5) ? i : (i - 5);
        bandLayout->addWidget(btn, row, col);
    }

    connect(m_bandGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, &MainWindow::onBandSelected);

    mainLayout->addWidget(bandGroup, 0);  // stretch = 0 (fixed)

    // ===== Mode + Tuner Section - Fixed width (same as Band) =====
    QGroupBox* modeTunerGroup = new QGroupBox("Mode / Tuner", container);
    modeTunerGroup->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    QVBoxLayout* modeTunerLayout = new QVBoxLayout(modeTunerGroup);
    modeTunerLayout->setSpacing(BUTTON_SPACING);
    modeTunerLayout->setContentsMargins(8, 8, 8, 8);

    // Mode row
    QHBoxLayout* modeLayout = new QHBoxLayout();
    modeLayout->setSpacing(BUTTON_SPACING);

    m_modeGroup = new QButtonGroup(this);
    m_modeGroup->setExclusive(true);

    const char* modeLabels[] = {"LSB", "USB", "CW", "AM", "FM"};

    QString modeButtonStyle =
        "QPushButton { "
        "  background-color: #3A3A3A; "
        "  border: 1px solid #555; "
        "  border-radius: 4px; "
        "  padding: 2px 2px; "
        "  font-weight: bold; "
        "  font-size: 9pt; "
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
        QPushButton* btn = new QPushButton(modeLabels[i], modeTunerGroup);
        btn->setCheckable(true);
        btn->setAutoDefault(false);
        btn->setDefault(false);
        btn->setFixedSize(BUTTON_WIDTH, BUTTON_HEIGHT);
        btn->setStyleSheet(modeButtonStyle);
        btn->setEnabled(false);  // Disabled until connected
        m_modeButtons[i] = btn;
        m_modeGroup->addButton(btn, i);
        modeLayout->addWidget(btn);
    }

    modeTunerLayout->addLayout(modeLayout);

    // Tuner row
    QHBoxLayout* tunerLayout = new QHBoxLayout();
    tunerLayout->setSpacing(BUTTON_SPACING);

    m_tuneButton = new QPushButton("TUNE", modeTunerGroup);
    m_tuneButton->setAutoDefault(false);
    m_tuneButton->setDefault(false);
    m_tuneButton->setFixedHeight(BUTTON_HEIGHT);
    m_tuneButton->setStyleSheet(
        "QPushButton { "
        "  background-color: #FF9800; "
        "  border: 1px solid #FFB74D; "
        "  border-radius: 4px; "
        "  padding: 2px 10px; "
        "  font-weight: bold; "
        "  font-size: 10pt; "
        "  color: #222; "
        "}"
        "QPushButton:hover { background-color: #FFB74D; }"
        "QPushButton:pressed { background-color: #E65100; }"
        "QPushButton:disabled { background-color: #5A4A20; color: #888; }"
    );
    m_tuneButton->setToolTip("Start antenna tuner");
    m_tuneButton->setEnabled(false);
    connect(m_tuneButton, &QPushButton::clicked, this, &MainWindow::onTuneClicked);

    m_tunerToggle = new QPushButton("ATU OFF", modeTunerGroup);
    m_tunerToggle->setAutoDefault(false);
    m_tunerToggle->setDefault(false);
    m_tunerToggle->setCheckable(true);
    m_tunerToggle->setFixedHeight(BUTTON_HEIGHT);
    m_tunerToggle->setStyleSheet(
        "QPushButton { "
        "  background-color: #424242; "
        "  border: 1px solid #555; "
        "  border-radius: 4px; "
        "  padding: 2px 10px; "
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
    m_tunerToggle->setEnabled(false);
    connect(m_tunerToggle, &QPushButton::clicked, this, &MainWindow::onTunerToggled);

    tunerLayout->addWidget(m_tuneButton, 1);  // stretch = 1
    tunerLayout->addWidget(m_tunerToggle, 1);  // stretch = 1

    modeTunerLayout->addLayout(tunerLayout);

    connect(m_modeGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, &MainWindow::onModeSelected);

    mainLayout->addWidget(modeTunerGroup, 0);  // stretch = 0 (fixed)

    // ===== Voice Memory Section (2 rows x 4 cols) - Expands to fill =====
    QGroupBox* voiceGroup = new QGroupBox("Voice Memory", container);
    voiceGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QGridLayout* voiceLayout = new QGridLayout(voiceGroup);
    voiceLayout->setSpacing(BUTTON_SPACING);
    voiceLayout->setContentsMargins(8, 8, 8, 8);

    QString voiceButtonStyle =
        "QPushButton { "
        "  background-color: #3A3A3A; "
        "  border: 1px solid #555; "
        "  border-radius: 4px; "
        "  padding: 2px 4px; "
        "  font-weight: bold; "
        "  font-size: 10pt; "
        "  color: #DDD; "
        "}"
        "QPushButton:hover { background-color: #5A5A5A; }"
        "QPushButton:pressed { background-color: #9C27B0; color: white; }"
        "QPushButton:disabled { background-color: #2A2A2A; color: #555; }";

    // Row 0: M1, M2, M3, M4
    // Row 1: M5, M6, M7, M8
    for (int i = 0; i < 8; i++) {
        QPushButton* btn = new QPushButton(QString("M%1").arg(i + 1), voiceGroup);
        btn->setAutoDefault(false);
        btn->setDefault(false);
        btn->setFixedHeight(BUTTON_HEIGHT);
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        btn->setStyleSheet(voiceButtonStyle);
        btn->setToolTip(QString("Play voice memory %1").arg(i + 1));
        btn->setProperty("memoryNumber", i + 1);
        btn->setEnabled(false);  // Disabled until connected
        m_voiceButtons[i] = btn;
        connect(btn, &QPushButton::clicked, this, &MainWindow::onVoiceMemoryClicked);

        int row = (i < 4) ? 0 : 1;
        int col = (i < 4) ? i : (i - 4);
        voiceLayout->addWidget(btn, row, col);
    }

    mainLayout->addWidget(voiceGroup, 1);  // stretch = 1 (expands)

    return container;
}

void MainWindow::connectSignals()
{
    // Tools section - record button
    connect(m_radioControlPanel, &RadioControlPanel::recordClicked,
            this, &MainWindow::onRecordClicked);

    // Delay and sync controls
    connect(m_delaySlider, &QSlider::valueChanged, this, &MainWindow::onDelayChanged);
    connect(m_syncButton, &QPushButton::clicked, this, &MainWindow::onSyncClicked);
    connect(m_autoSyncToggle, &QPushButton::toggled, this, &MainWindow::onAutoSyncToggled);

    // Crossfader
    connect(m_crossfader, &Crossfader::crossfaderChanged, this, &MainWindow::onCrossfaderChanged);

    // Mute buttons - with pan override for single-channel listening
    connect(m_radioStrip, &ChannelStrip::muteChanged, this, &MainWindow::onMuteChanged);
    connect(m_websdrStrip, &ChannelStrip::muteChanged, this, &MainWindow::onMuteChanged);

    // Channel volume controls - apply through applyPanningWithMuteOverride()
    // to properly combine slider volume with crossfader position
    connect(m_radioStrip, &ChannelStrip::volumeChanged, this, [this](int) {
        applyPanningWithMuteOverride();
        m_settings.markDirty();
    });
    connect(m_websdrStrip, &ChannelStrip::volumeChanged, this, [this](int) {
        applyPanningWithMuteOverride();
        m_settings.markDirty();
    });

    // Master controls
    connect(m_masterStrip, &MasterStrip::volumeChanged, this, [this](int volume) {
        if (m_audioManager->mixer()) {
            m_audioManager->mixer()->setMasterVolume(volume / 100.0f);
        }
        m_settings.markDirty();
    });
    connect(m_masterStrip, &MasterStrip::muteChanged, this, [this](bool muted) {
        if (m_audioManager->mixer()) {
            m_audioManager->mixer()->setMasterMute(muted);
        }
        m_settings.markDirty();
    });

    // Audio manager signals
    connect(m_audioManager.get(), &AudioManager::errorOccurred, this, [this](const QString& error) {
        QMessageBox::warning(this, "Audio Error", error);
    });

    // ========== Radio Control Panel signals ==========
    connect(m_radioControlPanel, &RadioControlPanel::serialConnectClicked,
            this, &MainWindow::onSerialConnectClicked);
    connect(m_radioControlPanel, &RadioControlPanel::serialDisconnectClicked,
            this, &MainWindow::onSerialDisconnectClicked);
    // Note: Radio controller signals are connected after auto-detection in onSerialConnectClicked()

    // ========== WebSDR Manager signals ==========
    connect(m_radioControlPanel, &RadioControlPanel::webSdrSiteChanged,
            this, &MainWindow::onWebSdrSiteChanged);
    connect(m_radioControlPanel, &RadioControlPanel::manageSitesClicked,
            this, &MainWindow::onManageWebSdr);
    connect(m_radioControlPanel, &RadioControlPanel::webSdrViewToggled,
            this, &MainWindow::onToggleWebSdrView);

    connect(m_webSdrManager, &WebSdrManager::stateChanged,
            this, &MainWindow::onWebSdrStateChanged);
    connect(m_webSdrManager, &WebSdrManager::smeterChanged,
            this, &MainWindow::onWebSdrSmeterChanged);
    connect(m_webSdrManager, &WebSdrManager::siteReady,
            this, [this](const QString& siteId) {
                qDebug() << "WebSDR site ready:" << siteId;

                // Keep channel muted while we check audio levels
                // Wait 300ms for audio to stabilize, then check peak levels
                QTimer::singleShot(300, this, [this, siteId]() {
                    checkAndUnmuteWebSdrChannel(siteId);
                });
            });
}

void MainWindow::loadSettings()
{
    // Load from default AppData config (only called at startup)
    m_settings.load();
    m_settings.loadRecentConfigs();

    // Apply to UI
    applySettingsToUI();
}

void MainWindow::applySettingsToUI()
{
    // Apply current m_settings to UI widgets (without reloading from file)
    m_delaySlider->setValue(m_settings.channel1().delayMs);

    // Block signals when restoring toggle state to prevent starting timers
    // Auto-sync will only actually start when user connects and toggle is checked
    m_autoSyncToggle->blockSignals(true);
    m_autoSyncToggle->setChecked(m_settings.channel1().autoSyncEnabled);
    m_autoSyncToggle->blockSignals(false);

    m_masterStrip->setVolume(m_settings.master().volume);
    m_masterStrip->setMuted(m_settings.master().muted);
    m_radioStrip->setMuted(m_settings.channel1().muted);
    m_websdrStrip->setMuted(m_settings.channel2().muted);

    // Load channel volumes
    m_radioStrip->setVolume(m_settings.channel1().volume);
    m_websdrStrip->setVolume(m_settings.channel2().volume);

    // Set recording directory
    if (m_audioManager->recorder()) {
        m_audioManager->recorder()->setRecordingDirectory(m_settings.recording().directory);
    }

    // Apply serial settings
    if (!m_settings.serial().portName.isEmpty()) {
        m_radioControlPanel->setSelectedPort(m_settings.serial().portName);
    }
    m_radioControlPanel->setDialStepIndex(m_settings.serial().dialStepIndex);

    // Apply voice memory labels to buttons
    m_voiceMemoryLabels = m_settings.voiceMemoryLabels();
    updateVoiceButtonLabels();

    // Apply WebSDR settings - update site list from loaded settings
    // This is important because setupUI() ran before load() with default sites
    m_webSdrManager->setSiteList(m_settings.webSdrSites());
    m_radioControlPanel->setSiteList(m_settings.webSdrSites());
    m_radioControlPanel->setSelectedSite(m_settings.webSdr().selectedSiteId);

    // Apply WebSDR browser view state (compact/full mode)
    if (m_browserGroup) {
        bool showBrowser = m_settings.webSdr().showBrowser;
        m_radioControlPanel->setWebSdrViewVisible(showBrowser);

        if (!showBrowser) {
            // Start in compact mode - same height as toggle function
            static constexpr int COMPACT_HEIGHT = 563;
            m_browserGroup->hide();
            setMinimumSize(1200, COMPACT_HEIGHT);
            resize(width(), COMPACT_HEIGHT);
        }
    }

    // Clear dirty flag since we just loaded/applied settings
    m_settings.clearDirty();

    // NOTE: WebSDR sites are NOT preloaded at startup anymore
    // They will be loaded when CONNECT is clicked
}

void MainWindow::saveSettings()
{
    // Save window geometry
    m_settings.window().position = pos();
    m_settings.window().size = size();

    // Save device selections
    m_settings.devices().radioInput = m_devicePanel->getSelectedInputName();
    m_settings.devices().systemLoopback = m_devicePanel->getSelectedLoopbackName();
    m_settings.devices().output = m_devicePanel->getSelectedOutputName();

    // Save channel settings
    m_settings.channel1().delayMs = m_delaySlider->value();
    m_settings.channel1().muted = m_radioStrip->isMuted();
    m_settings.channel1().volume = m_radioStrip->getVolume();
    m_settings.channel1().autoSyncEnabled = m_autoSyncToggle->isChecked();
    m_settings.channel2().muted = m_websdrStrip->isMuted();
    m_settings.channel2().volume = m_websdrStrip->getVolume();
    m_settings.master().volume = m_masterStrip->getVolume();
    m_settings.master().muted = m_masterStrip->isMuted();

    // Save serial settings
    m_settings.serial().portName = m_radioControlPanel->selectedPort();
    m_settings.serial().dialStepIndex = m_radioControlPanel->dialStepIndex();

    // Save WebSDR settings
    WebSdrSite site = m_radioControlPanel->selectedSite();
    m_settings.webSdr().selectedSiteId = site.id;

    m_settings.save();
}

void MainWindow::refreshDevices()
{
    m_audioManager->refreshDevices();

    m_devicePanel->populateInputDevices(m_audioManager->getInputDevices());
    m_devicePanel->populateLoopbackDevices(m_audioManager->getLoopbackDevices());
    m_devicePanel->populateOutputDevices(m_audioManager->getOutputDevices());

    // Restore saved selections
    m_devicePanel->setSelectedInputByName(m_settings.devices().radioInput);
    m_devicePanel->setSelectedLoopbackByName(m_settings.devices().systemLoopback);
    m_devicePanel->setSelectedOutputByName(m_settings.devices().output);
}

void MainWindow::onRecordClicked()
{
    Recorder* recorder = m_audioManager->recorder();
    if (!recorder) return;

    if (recorder->isRecording()) {
        recorder->stopRecording();
        m_radioControlPanel->setRecordingActive(false);
        qDebug() << "Recording stopped";
    } else {
        QString filename = recorder->startRecording();
        if (!filename.isEmpty()) {
            m_radioControlPanel->setRecordingActive(true);
            qDebug() << "Recording started:" << filename;
        } else {
            QMessageBox::warning(this, "Error", "Failed to start recording");
        }
    }
}

void MainWindow::onDelayChanged(int value)
{
    m_delayLabel->setText(QString("%1 ms").arg(value));

    if (m_audioManager->mixer()) {
        m_audioManager->mixer()->setDelayMs(static_cast<float>(value));
    }

    m_settings.markDirty();
}

void MainWindow::onSyncClicked()
{
    if (!m_audioManager->isRunning()) {
        QMessageBox::information(this, "Sync",
            "Please connect first to start the audio streams.");
        return;
    }

    MixerCore* mixer = m_audioManager->mixer();
    if (!mixer) return;

    // If already capturing, cancel
    if (mixer->isSyncCapturing()) {
        mixer->cancelSyncCapture();
        m_syncButton->setText("Sync");
        m_syncTimer->stop();
        return;
    }

    // Determine sync mode based on radio's current operating mode
    // CW and RTTY modes use tone-based (CW) sync algorithm
    // All other modes (SSB, AM, FM) use voice-based sync algorithm
    AudioSync::SignalMode syncMode = AudioSync::VOICE;  // Default

    if (m_radioController) {
        uint8_t radioMode = m_radioController->currentMode();

        // CW modes use envelope correlation (pitch-independent)
        if (radioMode == CIVProtocol::MODE_CW ||
            radioMode == CIVProtocol::MODE_CW_R ||
            radioMode == CIVProtocol::MODE_RTTY ||
            radioMode == CIVProtocol::MODE_RTTY_R) {
            syncMode = AudioSync::CW;
            qDebug() << "Sync: Using CW mode (tone-based, pitch-independent)";
        } else {
            qDebug() << "Sync: Using VOICE mode (VAD-based)";
        }
    }

    // Start sync capture with the appropriate mode
    mixer->startSyncCapture(syncMode);
    m_syncButton->setText("Syncing...");

    // Start timer to monitor progress
    m_syncTimer->start(100);  // Check every 100ms
}

void MainWindow::onAutoSyncToggled(bool enabled)
{
    if (enabled) {
        // Start countdown and periodic sync
        m_countdownSeconds = AUTO_SYNC_INTERVAL_SEC;
        m_autoSyncCountdown->setText(QString("%1s").arg(m_countdownSeconds));
        m_autoSyncCountdown->setStyleSheet(
            "QLabel { font-family: 'Consolas'; font-size: 11pt; font-weight: bold; color: #8f8; }");
        m_countdownTimer->start(1000);  // Update every second
        m_autoSyncTimer->start(AUTO_SYNC_INTERVAL_SEC * 1000);
        qDebug() << "Auto-Sync: Enabled, interval =" << AUTO_SYNC_INTERVAL_SEC << "seconds";
    } else {
        // Stop everything
        m_autoSyncTimer->stop();
        m_countdownTimer->stop();
        m_autoSyncCountdown->setText("");
        m_autoSyncCountdown->setStyleSheet(
            "QLabel { font-family: 'Consolas'; font-size: 11pt; font-weight: bold; color: #666; }");
        qDebug() << "Auto-Sync: Disabled";
    }
    m_settings.markDirty();
}

void MainWindow::onCountdownTick()
{
    MixerCore* mixer = m_audioManager->mixer();

    // If currently syncing, hide countdown (progress shown in Sync button)
    if (mixer && mixer->isSyncCapturing()) {
        m_autoSyncCountdown->setText("...");
        m_autoSyncCountdown->setStyleSheet(
            "QLabel { font-family: 'Consolas'; font-size: 11pt; font-weight: bold; color: #ff0; }");
        return;
    }

    // Normal countdown
    if (m_countdownSeconds > 0) {
        m_countdownSeconds--;
        m_autoSyncCountdown->setText(QString("%1s").arg(m_countdownSeconds));

        // Change color as it gets close to zero
        if (m_countdownSeconds <= 3) {
            m_autoSyncCountdown->setStyleSheet(
                "QLabel { font-family: 'Consolas'; font-size: 11pt; font-weight: bold; color: #fa0; }");
        } else {
            m_autoSyncCountdown->setStyleSheet(
                "QLabel { font-family: 'Consolas'; font-size: 11pt; font-weight: bold; color: #8f8; }");
        }
    }
}

void MainWindow::onAutoSyncTimerTick()
{
    // Skip if not connected or already syncing
    if (!m_audioManager->isRunning()) {
        return;
    }

    MixerCore* mixer = m_audioManager->mixer();
    if (!mixer || mixer->isSyncCapturing()) {
        return;  // Skip if already syncing
    }

    // Mark this as an auto-triggered sync (for threshold checking)
    m_isAutoTriggeredSync = true;

    // Determine sync mode based on radio's current operating mode (same as manual sync)
    AudioSync::SignalMode syncMode = AudioSync::VOICE;

    if (m_radioController) {
        uint8_t radioMode = m_radioController->currentMode();

        if (radioMode == CIVProtocol::MODE_CW ||
            radioMode == CIVProtocol::MODE_CW_R ||
            radioMode == CIVProtocol::MODE_RTTY ||
            radioMode == CIVProtocol::MODE_RTTY_R) {
            syncMode = AudioSync::CW;
        }
    }

    // Reset countdown for next cycle
    m_countdownSeconds = AUTO_SYNC_INTERVAL_SEC;

    // Start sync capture
    mixer->startSyncCapture(syncMode);
    m_syncButton->setText("Syncing...");

    // Start timer to monitor progress
    m_syncTimer->start(100);

    qDebug() << "Auto-Sync: Timer triggered sync";
}

void MainWindow::onCrossfaderChanged(float radioVol, float radioPan, float websdrVol, float websdrPan)
{
    Q_UNUSED(radioVol);
    Q_UNUSED(radioPan);
    Q_UNUSED(websdrVol);
    Q_UNUSED(websdrPan);

    // Use applyPanningWithMuteOverride to handle both crossfader and mute state
    applyPanningWithMuteOverride();
}

void MainWindow::checkAndUnmuteWebSdrChannel(const QString& siteId)
{
    MixerCore* mixer = m_audioManager->mixer();
    if (!mixer) {
        qDebug() << "MainWindow: No mixer available for level check";
        // Unmute anyway
        m_websdrStrip->setMuted(false);
        mixer = m_audioManager->mixer();
        if (mixer) mixer->setChannel2Mute(false);
        applyPanningWithMuteOverride();
        return;
    }

    // Get current raw peak level of channel 2 (linear scale 0.0-1.0)
    float ch1Level, ch2Level;
    mixer->getRawLevels(ch1Level, ch2Level);

    qDebug() << "MainWindow: WebSDR channel peak level:" << ch2Level
             << "(threshold:" << WEBSDR_PEAK_THRESHOLD << ")";

    // If peak level exceeds threshold, calculate and apply volume reduction
    if (ch2Level > WEBSDR_PEAK_THRESHOLD) {
        // Calculate what volume would bring peaks to ~60% (slightly below threshold)
        // targetLevel = currentLevel * newVolume/currentVolume
        // newVolume = targetLevel / currentLevel * currentVolume
        float currentVolume = m_websdrStrip->getVolume() / 100.0f;
        float targetLevel = 0.60f;  // Target 60%, giving some headroom

        // Avoid division by zero
        if (ch2Level > 0.001f) {
            float newVolume = (targetLevel / ch2Level) * currentVolume;
            // Clamp to valid range and convert back to 0-100 scale
            int newVolumePercent = static_cast<int>(std::clamp(newVolume * 100.0f, 5.0f, 100.0f));

            qDebug() << "MainWindow: Auto-reducing WebSDR volume from"
                     << m_websdrStrip->getVolume() << "to" << newVolumePercent
                     << "(peak was" << ch2Level << ")";

            // Apply the reduced volume
            m_websdrStrip->setVolume(newVolumePercent);
        }
    }

    // Now unmute the channel
    m_websdrStrip->setMuted(false);
    mixer->setChannel2Mute(false);

    // Apply panning with the new mute state
    applyPanningWithMuteOverride();

    qDebug() << "MainWindow: Unmuted WebSDR channel after site ready:" << siteId;
}

void MainWindow::updateMeters()
{
    if (!m_audioManager->isRunning()) return;

    MixerCore* mixer = m_audioManager->mixer();
    if (!mixer) return;

    float ch1Left, ch1Right, ch2Left, ch2Right, masterLeft, masterRight;
    mixer->getLevels(ch1Left, ch1Right, ch2Left, ch2Right, masterLeft, masterRight);

    // Update channel meters (use average of L/R for mono display)
    float ch1Level = (ch1Left + ch1Right) / 2.0f;
    float ch2Level = (ch2Left + ch2Right) / 2.0f;

    m_radioStrip->updateLevel(ch1Level);
    m_websdrStrip->updateLevel(ch2Level);
    m_masterStrip->updateLevel(masterLeft, masterRight);

    // Radio S-Meter: Use delayed CI-V data if connected (synced with audio delay)
    float radioSMeterLevel;
    if (m_civConnected) {
        radioSMeterLevel = getDelayedSMeterValue();
    } else {
        radioSMeterLevel = -80.0f;  // S0 / "No Signal" when not connected
    }
    m_radioSMeter->setLevel(radioSMeterLevel);

    // WebSDR S-Meter: Use page's S-meter data if available, otherwise audio level.
    // Apply 200ms delay to compensate for browser audio buffering.
    float websdrSMeterLevel;
    if (m_websdrSmeterValid) {
        websdrSMeterLevel = getDelayedWebSdrSMeterValue();
    } else {
        websdrSMeterLevel = ch2Level;  // Fallback to audio level
    }
    m_websdrSMeter->setLevel(websdrSMeterLevel);
}

void MainWindow::checkSyncResult()
{
    MixerCore* mixer = m_audioManager->mixer();
    if (!mixer) {
        m_syncTimer->stop();
        m_isAutoTriggeredSync = false;
        return;
    }

    // Update progress display
    if (mixer->isSyncCapturing()) {
        int progress = static_cast<int>(mixer->getSyncProgress() * 100);
        m_syncButton->setText(QString("Syncing %1%").arg(progress));
        return;
    }

    // Check if result is ready
    if (mixer->hasSyncResult()) {
        m_syncTimer->stop();
        m_syncButton->setText("Sync");

        AudioSync::SyncResult result = mixer->getSyncResult();
        bool wasAutoTriggered = m_isAutoTriggeredSync;
        m_isAutoTriggeredSync = false;

        if (result.success) {
            // Apply the detected delay (can be positive or negative)
            int newDelayMs = static_cast<int>(result.delayMs);
            int currentDelayMs = m_delaySlider->value();

            // For auto-triggered syncs, check if delta is within threshold
            if (wasAutoTriggered) {
                float delta = std::abs(static_cast<float>(newDelayMs - currentDelayMs));

                if (delta > AUTO_SYNC_THRESHOLD_MS) {
                    // Delta too large - skip this correction
                    qDebug() << "Auto-Sync: Skipped, delta =" << delta
                             << "ms exceeds threshold of" << AUTO_SYNC_THRESHOLD_MS << "ms"
                             << "(current:" << currentDelayMs << ", detected:" << newDelayMs << ")";
                    return;
                }

                // Apply silently (no message box for auto sync)
                if (newDelayMs >= 0) {
                    m_delaySlider->setValue(newDelayMs);
                    qDebug() << "Auto-Sync: Applied delay =" << newDelayMs
                             << "ms (delta:" << delta << "ms, confidence:" << result.confidence * 100 << "%)";
                } else {
                    m_delaySlider->setValue(0);
                    qDebug() << "Auto-Sync: WebSDR ahead, set delay to 0";
                }
                return;
            }

            // Manual sync - show message boxes
            if (newDelayMs >= 0) {
                // Normal case: WebSDR is behind Radio, add delay to Radio channel
                m_delaySlider->setValue(newDelayMs);

                QMessageBox::information(this, "Sync Complete",
                    QString("Detected delay: %1 ms\nConfidence: %2%\n\n"
                            "The delay has been applied automatically.")
                        .arg(newDelayMs)
                        .arg(static_cast<int>(result.confidence * 100)));
            } else {
                // Unusual case: WebSDR is ahead of Radio
                // We can only delay Radio, not advance it, so set to 0
                m_delaySlider->setValue(0);

                QMessageBox::information(this, "Sync Complete",
                    QString("Detected offset: %1 ms (WebSDR ahead)\nConfidence: %2%\n\n"
                            "The WebSDR signal arrives before the Radio signal.\n"
                            "Delay has been set to 0 ms (minimum possible).")
                        .arg(newDelayMs)
                        .arg(static_cast<int>(result.confidence * 100)));
            }
        } else {
            // Sync failed
            if (wasAutoTriggered) {
                // Silent failure for auto sync
                qDebug() << "Auto-Sync: Failed, confidence =" << result.confidence * 100 << "%";
                return;
            }

            // Manual sync - show warning
            QMessageBox::warning(this, "Sync Failed",
                QString("Could not detect reliable sync.\n"
                        "Confidence: %1%\n\n"
                        "Tips:\n"
                        "- Ensure both audio sources are active\n"
                        "- Try with stronger signals\n"
                        "- Verify correct device selection")
                    .arg(static_cast<int>(result.confidence * 100)));
        }
    }
}

void MainWindow::onSaveConfig()
{
    QString fileName = m_settings.currentConfigPath();

    // If we have a current config file, save directly to it (no file picker)
    if (!fileName.isEmpty() && QFile::exists(fileName)) {
        // Update current settings before saving
        saveSettings();

        if (m_settings.saveToFile(fileName)) {
            // Saved silently to current file
            qDebug() << "Config saved to:" << fileName;
        } else {
            QMessageBox::warning(this, "Save Failed",
                QString("Failed to save configuration to:\n%1").arg(fileName));
        }
        return;
    }

    // No current config file - show file picker for "Save As" behavior
    QString configDir = Settings::getConfigurationsDir();
    QDir().mkpath(configDir);

    fileName = QFileDialog::getSaveFileName(
        this,
        "Save Configuration",
        configDir,
        "HamMixer Config (*.json);;All Files (*)"
    );

    if (fileName.isEmpty()) {
        return;
    }

    // Ensure .json extension
    if (!fileName.endsWith(".json", Qt::CaseInsensitive)) {
        fileName += ".json";
    }

    // Update current settings before saving
    saveSettings();

    if (m_settings.saveToFile(fileName)) {
        updateRecentConfigsMenu();
        // No popup - just save silently
    } else {
        QMessageBox::warning(this, "Save Failed",
            QString("Failed to save configuration to:\n%1").arg(fileName));
    }
}

void MainWindow::onOpenConfig()
{
    // Open directly without confirmation - user knows what they want
    QString configDir = Settings::getConfigurationsDir();
    QDir().mkpath(configDir);

    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Open Configuration",
        configDir,
        "HamMixer Config (*.json);;All Files (*)"
    );

    if (fileName.isEmpty()) {
        return;
    }

    if (m_settings.loadFromFile(fileName)) {
        // Apply loaded settings to UI (don't reload from default config!)
        applySettingsToUI();
        updateRecentConfigsMenu();
        // No popup - just load silently
    } else {
        QMessageBox::warning(this, "Load Failed",
            QString("Failed to load configuration from:\n%1").arg(fileName));
    }
}

void MainWindow::onOpenRecentConfig()
{
    QAction* action = qobject_cast<QAction*>(sender());
    if (!action) return;

    QString filePath = action->data().toString();
    if (filePath.isEmpty()) return;

    // Open directly without confirmation - user knows what they want
    if (m_settings.loadFromFile(filePath)) {
        // Apply loaded settings to UI (don't reload from default config!)
        applySettingsToUI();
        updateRecentConfigsMenu();
    } else {
        QMessageBox::warning(this, "Load Failed",
            QString("Failed to load configuration from:\n%1\n\nThe file may have been moved or deleted.")
                .arg(filePath));
        // Remove from recent list since it's invalid
        m_settings.loadRecentConfigs();  // Reload to remove invalid entries
        updateRecentConfigsMenu();
    }
}

void MainWindow::updateRecentConfigsMenu()
{
    m_recentConfigsMenu->clear();

    QStringList recentConfigs = m_settings.recentConfigs();

    if (recentConfigs.isEmpty()) {
        QAction* noRecent = m_recentConfigsMenu->addAction("(No recent files)");
        noRecent->setEnabled(false);
        return;
    }

    for (const QString& filePath : recentConfigs) {
        QFileInfo fileInfo(filePath);
        QString displayName = fileInfo.fileName();

        QAction* action = m_recentConfigsMenu->addAction(displayName);
        action->setData(filePath);
        action->setToolTip(filePath);
        connect(action, &QAction::triggered, this, &MainWindow::onOpenRecentConfig);
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // Check for unsaved changes
    if (m_settings.isDirty()) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            "Unsaved Changes",
            "You have unsaved configuration changes.\n\nDo you want to save before closing?",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save
        );

        if (reply == QMessageBox::Cancel) {
            event->ignore();
            return;
        }
        if (reply == QMessageBox::Save) {
            onSaveConfig();
        }
    }

    // Use the same cleanup sequence as disconnect
    onSerialDisconnectClicked();

    saveSettings();
    event->accept();
}

void MainWindow::onMuteChanged()
{
    MixerCore* mixer = m_audioManager->mixer();
    if (!mixer) return;

    // Apply the mute button states directly to the mixer
    mixer->setChannel1Mute(m_radioStrip->isMuted());
    mixer->setChannel2Mute(m_websdrStrip->isMuted());

    // Apply panning with mute override
    applyPanningWithMuteOverride();

    m_settings.markDirty();
}

void MainWindow::applyPanningWithMuteOverride()
{
    MixerCore* mixer = m_audioManager->mixer();
    if (!mixer) return;

    bool ch1Muted = m_radioStrip->isMuted();
    bool ch2Muted = m_websdrStrip->isMuted();

    // Get crossfader values
    float radioVol, radioPan, websdrVol, websdrPan;
    Crossfader::calculateChannelParams(m_crossfader->getPosition(),
                                        radioVol, radioPan, websdrVol, websdrPan);

    // Apply pan override when one channel is muted
    // The remaining channel is center-panned so audio plays in both ears
    if (ch1Muted && !ch2Muted) {
        // Radio muted - center-pan WebSDR
        websdrPan = 0.0f;
    } else if (ch2Muted && !ch1Muted) {
        // WebSDR muted - center-pan Radio
        radioPan = 0.0f;
    }

    // Get channel slider volumes (0-100 scale -> 0.0-1.0)
    float ch1SliderVol = m_radioStrip->getVolume() / 100.0f;
    float ch2SliderVol = m_websdrStrip->getVolume() / 100.0f;

    // Apply to mixer - MULTIPLY slider volume with crossfader volume
    // This ensures both the channel slider AND crossfader affect the final volume
    mixer->setChannel1Volume(ch1SliderVol * radioVol);
    mixer->setChannel1Pan(radioPan);
    mixer->setChannel2Volume(ch2SliderVol * websdrVol);
    mixer->setChannel2Pan(websdrPan);
}

// ========== CI-V Controller Slots ==========

void MainWindow::onSerialConnectClicked()
{
    QString port = m_radioControlPanel->selectedPort();

    // Step 1: Validate port selection
    if (port.isEmpty() || port == "---") {
        QMessageBox::warning(this, "No Port Selected",
            "Please select a COM port to connect to the radio.");
        return;
    }

    // Clean up existing controller if any
    if (m_radioController) {
        m_radioController->disconnect();
        delete m_radioController;
        m_radioController = nullptr;
    }

    // Update UI state to show "Connecting..." while we detect
    m_radioControlPanel->setSerialConnectionState(RadioController::Connecting);

    qDebug() << "MainWindow: Auto-detecting radio on port" << port;

    // Step 2: Auto-detect radio protocol (tries Icom CI-V, then Kenwood CAT)
    m_radioController = RadioController::detectAndConnect(port, this);

    if (!m_radioController) {
        // No radio detected
        QMessageBox::critical(this, "No Radio Detected",
            "No compatible radio detected on this port.\n\n"
            "Supported radios:\n"
            "• Icom (IC-7300, IC-7600, IC-7610, etc.)\n"
            "• Kenwood (TS-590, TS-890, TS-990, etc.)\n"
            "• Elecraft (K3, K4, KX2, KX3, etc.)\n"
            "• Yaesu (FT-991, FTDX101, FT-710, etc.)\n\n"
            "Please check:\n"
            "• Radio is powered on\n"
            "• USB cable is connected\n"
            "• Correct COM port selected");

        m_radioControlPanel->setSerialConnectionState(RadioController::Disconnected);
        return;
    }

    // Log detected protocol
    QString protoName = (m_radioController->protocol() == RadioController::IcomCIV)
        ? "Icom CI-V" : "Kenwood/Elecraft CAT";
    qDebug() << "MainWindow: Detected" << protoName << "protocol";
    qDebug() << "MainWindow: Frequency:" << m_radioController->currentFrequency() << "Hz";

    m_civConnected = true;

    // Connect radio controller signals
    connect(m_radioController, &RadioController::connectionStateChanged,
            this, &MainWindow::onRadioConnectionStateChanged);
    connect(m_radioController, &RadioController::frequencyChanged,
            this, &MainWindow::onCIVFrequencyChanged);
    connect(m_radioController, &RadioController::modeChanged,
            this, &MainWindow::onCIVModeChanged);
    connect(m_radioController, &RadioController::smeterChanged,
            this, &MainWindow::onCIVSMeterChanged);
    connect(m_radioController, &RadioController::errorOccurred,
            this, &MainWindow::onCIVError);
    connect(m_radioController, &RadioController::radioModelDetected,
            m_radioControlPanel, &RadioControlPanel::setRadioModel);
    connect(m_radioController, &RadioController::txStatusChanged,
            this, &MainWindow::onTxStatusChanged);
    connect(m_radioController, &RadioController::tunerStateChanged,
            this, &MainWindow::onTunerStateChanged);

    // Enable radio controls (Band/Mode/Tuner/Voice buttons)
    setRadioControlsEnabled(true);

    // Request current tuner state from radio (so ATU button shows correct state)
    m_radioController->requestTunerState();

    // Update display with initial values from detection
    // (The signals were consumed by detection lambda, so we need to manually update)
    if (m_radioController->currentFrequency() > 0) {
        m_localFrequency = m_radioController->currentFrequency();
        m_radioControlPanel->setFrequencyDisplay(m_localFrequency);
        // Update band button selection
        updateBandSelection(m_localFrequency);
        // Also set the frequency in WebSDR manager so it's ready when site loads
        m_webSdrManager->setFrequency(m_localFrequency);
    }
    if (!m_radioController->currentModeName().isEmpty() &&
        m_radioController->currentModeName() != "---") {
        m_radioControlPanel->setModeDisplay(m_radioController->currentModeName());
        // Update mode button selection
        updateModeSelection(m_radioController->currentMode());
        // Also set the mode in WebSDR manager
        QString webSdrMode = CIVProtocol::modeToWebSdr(m_radioController->currentMode());
        if (!webSdrMode.isEmpty()) {
            m_webSdrManager->setMode(webSdrMode);
        }
    }
    if (!m_radioController->radioModel().isEmpty()) {
        m_radioControlPanel->setRadioModel(m_radioController->radioModel());
    }

    // Step 3: Load WebSDR site (m_lastFrequencyHz is now set from above)
    WebSdrSite selectedSite = m_radioControlPanel->selectedSite();
    if (selectedSite.isValid()) {
        m_webSdrManager->loadSite(selectedSite.id);
    } else if (!m_settings.webSdrSites().isEmpty()) {
        // Fallback to first site in list
        m_webSdrManager->loadSite(m_settings.webSdrSites().first().id);
    }

    // Step 4: Wait for WebSDR to start loading, then start audio engine
    QTimer::singleShot(200, this, [this]() {
        // Step 5: Start audio engine
        QString radioInput = m_devicePanel->getSelectedInputId();
        QString loopback = m_devicePanel->getSelectedLoopbackId();
        QString output = m_devicePanel->getSelectedOutputId();

        if (radioInput.isEmpty() && loopback.isEmpty()) {
            QMessageBox::warning(this, "Warning",
                "No audio input devices selected. Please select at least one input device.");
        } else if (output.isEmpty()) {
            QMessageBox::warning(this, "Warning",
                "No audio output device selected. Please select an output device.");
        } else if (m_audioManager->startStreams(radioInput, loopback, output)) {
            // Apply mixer settings
            MixerCore* mixer = m_audioManager->mixer();
            if (mixer) {
                mixer->setDelayMs(static_cast<float>(m_delaySlider->value()));
                mixer->setMasterVolume(m_masterStrip->getVolume() / 100.0f);
                mixer->setMasterMute(m_masterStrip->isMuted());
                mixer->setChannel1Volume(m_radioStrip->getVolume() / 100.0f);
                mixer->setChannel2Volume(m_websdrStrip->getVolume() / 100.0f);
                mixer->setChannel1Mute(m_radioStrip->isMuted());
                mixer->setChannel2Mute(m_websdrStrip->isMuted());
            }

            // Apply crossfader settings with mute override
            applyPanningWithMuteOverride();

            // Enable recording
            m_radioControlPanel->setRecordEnabled(true);

            qDebug() << "Audio streams started";
        } else {
            QMessageBox::warning(this, "Audio Error",
                QString("Failed to start audio streams.\n\n%1")
                    .arg(m_audioManager->lastError()));
        }

        // Step 6: Start radio polling
        m_radioController->startPolling(100);

        // Step 7: WebSDR window is shown automatically when loaded
        // Audio and S-meter are started by WebSdrManager::onControllerPageReady()

        // Update UI state to Connected
        m_radioControlPanel->setSerialConnectionState(RadioController::Connected);

        qDebug() << "MainWindow: Connection sequence complete";
    });
}

void MainWindow::onSerialDisconnectClicked()
{
    // Step 1: Stop radio polling
    if (m_radioController && m_radioController->isPolling()) {
        m_radioController->stopPolling();
    }

    // Step 2: Stop audio engine
    if (m_audioManager->isRunning()) {
        m_audioManager->stopStreams();
    }

    // Step 3: Stop recording if active
    if (m_audioManager->recorder() && m_audioManager->recorder()->isRecording()) {
        m_audioManager->recorder()->stopRecording();
        m_radioControlPanel->setRecordingActive(false);
    }

    // Disable recording button
    m_radioControlPanel->setRecordEnabled(false);

    // Step 4: Unload WebSDR (single site)
    if (m_webSdrManager) {
        m_webSdrManager->unloadCurrent();
    }

    // Step 5: Disconnect radio and clean up controller
    if (m_radioController) {
        m_radioController->disconnect();
        delete m_radioController;
        m_radioController = nullptr;
    }
    m_civConnected = false;
    m_civSMeterDb = -80.0f;

    // Reset TX mute state (in case we were transmitting when disconnected)
    if (m_txMuteActive) {
        m_txMuteActive = false;
        // Restore master mute to what it was before TX
        if (m_audioManager->mixer()) {
            m_audioManager->mixer()->setMasterMute(m_masterMuteBeforeTx);
        }
    }

    // Step 7: Reset UI state
    m_radioControlPanel->setSerialConnectionState(RadioController::Disconnected);
    m_radioControlPanel->clearRadioInfo();
    m_radioControlPanel->setTransmitting(false);
    m_radioStrip->resetMeter();
    m_websdrStrip->resetMeter();
    m_masterStrip->resetMeter();
    m_radioSMeter->setLevel(-80.0f);
    m_websdrSMeter->setLevel(-80.0f);

    // Reset WebSDR S-meter state
    m_websdrSmeterValid = false;

    // Reset dial state
    m_dialActive = false;
    m_localFrequency = 0;
    m_dialInactiveTimer->stop();

    // Disable radio controls (Band/Mode/Tuner/Voice buttons)
    setRadioControlsEnabled(false);
    m_activeVoiceMemory = 0;

    // Reset tuner state
    m_tunerEnabled = false;
    m_tunerToggle->setChecked(false);
    m_tunerToggle->setText("ATU OFF");

    qDebug() << "Disconnected and cleaned up";
}

void MainWindow::onRadioConnectionStateChanged(RadioController::ConnectionState state)
{
    m_radioControlPanel->setSerialConnectionState(state);

    if (state == RadioController::Connected) {
        m_civConnected = true;
        qDebug() << "Radio: Connected";
    } else {
        m_civConnected = false;
        m_civSMeterDb = -80.0f;

        if (state == RadioController::Disconnected) {
            qDebug() << "Radio: Disconnected";
        }
    }
}

void MainWindow::onCIVFrequencyChanged(uint64_t frequencyHz)
{
    // Store the frequency for dial handling
    m_localFrequency = frequencyHz;

    // Update display - but not if we're actively dialing (prevents jump-back)
    if (!m_dialActive) {
        m_radioControlPanel->setFrequencyDisplay(frequencyHz);
    }

    // Update band button selection
    updateBandSelection(frequencyHz);

    // Broadcast to ALL WebSDR sites (keeps them synced even when muted)
    if (m_webSdrManager) {
        m_webSdrManager->setFrequency(frequencyHz);
    }

    qDebug() << "CI-V: Frequency changed to" << frequencyHz << "Hz";
}

void MainWindow::onCIVModeChanged(uint8_t mode, const QString& modeName)
{
    // Update display
    m_radioControlPanel->setModeDisplay(modeName);

    // Update mode button selection
    updateModeSelection(mode);

    // Broadcast to ALL WebSDR sites
    if (m_webSdrManager) {
        QString webSdrMode = CIVProtocol::modeToWebSdr(mode);
        m_webSdrManager->setMode(webSdrMode);
    }

    qDebug() << "CI-V: Mode changed to" << modeName;
}

void MainWindow::onCIVSMeterChanged(int value)
{
    // Convert CI-V S-meter value (0-255) to dB scale (-80 to 0)
    float valueDb = CIVProtocol::smeterToDb(value);

    // Store in delay buffer with timestamp
    SMeterSample sample;
    sample.timestamp = m_smeterTimer.elapsed();
    sample.valueDb = valueDb;
    m_smeterBuffer.push_back(sample);

    // Keep buffer size bounded (remove old samples)
    while (m_smeterBuffer.size() > SMETER_BUFFER_MAX) {
        m_smeterBuffer.pop_front();
    }

    // Also update immediate value for fallback
    m_civSMeterDb = valueDb;
}

void MainWindow::onCIVError(const QString& error)
{
    qWarning() << "CI-V Error:" << error;
    // Don't show message box for every error to avoid spam
    // UI status indicator will show error state
}

void MainWindow::onTxStatusChanged(bool transmitting)
{
    qDebug() << "TX status changed:" << (transmitting ? "TX" : "RX");

    // Update TX indicator in UI
    m_radioControlPanel->setTransmitting(transmitting);

    // Update voice button states (shows red when transmitting)
    if (!transmitting && m_activeVoiceMemory > 0) {
        // TX ended - reset active voice memory
        m_activeVoiceMemory = 0;
    }
    updateVoiceButtonStates();

    // Mute/unmute master during TX to prevent hearing own voice from WebSDR
    if (transmitting) {
        // Save current master mute state before TX muting
        if (!m_txMuteActive) {
            m_masterMuteBeforeTx = m_masterStrip->isMuted();
        }
        m_txMuteActive = true;

        // Force master mute during TX
        if (m_audioManager->mixer()) {
            m_audioManager->mixer()->setMasterMute(true);
        }
    } else {
        // TX ended - restore previous master mute state
        if (m_txMuteActive) {
            m_txMuteActive = false;

            // Restore master mute to what it was before TX
            if (m_audioManager->mixer()) {
                m_audioManager->mixer()->setMasterMute(m_masterMuteBeforeTx);
            }
        }
    }
}

// ========== WebSDR Controller Slots ==========

void MainWindow::onWebSdrSiteChanged(const WebSdrSite& site)
{
    if (!site.isValid()) return;

    // Switch to the selected site (unloads current site, loads new one)
    // Only switch if we're connected (sites are loaded after connect)
    if (m_webSdrManager && m_civConnected) {
        // Mute WebSDR channel to avoid audio spikes during site switch
        m_websdrStrip->setMuted(true);
        if (m_audioManager->mixer()) {
            m_audioManager->mixer()->setChannel2Mute(true);
        }
        qDebug() << "MainWindow: Muted WebSDR channel for site switch";

        // Load new site (will unmute when ready via siteReady signal)
        m_webSdrManager->loadSite(site.id);
    }

    // Save selection
    m_settings.webSdr().selectedSiteId = site.id;
}

void MainWindow::onWebSdrStateChanged(WebSdrController::State state)
{
    m_radioControlPanel->setWebSdrState(state);

    switch (state) {
        case WebSdrController::Unloaded:
            qDebug() << "WebSDR: Unloaded";
            m_websdrSmeterValid = false;  // Reset S-meter when unloaded
            break;
        case WebSdrController::Loading:
            qDebug() << "WebSDR: Loading...";
            m_websdrSmeterValid = false;  // Reset S-meter during loading
            break;
        case WebSdrController::Ready:
            qDebug() << "WebSDR: Ready";
            break;
        case WebSdrController::Error:
            qDebug() << "WebSDR: Error";
            m_websdrSmeterValid = false;  // Reset S-meter on error
            break;
    }
}

void MainWindow::onManageWebSdr()
{
    WebSdrManagerDialog dialog(m_settings.webSdrSites(), this);

    if (dialog.exec() == QDialog::Accepted) {
        QList<WebSdrSite> newSites = dialog.sites();

        // Update site list in settings (doesn't save to file yet)
        m_settings.setWebSdrSites(newSites);
        m_settings.markDirty();  // Mark dirty so user is prompted to save on exit

        // Update site list in manager (this doesn't reload any site)
        m_webSdrManager->setSiteList(newSites);

        // Update dropdown in RadioControlPanel (signals blocked, won't trigger site change)
        m_radioControlPanel->setSiteList(newSites);

        // Keep the current selection if it still exists in the list
        QString currentSiteId = m_settings.webSdr().selectedSiteId;
        bool currentSiteExists = false;
        for (const WebSdrSite& site : newSites) {
            if (site.id == currentSiteId) {
                currentSiteExists = true;
                break;
            }
        }

        if (currentSiteExists) {
            // Keep the current site selected (signals blocked)
            m_radioControlPanel->setSelectedSite(currentSiteId);
        } else if (!newSites.isEmpty()) {
            // Current site was deleted, select the first site
            m_settings.webSdr().selectedSiteId = newSites.first().id;
            m_radioControlPanel->setSelectedSite(newSites.first().id);

            // If connected, we need to load the new site since the old one is gone
            if (m_civConnected) {
                // Mute WebSDR channel during site switch
                m_websdrStrip->setMuted(true);
                if (m_audioManager->mixer()) {
                    m_audioManager->mixer()->setChannel2Mute(true);
                }
                m_webSdrManager->loadSite(newSites.first().id);
            }
        }

        qDebug() << "WebSDR sites updated:" << newSites.size() << "sites";
    }
}

void MainWindow::onAudioDevicesClicked()
{
    AudioDevicesDialog dialog(this);

    // Populate the dialog's device panel with available devices
    dialog.devicePanel()->populateInputDevices(m_audioManager->getInputDevices());
    dialog.devicePanel()->populateLoopbackDevices(m_audioManager->getLoopbackDevices());
    dialog.devicePanel()->populateOutputDevices(m_audioManager->getOutputDevices());

    // Set current selections
    dialog.devicePanel()->setSelectedInputByName(m_devicePanel->getSelectedInputName());
    dialog.devicePanel()->setSelectedLoopbackByName(m_devicePanel->getSelectedLoopbackName());
    dialog.devicePanel()->setSelectedOutputByName(m_devicePanel->getSelectedOutputName());

    if (dialog.exec() == QDialog::Accepted) {
        // Copy selections from dialog to main device panel
        QString inputName = dialog.devicePanel()->getSelectedInputName();
        QString loopbackName = dialog.devicePanel()->getSelectedLoopbackName();
        QString outputName = dialog.devicePanel()->getSelectedOutputName();

        m_devicePanel->setSelectedInputByName(inputName);
        m_devicePanel->setSelectedLoopbackByName(loopbackName);
        m_devicePanel->setSelectedOutputByName(outputName);

        // Save settings
        m_settings.devices().radioInput = inputName;
        m_settings.devices().systemLoopback = loopbackName;
        m_settings.devices().output = outputName;
        m_settings.save();

        qDebug() << "Audio devices updated - Input:" << inputName
                 << "Loopback:" << loopbackName << "Output:" << outputName;
    }
}

void MainWindow::onToggleWebSdrView(bool checked)
{
    if (!m_browserGroup) return;

    // Update setting
    m_settings.webSdr().showBrowser = checked;

    // Compact mode height: RadioControlPanel (~60) + Content (~290) + RadioControls (~120) + margins (~93) = ~563
    static constexpr int COMPACT_HEIGHT = 563;

    // Minimum full view height (used as minimum constraint)
    static constexpr int MIN_FULL_HEIGHT = 916;

    if (checked) {
        // Show WebSDR browser view (full mode) - expand to fill monitor height
        m_browserGroup->show();
        setMinimumSize(1200, MIN_FULL_HEIGHT);
        setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);  // Remove max constraint

        // Get available screen geometry (excludes taskbar) for the monitor where window is located
        QScreen* currentScreen = screen();
        if (currentScreen) {
            QRect available = currentScreen->availableGeometry();

            // Store current position before expanding
            m_preExpandPos = pos();

            // Calculate window frame height (title bar + borders)
            // frameGeometry includes decorations, geometry/height is client area only
            int frameHeight = frameGeometry().height() - height();

            // Calculate target: available height minus frame, position at top of screen
            int targetHeight = available.height() - frameHeight;
            int targetY = available.top();

            // Move to top of screen and expand to fill available height (accounting for frame)
            move(x(), targetY);
            resize(width(), targetHeight);

            qDebug() << "WebSDR view expanded to fill screen:" << targetHeight << "px (frame:" << frameHeight << "px) on" << currentScreen->name();
        }
    } else {
        // Hide WebSDR browser view (compact mode)
        m_browserGroup->hide();
        setMinimumSize(1200, COMPACT_HEIGHT);
        setMaximumSize(QWIDGETSIZE_MAX, COMPACT_HEIGHT);  // Temporarily constrain max height

        // Restore to compact height, keep X position but restore Y if we have a saved position
        int targetY = m_preExpandPos.isNull() ? y() : m_preExpandPos.y();
        move(x(), targetY);
        resize(width(), COMPACT_HEIGHT);

        // Remove max height constraint after resize (allow some flexibility)
        setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

        qDebug() << "WebSDR view collapsed to compact mode";
    }

    // Mark settings as dirty so the view preference is saved
    m_settings.markDirty();
}

float MainWindow::getDelayedSMeterValue() const
{
    // Get the current delay setting (in ms)
    int delayMs = m_delaySlider ? m_delaySlider->value() : 0;

    // Compensation for CI-V polling latency (~100ms) + physics smoothing (~50ms)
    static constexpr int SMETER_LATENCY_COMPENSATION_MS = 150;

    if (delayMs == 0 || m_smeterBuffer.empty()) {
        // No delay or no samples - return latest value
        return m_civSMeterDb;
    }

    // Calculate target timestamp (now - delay + compensation)
    // We look further back to compensate for display latency
    qint64 now = m_smeterTimer.elapsed();
    int adjustedDelay = std::max(0, delayMs - SMETER_LATENCY_COMPENSATION_MS);
    qint64 targetTime = now - adjustedDelay;

    // Find the sample closest to the target time
    // Buffer is ordered oldest to newest
    float result = m_civSMeterDb;  // Default to latest

    for (auto it = m_smeterBuffer.rbegin(); it != m_smeterBuffer.rend(); ++it) {
        if (it->timestamp <= targetTime) {
            result = it->valueDb;
            break;
        }
        // Keep updating result with older samples
        result = it->valueDb;
    }

    return result;
}

float MainWindow::getDelayedWebSdrSMeterValue() const
{
    // Fixed 200ms delay to compensate for browser audio buffering
    if (m_websdrSmeterBuffer.empty()) {
        return m_websdrSMeterDb;
    }

    // Calculate target timestamp (now - 200ms)
    qint64 now = m_smeterTimer.elapsed();
    qint64 targetTime = now - WEBSDR_SMETER_DELAY_MS;

    // Find the sample closest to the target time
    float result = m_websdrSMeterDb;  // Default to latest

    for (auto it = m_websdrSmeterBuffer.rbegin(); it != m_websdrSmeterBuffer.rend(); ++it) {
        if (it->timestamp <= targetTime) {
            result = it->valueDb;
            break;
        }
        result = it->valueDb;
    }

    return result;
}

void MainWindow::onWebSdrSmeterChanged(int value)
{
    // Convert WebSDR smeter value to dB scale for display
    // WebSDR uses soundapplet.smeter() which returns:
    //   S0  = 0
    //   S9  = 5400  (each S-unit is 600)
    //   S9+60dB = ~11400
    //
    // Map to our display range: -80 dB (S0) to 0 dB (S9+60)
    // Linear mapping from 0-11400 to -80 to 0 dB

    constexpr float RAW_MIN = 0.0f;
    constexpr float RAW_MAX = 11400.0f;  // S9+60dB
    constexpr float DB_MIN = -80.0f;
    constexpr float DB_MAX = 0.0f;

    // Add 300 (half S-unit) to match WebSDR page display
    float rawValue = static_cast<float>(value) + 300.0f;
    float normalized = (rawValue - RAW_MIN) / (RAW_MAX - RAW_MIN);
    normalized = std::clamp(normalized, 0.0f, 1.0f);

    float dbValue = DB_MIN + normalized * (DB_MAX - DB_MIN);

    // Store in delay buffer to compensate for browser audio latency
    qint64 now = m_smeterTimer.elapsed();
    m_websdrSmeterBuffer.push_back({now, dbValue});

    // Limit buffer size
    while (m_websdrSmeterBuffer.size() > WEBSDR_SMETER_BUFFER_MAX) {
        m_websdrSmeterBuffer.pop_front();
    }

    m_websdrSmeterValid = true;
}

// ========== Band/Mode/Tuner/Voice Control Handlers ==========

void MainWindow::onBandSelected(int bandIndex)
{
    if (!m_radioController || !m_civConnected) return;
    if (bandIndex < 0 || bandIndex >= BAND_COUNT) return;

    qDebug() << "MainWindow: Band button" << bandIndex << "clicked";

    // Send frequency to change to the target band
    uint64_t freq = BAND_FREQS[bandIndex];
    qDebug() << "MainWindow: Setting frequency to" << freq << "Hz for band" << bandIndex;
    m_radioController->setFrequency(freq);

    // Set mode based on band:
    // 160m, 80m, 40m, 30m (bands 0, 1, 2, 3) -> LSB
    // All others (20m, 17m, 15m, 12m, 10m, 6m) -> USB
    uint8_t mode;
    if (bandIndex <= 3) {
        mode = 0x00;  // LSB
    } else {
        mode = 0x01;  // USB
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

void MainWindow::onModeSelected(int modeIndex)
{
    if (!m_radioController || !m_civConnected) return;
    if (modeIndex < 0 || modeIndex >= MODE_COUNT) return;

    uint8_t mode = MODE_CODES[modeIndex];
    qDebug() << "MainWindow: Mode selected:" << modeIndex << "-> mode" << mode;
    m_radioController->setMode(mode);
}

void MainWindow::onTuneClicked()
{
    if (!m_radioController || !m_civConnected) return;

    qDebug() << "MainWindow: Starting tune";
    m_radioController->startTune();
}

void MainWindow::onTunerToggled()
{
    if (!m_radioController || !m_civConnected) return;

    bool enable = m_tunerToggle->isChecked();
    qDebug() << "MainWindow: Tuner toggle ->" << (enable ? "ON" : "OFF");
    m_radioController->setTunerState(enable);

    // Update button text immediately (will be confirmed by radio response)
    m_tunerToggle->setText(enable ? "ATU ON" : "ATU OFF");
}

void MainWindow::onVoiceMemoryClicked()
{
    if (!m_radioController || !m_civConnected) return;

    QPushButton* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;

    int memNum = btn->property("memoryNumber").toInt();
    if (memNum < 1 || memNum > 8) return;

    // If currently transmitting the same memory, stop it
    if (m_radioControlPanel->isTransmitting() && m_activeVoiceMemory == memNum) {
        qDebug() << "MainWindow: Stopping voice memory" << memNum;
        m_radioController->stopVoiceMemory();
        m_activeVoiceMemory = 0;
        updateVoiceButtonStates();
        return;
    }

    // If transmitting a different memory, stop first then play new one
    if (m_radioControlPanel->isTransmitting() && m_activeVoiceMemory > 0) {
        qDebug() << "MainWindow: Stopping voice memory" << m_activeVoiceMemory << "to play" << memNum;
        m_radioController->stopVoiceMemory();
    }

    // Play the requested memory
    qDebug() << "MainWindow: Playing voice memory" << memNum;
    m_radioController->playVoiceMemory(memNum);
    m_activeVoiceMemory = memNum;
    updateVoiceButtonStates();
}

void MainWindow::onTunerStateChanged(bool enabled)
{
    m_tunerEnabled = enabled;
    m_tunerToggle->setChecked(enabled);
    m_tunerToggle->setText(enabled ? "ATU ON" : "ATU OFF");
}

void MainWindow::updateBandSelection(uint64_t freqHz)
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

void MainWindow::updateModeSelection(uint8_t mode)
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

int MainWindow::frequencyToBandIndex(uint64_t freqHz) const
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

int MainWindow::modeToIndex(uint8_t mode) const
{
    for (int i = 0; i < MODE_COUNT; i++) {
        if (MODE_CODES[i] == mode) {
            return i;
        }
    }
    return -1;
}

void MainWindow::setRadioControlsEnabled(bool enabled)
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

void MainWindow::updateVoiceButtonStates()
{
    bool isTransmitting = m_radioControlPanel->isTransmitting();

    QString activeStyle =
        "QPushButton { "
        "  background-color: #D32F2F; "
        "  border: 1px solid #F44336; "
        "  border-radius: 4px; "
        "  padding: 2px 4px; "
        "  font-weight: bold; "
        "  font-size: 10pt; "
        "  color: white; "
        "}"
        "QPushButton:hover { background-color: #E53935; }"
        "QPushButton:pressed { background-color: #B71C1C; }";

    QString normalStyle =
        "QPushButton { "
        "  background-color: #3A3A3A; "
        "  border: 1px solid #555; "
        "  border-radius: 4px; "
        "  padding: 2px 4px; "
        "  font-weight: bold; "
        "  font-size: 10pt; "
        "  color: #DDD; "
        "}"
        "QPushButton:hover { background-color: #5A5A5A; }"
        "QPushButton:pressed { background-color: #9C27B0; color: white; }"
        "QPushButton:disabled { background-color: #2A2A2A; color: #555; }";

    for (int i = 0; i < 8; i++) {
        int memNum = i + 1;
        if (isTransmitting && m_activeVoiceMemory == memNum) {
            // This button is actively transmitting - show red
            m_voiceButtons[i]->setStyleSheet(activeStyle);
        } else {
            // Normal state
            m_voiceButtons[i]->setStyleSheet(normalStyle);
        }
    }
}

// ========== USB Jog Wheel / Dial Handling ==========

bool MainWindow::event(QEvent* e)
{
    // Intercept key press events before child widgets get them
    if (e->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(e);
        int key = keyEvent->key();

        // Handle "+" key to cycle dial step
        if (key == Qt::Key_Plus || key == Qt::Key_Equal) {
            m_radioControlPanel->cycleDialStep();
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

    return QMainWindow::event(e);
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    // Handle "+" key to cycle dial step
    if (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal) {
        m_radioControlPanel->cycleDialStep();
        event->accept();
        return;
    }

    // Handle frequency tuning keys (requires connection)
    if (!m_radioController || !m_civConnected) {
        QMainWindow::keyPressEvent(event);
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

    QMainWindow::keyPressEvent(event);
}

void MainWindow::handleDialInput(int direction)
{
    // direction: +1 = frequency up, -1 = frequency down
    if (!m_radioController || !m_civConnected) return;

    int dialStep = m_radioControlPanel->currentDialStep();

    // Use local frequency if available, otherwise get from radio
    int64_t currentFreq = static_cast<int64_t>(m_localFrequency);
    if (currentFreq == 0 && m_radioController) {
        currentFreq = static_cast<int64_t>(m_radioController->currentFrequency());
    }

    int64_t newFreq = currentFreq + (direction * dialStep);

    // Clamp to valid range
    if (newFreq < 100000) newFreq = 100000;
    if (newFreq > 60000000) newFreq = 60000000;

    // Update local frequency and display immediately for instant visual feedback
    m_localFrequency = static_cast<uint64_t>(newFreq);
    m_radioControlPanel->setFrequencyDisplay(m_localFrequency);

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

void MainWindow::onDialInactive()
{
    // Dial has been inactive for 500ms - resume accepting frequency updates from radio
    m_dialActive = false;
    qDebug() << "MainWindow: Dial inactive, resuming radio feedback";
}

// ========== Voice Memory Configuration ==========

void MainWindow::onVoiceMemoryConfig()
{
    VoiceMemoryDialog dialog(m_voiceMemoryLabels, this);

    if (dialog.exec() == QDialog::Accepted) {
        m_voiceMemoryLabels = dialog.labels();
        m_settings.setVoiceMemoryLabels(m_voiceMemoryLabels);
        m_settings.markDirty();
        m_settings.save();

        // Update button labels
        updateVoiceButtonLabels();

        qDebug() << "Voice memory labels updated:" << m_voiceMemoryLabels;
    }
}

void MainWindow::updateVoiceButtonLabels()
{
    // Reset marquee state
    for (int i = 0; i < 8; i++) {
        m_marqueeOffset[i] = 0;
        m_marqueePauseCount[i] = 0;
    }

    bool needsMarquee = false;

    for (int i = 0; i < 8; i++) {
        // Safety check - ensure button exists
        if (!m_voiceButtons[i]) continue;

        QString label = (i < m_voiceMemoryLabels.size() && !m_voiceMemoryLabels[i].isEmpty())
                        ? m_voiceMemoryLabels[i]
                        : QString("M%1").arg(i + 1);

        // Set tooltip to full label
        m_voiceButtons[i]->setToolTip(label);

        // Set button text (truncated if needed)
        if (label.length() > MARQUEE_MAX_CHARS) {
            m_voiceButtons[i]->setText(label.left(MARQUEE_MAX_CHARS));
            needsMarquee = true;
        } else {
            m_voiceButtons[i]->setText(label);
        }
    }

    // Start or stop marquee timer based on whether any labels need scrolling
    if (needsMarquee) {
        if (m_marqueeTimer && !m_marqueeTimer->isActive()) {
            m_marqueeTimer->start();
        }
    } else if (m_marqueeTimer) {
        m_marqueeTimer->stop();
    }
}

void MainWindow::updateMarqueeLabels()
{
    for (int i = 0; i < 8; i++) {
        // Safety check - ensure button exists
        if (!m_voiceButtons[i]) continue;

        QString fullLabel = (i < m_voiceMemoryLabels.size() && !m_voiceMemoryLabels[i].isEmpty())
                            ? m_voiceMemoryLabels[i]
                            : QString("M%1").arg(i + 1);

        // Only scroll labels longer than MARQUEE_MAX_CHARS
        if (fullLabel.length() <= MARQUEE_MAX_CHARS) {
            continue;
        }

        // Handle pause at start/end of scroll
        if (m_marqueePauseCount[i] > 0) {
            m_marqueePauseCount[i]--;
            continue;
        }

        // Calculate the visible portion with current offset
        int maxOffset = fullLabel.length() - MARQUEE_MAX_CHARS;

        // Advance scroll offset
        m_marqueeOffset[i]++;

        // Check if we've scrolled to the end
        if (m_marqueeOffset[i] > maxOffset) {
            // Reset to start and pause
            m_marqueeOffset[i] = 0;
            m_marqueePauseCount[i] = MARQUEE_PAUSE_TICKS;
        } else if (m_marqueeOffset[i] == 1) {
            // Just started scrolling - no additional pause needed
        }

        // Update button text with current visible portion
        QString visibleText = fullLabel.mid(m_marqueeOffset[i], MARQUEE_MAX_CHARS);
        m_voiceButtons[i]->setText(visibleText);
    }
}
