#ifndef AUDIOMANAGER_H
#define AUDIOMANAGER_H

#include <QObject>
#include <QString>
#include <QList>
#include <memory>
#include <atomic>
#include <mutex>

#include "audio/DeviceInfo.h"
#include "audio/WasapiDevice.h"
#include "audio/RingBuffer.h"
#include "audio/MixerCore.h"
#include "audio/Recorder.h"

/**
 * @brief Audio stream manager for HamMixer
 *
 * Manages three WASAPI audio streams:
 * - Radio input (IC-7300 or other audio device)
 * - Loopback capture (WebSDR system audio)
 * - Output (mixed audio to speakers/headphones)
 */
class AudioManager : public QObject {
    Q_OBJECT

public:
    static constexpr int SAMPLE_RATE = 48000;
    static constexpr int CHANNELS = 2;
    static constexpr int BUFFER_SIZE = 1024;
    static constexpr int RING_BUFFER_SIZE = 4096;

    explicit AudioManager(QObject* parent = nullptr);
    ~AudioManager();

    /**
     * @brief Initialize the audio system
     * @return true if successful
     */
    bool initialize();

    /**
     * @brief Shutdown the audio system
     */
    void shutdown();

    /**
     * @brief Get list of available input devices
     */
    QList<DeviceInfo> getInputDevices() const { return m_inputDevices; }

    /**
     * @brief Get list of available loopback devices
     */
    QList<DeviceInfo> getLoopbackDevices() const { return m_loopbackDevices; }

    /**
     * @brief Get list of available output devices
     */
    QList<DeviceInfo> getOutputDevices() const { return m_outputDevices; }

    /**
     * @brief Refresh device lists
     */
    void refreshDevices();

    /**
     * @brief Start audio streams
     * @param inputDeviceId Radio input device ID
     * @param loopbackDeviceId System audio loopback device ID
     * @param outputDeviceId Output device ID
     * @return true if successful
     */
    bool startStreams(const QString& inputDeviceId,
                      const QString& loopbackDeviceId,
                      const QString& outputDeviceId);

    /**
     * @brief Stop all audio streams
     */
    void stopStreams();

    /**
     * @brief Check if streams are running
     */
    bool isRunning() const { return m_running.load(); }

    /**
     * @brief Get the mixer core for DSP control
     */
    MixerCore* mixer() { return m_mixer.get(); }

    /**
     * @brief Get the recorder
     */
    Recorder* recorder() { return m_recorder.get(); }

    /**
     * @brief Get last error message
     */
    QString lastError() const { return m_lastError; }

signals:
    void errorOccurred(const QString& error);
    void streamsStarted();
    void streamsStopped();

private:
    // Devices
    QList<DeviceInfo> m_inputDevices;
    QList<DeviceInfo> m_loopbackDevices;
    QList<DeviceInfo> m_outputDevices;

    // WASAPI devices
    std::unique_ptr<WasapiDevice> m_inputDevice;
    std::unique_ptr<WasapiDevice> m_loopbackDevice;
    std::unique_ptr<WasapiDevice> m_outputDevice;

    // Ring buffers for inter-stream communication
    std::unique_ptr<RingBuffer> m_radioRing;
    std::unique_ptr<RingBuffer> m_loopbackRing;

    // Audio processing
    std::unique_ptr<MixerCore> m_mixer;
    std::unique_ptr<Recorder> m_recorder;

    // State
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_initialized{false};
    QString m_lastError;
    std::mutex m_mutex;

    // Callback handlers
    void onRadioInput(int16_t* data, int frames, int channels);
    void onLoopbackInput(int16_t* data, int frames, int channels);
    void onOutputNeeded(int16_t* data, int frames, int channels);
};

#endif // AUDIOMANAGER_H
