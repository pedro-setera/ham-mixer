/*
 * WebSdrManager.cpp
 *
 * Manages SDR controllers (WebSDR 2.x and KiwiSDR) - only one site loaded at a time
 * Part of HamMixer CT7BAC
 */

#include "WebSdrManager.h"
#include <QDebug>

WebSdrManager::WebSdrManager(QWidget* parentWidget, QObject* parent)
    : QObject(parent)
    , m_parentWidget(parentWidget)
    , m_webSdrController(nullptr)
    , m_kiwiSdrController(nullptr)
    , m_activeSiteType(SdrSiteType::WebSDR)
    , m_lastFrequencyHz(0)
{
}

WebSdrManager::~WebSdrManager()
{
    unloadCurrent();
}

void WebSdrManager::setSiteList(const QList<WebSdrSite>& sites)
{
    m_sites = sites;
    qDebug() << "WebSdrManager: Site list set with" << sites.size() << "sites";
}

void WebSdrManager::preInitialize()
{
    // Pre-create the WebSDR controller to initialize Chromium engine at startup
    // This avoids the visual blink/glitch that occurs on first WebEngine use
    if (!m_webSdrController && m_parentWidget) {
        m_webSdrController = new WebSdrController(m_parentWidget, this);
        connectWebSdrSignals();

        // Load blank page to trigger Chromium initialization
        if (m_webSdrController->webView()) {
            m_webSdrController->webView()->load(QUrl("about:blank"));
        }

        qDebug() << "WebSdrManager: Pre-initialized WebEngine";
    }
}

WebSdrSite WebSdrManager::findSite(const QString& siteId) const
{
    for (const WebSdrSite& site : m_sites) {
        if (site.id == siteId) {
            return site;
        }
    }
    return WebSdrSite();  // Return invalid site if not found
}

void WebSdrManager::connectWebSdrSignals()
{
    if (!m_webSdrController) return;

    connect(m_webSdrController, &WebSdrController::stateChanged,
            this, &WebSdrManager::onWebSdrStateChanged);
    connect(m_webSdrController, &WebSdrController::smeterChanged,
            this, &WebSdrManager::onWebSdrSmeterChanged);
    connect(m_webSdrController, &WebSdrController::pageReady,
            this, &WebSdrManager::onWebSdrPageReady);
    connect(m_webSdrController, &WebSdrController::errorOccurred,
            this, &WebSdrManager::onWebSdrError);
}

void WebSdrManager::connectKiwiSdrSignals()
{
    if (!m_kiwiSdrController) return;

    connect(m_kiwiSdrController, &KiwiSdrController::stateChanged,
            this, &WebSdrManager::onKiwiSdrStateChanged);
    connect(m_kiwiSdrController, &KiwiSdrController::smeterChanged,
            this, &WebSdrManager::onKiwiSdrSmeterChanged);
    connect(m_kiwiSdrController, &KiwiSdrController::pageReady,
            this, &WebSdrManager::onKiwiSdrPageReady);
    connect(m_kiwiSdrController, &KiwiSdrController::errorOccurred,
            this, &WebSdrManager::onKiwiSdrError);
}

void WebSdrManager::loadSite(const QString& siteId)
{
    // Find the site in our list
    WebSdrSite site = findSite(siteId);
    if (!site.isValid()) {
        qWarning() << "WebSdrManager: Site not found:" << siteId;
        emit siteError(siteId, "Site not found in list");
        return;
    }

    // If same site is already loaded, just show the window
    if (m_activeSiteId == siteId) {
        qDebug() << "WebSdrManager: Site already loaded:" << siteId;
        showWindow();
        return;
    }

    qDebug() << "WebSdrManager: Loading site" << site.name << "(" << siteId << ")"
             << "Type:" << (site.isKiwiSDR() ? "KiwiSDR" : "WebSDR");

    // Unload the opposite controller type if active
    if (site.isKiwiSDR()) {
        // Loading KiwiSDR - unload WebSDR if active
        unloadWebSdr();
    } else {
        // Loading WebSDR - unload KiwiSDR if active
        unloadKiwiSdr();
    }

    // Store active site info
    m_activeSiteId = siteId;
    m_activeSiteType = site.type;

    if (site.isKiwiSDR()) {
        // Create KiwiSDR controller if needed
        if (!m_kiwiSdrController) {
            m_kiwiSdrController = new KiwiSdrController(m_parentWidget, this);
            connectKiwiSdrSignals();
        }

        // Apply pending frequency/mode before loading
        if (m_lastFrequencyHz > 0) {
            m_kiwiSdrController->tune(m_lastFrequencyHz, m_lastMode);
        }

        // Load the site
        m_kiwiSdrController->loadSite(site);

    } else {
        // WebSDR 2.x site
        if (!m_webSdrController) {
            m_webSdrController = new WebSdrController(m_parentWidget, this);
            connectWebSdrSignals();
        }

        // Apply pending frequency/mode before loading (same as KiwiSDR)
        // This sets controller's m_pendingFrequencyHz which gets applied during init
        if (m_lastFrequencyHz > 0) {
            m_webSdrController->tune(m_lastFrequencyHz, m_lastMode);
        }

        // Load the site
        m_webSdrController->loadSite(site);
    }

    emit activeSiteChanged(siteId);
}

void WebSdrManager::unloadWebSdr()
{
    if (m_webSdrController) {
        m_webSdrController->stopSmeterPolling();
        m_webSdrController->unload();
        // Hide the webview in embedded mode so it doesn't take up space
        m_webSdrController->hideWindow();
        // Don't delete - keep for reuse and to avoid Chromium reinitialization
    }
}

