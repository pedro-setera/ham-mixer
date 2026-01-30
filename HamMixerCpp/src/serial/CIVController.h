/*
 * CIVController.h
 *
 * CI-V Serial Communication Controller for Icom radios
 * Part of HamMixer CT7BAC
 */

#ifndef CIVCONTROLLER_H
#define CIVCONTROLLER_H

#include "RadioController.h"
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>
#include <QByteArray>
#include <QString>
#include <QStringList>

class CIVController : public RadioController
{
    Q_OBJECT

public:
    explicit CIVController(QObject* parent = nullptr);
    ~CIVController();

    // RadioController interface implementation
    bool connect(const QString& portName, int baudRate = 57600) override;
    void disconnect() override;
    ConnectionState state() const override { return m_state; }
    QString lastError() const override { return m_lastError; }
    bool isConnected() const override { return m_state == Connected; }
    Protocol protocol() const override { return IcomCIV; }

    // Port enumeration (static, not part of interface)
    static QStringList availablePorts();
    static QList<QSerialPortInfo> availablePortsInfo();

    // Polling control
    void startPolling(int intervalMs = 100) override;
    void stopPolling() override;
    bool isPolling() const override { return m_pollTimer && m_pollTimer->isActive(); }

    // Manual queries (results come via signals)
    void requestFrequency() override;
    void requestMode() override;
    void requestSMeter() override;
    void requestTXStatus();

    // Current values (cached from last poll)
    uint64_t currentFrequency() const override { return m_currentFrequencyHz; }
    uint8_t currentMode() const override { return m_currentMode; }
    QString currentModeName() const override { return m_currentModeName; }
    int currentSMeter() const override { return m_currentSMeter; }
    QString radioModel() const override { return m_radioModel; }

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
    bool m_currentTxStatus;  // true = transmitting
    QString m_radioModel;

    // Polling state machine
    int m_pollPhase;  // 0=freq, 1=mode, 2=smeter
};

#endif // CIVCONTROLLER_H
