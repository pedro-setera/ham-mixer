/*
 * CIVController.cpp
 *
 * CI-V Serial Communication Controller implementation
 * Part of HamMixer CT7BAC
 */

#include "CIVController.h"
#include "CIVProtocol.h"
#include <QDebug>

CIVController::CIVController(QObject* parent)
    : RadioController(parent)
    , m_serialPort(nullptr)
    , m_pollTimer(nullptr)
    , m_state(Disconnected)
    , m_currentFrequencyHz(0)  // Initialize to 0 so first read always triggers update
    , m_currentMode(0xFF)      // Invalid mode so first read always triggers update
    , m_currentModeName("---")
    , m_currentSMeter(0)
    , m_currentTxStatus(false) // Start assuming RX
    , m_radioModel()
    , m_pollPhase(0)
{
}

CIVController::~CIVController()
{
    disconnect();
}

bool CIVController::connect(const QString& portName, int baudRate)
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
                     this, &CIVController::onReadyRead);
    QObject::connect(m_serialPort, &QSerialPort::errorOccurred,
                     this, &CIVController::onSerialError);

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

    qDebug() << "CIVController: Connected to" << portName << "at" << baudRate << "baud";
    setState(Connected);

    // Request initial state (frequency and mode)
    requestFrequency();
    requestMode();

    return true;
}

void CIVController::disconnect()
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
    qDebug() << "CIVController: Disconnected";
}

QStringList CIVController::availablePorts()
{
    QStringList ports;
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo& info : infos) {
        ports.append(info.portName());
    }
    return ports;
}

QList<QSerialPortInfo> CIVController::availablePortsInfo()
{
    return QSerialPortInfo::availablePorts();
}

void CIVController::startPolling(int intervalMs)
{
    if (!m_serialPort || !m_serialPort->isOpen()) {
        qWarning() << "CIVController: Cannot start polling - not connected";
        return;
    }

    if (!m_pollTimer) {
        m_pollTimer = new QTimer(this);
        QObject::connect(m_pollTimer, &QTimer::timeout,
                         this, &CIVController::onPollTimer);
    }

    m_pollPhase = 0;
    m_pollTimer->start(intervalMs);
    qDebug() << "CIVController: Started polling at" << intervalMs << "ms interval";
}

void CIVController::stopPolling()
{
    if (m_pollTimer) {
        m_pollTimer->stop();
        qDebug() << "CIVController: Stopped polling";
    }
}

void CIVController::requestFrequency()
{
    sendCommand(CIVProtocol::buildCommand(CIVProtocol::CMD_READ_FREQ));
}

void CIVController::requestMode()
{
    sendCommand(CIVProtocol::buildCommand(CIVProtocol::CMD_READ_MODE));
}

void CIVController::requestSMeter()
{
    sendCommand(CIVProtocol::buildCommand(CIVProtocol::CMD_READ_METER,
                                          CIVProtocol::SUBCMD_SMETER));
}

void CIVController::requestTXStatus()
{
    sendCommand(CIVProtocol::buildCommand(CIVProtocol::CMD_TX_STATUS,
                                          CIVProtocol::SUBCMD_TX_STATE));
}

void CIVController::onReadyRead()
{
    if (!m_serialPort) return;

    // Read all available data
    QByteArray data = m_serialPort->readAll();

    // DEBUG: Log raw received data
    qDebug() << "CIVController: RX RAW (" << data.size() << "bytes):" << data.toHex(' ');

    m_rxBuffer.append(data);
    qDebug() << "CIVController: Buffer size now:" << m_rxBuffer.size() << "bytes";

    // Process any complete frames
    extractFrames();
}

void CIVController::onPollTimer()
{
    if (!m_serialPort) {
        qWarning() << "CIVController: Poll timer fired but serialPort is NULL - stopping polling";
        stopPolling();
        return;
    }

    if (!m_serialPort->isOpen()) {
        qWarning() << "CIVController: Poll timer fired but serialPort is not open - stopping polling";
        stopPolling();
        return;
    }

    // S-Meter and TX status are polled every cycle for real-time display
    // Frequency and Mode are polled less frequently (every 5th cycle each)
    // This gives ~10 S-meter/TX updates per second at 100ms interval
    //
    // Phase:  0   1   2   3   4   5   6   7   8   9   ...
    // SMeter: X   X   X   X   X   X   X   X   X   X   (every cycle)
    // TXstat: X   X   X   X   X   X   X   X   X   X   (every cycle)
    // Freq:   X               X               X       (every 5th, phase 0)
    // Mode:       X               X               X   (every 5th, phase 1)

    // Always request S-meter and TX status for real-time display
    requestSMeter();
    requestTXStatus();

    // Request frequency every 5 cycles (phase 0, 5, 10, ...)
    if (m_pollPhase % 5 == 0) {
        requestFrequency();
    }

    // Request mode every 5 cycles, offset by 1 (phase 1, 6, 11, ...)
    if (m_pollPhase % 5 == 1) {
        requestMode();
    }

    m_pollPhase = (m_pollPhase + 1) % 10;
}

