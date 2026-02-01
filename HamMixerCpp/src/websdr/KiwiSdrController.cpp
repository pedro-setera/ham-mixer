/*
 * KiwiSdrController.cpp
 *
 * KiwiSDR browser controller implementation
 * Part of HamMixer CT7BAC
 */

#include "KiwiSdrController.h"
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

KiwiSdrController::KiwiSdrController(QWidget* parentWidget, QObject* parent)
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
    , m_initialized(false)
{
    if (m_embedded) {
        // Embedded mode: create web view and add to parent's layout
        m_webView = new QWebEngineView(parentWidget);

        // Add to parent's layout if it has one
        QLayout* parentLayout = parentWidget->layout();
        if (parentLayout) {
            parentLayout->addWidget(m_webView);
            qDebug() << "KiwiSdrController: Created in embedded mode, added to parent layout";
        } else {
            qDebug() << "KiwiSdrController: Created in embedded mode (no parent layout)";
        }
    } else {
        // Separate window mode
        m_browserWindow = new QWidget(nullptr, Qt::Window);
        m_browserWindow->setWindowTitle(QString("%1 V%2 (%3) - KiwiSDR")
            .arg(HAMMIXER_WEBSDR_NAME)
            .arg(HAMMIXER_VERSION_STRING)
            .arg(HAMMIXER_VERSION_DATE));
        m_browserWindow->setWindowIcon(QIcon(":/icons/icons/antenna.png"));
        m_browserWindow->resize(1024, 768);

        // Create layout and web view
        QVBoxLayout* layout = new QVBoxLayout(m_browserWindow);
        layout->setContentsMargins(0, 0, 0, 0);

        m_webView = new QWebEngineView(m_browserWindow);
        layout->addWidget(m_webView);

        // Install event filter to handle window close
        m_browserWindow->installEventFilter(this);

        qDebug() << "KiwiSdrController: Created with separate window";
    }

    // Configure web engine settings to allow autoplay without user gesture
    QWebEngineSettings* settings = m_webView->page()->settings();
    settings->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, false);
    settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    settings->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, false);
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    settings->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
    settings->setAttribute(QWebEngineSettings::AutoLoadImages, true);
    qDebug() << "KiwiSdrController: Configured WebEngine for KiwiSDR";

    // Connect signals
    QObject::connect(m_webView, &QWebEngineView::loadStarted,
                     this, &KiwiSdrController::onLoadStarted);
    QObject::connect(m_webView, &QWebEngineView::loadProgress,
                     this, &KiwiSdrController::onLoadProgress);
    QObject::connect(m_webView, &QWebEngineView::loadFinished,
                     this, &KiwiSdrController::onLoadFinished);
}

bool KiwiSdrController::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_browserWindow && event->type() == QEvent::Close) {
        // When user closes the window, just hide it
        qDebug() << "KiwiSdrController: Window close requested - hiding window";
        m_browserWindow->hide();
        event->ignore();
        return true;
    }
    return QObject::eventFilter(obj, event);
}

