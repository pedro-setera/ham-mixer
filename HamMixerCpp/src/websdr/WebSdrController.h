/*
 * WebSdrController.h
 *
 * WebSDR browser controller using Qt WebEngine in a separate window
 * Part of HamMixer CT7BAC
 */

#ifndef WEBSDRCONTROLLER_H
#define WEBSDRCONTROLLER_H

#include <QObject>
#include <QWidget>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QTimer>
#include <QString>
#include <functional>
#include <cstdint>

#include "WebSdrSite.h"

class WebSdrController : public QObject
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

    explicit WebSdrController(QObject* parent = nullptr);
    ~WebSdrController();

    // Show/hide the browser window
    void showWindow();
    void hideWindow();
    bool isWindowVisible() const;

    // Load/unload site
    void loadSite(const WebSdrSite& site);
    void unload();
    void reload();

    // State
    State state() const { return m_state; }
    bool isReady() const { return m_state == Ready; }
    const WebSdrSite& currentSite() const { return m_currentSite; }

    // Control methods (via JavaScript injection)
    // Frequency in Hz, will be converted to kHz for WebSDR
    void setFrequency(uint64_t frequencyHz);

    // Mode: "lsb", "usb", "cw", "am", "fm"
    void setMode(const QString& mode);

    // Audio control
    void startAudio();
    void stopAudio();

    // Combined set frequency and mode
    void tune(uint64_t frequencyHz, const QString& mode);

    // Start/stop S-meter polling
    void startSmeterPolling(int intervalMs = 100);
    void stopSmeterPolling();

signals:
    void stateChanged(WebSdrController::State state);
    void loadProgress(int percent);
    void pageReady();
    void errorOccurred(const QString& error);
    void smeterChanged(int value);  // WebSDR S-meter value (raw units)

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
    void initializeWebSdr();

    QWidget* m_browserWindow;      // Separate window for the browser
    QWebEngineView* m_webView;
    WebSdrSite m_currentSite;
    State m_state;
    bool m_audioStarted;

    // Pending operations (to apply after page loads)
    uint64_t m_pendingFrequencyHz;
    QString m_pendingMode;
    bool m_hasPendingTune;

    // S-meter polling
    QTimer* m_smeterTimer;
    int m_lastSmeterValue;
};

#endif // WEBSDRCONTROLLER_H
