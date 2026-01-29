/*
 * WebSdrManager.h
 *
 * Manages SDR controllers (WebSDR 2.x and KiwiSDR) - only one site loaded at a time
 * Part of HamMixer CT7BAC
 */

#ifndef WEBSDRMANAGER_H
#define WEBSDRMANAGER_H

#include <QObject>
#include <QList>
#include "WebSdrController.h"
#include "KiwiSdrController.h"
#include "WebSdrSite.h"

/**
 * @brief Manages SDR site controllers (WebSDR 2.x and KiwiSDR)
 *
 * Only one site is loaded at a time to minimize CPU usage.
 * When switching sites, the current site is unloaded before loading the new one.
 * Supports both WebSDR 2.x and KiwiSDR site types.
 */
class WebSdrManager : public QObject
{
    Q_OBJECT

public:
    /**
     * Constructor
     * @param parentWidget If provided, WebSDR browser will be embedded in this widget
     * @param parent QObject parent
     */
    explicit WebSdrManager(QWidget* parentWidget = nullptr, QObject* parent = nullptr);
    ~WebSdrManager();

    /**
     * Set the available site list (does NOT load any sites)
     * @param sites List of available sites
     */
    void setSiteList(const QList<WebSdrSite>& sites);

    /**
     * Pre-initialize the WebEngine to avoid visual glitch on first use.
     * Call this at startup before loading any site.
     */
    void preInitialize();

    /**
     * Get list of available sites
     */
    QList<WebSdrSite> sites() const { return m_sites; }

    /**
     * Load a specific site (unloads current site first if any)
     * @param siteId Site ID to load
     */
    void loadSite(const QString& siteId);

    /**
     * Unload the current site
     */
    void unloadCurrent();

    /**
     * Get current active site ID
     */
    QString activeSiteId() const { return m_activeSiteId; }

    /**
     * Get active WebSDR controller (nullptr if KiwiSDR is active)
     */
    WebSdrController* activeWebSdrController() const { return m_webSdrController; }

    /**
     * Get active KiwiSDR controller (nullptr if WebSDR is active)
     */
    KiwiSdrController* activeKiwiSdrController() const { return m_kiwiSdrController; }

    /**
     * Get the active site type
     */
    SdrSiteType activeSiteType() const { return m_activeSiteType; }

    /**
     * Check if a site is currently loaded
     */
    bool isLoaded() const;

    /**
     * Set frequency on active controller
     * @param frequencyHz Frequency in Hz
     */
    void setFrequency(uint64_t frequencyHz);

    /**
     * Set mode on active controller
     * @param mode Mode string: "lsb", "usb", "cw", "am", "fm"
     */
    void setMode(const QString& mode);

    /**
     * Show the WebSDR window
     */
    void showWindow();

    /**
     * Hide the WebSDR window
     */
    void hideWindow();

signals:
    /**
     * Emitted when the site finishes loading and is ready
     */
    void siteReady(const QString& siteId);

    /**
     * Emitted when a site fails to load
     */
    void siteError(const QString& siteId, const QString& error);

    /**
     * Emitted when active site changes
     */
    void activeSiteChanged(const QString& siteId);

    /**
     * Emitted when S-meter value changes
     */
    void smeterChanged(int value);

    /**
     * Emitted when WebSDR state changes
     */
    void stateChanged(WebSdrController::State state);

private slots:
    void onWebSdrStateChanged(WebSdrController::State state);
    void onWebSdrSmeterChanged(int value);
    void onWebSdrPageReady();
    void onWebSdrError(const QString& error);

    void onKiwiSdrStateChanged(KiwiSdrController::State state);
    void onKiwiSdrSmeterChanged(int value);
    void onKiwiSdrPageReady();
    void onKiwiSdrError(const QString& error);

private:
    WebSdrSite findSite(const QString& siteId) const;
    void connectWebSdrSignals();
    void connectKiwiSdrSignals();
    void unloadWebSdr();
    void unloadKiwiSdr();

    QWidget* m_parentWidget;              // Parent widget for embedded mode
    WebSdrController* m_webSdrController; // WebSDR 2.x controller
    KiwiSdrController* m_kiwiSdrController; // KiwiSDR controller
    QList<WebSdrSite> m_sites;            // Available sites (not all loaded)
    QString m_activeSiteId;
    SdrSiteType m_activeSiteType;         // Track which type is currently active
    uint64_t m_lastFrequencyHz;           // For applying to newly loaded sites
    QString m_lastMode;
};

#endif // WEBSDRMANAGER_H
