#include "audio/Recorder.h"
#include <QDir>
#include <QDateTime>
#include <QStorageInfo>
#include <QCoreApplication>
#include <QDebug>

Recorder::Recorder(int sampleRate, int channels)
    : m_sampleRate(sampleRate)
    , m_channels(channels)
{
    // Default recording directory next to the executable
    m_recordingDir = QCoreApplication::applicationDirPath() + "/recordings";
}

Recorder::~Recorder()
{
    if (m_recording.load()) {
        stopRecording();
    }
}

void Recorder::setRecordingDirectory(const QString& directory)
{
    m_recordingDir = directory;
}

QString Recorder::generateFilename() const
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    return QString("HamMixer_%1.wav").arg(timestamp);
}

QString Recorder::startRecording()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_recording.load()) {
        return m_currentFilename;
    }

    // Ensure directory exists
    QDir dir(m_recordingDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qWarning() << "Failed to create recording directory:" << m_recordingDir;
            return QString();
        }
    }

    // Generate filename
    m_currentFilename = generateFilename();
    QString fullPath = m_recordingDir + "/" + m_currentFilename;

    // Create file
    m_file = new QFile(fullPath);
    if (!m_file->open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to create recording file:" << fullPath;
        delete m_file;
        m_file = nullptr;
        return QString();
    }

    // Reset counters
    m_sampleCount.store(0);
    m_dataSize = 0;

    // Write WAV header (placeholder, will be updated on stop)
    writeWavHeader();

    m_recording.store(true);

    // Start elapsed timer for display
    m_elapsedTimer.start();

    qDebug() << "Started recording:" << fullPath;

    return m_currentFilename;
}

void Recorder::stopRecording()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_recording.load() || !m_file) {
        return;
    }

    m_recording.store(false);

    // Finalize WAV header with actual sizes
    finalizeWavHeader();

    m_file->close();
    delete m_file;
    m_file = nullptr;

    qDebug() << "Stopped recording:" << m_currentFilename
             << "Size:" << getFileSizeFormatted()
             << "Duration:" << getElapsedTimeFormatted();
}

void Recorder::writeSamples(const int16_t* samples, int frameCount)
{
    if (!m_recording.load() || !m_file) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_file->isOpen()) {
        return;
    }

    // Write samples
    qint64 bytesToWrite = frameCount * m_channels * sizeof(int16_t);
    qint64 bytesWritten = m_file->write(reinterpret_cast<const char*>(samples), bytesToWrite);

    if (bytesWritten > 0) {
        m_dataSize += bytesWritten;
        m_sampleCount.fetch_add(frameCount);
    }
}

void Recorder::writeWavHeader()
{
    // WAV header structure (44 bytes)
    struct WavHeader {
        // RIFF chunk
        char riffId[4] = {'R', 'I', 'F', 'F'};
        uint32_t riffSize = 0;  // Placeholder
        char waveId[4] = {'W', 'A', 'V', 'E'};

        // fmt chunk
        char fmtId[4] = {'f', 'm', 't', ' '};
        uint32_t fmtSize = 16;
        uint16_t audioFormat = 1;  // PCM
        uint16_t numChannels = 2;
        uint32_t sampleRate = 48000;
        uint32_t byteRate = 0;
        uint16_t blockAlign = 0;
        uint16_t bitsPerSample = 16;

        // data chunk
        char dataId[4] = {'d', 'a', 't', 'a'};
        uint32_t dataSize = 0;  // Placeholder
    };

    WavHeader header;
    header.numChannels = static_cast<uint16_t>(m_channels);
    header.sampleRate = static_cast<uint32_t>(m_sampleRate);
    header.bitsPerSample = BITS_PER_SAMPLE;
    header.blockAlign = header.numChannels * header.bitsPerSample / 8;
    header.byteRate = header.sampleRate * header.blockAlign;

    m_file->write(reinterpret_cast<const char*>(&header), sizeof(header));
}

void Recorder::finalizeWavHeader()
{
    if (!m_file || !m_file->isOpen()) {
        return;
    }

    // Calculate sizes
    uint32_t dataSize = static_cast<uint32_t>(m_dataSize);
    uint32_t riffSize = dataSize + 36;  // dataSize + header size - 8

    // Seek to RIFF size (offset 4)
    m_file->seek(4);
    m_file->write(reinterpret_cast<const char*>(&riffSize), sizeof(riffSize));

    // Seek to data size (offset 40)
    m_file->seek(40);
    m_file->write(reinterpret_cast<const char*>(&dataSize), sizeof(dataSize));
}

float Recorder::getElapsedTime() const
{
    if (!m_recording.load()) {
        return 0.0f;
    }
    return static_cast<float>(m_sampleCount.load()) / m_sampleRate;
}

QString Recorder::getElapsedTimeFormatted() const
{
    if (!m_recording.load()) {
        return "00:00:00";
    }

    // Use real-time elapsed timer for consistent display updates
    qint64 elapsedMs = m_elapsedTimer.elapsed();
    int totalSeconds = static_cast<int>(elapsedMs / 1000);
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int secs = totalSeconds % 60;

    return QString("%1:%2:%3")
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(secs, 2, 10, QChar('0'));
}

qint64 Recorder::getFileSize() const
{
    return m_dataSize + 44;  // Data + header
}

QString Recorder::getFileSizeFormatted() const
{
    qint64 size = getFileSize();

    if (size < 1024) {
        return QString("%1 B").arg(size);
    } else if (size < 1024 * 1024) {
        return QString("%1 KB").arg(size / 1024.0, 0, 'f', 1);
    } else if (size < 1024 * 1024 * 1024) {
        return QString("%1 MB").arg(size / (1024.0 * 1024.0), 0, 'f', 1);
    } else {
        return QString("%1 GB").arg(size / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
    }
}

qint64 Recorder::checkDiskSpace() const
{
    QStorageInfo storage(m_recordingDir);
    return storage.bytesAvailable() / (1024 * 1024);  // MB
}

bool Recorder::isDiskSpaceLow() const
{
    return checkDiskSpace() < 500;  // Less than 500MB
}