void CIVController::onSerialError(QSerialPort::SerialPortError error)
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
    qWarning() << "CIVController: Serial error -" << errorMsg;
}

void CIVController::setState(ConnectionState newState)
{
    if (m_state != newState) {
        m_state = newState;
        emit connectionStateChanged(newState);
    }
}

void CIVController::setError(const QString& error)
{
    m_lastError = error;
    emit errorOccurred(error);
}

void CIVController::sendCommand(const QByteArray& data)
{
    if (!m_serialPort || !m_serialPort->isOpen()) {
        qWarning() << "CIVController: Cannot send command - port not open";
        return;
    }

    qDebug() << "CIVController: TX:" << data.toHex(' ');
    qint64 bytesWritten = m_serialPort->write(data);
    m_serialPort->flush();
    qDebug() << "CIVController: Wrote" << bytesWritten << "bytes";
}

bool CIVController::extractFrames()
{
    bool foundFrame = false;

    qDebug() << "CIVController: extractFrames() called, buffer:" << m_rxBuffer.toHex(' ');

    while (true) {
        // Find start of frame (FE FE)
        int startIdx = -1;
        for (int i = 0; i < m_rxBuffer.size() - 1; i++) {
            if (static_cast<uint8_t>(m_rxBuffer[i]) == CIVProtocol::PREAMBLE &&
                static_cast<uint8_t>(m_rxBuffer[i + 1]) == CIVProtocol::PREAMBLE) {
                startIdx = i;
                qDebug() << "CIVController: Found FE FE preamble at index" << i;
                break;
            }
        }

        if (startIdx < 0) {
            // No frame start found, clear buffer up to last byte
            qDebug() << "CIVController: No FE FE preamble found in buffer";
            if (m_rxBuffer.size() > 1) {
                qDebug() << "CIVController: Trimming buffer, discarding" << (m_rxBuffer.size() - 1) << "bytes";
                m_rxBuffer = m_rxBuffer.right(1);
            }
            break;
        }

        // Discard any garbage before frame start
        if (startIdx > 0) {
            qDebug() << "CIVController: Discarding" << startIdx << "bytes of garbage before frame";
            m_rxBuffer = m_rxBuffer.mid(startIdx);
        }

        // Find end of frame (FD)
        int endIdx = -1;
        for (int i = 2; i < m_rxBuffer.size(); i++) {
            if (static_cast<uint8_t>(m_rxBuffer[i]) == CIVProtocol::EOM) {
                endIdx = i;
                qDebug() << "CIVController: Found FD (end of message) at index" << i;
                break;
            }
        }

        if (endIdx < 0) {
            // Frame not complete yet
            qDebug() << "CIVController: No FD found - frame incomplete, waiting for more data";
            break;
        }

        // Extract the frame
        QByteArray frame = m_rxBuffer.left(endIdx + 1);
        m_rxBuffer = m_rxBuffer.mid(endIdx + 1);

        qDebug() << "CIVController: Extracted frame:" << frame.toHex(' ');

        // Process the frame
        if (CIVProtocol::isValidFrame(frame)) {
            qDebug() << "CIVController: Frame is valid, processing...";
            processFrame(frame);
            foundFrame = true;
        } else {
            qWarning() << "CIVController: Frame failed validation:" << frame.toHex(' ');
        }
    }

    return foundFrame;
}

