/*
 * KenwoodController.cpp
 *
 * ASCII CAT Protocol Controller implementation
 * Supports Kenwood, Elecraft, and newer Yaesu radios
 * Part of HamMixer CT7BAC
 */

#include "KenwoodController.h"
#include <QDebug>

KenwoodController::KenwoodController(QObject* parent)
    : RadioController(parent)
    , m_serialPort(nullptr)
    , m_pollTimer(nullptr)
    , m_state(Disconnected)
    , m_currentFrequencyHz(0)
    , m_currentMode(0xFF)
    , m_currentModeName("---")
    , m_currentSMeter(0)
    , m_pollPhase(0)
{
}

KenwoodController::~KenwoodController()
{
    disconnect();
}

bool KenwoodController::connect(const QString& portName, int baudRate)
{
    // Disconnect if already connected
    if (m_serialPort) {
        disconnect();
    }

    setState(Connecting);

    m_serialPort = new QSerialPort(this);
    m_serialPort->setPortName(portName);
    m_serialPort->setBaudRate(baudRate);
    m_serialPort->setDataBits(QSerialPort::Data8);
    m_serialPort->setParity(QSerialPort::NoParity);
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);

    // Connect signals
    QObject::connect(m_serialPort, &QSerialPort::readyRead,
                     this, &KenwoodController::onReadyRead);
    QObject::connect(m_serialPort, &QSerialPort::errorOccurred,
                     this, &KenwoodController::onSerialError);

    if (!m_serialPort->open(QIODevice::ReadWrite)) {
        setError(QString("Failed to open port %1: %2")
                 .arg(portName)
                 .arg(m_serialPort->errorString()));
        delete m_serialPort;
        m_serialPort = nullptr;
        setState(Error);
        return false;
    }

    // Clear any stale data
    m_serialPort->clear();
    m_rxBuffer.clear();

    qDebug() << "KenwoodController: Connected to" << portName << "at" << baudRate << "baud";
    setState(Connected);

    return true;
}

void KenwoodController::disconnect()
{
    stopPolling();

    if (m_serialPort) {
        if (m_serialPort->isOpen()) {
            m_serialPort->close();
        }
        delete m_serialPort;
        m_serialPort = nullptr;
    }

    m_rxBuffer.clear();
    setState(Disconnected);
    qDebug() << "KenwoodController: Disconnected";
}

void KenwoodController::startPolling(int intervalMs)
{
    if (!m_serialPort || !m_serialPort->isOpen()) {
        qWarning() << "KenwoodController: Cannot start polling - not connected";
        return;
    }

    if (!m_pollTimer) {
        m_pollTimer = new QTimer(this);
        QObject::connect(m_pollTimer, &QTimer::timeout,
                         this, &KenwoodController::onPollTimer);
    }

    m_pollPhase = 0;
    m_pollTimer->start(intervalMs);
    qDebug() << "KenwoodController: Started polling at" << intervalMs << "ms interval";
}

void KenwoodController::stopPolling()
{
    if (m_pollTimer) {
        m_pollTimer->stop();
        qDebug() << "KenwoodController: Stopped polling";
    }
}

void KenwoodController::requestFrequency()
{
    sendCommand("FA;");
}

void KenwoodController::requestMode()
{
    sendCommand("MD;");
}

void KenwoodController::requestSMeter()
{
    sendCommand("SM;");
}

void KenwoodController::onReadyRead()
{
    if (!m_serialPort) return;

    // Read all available data
    QByteArray data = m_serialPort->readAll();
    m_rxBuffer += QString::fromLatin1(data);

    // Process complete responses (terminated by ';')
    int idx;
    while ((idx = m_rxBuffer.indexOf(';')) != -1) {
        QString response = m_rxBuffer.left(idx + 1);
        m_rxBuffer = m_rxBuffer.mid(idx + 1);

        qDebug() << "KenwoodController: RX:" << response;
        processResponse(response);
    }
}

void KenwoodController::onPollTimer()
{
    if (!m_serialPort || !m_serialPort->isOpen()) {
        qWarning() << "KenwoodController: Poll timer fired but not connected - stopping";
        stopPolling();
        return;
    }

    // Polling pattern:
    // Phase 0: S-meter + Frequency
    // Phase 1: S-meter
    // Phase 2: S-meter
    // Phase 3: S-meter
    // Phase 4: S-meter + Mode
    // Repeat

    // Always request S-meter
    requestSMeter();

    // Request frequency every 5 cycles
    if (m_pollPhase % 5 == 0) {
        requestFrequency();
    }

    // Request mode every 5 cycles, offset by 2
    if (m_pollPhase % 5 == 2) {
        requestMode();
    }

    m_pollPhase = (m_pollPhase + 1) % 10;
}