KiwiSdrController::~KiwiSdrController()
{
    stopSmeterPolling();
    unload();

    if (m_embedded) {
        if (m_webView) {
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

void KiwiSdrController::showWindow()
{
    if (m_embedded) {
        if (m_webView) {
            m_webView->show();
        }
    } else if (m_browserWindow) {
        m_browserWindow->show();
        m_browserWindow->raise();
        m_browserWindow->activateWindow();
    }
}

void KiwiSdrController::hideWindow()
{
    if (m_embedded) {
        if (m_webView) {
            m_webView->hide();
        }
    } else if (m_browserWindow) {
        m_browserWindow->hide();
    }
}

bool KiwiSdrController::isWindowVisible() const
{
    if (m_embedded) {
        return m_webView && m_webView->isVisible();
    }
    return m_browserWindow && m_browserWindow->isVisible();
}

QString KiwiSdrController::buildKiwiSdrUrl(const WebSdrSite& site) const
{
    QString baseUrl = site.effectiveUrl();

    // Ensure URL ends without trailing slash for parameter appending
    while (baseUrl.endsWith('/')) {
        baseUrl.chop(1);
    }

    // Build URL with frequency and mode if pending
    QString loadUrl = baseUrl;
    if (m_pendingFrequencyHz > 0) {
        double freqKHz = static_cast<double>(m_pendingFrequencyHz) / 1000.0;
        QString mode = m_pendingMode.isEmpty() ? "usb" : m_pendingMode.toLower();
        // KiwiSDR URL format: ?f=14230usb
        loadUrl += QString("/?f=%1%2").arg(freqKHz, 0, 'f', 2).arg(mode);
    }

    // Add reduced audio buffer for lower latency (0.5s instead of default ~1.5s)
    // 0.5s is a balance: low enough for good latency (~1.2-1.5s),
    // but enough headroom to prevent buffer underruns that trigger adaptive increases
    // Format: abuf=min,max (values 0.25-5.0 seconds)
    loadUrl += loadUrl.contains('?') ? "&" : "?";
    loadUrl += "abuf=0.5";

    // Set volume to maximum via URL parameter (0-100 scale)
    loadUrl += "&vol=100";

    // Add password if provided
    if (!site.password.isEmpty()) {
        loadUrl += QString("&p=%1").arg(site.password);
    }

    return loadUrl;
}

void KiwiSdrController::loadSite(const WebSdrSite& site)
{
    if (!site.isValid() || !site.isKiwiSDR()) {
        qWarning() << "KiwiSdrController: Invalid or non-KiwiSDR site configuration";
        setState(Error);
        emit errorOccurred("Invalid KiwiSDR site configuration");
        return;
    }

    m_currentSite = site;
    m_audioStarted = false;
    m_initialized = false;

    // Update window title (only for separate window mode)
    if (!m_embedded && m_browserWindow) {
        m_browserWindow->setWindowTitle(QString("%1 V%2 (%3) - %4")
            .arg(HAMMIXER_WEBSDR_NAME)
            .arg(HAMMIXER_VERSION_STRING)
            .arg(HAMMIXER_VERSION_DATE)
            .arg(site.name));

        m_browserWindow->setMinimumSize(400, 300);
        m_browserWindow->resize(1024, 700);
    }

    QString loadUrl = buildKiwiSdrUrl(site);
    qDebug() << "KiwiSdrController: Loading site" << site.name << "from" << loadUrl;
    setState(Loading);

    // Show the window/view and load the site
    showWindow();
    m_webView->load(QUrl(loadUrl));
}

void KiwiSdrController::unload()
{
    if (m_state != Unloaded) {
        stopSmeterPolling();

        // Stop audio before unloading
        if (m_webView && m_webView->page()) {
            QString script =
                "(function() {"
                "  try {"
                "    if (typeof toggle_or_set_audio === 'function') {"
                "      toggle_or_set_audio(0);"
                "    }"
                "  } catch(e) {}"
                "})();";
            m_webView->page()->runJavaScript(script);
        }

        // Navigate to blank page
        if (m_webView) {
            m_webView->setUrl(QUrl("about:blank"));
        }

        if (!m_embedded) {
            hideWindow();
        }

        m_currentSite = WebSdrSite();
        m_audioStarted = false;
        m_hasPendingTune = false;
        m_initialized = false;
        m_lastSmeterValue = -1;
        setState(Unloaded);
        qDebug() << "KiwiSdrController: Unloaded";
    }
}

void KiwiSdrController::setFrequency(uint64_t frequencyHz)
{
    m_pendingFrequencyHz = frequencyHz;

    if (m_state != Ready || !m_initialized) {
        m_hasPendingTune = true;
        return;
    }

    // KiwiSDR frequency control via JavaScript
    // Convert Hz to kHz for the API
    double freqKHz = static_cast<double>(frequencyHz) / 1000.0;

    // KiwiSDR JavaScript API - try multiple methods based on actual KiwiSDR source
    QString script = QString(R"(
        (function() {
            try {
                var freqKHz = %1;
                var freqHz = freqKHz * 1000;

                // Method 1: Use demodulator_set_frequency (from openwebrx.js)
                // This is the main frequency control function
                if (typeof demodulator_set_frequency === 'function') {
                    demodulator_set_frequency(0, freqHz);
                    console.log('HamMixer: Set frequency via demodulator_set_frequency:', freqHz);
                    return true;
                }

                // Method 2: Set frequency input field and trigger change
                var freqInput = document.getElementById('id-freq-input');
                if (freqInput) {
                    freqInput.value = freqKHz.toFixed(2);
                    // Trigger the frequency set
                    if (typeof freqset_complete === 'function') {
                        freqset_complete(1);
                        console.log('HamMixer: Set frequency via input field:', freqKHz);
                        return true;
                    }
                    // Fallback: dispatch events
                    freqInput.dispatchEvent(new Event('change', { bubbles: true }));
                    freqInput.dispatchEvent(new KeyboardEvent('keyup', { key: 'Enter', keyCode: 13 }));
                    console.log('HamMixer: Set frequency via input events:', freqKHz);
                    return true;
                }

                // Method 3: Use freqmode_set_dsp_kHz if available
                if (typeof freqmode_set_dsp_kHz === 'function') {
                    freqmode_set_dsp_kHz(freqKHz, null);
                    console.log('HamMixer: Set frequency via freqmode_set_dsp_kHz:', freqKHz);
                    return true;
                }

                // Method 4: Use wf_cur_freq to calculate offset and set
                if (typeof wf_cur_freq !== 'undefined' && typeof demodulator_set_offset_frequency === 'function') {
                    var offset = freqHz - wf_cur_freq;
                    demodulator_set_offset_frequency(0, offset);
                    console.log('HamMixer: Set frequency via offset:', offset);
                    return true;
                }

                console.log('HamMixer: No frequency control method found. Available globals:', Object.keys(window).filter(k => k.includes('freq')).join(', '));
                return false;
            } catch(e) {
                console.log('HamMixer: Frequency error:', e.message);
                return false;
            }
        })()
    )").arg(freqKHz, 0, 'f', 3);

    runJavaScript(script);
    qDebug() << "KiwiSdrController: Set frequency to" << frequencyHz << "Hz (" << freqKHz << "kHz)";
}

void KiwiSdrController::setMode(const QString& mode)
{
    m_pendingMode = mode.toLower();

    if (m_state != Ready || !m_initialized) {
        m_hasPendingTune = true;
        return;
    }

    // Map common mode names to KiwiSDR mode codes
    // KiwiSDR modes: am, amn, sam, sal, sau, sas, qam, drm, usb, usn, lsb, lsn, cw, cwn, nbfm, nnfm, iq
    QString kiwiMode = mode.toLower();
    if (kiwiMode == "fm") {
        kiwiMode = "nbfm";
    } else if (kiwiMode == "cw-r" || kiwiMode == "cwr") {
        kiwiMode = "cwn";
    }

    QString script = QString(R"(
        (function() {
            try {
                var mode = '%1';

                // Method 1: Use demodulator_analog_replace (from openwebrx.js)
                // This is the main mode control function, pass null for freq to keep current
                if (typeof demodulator_analog_replace === 'function') {
                    demodulator_analog_replace(mode, null);
                    console.log('HamMixer: Set mode via demodulator_analog_replace:', mode);
                    return true;
                }

                // Method 2: Click the mode button directly
                var modeBtn = document.getElementById('id-button-' + mode);
                if (modeBtn) {
                    modeBtn.click();
                    console.log('HamMixer: Set mode via button click:', mode);
                    return true;
                }

                // Method 3: Try clicking mode buttons by class/text
                var buttons = document.querySelectorAll('.cl-button, button');
                for (var i = 0; i < buttons.length; i++) {
                    var btn = buttons[i];
                    var text = (btn.innerText || btn.textContent || '').toLowerCase().trim();
                    if (text === mode) {
                        btn.click();
                        console.log('HamMixer: Set mode via button text match:', mode);
                        return true;
                    }
                }

                // Method 4: Use ext_set_mode if available
                if (typeof ext_set_mode === 'function') {
                    ext_set_mode(mode);
                    console.log('HamMixer: Set mode via ext_set_mode:', mode);
                    return true;
                }

                console.log('HamMixer: No mode control method found. Buttons found:', document.querySelectorAll('button, .cl-button').length);
                return false;
            } catch(e) {
                console.log('HamMixer: Mode error:', e.message);
                return false;
            }
        })()
    )").arg(kiwiMode);

    runJavaScript(script);
    qDebug() << "KiwiSdrController: Set mode to" << kiwiMode;
}

void KiwiSdrController::startAudio()
{
    if (m_state != Ready) {
        return;
    }

    // KiwiSDR audio control
    QString script = R"(
        (function() {
            try {
                // Method 1: toggle_or_set_audio(1) to enable
                if (typeof toggle_or_set_audio === 'function') {
                    toggle_or_set_audio(1);
                    console.log('HamMixer: Audio started via toggle_or_set_audio');
                    return true;
                }

                // Method 2: audio_start
                if (typeof audio_start === 'function') {
                    audio_start();
                    console.log('HamMixer: Audio started via audio_start');
                    return true;
                }

                // Method 3: Click the power/audio button
                var btn = document.getElementById('id-button-mute') ||
                          document.querySelector('[id*="mute"]') ||
                          document.querySelector('[id*="audio"]');
                if (btn) {
                    // Check if audio is off and click to enable
                    if (btn.classList.contains('off') || btn.style.opacity === '0.5') {
                        btn.click();
                        console.log('HamMixer: Audio started via button click');
                        return true;
                    }
                }

                console.log('HamMixer: Audio may already be running or no control found');
                return false;
            } catch(e) {
                console.log('HamMixer: Audio start error:', e.message);
                return false;
            }
        })()
    )";

    runJavaScript(script);
    m_audioStarted = true;
    qDebug() << "KiwiSdrController: Attempted to start audio";
}

void KiwiSdrController::stopAudio()
{
    if (m_state != Ready) {
        return;
    }

    QString script = R"(
        (function() {
            try {
                if (typeof toggle_or_set_audio === 'function') {
                    toggle_or_set_audio(0);
                    console.log('HamMixer: Audio stopped');
                }
            } catch(e) {}
        })()
    )";

    runJavaScript(script);
    m_audioStarted = false;
    qDebug() << "KiwiSdrController: Stopped audio";
}

void KiwiSdrController::setVolume(int percent)
{
    if (m_state != Ready) {
        return;
    }

    // Volume 0-100
    int vol = qBound(0, percent, 100);

    QString script = QString(R"(
        (function() {
            try {
                if (typeof setvolume === 'function') {
                    setvolume(%1);
                } else if (typeof kiwi_volume === 'function') {
                    kiwi_volume(%1);
                }
            } catch(e) {}
        })()
    )").arg(vol);

    runJavaScript(script);
    qDebug() << "KiwiSdrController: Set volume to" << vol << "%";
}

void KiwiSdrController::tune(uint64_t frequencyHz, const QString& mode)
{
    if (m_state != Ready || !m_initialized) {
        m_pendingFrequencyHz = frequencyHz;
        m_pendingMode = mode;
        m_hasPendingTune = true;
        return;
    }

    setMode(mode);
    setFrequency(frequencyHz);
}

void KiwiSdrController::onLoadStarted()
{
    setState(Loading);
    emit loadProgress(0);
}

void KiwiSdrController::onLoadProgress(int progress)
{
    emit loadProgress(progress);
}

void KiwiSdrController::onLoadFinished(bool ok)
{
    if (ok) {
        qDebug() << "KiwiSdrController: Page loaded successfully";
        // Wait for KiwiSDR JavaScript to initialize
        // KiwiSDR takes much longer to initialize than WebSDR (waterfall, audio, etc.)
        QTimer::singleShot(3500, this, &KiwiSdrController::initializeKiwiSdr);
    } else {
        qWarning() << "KiwiSdrController: Page load failed";
        setState(Error);
        emit errorOccurred("Failed to load KiwiSDR page");
    }
}

void KiwiSdrController::initializeKiwiSdr()
{
    setState(Ready);
    qDebug() << "KiwiSdrController: Starting initialization sequence...";

    // KiwiSDR typically auto-starts audio, but ensure it's playing
    // and set volume to maximum
    QString setupScript = R"(
        (function() {
            try {
                // Unmute and set volume high
                if (typeof toggle_or_set_mute === 'function') {
                    toggle_or_set_mute(0);  // Unmute
                }
                if (typeof setvolume === 'function') {
                    setvolume(100);
                }
                console.log('HamMixer: KiwiSDR audio setup complete');
            } catch(e) {
                console.log('HamMixer: KiwiSDR setup error:', e.message);
            }
        })()
    )";

    if (m_webView && m_webView->page()) {
        m_webView->page()->runJavaScript(setupScript);
    }

    // Wait for KiwiSDR JavaScript to be fully ready before applying frequency/mode
    QTimer::singleShot(1000, this, [this]() {
        if (m_state != Ready) return;

        m_initialized = true;

        // Apply pending frequency
        if (m_hasPendingTune || m_pendingFrequencyHz > 0) {
            if (m_pendingFrequencyHz > 0) {
                setFrequency(m_pendingFrequencyHz);
            }

            // Apply mode after a short delay
            QTimer::singleShot(300, this, [this]() {
                if (m_state != Ready) return;

                if (!m_pendingMode.isEmpty()) {
                    setMode(m_pendingMode);
                }
                m_hasPendingTune = false;
            });
        }

        // Start audio explicitly
        QTimer::singleShot(500, this, [this]() {
            if (m_state != Ready) return;
            startAudio();

            // Start S-meter polling
            startSmeterPolling(100);

            // Emit ready
            emit pageReady();
            qDebug() << "KiwiSdrController: Initialization complete - page ready";
        });
    });
}