void CIVController::processFrame(const QByteArray& frame)
{
    uint8_t srcAddr = CIVProtocol::getSourceAddress(frame);
    uint8_t dstAddr = CIVProtocol::getDestAddress(frame);

    qDebug() << "CIVController: processFrame - src:" << QString("0x%1").arg(srcAddr, 2, 16, QChar('0'))
             << "dst:" << QString("0x%1").arg(dstAddr, 2, 16, QChar('0'));

    // Detect radio model from source address (only once)
    if (m_radioModel.isEmpty()) {
        QString detectedModel = CIVProtocol::addressToModelName(srcAddr);
        if (!detectedModel.isEmpty()) {
            m_radioModel = detectedModel;
            qDebug() << "CIVController: Detected radio model:" << m_radioModel;
            emit radioModelDetected(m_radioModel);
        }
    }

    // Process frames from known Icom radios - accept any valid CI-V source
    // Check if source is a known Icom address
    QString modelCheck = CIVProtocol::addressToModelName(srcAddr);
    if (modelCheck.isEmpty()) {
        qDebug() << "CIVController: Ignoring frame from unknown source";
        return;
    }

    // Skip scope/waveform data (command 0x27) - it's high-frequency noise
    uint8_t cmdCheck = CIVProtocol::getCommand(frame);
    if (cmdCheck == 0x27) {
        // Scope data, ignore silently
        return;
    }

    uint8_t cmd = CIVProtocol::getCommand(frame);
    QByteArray data = CIVProtocol::getData(frame);

    qDebug() << "CIVController: Command:" << QString("0x%1").arg(cmd, 2, 16, QChar('0'))
             << "Data:" << data.toHex(' ') << "(" << data.size() << "bytes)";

    switch (cmd) {
        case CIVProtocol::CMD_READ_FREQ:
        case CIVProtocol::CMD_TRANSCEIVE_FREQ:
        {
            qDebug() << "CIVController: Processing FREQUENCY command";
            // Frequency response: 5 BCD bytes
            if (data.size() >= 5) {
                uint64_t freq = CIVProtocol::parseFrequency(data);
                qDebug() << "CIVController: Parsed frequency:" << freq << "Hz"
                         << "(current:" << m_currentFrequencyHz << "Hz)";
                if (freq != m_currentFrequencyHz) {
                    m_currentFrequencyHz = freq;
                    qDebug() << "CIVController: EMITTING frequencyChanged signal:" << freq;
                    emit frequencyChanged(freq);
                } else {
                    qDebug() << "CIVController: Frequency unchanged, not emitting signal";
                }
            } else {
                qWarning() << "CIVController: Frequency data too short:" << data.size() << "bytes";
            }
            break;
        }

        case CIVProtocol::CMD_READ_MODE:
        case CIVProtocol::CMD_TRANSCEIVE_MODE:
        {
            qDebug() << "CIVController: Processing MODE command";
            // Mode response: 1-2 bytes (mode, optional filter)
            if (!data.isEmpty()) {
                uint8_t mode = CIVProtocol::parseMode(data);
                qDebug() << "CIVController: Parsed mode:" << QString("0x%1").arg(mode, 2, 16, QChar('0'))
                         << "(current:" << QString("0x%1").arg(m_currentMode, 2, 16, QChar('0')) << ")";
                if (mode != m_currentMode) {
                    m_currentMode = mode;
                    m_currentModeName = CIVProtocol::modeToString(mode);
                    qDebug() << "CIVController: EMITTING modeChanged signal:" << m_currentModeName;
                    emit modeChanged(mode, m_currentModeName);
                } else {
                    qDebug() << "CIVController: Mode unchanged, not emitting signal";
                }
            } else {
                qWarning() << "CIVController: Mode data is empty";
            }
            break;
        }

        case CIVProtocol::CMD_READ_METER:
        {
            // S-meter response: subcommand + 2 BCD bytes
            // Frame data format: [subcmd] [BCD high] [BCD low]
            qDebug() << "CIVController: Processing METER command, data size:" << data.size()
                     << "data:" << data.toHex(' ');

            if (data.size() >= 3) {
                uint8_t subcmd = static_cast<uint8_t>(data[0]);
                if (subcmd == CIVProtocol::SUBCMD_SMETER) {
                    int smeter = CIVProtocol::parseSMeter(data.mid(1));
                    qDebug() << "CIVController: S-meter raw value:" << smeter
                             << "dB:" << CIVProtocol::smeterToDb(smeter);
                    m_currentSMeter = smeter;
                    emit smeterChanged(smeter);
                } else {
                    qDebug() << "CIVController: Meter subcmd is" << QString("0x%1").arg(subcmd, 2, 16, QChar('0'))
                             << "(not S-meter 0x02)";
                }
            } else {
                qWarning() << "CIVController: Meter data too short:" << data.size() << "bytes (need 3)";
            }
            break;
        }

        case CIVProtocol::CMD_TX_STATUS:
        {
            // TX status response: subcommand + state byte
            // Format: [subcmd 0x00] [state: 0x00=RX, 0x01=TX]
            qDebug() << "CIVController: Processing TX_STATUS command, data size:" << data.size()
                     << "data:" << data.toHex(' ');

            if (data.size() >= 2) {
                uint8_t subcmd = static_cast<uint8_t>(data[0]);
                if (subcmd == CIVProtocol::SUBCMD_TX_STATE) {
                    bool txActive = (static_cast<uint8_t>(data[1]) == 0x01);
                    if (txActive != m_currentTxStatus) {
                        m_currentTxStatus = txActive;
                        qDebug() << "CIVController: TX status changed to:" << (txActive ? "TX" : "RX");
                        emit txStatusChanged(txActive);
                    }
                }
            }
            break;
        }

        case CIVProtocol::CMD_OK:
            qDebug() << "CIVController: Received OK (FB) acknowledgment";
            break;

        case CIVProtocol::CMD_NG:
            qWarning() << "CIVController: Command rejected by radio (NG)";
            break;

        default:
            qDebug() << "CIVController: Unknown/unhandled command:" << QString("0x%1").arg(cmd, 2, 16, QChar('0'));
            break;
    }
}
