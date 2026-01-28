/*
 * CIVController.h
 *
 * CI-V Serial Communication Controller for IC-7300
 * Part of HamMixer CT7BAC
 */

#ifndef CIVCONTROLLER_H
#define CIVCONTROLLER_H

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <cstdint>

class CIVController : public QObject
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

    explicit CIVController(QObject* parent = nullptr);
    ~CIVController();

    // Connection management
    bool connect(const QString& portName, int baudRate = 57600);
    void disconnect();
    ConnectionState state() const { return m_state; }
    QString lastError() const { return m_lastError; }
    bool isConnected() const { return m_state == Connected; }

    // Port enumeration
    static QStringList availablePorts();
    static QList<QSerialPortInfo> availablePortsInfo();

    // Polling control
    void startPolling(int intervalMs = 100);
    void stopPolling();
    bool isPolling() const { return m_pollTimer && m_pollTimer->isActive(); }

    // Manual queries (results come via signals)
    void requestFrequency();
    void requestMode();
    void requestSMeter();

    // Current values (cached from last poll)
    uint64_t currentFrequency() const { return m_currentFrequencyHz; }
    uint8_t currentMode() const { return m_currentMode; }
    QString currentModeName() const { return m_currentModeName; }
    int currentSMeter() const { return m_currentSMeter; }

signals:
    void connectionStateChanged(CIVController::ConnectionState state);
    void frequencyChanged(uint64_t frequencyHz);
    void modeChanged(uint8_t mode, const QString& modeName);
    void smeterChanged(int value);  // 0-255 scale
    void errorOccurred(const QString& error);

private slots:
    void onReadyRead();
    void onPollTimer();
    void onSerialError(QSerialPort::SerialPortError error);

private:
    void setState(ConnectionState newState);
    void setError(const QString& error);
    void processFrame(const QByteArray& frame);
    void sendCommand(const QByteArray& data);
    bool extractFrames();  // Extract complete frames from buffer

    QSerialPort* m_serialPort;
    QTimer* m_pollTimer;
    QByteArray m_rxBuffer;
    ConnectionState m_state;
    QString m_lastError;

    // Cached current state
    uint64_t m_currentFrequencyHz;
    uint8_t m_currentMode;
    QString m_currentModeName;
    int m_currentSMeter;

    // Polling state machine
    int m_pollPhase;  // 0=freq, 1=mode, 2=smeter
};

#endif // CIVCONTROLLER_H