void KiwiSdrController::setState(State newState)
{
    if (m_state != newState) {
        m_state = newState;
        emit stateChanged(newState);
    }
}

void KiwiSdrController::runJavaScript(const QString& script)
{
    if (m_state != Ready && m_state != Loading) {
        qWarning() << "KiwiSdrController: Cannot run JavaScript - not in ready/loading state";
        return;
    }

    if (!m_webView || !m_webView->page()) {
        qWarning() << "KiwiSdrController: Cannot run JavaScript - webView/page is null";
        return;
    }

    QString safeScript = QString("try { %1 } catch(e) { console.log('HamMixer JS error: ' + e.message); }").arg(script);
    m_webView->page()->runJavaScript(safeScript);
}

void KiwiSdrController::runJavaScript(const QString& script,
                                       std::function<void(const QVariant&)> callback)
{
    if (m_state != Ready && m_state != Loading) {
        qWarning() << "KiwiSdrController: Cannot run JavaScript - not in ready/loading state";
        if (callback) callback(QVariant());
        return;
    }

    if (!m_webView || !m_webView->page()) {
        qWarning() << "KiwiSdrController: Cannot run JavaScript - webView/page is null";
        if (callback) callback(QVariant());
        return;
    }

    m_webView->page()->runJavaScript(script, callback);
}

