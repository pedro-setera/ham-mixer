#include "audio/WasapiDevice.h"
#include <QDebug>
#include <comdef.h>
#include <Audioclient.h>
#include <avrt.h>

#pragma comment(lib, "avrt.lib")

// Define PKEY_Device_FriendlyName manually for MinGW compatibility
// This avoids linker issues with the Windows SDK property keys
static const PROPERTYKEY LocalPKEY_Device_FriendlyName = {
    { 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } },
    14
};

// GUID for loopback capture
static const GUID CLSID_MMDeviceEnumerator_Local = __uuidof(MMDeviceEnumerator);
static const GUID IID_IMMDeviceEnumerator_Local = __uuidof(IMMDeviceEnumerator);
static const GUID IID_IAudioClient_Local = __uuidof(IAudioClient);
static const GUID IID_IAudioCaptureClient_Local = __uuidof(IAudioCaptureClient);
static const GUID IID_IAudioRenderClient_Local = __uuidof(IAudioRenderClient);

WasapiDevice::WasapiDevice()
{
}

WasapiDevice::~WasapiDevice()
{
    close();
}

bool WasapiDevice::initializeCOM()
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        qWarning() << "Failed to initialize COM:" << hr;
        return false;
    }
    return true;
}

void WasapiDevice::uninitializeCOM()
{
    CoUninitialize();
}

QList<DeviceInfo> WasapiDevice::enumerateDevices(DeviceType type)
{
    QList<DeviceInfo> devices;

    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr = CoCreateInstance(
        CLSID_MMDeviceEnumerator_Local,
        nullptr,
        CLSCTX_ALL,
        IID_IMMDeviceEnumerator_Local,
        (void**)&enumerator
    );

    if (FAILED(hr) || !enumerator) {
        qWarning() << "Failed to create device enumerator:" << hr;
        return devices;
    }

    // Determine data flow direction
    EDataFlow dataFlow;
    bool isLoopback = false;

    switch (type) {
        case DeviceType::Capture:
            dataFlow = eCapture;
            break;
        case DeviceType::Render:
        case DeviceType::Loopback:
            dataFlow = eRender;
            isLoopback = (type == DeviceType::Loopback);
            break;
    }

    // Enumerate devices
    IMMDeviceCollection* collection = nullptr;
    hr = enumerator->EnumAudioEndpoints(dataFlow, DEVICE_STATE_ACTIVE, &collection);

    if (FAILED(hr) || !collection) {
        enumerator->Release();
        return devices;
    }

    UINT count = 0;
    collection->GetCount(&count);

    for (UINT i = 0; i < count; i++) {
        IMMDevice* device = nullptr;
        if (SUCCEEDED(collection->Item(i, &device))) {
            // Get device ID
            LPWSTR deviceId = nullptr;
            device->GetId(&deviceId);

            // Get device properties
            IPropertyStore* props = nullptr;
            device->OpenPropertyStore(STGM_READ, &props);

            QString name;
            if (props) {
                PROPVARIANT varName;
                PropVariantInit(&varName);
                if (SUCCEEDED(props->GetValue(LocalPKEY_Device_FriendlyName, &varName))) {
                    name = QString::fromWCharArray(varName.pwszVal);
                }
                PropVariantClear(&varName);
                props->Release();
            }

            // Get default format info
            IAudioClient* audioClient = nullptr;
            int sampleRate = 48000;
            int maxChannels = 2;

            if (SUCCEEDED(device->Activate(IID_IAudioClient_Local, CLSCTX_ALL, nullptr, (void**)&audioClient))) {
                WAVEFORMATEX* format = nullptr;
                if (SUCCEEDED(audioClient->GetMixFormat(&format))) {
                    sampleRate = format->nSamplesPerSec;
                    maxChannels = format->nChannels;
                    CoTaskMemFree(format);
                }
                audioClient->Release();
            }

            DeviceInfo info;
            info.id = QString::fromWCharArray(deviceId);
            info.name = name;
            info.index = static_cast<int>(i);
            info.maxChannels = maxChannels;
            info.defaultSampleRate = sampleRate;
            info.isLoopback = isLoopback;

            devices.append(info);

            CoTaskMemFree(deviceId);
            device->Release();
        }
    }

    collection->Release();
    enumerator->Release();

    return devices;
}