void WebSdrManager::unloadKiwiSdr()
{
    if (m_kiwiSdrController) {
        m_kiwiSdrController->stopSmeterPolling();
        m_kiwiSdrController->hideWindow();
        m_kiwiSdrController->unload();
        m_kiwiSdrController->deleteLater();
        m_kiwiSdrController = nullptr;
    }
}

void WebSdrManager::unloadCurrent()
{
    qDebug() << "WebSdrManager: Unloading current site:" << m_activeSiteId;

    if (m_activeSiteType == SdrSiteType::KiwiSDR) {
        unloadKiwiSdr();
    } else {
        unloadWebSdr();
    }

    m_activeSiteId.clear();
}

bool WebSdrManager::isLoaded() const
{
    if (m_activeSiteType == SdrSiteType::KiwiSDR) {
        return m_kiwiSdrController != nullptr && m_kiwiSdrController->isReady();
    } else {
        return m_webSdrController != nullptr && m_webSdrController->isReady();
    }
}

void WebSdrManager::setFrequency(uint64_t frequencyHz)
{
    m_lastFrequencyHz = frequencyHz;

    if (m_activeSiteType == SdrSiteType::KiwiSDR) {
        if (m_kiwiSdrController && m_kiwiSdrController->isReady()) {
            m_kiwiSdrController->setFrequency(frequencyHz);
        }
    } else {
        if (m_webSdrController && m_webSdrController->isReady()) {
            m_webSdrController->setFrequency(frequencyHz);
        }
    }
}

void WebSdrManager::setMode(const QString& mode)
{
    m_lastMode = mode;

    if (m_activeSiteType == SdrSiteType::KiwiSDR) {
        if (m_kiwiSdrController && m_kiwiSdrController->isReady()) {
            m_kiwiSdrController->setMode(mode);
        }
    } else {
        if (m_webSdrController && m_webSdrController->isReady()) {
            m_webSdrController->setMode(mode);
        }
    }
}

void WebSdrManager::showWindow()
{
    if (m_activeSiteType == SdrSiteType::KiwiSDR) {
        if (m_kiwiSdrController) {
            m_kiwiSdrController->showWindow();
        }
    } else {
        if (m_webSdrController) {
            m_webSdrController->showWindow();
        }
    }
}

void WebSdrManager::hideWindow()
{
    if (m_activeSiteType == SdrSiteType::KiwiSDR) {
        if (m_kiwiSdrController) {
            m_kiwiSdrController->hideWindow();
        }
    } else {
        if (m_webSdrController) {
            m_webSdrController->hideWindow();
        }
    }
}

// WebSDR signal handlers
void WebSdrManager::onWebSdrStateChanged(WebSdrController::State state)
{
    if (m_activeSiteType == SdrSiteType::WebSDR) {
        emit stateChanged(state);
    }
}

void WebSdrManager::onWebSdrSmeterChanged(int value)
{
    if (m_activeSiteType == SdrSiteType::WebSDR) {
        emit smeterChanged(value);
    }
}

void WebSdrManager::onWebSdrPageReady()
{
    if (m_activeSiteType == SdrSiteType::WebSDR) {
        qDebug() << "WebSdrManager: WebSDR site ready:" << m_activeSiteId;

        // Apply current frequency/mode
        if (m_lastFrequencyHz > 0) {
            m_webSdrController->setFrequency(m_lastFrequencyHz);
        }
        if (!m_lastMode.isEmpty()) {
            m_webSdrController->setMode(m_lastMode);
        }

        emit siteReady(m_activeSiteId);
    }
}

void WebSdrManager::onWebSdrError(const QString& error)
{
    if (m_activeSiteType == SdrSiteType::WebSDR) {
        qWarning() << "WebSdrManager: WebSDR error for site" << m_activeSiteId << ":" << error;
        emit siteError(m_activeSiteId, error);
    }
}

// KiwiSDR signal handlers
void WebSdrManager::onKiwiSdrStateChanged(KiwiSdrController::State state)
{
    if (m_activeSiteType == SdrSiteType::KiwiSDR) {
        // Map KiwiSdrController::State to WebSdrController::State for compatibility
        WebSdrController::State mappedState;
        switch (state) {
            case KiwiSdrController::Unloaded:
                mappedState = WebSdrController::Unloaded;
                break;
            case KiwiSdrController::Loading:
                mappedState = WebSdrController::Loading;
                break;
            case KiwiSdrController::Ready:
                mappedState = WebSdrController::Ready;
                break;
            case KiwiSdrController::Error:
            default:
                mappedState = WebSdrController::Error;
                break;
        }
        emit stateChanged(mappedState);
    }
}

void WebSdrManager::onKiwiSdrSmeterChanged(int value)
{
    if (m_activeSiteType == SdrSiteType::KiwiSDR) {
        emit smeterChanged(value);
    }
}

void WebSdrManager::onKiwiSdrPageReady()
{
    if (m_activeSiteType == SdrSiteType::KiwiSDR) {
        qDebug() << "WebSdrManager: KiwiSDR site ready:" << m_activeSiteId;

        // Frequency/mode already applied during load, but ensure current state
        if (m_lastFrequencyHz > 0) {
            m_kiwiSdrController->setFrequency(m_lastFrequencyHz);
        }
        if (!m_lastMode.isEmpty()) {
            m_kiwiSdrController->setMode(m_lastMode);
        }

        emit siteReady(m_activeSiteId);
    }
}

void WebSdrManager::onKiwiSdrError(const QString& error)
{
    if (m_activeSiteType == SdrSiteType::KiwiSDR) {
        qWarning() << "WebSdrManager: KiwiSDR error for site" << m_activeSiteId << ":" << error;
        emit siteError(m_activeSiteId, error);
    }
}
