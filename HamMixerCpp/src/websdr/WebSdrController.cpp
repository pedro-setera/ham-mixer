/*
 * WebSdrController.cpp
 *
 * WebSDR browser controller implementation with separate window
 * Part of HamMixer CT7BAC
 */

#include "WebSdrController.h"
#include "HamMixer/Version.h"
#include <QDebug>
#include <QUrl>
#include <QTimer>
#include <QVBoxLayout>
#include <QCloseEvent>
#include <QWebEngineSettings>
#include <QWebEngineProfile>
#include <QIcon>
#include <cmath>

WebSdrController::WebSdrController(QWidget* parentWidget, QObject* parent)
    : QObject(parent)
    , m_browserWindow(nullptr)
    , m_webView(nullptr)
    , m_state(Unloaded)
    , m_audioStarted(false)
    , m_embedded(parentWidget != nullptr)
    , m_pendingFrequencyHz(0)
    , m_hasPendingTune(false)
    , m_smeterTimer(nullptr)
    , m_lastSmeterValue(-1)
{
    if (m_embedded) {
        // Embedded mode: create web view and add to parent's layout
        m_webView = new QWebEngineView(parentWidget);

        // Add to parent's layout if it has one
        QLayout* parentLayout = parentWidget->layout();
        if (parentLayout) {
            parentLayout->addWidget(m_webView);
            qDebug() << "WebSdrController: Created in embedded mode, added to parent layout";
        } else {
            qDebug() << "WebSdrController: Created in embedded mode (no parent layout)";
        }
    } else {
        // Separate window mode
        m_browserWindow = new QWidget(nullptr, Qt::Window);
        m_browserWindow->setWindowTitle(QString("%1 V%2 (%3)").arg(HAMMIXER_WEBSDR_NAME).arg(HAMMIXER_VERSION_STRING).arg(HAMMIXER_VERSION_DATE));
        m_browserWindow->setWindowIcon(QIcon(":/icons/icons/antenna.png"));
        m_browserWindow->resize(1024, 768);  // Default size

        // Create layout and web view
        QVBoxLayout* layout = new QVBoxLayout(m_browserWindow);
        layout->setContentsMargins(0, 0, 0, 0);

        m_webView = new QWebEngineView(m_browserWindow);
        layout->addWidget(m_webView);

        // Install event filter to handle window close
        m_browserWindow->installEventFilter(this);

        qDebug() << "WebSdrController: Created with separate window";
    }

    // Configure web engine settings to allow autoplay without user gesture
    QWebEngineSettings* settings = m_webView->page()->settings();
    settings->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, false);
    settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    settings->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, false);
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    qDebug() << "WebSdrController: Configured WebEngine to allow autoplay";

    // Connect signals
    QObject::connect(m_webView, &QWebEngineView::loadStarted,
                     this, &WebSdrController::onLoadStarted);
    QObject::connect(m_webView, &QWebEngineView::loadProgress,
                     this, &WebSdrController::onLoadProgress);
    QObject::connect(m_webView, &QWebEngineView::loadFinished,
                     this, &WebSdrController::onLoadFinished);
}

bool WebSdrController::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_browserWindow && event->type() == QEvent::Close) {
        // When user closes the window, just hide it
        // The controller will be destroyed when switching sites or disconnecting
        qDebug() << "WebSdrController: Window close requested - hiding window";
        m_browserWindow->hide();
        event->ignore();
        return true;
    }
    return QObject::eventFilter(obj, event);
}

WebSdrController::~WebSdrController()
{
    stopSmeterPolling();
    unload();

    if (m_embedded) {
        // In embedded mode, we need to clean up the webView
        if (m_webView) {
            // Remove from parent's layout if present
            QWidget* parent = m_webView->parentWidget();
            if (parent && parent->layout()) {
                parent->layout()->removeWidget(m_webView);
            }
            delete m_webView;
            m_webView = nullptr;
        }
    } else if (m_browserWindow) {
        m_browserWindow->close();
        delete m_browserWindow;
        m_browserWindow = nullptr;
    }
}

