#include "ui/MainWindow.h"
#include "ui/Styles.h"
#include "ui/WebSdrManagerDialog.h"
#include "ui/AudioDevicesDialog.h"
#include "audio/MixerCore.h"
#include "serial/CIVProtocol.h"
#include "HamMixer/Version.h"
#include <algorithm>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFrame>
#include <QMenuBar>
#include <QMessageBox>
#include <QCloseEvent>
#include <QSplitter>
#include <QIcon>
#include <QDebug>
#include <QWebEngineView>
#include <QFileDialog>
#include <QDir>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_civSMeterDb(-80.0f)
    , m_civConnected(false)
    , m_websdrSMeterDb(-80.0f)
    , m_websdrSmeterValid(false)
    , m_recentConfigsMenu(nullptr)
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

    qDebug() << "MainWindow created";
}

MainWindow::~MainWindow()
{
    m_meterTimer->stop();
    m_syncTimer->stop();

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
    setMinimumSize(1200, 876);  // +50px more for KiwiSDR browser room
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

    // Delay controls - two rows: top row has button and label, bottom row has slider
    QGroupBox* delayGroup = new QGroupBox("Delay (Radio)", this);
    delayGroup->setFixedHeight(145);  // +10px for taller row
    QVBoxLayout* delayMainLayout = new QVBoxLayout(delayGroup);
    delayMainLayout->setSpacing(10);

    // Top row: Auto-Sync button (left), stretch, delay value (right)
    QHBoxLayout* delayTopRow = new QHBoxLayout();
    m_autoSyncButton = new QPushButton("Auto-Sync", this);
    m_autoSyncButton->setToolTip("Automatically detect optimal delay");
    m_autoSyncButton->setFixedWidth(120);
    delayTopRow->addWidget(m_autoSyncButton);

    delayTopRow->addStretch();

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

    // ========== Embedded WebSDR Browser (bottom section) ==========
    QGroupBox* browserGroup = new QGroupBox("WebSDR Browser", this);
    browserGroup->setFixedHeight(450);  // +50px more for KiwiSDR browser room
    browserGroup->setContentsMargins(0, 0, 0, 0);  // Remove group box margins
    QVBoxLayout* browserLayout = new QVBoxLayout(browserGroup);
    browserLayout->setContentsMargins(5, 0, 5, 5);  // Reduced top margin to avoid double spacing

    // Create a container widget for the browser with its own layout
    QWidget* browserContainer = new QWidget(browserGroup);
    browserContainer->setMinimumHeight(360);  // +50px more for KiwiSDR
    QVBoxLayout* containerLayout = new QVBoxLayout(browserContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);
    browserLayout->addWidget(browserContainer);

    mainLayout->addWidget(browserGroup);

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
    fileMenu->addAction("&Save Config...", this, &MainWindow::onSaveConfig, QKeySequence::Save);

    m_recentConfigsMenu = fileMenu->addMenu("Open &Recent");
    updateRecentConfigsMenu();

    fileMenu->addSeparator();
    fileMenu->addAction("E&xit", this, &QMainWindow::close, QKeySequence::Quit);

    // ===== Tools Menu =====
    QMenu* toolsMenu = menuBar()->addMenu("&Tools");

    toolsMenu->addAction("&Audio Devices...", this, &MainWindow::onAudioDevicesClicked);
    toolsMenu->addAction("Manage &SDR Sites...", this, &MainWindow::onManageWebSdr);

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

void MainWindow::connectSignals()
{
    // Tools section - record button (now in RadioControlPanel)
    connect(m_radioControlPanel, &RadioControlPanel::recordClicked,
            this, &MainWindow::onRecordClicked);

    // Delay
    connect(m_delaySlider, &QSlider::valueChanged, this, &MainWindow::onDelayChanged);
    connect(m_autoSyncButton, &QPushButton::clicked, this, &MainWindow::onAutoSyncClicked);

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
    m_settings.load();
    m_settings.loadRecentConfigs();

    // Apply settings
    m_delaySlider->setValue(m_settings.channel1().delayMs);
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

    // Apply WebSDR settings - update site list from loaded settings
    // This is important because setupUI() ran before load() with default sites
    m_webSdrManager->setSiteList(m_settings.webSdrSites());
    m_radioControlPanel->setSiteList(m_settings.webSdrSites());
    m_radioControlPanel->setSelectedSite(m_settings.webSdr().selectedSiteId);

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
    m_settings.channel2().muted = m_websdrStrip->isMuted();
    m_settings.channel2().volume = m_websdrStrip->getVolume();
    m_settings.master().volume = m_masterStrip->getVolume();
    m_settings.master().muted = m_masterStrip->isMuted();

    // Save serial settings
    m_settings.serial().portName = m_radioControlPanel->selectedPort();

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

void MainWindow::onAutoSyncClicked()
{
    if (!m_audioManager->isRunning()) {
        QMessageBox::information(this, "Auto-Sync",
            "Please connect first to start the audio streams.");
        return;
    }

    MixerCore* mixer = m_audioManager->mixer();
    if (!mixer) return;

    // If already capturing, cancel
    if (mixer->isSyncCapturing()) {
        mixer->cancelSyncCapture();
        m_autoSyncButton->setText("Auto-Sync");
        m_syncTimer->stop();
        return;
    }

    // Start sync capture
    mixer->startSyncCapture();
    m_autoSyncButton->setText("Syncing...");

    // Start timer to monitor progress
    m_syncTimer->start(100);  // Check every 100ms
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
        return;
    }

    // Update progress display
    if (mixer->isSyncCapturing()) {
        int progress = static_cast<int>(mixer->getSyncProgress() * 100);
        m_autoSyncButton->setText(QString("Syncing %1%").arg(progress));
        return;
    }

    // Check if result is ready
    if (mixer->hasSyncResult()) {
        m_syncTimer->stop();
        m_autoSyncButton->setText("Auto-Sync");

        AudioSync::SyncResult result = mixer->getSyncResult();

        if (result.success) {
            // Apply the detected delay (can be positive or negative)
            int delayMs = static_cast<int>(result.delayMs);

            if (delayMs >= 0) {
                // Normal case: WebSDR is behind Radio, add delay to Radio channel
                m_delaySlider->setValue(delayMs);

                QMessageBox::information(this, "Auto-Sync Complete",
                    QString("Detected delay: %1 ms\nConfidence: %2%\n\n"
                            "The delay has been applied automatically.")
                        .arg(delayMs)
                        .arg(static_cast<int>(result.confidence * 100)));
            } else {
                // Unusual case: WebSDR is ahead of Radio
                // We can only delay Radio, not advance it, so set to 0
                m_delaySlider->setValue(0);

                QMessageBox::information(this, "Auto-Sync Complete",
                    QString("Detected offset: %1 ms (WebSDR ahead)\nConfidence: %2%\n\n"
                            "The WebSDR signal arrives before the Radio signal.\n"
                            "Delay has been set to 0 ms (minimum possible).")
                        .arg(delayMs)
                        .arg(static_cast<int>(result.confidence * 100)));
            }
        } else {
            QMessageBox::warning(this, "Auto-Sync Failed",
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
    // Ensure configurations directory exists
    QString configDir = Settings::getConfigurationsDir();
    QDir().mkpath(configDir);

    QString fileName = QFileDialog::getSaveFileName(
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
        // Apply loaded settings to UI
        loadSettings();
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
        // Apply loaded settings to UI
        loadSettings();
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

    // Update display with initial values from detection
    // (The signals were consumed by detection lambda, so we need to manually update)
    if (m_radioController->currentFrequency() > 0) {
        m_radioControlPanel->setFrequencyDisplay(m_radioController->currentFrequency());
        // Also set the frequency in WebSDR manager so it's ready when site loads
        m_webSdrManager->setFrequency(m_radioController->currentFrequency());
    }
    if (!m_radioController->currentModeName().isEmpty() &&
        m_radioController->currentModeName() != "---") {
        m_radioControlPanel->setModeDisplay(m_radioController->currentModeName());
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

    // Step 6: Reset UI state
    m_radioControlPanel->setSerialConnectionState(RadioController::Disconnected);
    m_radioControlPanel->clearRadioInfo();
    m_radioStrip->resetMeter();
    m_websdrStrip->resetMeter();
    m_masterStrip->resetMeter();
    m_radioSMeter->setLevel(-80.0f);
    m_websdrSMeter->setLevel(-80.0f);

    // Reset WebSDR S-meter state
    m_websdrSmeterValid = false;

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
    // Update display
    m_radioControlPanel->setFrequencyDisplay(frequencyHz);

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

float MainWindow::getDelayedSMeterValue() const
{
    // Get the current delay setting (in ms)
    int delayMs = m_delaySlider ? m_delaySlider->value() : 0;

    if (delayMs == 0 || m_smeterBuffer.empty()) {
        // No delay or no samples - return latest value
        return m_civSMeterDb;
    }

    // Calculate target timestamp (now - delay)
    qint64 now = m_smeterTimer.elapsed();
    qint64 targetTime = now - delayMs;

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
