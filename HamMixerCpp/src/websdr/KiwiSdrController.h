/*
 * KiwiSdrController.h
 *
 * KiwiSDR browser controller using Qt WebEngine
 * Part of HamMixer CT7BAC
 */

#ifndef KIWISDRCONTROLLER_H
#define KIWISDRCONTROLLER_H

#include <QObject>
#include <QWidget>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QTimer>
#include <QString>
#include <functional>
#include <cstdint>

#include "WebSdrSite.h"

/**
 * @brief Controller for KiwiSDR sites via embedded browser
 *
 * Displays KiwiSDR in QWebEngineView and controls via JavaScript injection.
 * Audio is captured via system loopback (same as WebSDR).
 */
class KiwiSdrController : public QObject
{
    Q_OBJECT

public:
    enum State {
        Unloaded,
        Loading,
        Ready,
        Error
    };
    Q_ENUM(State)

    /**
     * Constructor
     * @param parentWidget If provided, webview will be embedded in this widget
     * @param parent QObject parent
     */
    explicit KiwiSdrController(QWidget* parentWidget = nullptr, QObject* parent = nullptr);
    ~KiwiSdrController();

    // Get the web view widget for embedding
    QWebEngineView* webView() const { return m_webView; }

    // Show/hide the browser window (only when using separate window mode)
    void showWindow();
    void hideWindow();
    bool isWindowVisible() const;

    // Check if using embedded mode
    bool isEmbedded() const { return m_embedded; }

    // Load/unload site
    void loadSite(const WebSdrSite& site);
    void unload();

    // State
    State state() const { return m_state; }
    bool isReady() const { return m_state == Ready; }
    const WebSdrSite& currentSite() const { return m_currentSite; }

    // Control methods (via JavaScript injection)
    // Frequency in Hz
    void setFrequency(uint64_t frequencyHz);

    // Mode: "lsb", "usb", "cw", "cwn", "am", "amn", "fm", "iq"
    void setMode(const QString& mode);

    // Audio control
    void startAudio();
    void stopAudio();
    void setVolume(int percent);

    // Combined set frequency and mode
    void tune(uint64_t frequencyHz, const QString& mode);

    // Start/stop S-meter polling
    void startSmeterPolling(int intervalMs = 100);
    void stopSmeterPolling();

signals:
    void stateChanged(KiwiSdrController::State state);
    void loadProgress(int percent);
    void pageReady();
    void errorOccurred(const QString& error);
    void smeterChanged(int value);  // S-meter value (scaled 0-255)

protected:
    // Event filter to handle window close
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onLoadStarted();
    void onLoadProgress(int progress);
    void onLoadFinished(bool ok);
    void pollSmeter();

private:
    void setState(State newState);
    void runJavaScript(const QString& script);
    void runJavaScript(const QString& script, std::function<void(const QVariant&)> callback);
    void initializeKiwiSdr();
    QString buildKiwiSdrUrl(const WebSdrSite& site) const;

    QWidget* m_browserWindow;      // Separate window for the browser (null if embedded)
    QWebEngineView* m_webView;
    WebSdrSite m_currentSite;
    State m_state;
    bool m_audioStarted;
    bool m_embedded;               // True if embedded in another window

    // Pending operations (to apply after page loads)
    uint64_t m_pendingFrequencyHz;
    QString m_pendingMode;
    bool m_hasPendingTune;

    // S-meter polling
    QTimer* m_smeterTimer;
    int m_lastSmeterValue;

    // Initialization state
    bool m_initialized;
};

#endif // KIWISDRCONTROLLER_H