void WebSdrController::showWindow()
{
    if (m_embedded) {
        // In embedded mode, just show the web view
        if (m_webView) {
            m_webView->show();
        }
    } else if (m_browserWindow) {
        m_browserWindow->show();
        m_browserWindow->raise();
        m_browserWindow->activateWindow();
    }
}

void WebSdrController::hideWindow()
{
    if (m_embedded) {
        // In embedded mode, just hide the web view
        if (m_webView) {
            m_webView->hide();
        }
    } else if (m_browserWindow) {
        m_browserWindow->hide();
    }
}

bool WebSdrController::isWindowVisible() const
{
    if (m_embedded) {
        return m_webView && m_webView->isVisible();
    }
    return m_browserWindow && m_browserWindow->isVisible();
}

void WebSdrController::loadSite(const WebSdrSite& site)
{
    if (!site.isValid()) {
        qWarning() << "WebSdrController: Invalid site configuration";
        setState(Error);
        emit errorOccurred("Invalid site configuration");
        return;
    }

    m_currentSite = site;
    m_audioStarted = false;
    m_hasPendingTune = false;

    // Update window title and size (only for separate window mode)
    if (!m_embedded && m_browserWindow) {
        m_browserWindow->setWindowTitle(QString("%1 V%2 (%3) - %4")
            .arg(HAMMIXER_WEBSDR_NAME)
            .arg(HAMMIXER_VERSION_STRING)
            .arg(HAMMIXER_VERSION_DATE)
            .arg(site.name));

        // Default window size for WebSDR sites
        static constexpr int DEFAULT_WIDTH = 1000;
        static constexpr int DEFAULT_HEIGHT = 400;

        m_browserWindow->setMinimumSize(400, 200);
        m_browserWindow->resize(DEFAULT_WIDTH, DEFAULT_HEIGHT);
        m_browserWindow->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    }

    qDebug() << "WebSdrController: Loading site" << site.name << "from" << site.url;
    setState(Loading);

    // Show the window/view and load the site
    showWindow();
    m_webView->load(QUrl(site.url));
}

void WebSdrController::unload()
{
    if (m_state != Unloaded) {
        // Stop S-meter polling first
        stopSmeterPolling();

        // Stop audio before unloading
        if (m_webView && m_webView->page()) {
            QString script =
                "(function() {"
                "  try {"
                "    if (typeof soundapplet !== 'undefined' && soundapplet !== null) {"
                "      soundapplet.mute(1);"
                "      soundapplet.setvolume(0);"
                "    }"
                "  } catch(e) {}"
                "})();";
            m_webView->page()->runJavaScript(script);
        }

        // Navigate to blank page
        if (m_webView) {
            m_webView->setUrl(QUrl("about:blank"));
        }

        // Hide the window (only in separate window mode)
        if (!m_embedded) {
            hideWindow();
        }

        m_currentSite = WebSdrSite();
        m_audioStarted = false;
        m_hasPendingTune = false;
        m_lastSmeterValue = -1;
        setState(Unloaded);
        qDebug() << "WebSdrController: Unloaded";
    }
}

void WebSdrController::reload()
{
    if (m_currentSite.isValid()) {
        m_audioStarted = false;
        m_webView->reload();
    }
}

void WebSdrController::setFrequency(uint64_t frequencyHz)
{
    if (m_state != Ready) {
        // Store for when page is ready
        m_pendingFrequencyHz = frequencyHz;
        m_hasPendingTune = true;
        return;
    }

    // WebSDR expects frequency in kHz with decimal precision
    double freqKHz = static_cast<double>(frequencyHz) / 1000.0;

    // Round to 0.01 kHz (10 Hz resolution)
    freqKHz = std::round(freqKHz * 100.0) / 100.0;

    // setfreqif() automatically switches to the correct band
    QString script = QString("setfreqif(%1);").arg(freqKHz, 0, 'f', 2);
    runJavaScript(script);

    qDebug() << "WebSdrController: Set frequency to" << freqKHz << "kHz";
}

