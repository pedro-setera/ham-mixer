#include "audio/AudioManager.h"
#include <QDebug>

AudioManager::AudioManager(QObject* parent)
    : QObject(parent)
{
}

AudioManager::~AudioManager()
{
    shutdown();
}

bool AudioManager::initialize()
{
    if (m_initialized.load()) {
        return true;
    }

    // Initialize COM for WASAPI
    if (!WasapiDevice::initializeCOM()) {
        m_lastError = "Failed to initialize COM";
        return false;
    }

    // Create components
    m_mixer = std::make_unique<MixerCore>(SAMPLE_RATE, BUFFER_SIZE);
    m_recorder = std::make_unique<Recorder>(SAMPLE_RATE, CHANNELS);

    // Create ring buffers
    m_radioRing = std::make_unique<RingBuffer>(RING_BUFFER_SIZE, CHANNELS);
    m_loopbackRing = std::make_unique<RingBuffer>(RING_BUFFER_SIZE, CHANNELS);

    // Enumerate devices
    refreshDevices();

    m_initialized.store(true);
    return true;
}

void AudioManager::shutdown()
{
    stopStreams();

    m_mixer.reset();
    m_recorder.reset();
    m_radioRing.reset();
    m_loopbackRing.reset();

    if (m_initialized.load()) {
        WasapiDevice::uninitializeCOM();
        m_initialized.store(false);
    }
}

void AudioManager::refreshDevices()
{
    m_inputDevices = WasapiDevice::enumerateDevices(WasapiDevice::DeviceType::Capture);
    m_loopbackDevices = WasapiDevice::enumerateDevices(WasapiDevice::DeviceType::Loopback);
    m_outputDevices = WasapiDevice::enumerateDevices(WasapiDevice::DeviceType::Render);

    qDebug() << "Found" << m_inputDevices.size() << "input devices";
    qDebug() << "Found" << m_loopbackDevices.size() << "loopback devices";
    qDebug() << "Found" << m_outputDevices.size() << "output devices";
}

bool AudioManager::startStreams(const QString& inputDeviceId,
                                const QString& loopbackDeviceId,
                                const QString& outputDeviceId)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_running.load()) {
        return true;
    }

    // Clear ring buffers
    m_radioRing->clear();
    m_loopbackRing->clear();

    // Reset mixer
    m_mixer->reset();

    // Create and open devices
    m_inputDevice = std::make_unique<WasapiDevice>();
    m_loopbackDevice = std::make_unique<WasapiDevice>();
    m_outputDevice = std::make_unique<WasapiDevice>();

    // Open input device (radio)
    if (!inputDeviceId.isEmpty()) {
        if (!m_inputDevice->open(inputDeviceId, WasapiDevice::DeviceType::Capture,
                                  SAMPLE_RATE, CHANNELS, 10)) {
            m_lastError = "Failed to open input device: " + m_inputDevice->lastError();
            qWarning() << m_lastError;
            emit errorOccurred(m_lastError);
            return false;
        }
        qDebug() << "Radio input device opened at" << m_inputDevice->sampleRate() << "Hz,"
                 << m_inputDevice->channels() << "channels";
        if (m_inputDevice->sampleRate() != SAMPLE_RATE) {
            qWarning() << "WARNING: Radio device sample rate mismatch! Expected" << SAMPLE_RATE
                       << "Hz, got" << m_inputDevice->sampleRate() << "Hz - audio may be distorted";
        }
    }

    // Open loopback device (WebSDR)
    if (!loopbackDeviceId.isEmpty()) {
        if (!m_loopbackDevice->open(loopbackDeviceId, WasapiDevice::DeviceType::Loopback,
                                     SAMPLE_RATE, CHANNELS, 10)) {
            m_lastError = "Failed to open loopback device: " + m_loopbackDevice->lastError();
            qWarning() << m_lastError;
            emit errorOccurred(m_lastError);
            return false;
        }
        qDebug() << "Loopback device opened at" << m_loopbackDevice->sampleRate() << "Hz,"
                 << m_loopbackDevice->channels() << "channels";
        if (m_loopbackDevice->sampleRate() != SAMPLE_RATE) {
            qWarning() << "WARNING: Loopback device sample rate mismatch! Expected" << SAMPLE_RATE
                       << "Hz, got" << m_loopbackDevice->sampleRate() << "Hz";
        }
    }

    // Open output device
    if (!outputDeviceId.isEmpty()) {
        if (!m_outputDevice->open(outputDeviceId, WasapiDevice::DeviceType::Render,
                                   SAMPLE_RATE, CHANNELS, 10)) {
            m_lastError = "Failed to open output device: " + m_outputDevice->lastError();
            qWarning() << m_lastError;
            emit errorOccurred(m_lastError);
            return false;
        }
        qDebug() << "Output device opened at" << m_outputDevice->sampleRate() << "Hz,"
                 << m_outputDevice->channels() << "channels";
        if (m_outputDevice->sampleRate() != SAMPLE_RATE) {
            qWarning() << "WARNING: Output device sample rate mismatch! Expected" << SAMPLE_RATE
                       << "Hz, got" << m_outputDevice->sampleRate() << "Hz";
            qWarning() << "Please set your Windows audio device to 48000 Hz (DVD Quality)";
        }
        // Update recorder sample rate to match actual output device rate
        if (m_recorder) {
            m_recorder->setSampleRate(m_outputDevice->sampleRate());
            qDebug() << "Recorder sample rate set to" << m_outputDevice->sampleRate() << "Hz";
        }
    }

    // Start input stream
    if (m_inputDevice->isOpen()) {
        if (!m_inputDevice->start([this](int16_t* data, int frames, int channels) {
            onRadioInput(data, frames, channels);
        })) {
            m_lastError = "Failed to start input stream: " + m_inputDevice->lastError();
            emit errorOccurred(m_lastError);
            return false;
        }
    }

    // Start loopback stream
    if (m_loopbackDevice->isOpen()) {
        if (!m_loopbackDevice->start([this](int16_t* data, int frames, int channels) {
            onLoopbackInput(data, frames, channels);
        })) {
            m_lastError = "Failed to start loopback stream: " + m_loopbackDevice->lastError();
            emit errorOccurred(m_lastError);
            return false;
        }
    }

    // Start output stream
    if (m_outputDevice->isOpen()) {
        if (!m_outputDevice->start([this](int16_t* data, int frames, int channels) {
            onOutputNeeded(data, frames, channels);
        })) {
            m_lastError = "Failed to start output stream: " + m_outputDevice->lastError();
            emit errorOccurred(m_lastError);
            return false;
        }
    }

    m_running.store(true);
    emit streamsStarted();
    qDebug() << "Audio streams started";

    return true;
}