bool WasapiDevice::open(const QString& deviceId, DeviceType type,
                        int sampleRate, int channels, int bufferMs)
{
    close();

    m_deviceType = type;
    m_sampleRate = sampleRate;
    m_channels = channels;

    // Create device enumerator
    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr = CoCreateInstance(
        CLSID_MMDeviceEnumerator_Local,
        nullptr,
        CLSCTX_ALL,
        IID_IMMDeviceEnumerator_Local,
        (void**)&enumerator
    );

    if (FAILED(hr)) {
        m_lastError = QString("Failed to create device enumerator: %1").arg(hr);
        return false;
    }

    // Get device by ID
    hr = enumerator->GetDevice(reinterpret_cast<LPCWSTR>(deviceId.utf16()), &m_device);
    enumerator->Release();

    if (FAILED(hr) || !m_device) {
        m_lastError = QString("Failed to get device: %1").arg(hr);
        return false;
    }

    // Activate audio client
    hr = m_device->Activate(IID_IAudioClient_Local, CLSCTX_ALL, nullptr, (void**)&m_audioClient);
    if (FAILED(hr) || !m_audioClient) {
        m_lastError = QString("Failed to activate audio client: %1").arg(hr);
        releaseResources();
        return false;
    }

    // Set up wave format
    WAVEFORMATEX format = {};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = static_cast<WORD>(channels);
    format.nSamplesPerSec = static_cast<DWORD>(sampleRate);
    format.wBitsPerSample = 16;
    format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
    format.cbSize = 0;

    if (!initializeStream(&format, bufferMs)) {
        releaseResources();
        return false;
    }

    // Create event for audio buffer notifications
    m_eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_eventHandle) {
        m_lastError = "Failed to create event handle";
        releaseResources();
        return false;
    }

    hr = m_audioClient->SetEventHandle(m_eventHandle);
    if (FAILED(hr)) {
        m_lastError = QString("Failed to set event handle: %1").arg(hr);
        releaseResources();
        return false;
    }

    // Get capture or render client
    if (type == DeviceType::Capture || type == DeviceType::Loopback) {
        hr = m_audioClient->GetService(IID_IAudioCaptureClient_Local, (void**)&m_captureClient);
        if (FAILED(hr)) {
            m_lastError = QString("Failed to get capture client: %1").arg(hr);
            releaseResources();
            return false;
        }
    } else {
        hr = m_audioClient->GetService(IID_IAudioRenderClient_Local, (void**)&m_renderClient);
        if (FAILED(hr)) {
            m_lastError = QString("Failed to get render client: %1").arg(hr);
            releaseResources();
            return false;
        }
    }

    return true;
}

bool WasapiDevice::initializeStream(WAVEFORMATEX* format, int bufferMs)
{
    REFERENCE_TIME bufferDuration = static_cast<REFERENCE_TIME>(bufferMs) * 10000;

    DWORD streamFlags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
    if (m_deviceType == DeviceType::Loopback) {
        streamFlags |= AUDCLNT_STREAMFLAGS_LOOPBACK;
    }

    HRESULT hr = m_audioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        streamFlags,
        bufferDuration,
        0,
        format,
        nullptr
    );

    if (FAILED(hr)) {
        // Try with mix format
        WAVEFORMATEX* mixFormat = nullptr;
        if (SUCCEEDED(m_audioClient->GetMixFormat(&mixFormat))) {
            // Update our format to match mix format but keep PCM 16-bit
            format->nSamplesPerSec = mixFormat->nSamplesPerSec;
            format->nChannels = mixFormat->nChannels;
            format->nBlockAlign = format->nChannels * format->wBitsPerSample / 8;
            format->nAvgBytesPerSec = format->nSamplesPerSec * format->nBlockAlign;

            m_sampleRate = format->nSamplesPerSec;
            m_channels = format->nChannels;

            CoTaskMemFree(mixFormat);

            hr = m_audioClient->Initialize(
                AUDCLNT_SHAREMODE_SHARED,
                streamFlags,
                bufferDuration,
                0,
                format,
                nullptr
            );
        }
    }

    if (FAILED(hr)) {
        m_lastError = QString("Failed to initialize audio client: %1").arg(hr);
        return false;
    }

    // Get actual buffer size
    UINT32 bufferFrames = 0;
    m_audioClient->GetBufferSize(&bufferFrames);
    m_bufferFrames = static_cast<int>(bufferFrames);

    return true;
}