void WebSdrController::setMode(const QString& mode)
{
    if (m_state != Ready) {
        m_pendingMode = mode;
        m_hasPendingTune = true;
        return;
    }

    // Normalize mode string to lowercase
    QString normalizedMode = mode.toLower();

    // Validate mode
    if (normalizedMode != "lsb" && normalizedMode != "usb" &&
        normalizedMode != "cw" && normalizedMode != "am" && normalizedMode != "fm") {
        qWarning() << "WebSdrController: Unknown mode" << mode << ", defaulting to USB";
        normalizedMode = "usb";
    }

    QString script = QString(
        "if (typeof set_mode === 'function') {"
        "  set_mode('%1');"
        "}").arg(normalizedMode);
    runJavaScript(script);

    qDebug() << "WebSdrController: Set mode to" << normalizedMode;
}

void WebSdrController::startAudio()
{
    if (m_state != Ready) {
        return;
    }

    // Try multiple methods to start audio
    QString script =
        "(function() {"
        "  var started = false;"
        "  "
        "  // Method 1: Try chrome_audio_start directly"
        "  try {"
        "    if (typeof chrome_audio_start === 'function') {"
        "      chrome_audio_start();"
        "      started = true;"
        "      console.log('HamMixer: Audio started via chrome_audio_start()');"
        "    }"
        "  } catch(e) {"
        "    console.log('HamMixer: chrome_audio_start failed: ' + e.message);"
        "  }"
        "  "
        "  // Method 2: Simulate click on audio start button"
        "  if (!started) {"
        "    try {"
        "      var audioBtn = document.querySelector('input[value*=\"Audio\"], button[onclick*=\"audio\"], #startbutton, .audiostart');"
        "      if (!audioBtn) {"
        "        var buttons = document.querySelectorAll('input[type=\"button\"], button');"
        "        for (var i = 0; i < buttons.length; i++) {"
        "          if (buttons[i].value && buttons[i].value.toLowerCase().indexOf('audio') >= 0) {"
        "            audioBtn = buttons[i];"
        "            break;"
        "          }"
        "        }"
        "      }"
        "      if (audioBtn) {"
        "        audioBtn.click();"
        "        started = true;"
        "        console.log('HamMixer: Audio started via button click');"
        "      }"
        "    } catch(e) {"
        "      console.log('HamMixer: Button click failed: ' + e.message);"
        "    }"
        "  }"
        "  "
        "  // Method 3: Try iOS_audio_start"
        "  if (!started) {"
        "    try {"
        "      if (typeof iOS_audio_start === 'function') {"
        "        iOS_audio_start();"
        "        started = true;"
        "        console.log('HamMixer: Audio started via iOS_audio_start()');"
        "      }"
        "    } catch(e) {"
        "      console.log('HamMixer: iOS_audio_start failed: ' + e.message);"
        "    }"
        "  }"
        "  "
        "  if (!started) {"
        "    console.log('HamMixer: Could not auto-start audio - manual click may be required');"
        "  }"
        "  return started;"
        "})();";

    runJavaScript(script);
    m_audioStarted = true;
    qDebug() << "WebSdrController: Attempted to start audio";
}

void WebSdrController::stopAudio()
{
    if (m_state != Ready) {
        return;
    }

    // Stop audio: mute + set volume to 0
    QString script =
        "(function() {"
        "  try {"
        "    if (typeof soundapplet !== 'undefined' && soundapplet !== null) {"
        "      soundapplet.mute(1);"
        "      soundapplet.setvolume(0);"
        "    }"
        "  } catch(e) {}"
        "})();";
    runJavaScript(script);
    m_audioStarted = false;
    qDebug() << "WebSdrController: Stopped audio";
}

