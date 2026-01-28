#ifndef WASAPIDEVICE_H
#define WASAPIDEVICE_H

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>

#include <QString>
#include <QList>
#include <functional>
#include <thread>
#include <atomic>
#include <memory>

#include "audio/DeviceInfo.h"

/**
 * @brief WASAPI audio device wrapper
 *
 * Provides device enumeration and audio streaming for Windows Audio Session API.
 * Supports input, output, and loopback capture modes.
 */
class WasapiDevice {
public:
    // Audio callback function type
    // Parameters: buffer, frameCount, channels
    using AudioCallback = std::function<void(int16_t*, int, int)>;

    enum class DeviceType {
        Capture,    // Input device (microphone, line-in)
        Render,     // Output device (speakers, headphones)
        Loopback    // Loopback capture (system audio)
    };

    WasapiDevice();
    ~WasapiDevice();

    // Non-copyable
    WasapiDevice(const WasapiDevice&) = delete;
    WasapiDevice& operator=(const WasapiDevice&) = delete;

    /**
     * @brief Initialize COM for WASAPI
     * @return true if successful
     */
    static bool initializeCOM();

    /**
     * @brief Uninitialize COM
     */
    static void uninitializeCOM();

    /**
     * @brief Enumerate audio devices
     * @param type Device type to enumerate
     * @return List of available devices
     */
    static QList<DeviceInfo> enumerateDevices(DeviceType type);

    /**
     * @brief Open an audio device
     * @param deviceId WASAPI device ID
     * @param type Device type
     * @param sampleRate Requested sample rate
     * @param channels Requested channel count
     * @param bufferMs Buffer size in milliseconds
     * @return true if successful
     */
    bool open(const QString& deviceId, DeviceType type,
              int sampleRate = 48000, int channels = 2, int bufferMs = 20);

    /**
     * @brief Start audio streaming
     * @param callback Function called when audio data is available/needed
     * @return true if successful
     */
    bool start(AudioCallback callback);

    /**
     * @brief Stop audio streaming
     */
    void stop();

    /**
     * @brief Close the device
     */
    void close();

    /**
     * @brief Check if device is open
     */
    bool isOpen() const { return m_audioClient != nullptr; }

    /**
     * @brief Check if streaming is active
     */
    bool isRunning() const { return m_running.load(); }

    /**
     * @brief Get actual sample rate
     */
    int sampleRate() const { return m_sampleRate; }

    /**
     * @brief Get actual channel count
     */
    int channels() const { return m_channels; }

    /**
     * @brief Get buffer size in frames
     */
    int bufferFrames() const { return m_bufferFrames; }

    /**
     * @brief Get last error message
     */
    QString lastError() const { return m_lastError; }

private:
    // COM interfaces
    IMMDevice* m_device = nullptr;
    IAudioClient* m_audioClient = nullptr;
    IAudioCaptureClient* m_captureClient = nullptr;
    IAudioRenderClient* m_renderClient = nullptr;
    HANDLE m_eventHandle = nullptr;

    // Stream parameters
    DeviceType m_deviceType = DeviceType::Capture;
    int m_sampleRate = 48000;
    int m_channels = 2;
    int m_bufferFrames = 0;

    // Threading
    std::atomic<bool> m_running{false};
    std::unique_ptr<std::thread> m_streamThread;
    AudioCallback m_callback;

    QString m_lastError;

    // Internal methods
    void streamThreadFunc();
    void captureThread();
    void renderThread();
    bool initializeStream(WAVEFORMATEX* format, int bufferMs);
    void releaseResources();
};

#endif // WASAPIDEVICE_H
