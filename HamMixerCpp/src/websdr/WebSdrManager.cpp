/*
 * WebSdrManager.cpp
 *
 * Manages a single WebSDR controller - only one site loaded at a time
 * Part of HamMixer CT7BAC
 */

#include "WebSdrManager.h"
#include <QDebug>

WebSdrManager::WebSdrManager(QObject* parent)
    : QObject(parent)
    , m_controller(nullptr)
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

WebSdrSite WebSdrManager::findSite(const QString& siteId) const
{
    for (const WebSdrSite& site : m_sites) {
        if (site.id == siteId) {
            return site;
        }
    }
    return WebSdrSite();  // Return invalid site if not found
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
    if (m_activeSiteId == siteId && m_controller != nullptr) {
        qDebug() << "WebSdrManager: Site already loaded:" << siteId;
        showWindow();
        return;
    }

    qDebug() << "WebSdrManager: Loading site" << site.name << "(" << siteId << ")";

    // Unload current site first (if any)
    unloadCurrent();

    // Create new controller
    m_controller = new WebSdrController(this);

    // Connect signals
    connect(m_controller, &WebSdrController::stateChanged,
            this, &WebSdrManager::onControllerStateChanged);
    connect(m_controller, &WebSdrController::smeterChanged,
            this, &WebSdrManager::onControllerSmeterChanged);
    connect(m_controller, &WebSdrController::pageReady,
            this, &WebSdrManager::onControllerPageReady);
    connect(m_controller, &WebSdrController::errorOccurred,
            this, &WebSdrManager::onControllerError);

    // Store active site ID
    m_activeSiteId = siteId;

    // Load the site (will open at maximum volume)
    m_controller->loadSite(site);

    emit activeSiteChanged(siteId);
}

void WebSdrManager::unloadCurrent()
{
    if (m_controller) {
        qDebug() << "WebSdrManager: Unloading current site:" << m_activeSiteId;

        // Stop S-meter polling
        m_controller->stopSmeterPolling();

        // Unload and delete the controller
        m_controller->unload();
        m_controller->deleteLater();
        m_controller = nullptr;
    }

    m_activeSiteId.clear();
}

void WebSdrManager::setFrequency(uint64_t frequencyHz)
{
    m_lastFrequencyHz = frequencyHz;

    if (m_controller && m_controller->isReady()) {
        m_controller->setFrequency(frequencyHz);
    }
}

void WebSdrManager::setMode(const QString& mode)
{
    m_lastMode = mode;

    if (m_controller && m_controller->isReady()) {
        m_controller->setMode(mode);
    }
}

void WebSdrManager::showWindow()
{
    if (m_controller) {
        m_controller->showWindow();
    }
}

void WebSdrManager::hideWindow()
{
    if (m_controller) {
        m_controller->hideWindow();
    }
}

void WebSdrManager::onControllerStateChanged(WebSdrController::State state)
{
    emit stateChanged(state);
}

void WebSdrManager::onControllerSmeterChanged(int value)
{
    emit smeterChanged(value);
}

void WebSdrManager::onControllerPageReady()
{
    qDebug() << "WebSdrManager: Site ready:" << m_activeSiteId;

    // Apply current frequency/mode (may have changed while site was loading)
    // Note: Audio and S-meter are already started by WebSdrController::initializeWebSdr()
    if (m_lastFrequencyHz > 0) {
        m_controller->setFrequency(m_lastFrequencyHz);
    }
    if (!m_lastMode.isEmpty()) {
        m_controller->setMode(m_lastMode);
    }

    emit siteReady(m_activeSiteId);
}

void WebSdrManager::onControllerError(const QString& error)
{
    qWarning() << "WebSdrManager: Error for site" << m_activeSiteId << ":" << error;
    emit siteError(m_activeSiteId, error);
}