void WebSdrController::tune(uint64_t frequencyHz, const QString& mode)
{
    if (m_state != Ready) {
        m_pendingFrequencyHz = frequencyHz;
        m_pendingMode = mode;
        m_hasPendingTune = true;
        return;
    }

    // Set mode first, then frequency
    setMode(mode);
    setFrequency(frequencyHz);
}

void WebSdrController::onLoadStarted()
{
    setState(Loading);
    emit loadProgress(0);
}

void WebSdrController::onLoadProgress(int progress)
{
    emit loadProgress(progress);
}

void WebSdrController::onLoadFinished(bool ok)
{
    if (ok) {
        qDebug() << "WebSdrController: Page loaded successfully";
        // Wait 1000ms for page JavaScript to fully initialize before sending commands
        // Some sites need more time for their JavaScript to be ready
        QTimer::singleShot(1000, this, &WebSdrController::initializeWebSdr);
    } else {
        qWarning() << "WebSdrController: Page load failed";
        setState(Error);
        emit errorOccurred("Failed to load WebSDR page");
    }
}

void WebSdrController::initializeWebSdr()
{
    setState(Ready);
    qDebug() << "WebSdrController: Starting initialization sequence...";

    // Step 1: Set audio volume to maximum and unmute
    // WebSDR uses dB scale: slider range -20 to 6, volume = Math.pow(10, slider/10)
    // Max volume (slider=6): Math.pow(10, 6/10) = 3.981
    QString audioSetupScript =
        "(function() {"
        "  try {"
        "    if (typeof soundapplet !== 'undefined' && soundapplet !== null) {"
        "      soundapplet.setvolume(3.981);"
        "      soundapplet.mute(0);"
        "    }"
        "    // Also set the slider element if present"
        "    var slider = document.getElementById('volumecontrol2');"
        "    if (slider) { slider.value = 6; }"
        "    if (typeof updateSliderVolume === 'function') { updateSliderVolume(); }"
        "  } catch(e) {}"
        "})();";

    if (m_webView && m_webView->page()) {
        m_webView->page()->runJavaScript(audioSetupScript);
    }
    qDebug() << "WebSdrController: Audio settings applied (volume=max, mute=off)";

    // Step 2: Wait 150ms then apply FREQUENCY first (this selects the correct band)
    QTimer::singleShot(150, this, [this]() {
        if (m_state != Ready) return;  // Check we're still valid

        // Apply frequency FIRST - setfreqif() switches to the correct band automatically
        if (m_hasPendingTune && m_pendingFrequencyHz > 0) {
            setFrequency(m_pendingFrequencyHz);
            qDebug() << "WebSdrController: Applied pending frequency:" << m_pendingFrequencyHz;
        }

        // Step 3: Wait 150ms then set MODE (after band is selected)
        QTimer::singleShot(150, this, [this]() {
            if (m_state != Ready) return;

            if (m_hasPendingTune && !m_pendingMode.isEmpty()) {
                setMode(m_pendingMode);
                qDebug() << "WebSdrController: Applied pending mode:" << m_pendingMode;
            }
            m_hasPendingTune = false;

            // Step 5: Wait 150ms then start audio
            QTimer::singleShot(150, this, [this]() {
                if (m_state != Ready) return;

                // Start audio
                startAudio();

                // Step 6: Wait 200ms after audio start, then confirm volume and emit ready
                // Some sites reset volume when audio starts, so we need to set it again
                QTimer::singleShot(200, this, [this]() {
                    if (m_state != Ready) return;

                    // Re-confirm volume at maximum and squelch off
                    // WebSDR uses dB scale: max volume = 3.981 (slider=6)
                    QString confirmScript =
                        "(function() {"
                        "  try {"
                        "    if (typeof soundapplet !== 'undefined' && soundapplet !== null) {"
                        "      soundapplet.setvolume(3.981);"
                        "      soundapplet.mute(0);"
                        "    }"
                        "    var slider = document.getElementById('volumecontrol2');"
                        "    if (slider) { slider.value = 6; }"
                        "    if (typeof updateSliderVolume === 'function') { updateSliderVolume(); }"
                        "    if (typeof setsquelch === 'function') { setsquelch(0); }"
                        "    console.log('HamMixer: Final volume/squelch confirmation');"
                        "  } catch(e) {}"
                        "})();";

                    if (m_webView && m_webView->page()) {
                        m_webView->page()->runJavaScript(confirmScript);
                    }

                    // Start S-meter polling
                    startSmeterPolling(100);

                    // Finally emit pageReady - site is fully configured
                    emit pageReady();
                    qDebug() << "WebSdrController: Initialization complete - page ready";
                });
            });
        });
    });
}

