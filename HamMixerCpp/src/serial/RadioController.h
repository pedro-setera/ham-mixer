/*
 * RadioController.h
 *
 * Abstract interface for radio control protocols
 * Supports Icom CI-V and Kenwood/Elecraft/Yaesu CAT
 * Part of HamMixer CT7BAC
 */

#ifndef RADIOCONTROLLER_H
#define RADIOCONTROLLER_H

#include <QObject>
#include <QString>
#include <cstdint>

class RadioController : public QObject
{
    Q_OBJECT

public:
    enum ConnectionState {
        Disconnected,
        Connecting,
        Connected,
        Error
    };
    Q_ENUM(ConnectionState)

    enum Protocol {
        Unknown,
        IcomCIV,
        KenwoodCAT
    };
    Q_ENUM(Protocol)

    explicit RadioController(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~RadioController() = default;

    // Connection management
    virtual bool connect(const QString& port, int baudRate) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual ConnectionState state() const = 0;
    virtual QString lastError() const = 0;
    virtual Protocol protocol() const = 0;

    // Polling control
    virtual void startPolling(int intervalMs) = 0;
    virtual void stopPolling() = 0;
    virtual bool isPolling() const = 0;

    // Manual queries (results come via signals)
    virtual void requestFrequency() = 0;
    virtual void requestMode() = 0;
    virtual void requestSMeter() = 0;
    virtual void requestTunerState() = 0;

    // Write commands (control the radio)
    virtual void setFrequency(uint64_t frequencyHz) = 0;
    virtual void setMode(uint8_t mode) = 0;
    virtual void setTunerState(bool enabled) = 0;
    virtual void startTune() = 0;
    virtual void playVoiceMemory(int memoryNumber) = 0;
    virtual void stopVoiceMemory() = 0;

    // Current values (cached from last poll)
    virtual uint64_t currentFrequency() const = 0;
    virtual uint8_t currentMode() const = 0;
    virtual QString currentModeName() const = 0;
    virtual int currentSMeter() const = 0;
    virtual QString radioModel() const = 0;

    // Static factory with auto-detection
    static RadioController* detectAndConnect(const QString& port, QObject* parent);

signals:
    void connectionStateChanged(RadioController::ConnectionState state);
    void frequencyChanged(uint64_t frequencyHz);
    void modeChanged(uint8_t mode, const QString& modeName);
    void smeterChanged(int value);  // 0-255 scale
    void txStatusChanged(bool transmitting);  // TX/RX state
    void tunerStateChanged(bool enabled);  // Tuner on/off state
    void errorOccurred(const QString& error);
    void radioModelDetected(const QString& modelName);
};

#endif // RADIOCONTROLLER_H