bool WasapiDevice::start(AudioCallback callback)
{
    if (!m_audioClient) {
        m_lastError = "Device not open";
        return false;
    }

    if (m_running.load()) {
        return true; // Already running
    }

    m_callback = callback;
    m_running.store(true);

    // Start the audio client
    HRESULT hr = m_audioClient->Start();
    if (FAILED(hr)) {
        m_lastError = QString("Failed to start audio client: %1").arg(hr);
        m_running.store(false);
        return false;
    }

    // Start stream thread
    m_streamThread = std::make_unique<std::thread>(&WasapiDevice::streamThreadFunc, this);

    return true;
}

void WasapiDevice::stop()
{
    m_running.store(false);

    if (m_streamThread && m_streamThread->joinable()) {
        // Signal the event to wake up the thread
        if (m_eventHandle) {
            SetEvent(m_eventHandle);
        }
        m_streamThread->join();
    }
    m_streamThread.reset();

    if (m_audioClient) {
        m_audioClient->Stop();
        m_audioClient->Reset();
    }
}

void WasapiDevice::close()
{
    stop();
    releaseResources();
}

void WasapiDevice::releaseResources()
{
    if (m_captureClient) {
        m_captureClient->Release();
        m_captureClient = nullptr;
    }
    if (m_renderClient) {
        m_renderClient->Release();
        m_renderClient = nullptr;
    }
    if (m_audioClient) {
        m_audioClient->Release();
        m_audioClient = nullptr;
    }
    if (m_device) {
        m_device->Release();
        m_device = nullptr;
    }
    if (m_eventHandle) {
        CloseHandle(m_eventHandle);
        m_eventHandle = nullptr;
    }
}

void WasapiDevice::streamThreadFunc()
{
    // Boost thread priority for audio
    DWORD taskIndex = 0;
    HANDLE hTask = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);

    if (m_deviceType == DeviceType::Render) {
        renderThread();
    } else {
        captureThread();
    }

    if (hTask) {
        AvRevertMmThreadCharacteristics(hTask);
    }
}

void WasapiDevice::captureThread()
{
    while (m_running.load()) {
        DWORD waitResult = WaitForSingleObject(m_eventHandle, 100);

        if (!m_running.load()) break;

        if (waitResult == WAIT_OBJECT_0) {
            UINT32 packetLength = 0;

            while (SUCCEEDED(m_captureClient->GetNextPacketSize(&packetLength)) && packetLength > 0) {
                BYTE* data = nullptr;
                UINT32 framesAvailable = 0;
                DWORD flags = 0;

                HRESULT hr = m_captureClient->GetBuffer(&data, &framesAvailable, &flags, nullptr, nullptr);

                if (SUCCEEDED(hr) && data && framesAvailable > 0) {
                    if (m_callback) {
                        if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                            // Fill with silence
                            std::vector<int16_t> silence(framesAvailable * m_channels, 0);
                            m_callback(silence.data(), framesAvailable, m_channels);
                        } else {
                            m_callback(reinterpret_cast<int16_t*>(data), framesAvailable, m_channels);
                        }
                    }

                    m_captureClient->ReleaseBuffer(framesAvailable);
                }

                if (!m_running.load()) break;
            }
        }
    }
}

void WasapiDevice::renderThread()
{
    // Pre-fill buffer with silence
    UINT32 bufferFrames = 0;
    m_audioClient->GetBufferSize(&bufferFrames);

    BYTE* data = nullptr;
    if (SUCCEEDED(m_renderClient->GetBuffer(bufferFrames, &data))) {
        memset(data, 0, bufferFrames * m_channels * sizeof(int16_t));
        m_renderClient->ReleaseBuffer(bufferFrames, 0);
    }

    while (m_running.load()) {
        DWORD waitResult = WaitForSingleObject(m_eventHandle, 100);

        if (!m_running.load()) break;

        if (waitResult == WAIT_OBJECT_0) {
            UINT32 padding = 0;
            m_audioClient->GetCurrentPadding(&padding);

            UINT32 framesAvailable = bufferFrames - padding;

            if (framesAvailable > 0) {
                BYTE* buffer = nullptr;
                HRESULT hr = m_renderClient->GetBuffer(framesAvailable, &buffer);

                if (SUCCEEDED(hr) && buffer) {
                    if (m_callback) {
                        m_callback(reinterpret_cast<int16_t*>(buffer), framesAvailable, m_channels);
                    } else {
                        memset(buffer, 0, framesAvailable * m_channels * sizeof(int16_t));
                    }

                    m_renderClient->ReleaseBuffer(framesAvailable, 0);
                }
            }
        }
    }
}