void WebSdrController::setState(State newState)
{
    if (m_state != newState) {
        m_state = newState;
        emit stateChanged(newState);
    }
}

void WebSdrController::runJavaScript(const QString& script)
{
    if (m_state != Ready && m_state != Loading) {
        qWarning() << "WebSdrController: Cannot run JavaScript - not in ready/loading state";
        return;
    }

    if (!m_webView || !m_webView->page()) {
        qWarning() << "WebSdrController: Cannot run JavaScript - webView/page is null";
        return;
    }

    // Wrap script in try-catch to prevent JavaScript errors from causing issues
    QString safeScript = QString("try { %1 } catch(e) { console.log('HamMixer JS error: ' + e.message); }").arg(script);
    m_webView->page()->runJavaScript(safeScript);
}

void WebSdrController::runJavaScript(const QString& script,
                                      std::function<void(const QVariant&)> callback)
{
    if (m_state != Ready && m_state != Loading) {
        qWarning() << "WebSdrController: Cannot run JavaScript - not in ready/loading state";
        if (callback) callback(QVariant());
        return;
    }

    if (!m_webView || !m_webView->page()) {
        qWarning() << "WebSdrController: Cannot run JavaScript - webView/page is null";
        if (callback) callback(QVariant());
        return;
    }

    m_webView->page()->runJavaScript(script, callback);
}

void WebSdrController::startSmeterPolling(int intervalMs)
{
    if (!m_smeterTimer) {
        m_smeterTimer = new QTimer(this);
        connect(m_smeterTimer, &QTimer::timeout, this, &WebSdrController::pollSmeter);
    }

    m_smeterTimer->start(intervalMs);
    qDebug() << "WebSdrController: Started S-meter polling at" << intervalMs << "ms";
}

void WebSdrController::stopSmeterPolling()
{
    if (m_smeterTimer) {
        m_smeterTimer->stop();
        qDebug() << "WebSdrController: Stopped S-meter polling";
    }
}

void WebSdrController::pollSmeter()
{
    if (m_state != Ready) {
        return;
    }

    // JavaScript to read the WebSDR S-meter value
    // Note: Some sites (e.g., Bordeaux) have soundapplet defined but null
    QString script =
        "(function() {"
        "  try {"
        "    if (typeof soundapplet !== 'undefined' && soundapplet !== null && typeof soundapplet.smeter === 'function') {"
        "      var s = soundapplet.smeter();"
        "      if (typeof scale4wf !== 'undefined') {"
        "        s = s + scale4wf;"
        "      }"
        "      return s;"
        "    }"
        "  } catch(e) {}"
        "  return -1;"
        "})();";

    runJavaScript(script, [this](const QVariant& result) {
        if (result.isValid()) {
            int value = result.toInt();
            if (value >= 0 && value != m_lastSmeterValue) {
                m_lastSmeterValue = value;
                emit smeterChanged(value);
            }
        }
    });
}
