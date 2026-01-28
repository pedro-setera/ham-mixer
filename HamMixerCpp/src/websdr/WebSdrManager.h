/*
 * WebSdrManager.h
 *
 * Manages a single WebSDR controller - only one site loaded at a time
 * Part of HamMixer CT7BAC
 */

#ifndef WEBSDRMANAGER_H
#define WEBSDRMANAGER_H

#include <QObject>
#include <QList>
#include "WebSdrController.h"
#include "WebSdrSite.h"

/**
 * @brief Manages a single WebSDR controller
 *
 * Only one WebSDR site is loaded at a time to minimize CPU usage.
 * When switching sites, the current site is unloaded before loading the new one.
 */
class WebSdrManager : public QObject
{
    Q_OBJECT

public:
    explicit WebSdrManager(QObject* parent = nullptr);
    ~WebSdrManager();

    /**
     * Set the available site list (does NOT load any sites)
     * @param sites List of available sites
     */
    void setSiteList(const QList<WebSdrSite>& sites);

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
     * Get active controller
     */
    WebSdrController* activeController() const { return m_controller; }

    /**
     * Check if a site is currently loaded
     */
    bool isLoaded() const { return m_controller != nullptr && m_controller->isReady(); }

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
    void onControllerStateChanged(WebSdrController::State state);
    void onControllerSmeterChanged(int value);
    void onControllerPageReady();
    void onControllerError(const QString& error);

private:
    WebSdrSite findSite(const QString& siteId) const;

    WebSdrController* m_controller;  // Single controller (only one site at a time)
    QList<WebSdrSite> m_sites;       // Available sites (not all loaded)
    QString m_activeSiteId;
    uint64_t m_lastFrequencyHz;      // For applying to newly loaded sites
    QString m_lastMode;
};

#endif // WEBSDRMANAGER_H