void AudioManager::stopStreams()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_running.load()) {
        return;
    }

    // Stop recording if active
    if (m_recorder && m_recorder->isRecording()) {
        m_recorder->stopRecording();
    }

    // Stop and close devices
    if (m_outputDevice) {
        m_outputDevice->close();
        m_outputDevice.reset();
    }
    if (m_loopbackDevice) {
        m_loopbackDevice->close();
        m_loopbackDevice.reset();
    }
    if (m_inputDevice) {
        m_inputDevice->close();
        m_inputDevice.reset();
    }

    m_running.store(false);
    emit streamsStopped();
    qDebug() << "Audio streams stopped";
}

void AudioManager::onRadioInput(int16_t* data, int frames, int channels)
{
    if (!m_running.load()) return;

    // Handle channel mismatch
    if (channels == 1) {
        // Convert mono to stereo
        std::vector<int16_t> stereo(frames * 2);
        for (int i = 0; i < frames; i++) {
            stereo[i * 2] = data[i];
            stereo[i * 2 + 1] = data[i];
        }
        m_radioRing->write(stereo.data(), frames);
    } else {
        m_radioRing->write(data, frames);
    }
}

void AudioManager::onLoopbackInput(int16_t* data, int frames, int channels)
{
    if (!m_running.load()) return;

    // Handle channel mismatch
    if (channels == 1) {
        // Convert mono to stereo
        std::vector<int16_t> stereo(frames * 2);
        for (int i = 0; i < frames; i++) {
            stereo[i * 2] = data[i];
            stereo[i * 2 + 1] = data[i];
        }
        m_loopbackRing->write(stereo.data(), frames);
    } else {
        m_loopbackRing->write(data, frames);
    }
}

void AudioManager::onOutputNeeded(int16_t* data, int frames, int channels)
{
    if (!m_running.load()) {
        memset(data, 0, frames * channels * sizeof(int16_t));
        return;
    }

    // Read from ring buffers
    std::vector<int16_t> radioData(frames * 2);
    std::vector<int16_t> loopbackData(frames * 2);

    m_radioRing->read(radioData.data(), frames);
    m_loopbackRing->read(loopbackData.data(), frames);

    // Process through mixer
    m_mixer->process(radioData.data(), loopbackData.data(), data, frames);

    // Record if active
    if (m_recorder && m_recorder->isRecording()) {
        m_recorder->writeSamples(data, frames);
    }
}