void KenwoodController::onSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) {
        return;
    }

    QString errorMsg;
    switch (error) {
        case QSerialPort::DeviceNotFoundError:
            errorMsg = "Device not found";
            break;
        case QSerialPort::PermissionError:
            errorMsg = "Permission denied";
            break;
        case QSerialPort::OpenError:
            errorMsg = "Failed to open port";
            break;
        case QSerialPort::NotOpenError:
            errorMsg = "Port not open";
            break;
        case QSerialPort::WriteError:
            errorMsg = "Write error";
            break;
        case QSerialPort::ReadError:
            errorMsg = "Read error";
            break;
        case QSerialPort::ResourceError:
            errorMsg = "Resource error (device disconnected?)";
            disconnect();
            break;
        case QSerialPort::TimeoutError:
            errorMsg = "Timeout";
            break;
        default:
            errorMsg = QString("Unknown error (%1)").arg(static_cast<int>(error));
            break;
    }

    setError(errorMsg);
    qWarning() << "KenwoodController: Serial error -" << errorMsg;
}

void KenwoodController::setState(ConnectionState newState)
{
    if (m_state != newState) {
        m_state = newState;
        emit connectionStateChanged(newState);
    }
}

void KenwoodController::setError(const QString& error)
{
    m_lastError = error;
    emit errorOccurred(error);
}

void KenwoodController::sendCommand(const QString& cmd)
{
    if (!m_serialPort || !m_serialPort->isOpen()) {
        qWarning() << "KenwoodController: Cannot send command - port not open";
        return;
    }

    QByteArray data = cmd.toLatin1();
    qDebug() << "KenwoodController: TX:" << cmd;
    m_serialPort->write(data);
    m_serialPort->flush();
}

void KenwoodController::processResponse(const QString& response)
{
    // Check for error response (command followed by ?;)
    if (response.endsWith("?;")) {
        qWarning() << "KenwoodController: Command error:" << response;
        return;
    }

    // FA - Frequency response (FAnnnnnnnnnnn;)
    // Format: FA followed by up to 11 digits (frequency in Hz), then ;
    if (response.startsWith("FA") && response.length() >= 4) {
        QString freqStr = response.mid(2, response.length() - 3);  // Remove "FA" and ";"
        bool ok;
        uint64_t freq = freqStr.toULongLong(&ok);
        if (ok && freq > 0) {
            if (freq != m_currentFrequencyHz) {
                m_currentFrequencyHz = freq;
                qDebug() << "KenwoodController: Frequency:" << freq << "Hz";
                emit frequencyChanged(freq);
            }
        }
    }
    // MD - Mode response (MDn;)
    // Format: MD followed by mode digit (1-9), then ;
    else if (response.startsWith("MD") && response.length() >= 4) {
        QString modeStr = response.mid(2, 1);
        bool ok;
        int mode = modeStr.toInt(&ok);
        if (ok) {
            if (mode != m_currentMode) {
                m_currentMode = static_cast<uint8_t>(mode);
                m_currentModeName = modeToString(mode);
                qDebug() << "KenwoodController: Mode:" << mode << "=" << m_currentModeName;
                emit modeChanged(m_currentMode, m_currentModeName);
            }
        }
    }
    // SM - S-Meter response (SMnnnn;)
    // Format: SM followed by 4 digits (0000-0030 for Kenwood, 0000-0042 for Elecraft K4)
    else if (response.startsWith("SM") && response.length() >= 6) {
        QString smStr = response.mid(2, response.length() - 3);  // Remove "SM" and ";"
        bool ok;
        int rawSm = smStr.toInt(&ok);
        if (ok) {
            // Scale to 0-255 for compatibility with Icom S-meter
            // Kenwood uses 0-30, Elecraft K4 uses 0-42
            // Use 30 as baseline (most common)
            int scaled = qBound(0, rawSm * 255 / 30, 255);
            m_currentSMeter = scaled;
            emit smeterChanged(scaled);
        }
    }
    // IF - Information response (contains frequency, mode, etc.)
    else if (response.startsWith("IF") && response.length() >= 38) {
        // IF response format varies by radio, but frequency is typically at positions 2-12
        QString freqStr = response.mid(2, 11);
        bool ok;
        uint64_t freq = freqStr.toULongLong(&ok);
        if (ok && freq > 0 && freq != m_currentFrequencyHz) {
            m_currentFrequencyHz = freq;
            qDebug() << "KenwoodController: IF Frequency:" << freq << "Hz";
            emit frequencyChanged(freq);
        }
    }
}

QString KenwoodController::modeToString(int mode)
{
    // Kenwood/Elecraft mode values
    switch (mode) {
        case 1: return "LSB";
        case 2: return "USB";
        case 3: return "CW";
        case 4: return "FM";
        case 5: return "AM";
        case 6: return "DATA";
        case 7: return "CW-R";
        case 9: return "DATA-R";
        default: return "???";
    }
}