void KiwiSdrController::startSmeterPolling(int intervalMs)
{
    if (!m_smeterTimer) {
        m_smeterTimer = new QTimer(this);
        connect(m_smeterTimer, &QTimer::timeout, this, &KiwiSdrController::pollSmeter);
    }

    m_smeterTimer->start(intervalMs);
    qDebug() << "KiwiSdrController: Started S-meter polling at" << intervalMs << "ms";
}

void KiwiSdrController::stopSmeterPolling()
{
    if (m_smeterTimer) {
        m_smeterTimer->stop();
        qDebug() << "KiwiSdrController: Stopped S-meter polling";
    }
}

void KiwiSdrController::pollSmeter()
{
    if (m_state != Ready) {
        return;
    }

    // JavaScript to read KiwiSDR S-meter value
    // KiwiSDR provides S-meter in dBm
    QString script = R"(
        (function() {
            try {
                // Method 1: smeter_dBm global variable
                if (typeof smeter_dBm !== 'undefined') {
                    return smeter_dBm;
                }

                // Method 2: get_smeter function
                if (typeof get_smeter === 'function') {
                    return get_smeter();
                }

                // Method 3: Read from smeter element
                var smeterEl = document.getElementById('id-smeter-dbm');
                if (smeterEl) {
                    var text = smeterEl.innerText || smeterEl.textContent;
                    var match = text.match(/-?\d+/);
                    if (match) {
                        return parseInt(match[0]);
                    }
                }

                return -127;  // No signal
            } catch(e) {
                return -127;
            }
        })()
    )";

    runJavaScript(script, [this](const QVariant& result) {
        if (result.isValid()) {
            int dBm = result.toInt();
            if (dBm > -127) {
                // Convert dBm to 0-255 scale similar to Icom S-meter
                // Typical range: -127 dBm (noise floor) to -60 dBm (S9+30)
                // Map -127..-60 to 0..255
                int scaled = qBound(0, static_cast<int>((dBm + 127) * 255.0 / 67.0), 255);

                if (scaled != m_lastSmeterValue) {
                    m_lastSmeterValue = scaled;
                    emit smeterChanged(scaled);
                }
            }
        }
    });
}
