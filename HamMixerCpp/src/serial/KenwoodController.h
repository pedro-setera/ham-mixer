/*
 * KenwoodController.h
 *
 * ASCII CAT Protocol Controller for Kenwood, Elecraft, and newer Yaesu radios
 * Part of HamMixer CT7BAC
 */

#ifndef KENWOODCONTROLLER_H
#define KENWOODCONTROLLER_H

#include "RadioController.h"
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>
#include <QByteArray>

class KenwoodController : public RadioController
{
    Q_OBJECT

public:
    explicit KenwoodController(QObject* parent = nullptr);
    ~KenwoodController();

    // RadioController interface implementation
    bool connect(const QString& port, int baudRate) override;
    void disconnect() override;
    bool isConnected() const override { return m_state == Connected; }
    ConnectionState state() const override { return m_state; }
    QString lastError() const override { return m_lastError; }
    Protocol protocol() const override { return KenwoodCAT; }

    void startPolling(int intervalMs) override;
    void stopPolling() override;
    bool isPolling() const override { return m_pollTimer && m_pollTimer->isActive(); }

    void requestFrequency() override;
    void requestMode() override;
    void requestSMeter() override;
    void requestTunerState() override;

    // Write commands (control the radio)
    void setFrequency(uint64_t frequencyHz) override;
    void setMode(uint8_t mode) override;
    void setTunerState(bool enabled) override;
    void startTune() override;
    void playVoiceMemory(int memoryNumber) override;
    void stopVoiceMemory() override;

    uint64_t currentFrequency() const override { return m_currentFrequencyHz; }
    uint8_t currentMode() const override { return m_currentMode; }
    QString currentModeName() const override { return m_currentModeName; }
    int currentSMeter() const override { return m_currentSMeter; }
    QString radioModel() const override { return QString(); }  // Not supported for Kenwood CAT

private slots:
    void onReadyRead();
    void onPollTimer();
    void onSerialError(QSerialPort::SerialPortError error);

private:
    void setState(ConnectionState newState);
    void setError(const QString& error);
    void sendCommand(const QString& cmd);
    void processResponse(const QString& response);
    QString modeToString(int mode);

    QSerialPort* m_serialPort;
    QTimer* m_pollTimer;
    QString m_rxBuffer;
    ConnectionState m_state;
    QString m_lastError;

    // Cached current state
    uint64_t m_currentFrequencyHz;
    uint8_t m_currentMode;
    QString m_currentModeName;
    int m_currentSMeter;

    // Polling state machine
    int m_pollPhase;
};

#endif // KENWOODCONTROLLER_H
