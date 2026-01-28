#ifndef DEVICEINFO_H
#define DEVICEINFO_H

#include <QString>

/**
 * @brief Audio device information structure
 */
struct DeviceInfo {
    QString id;           // WASAPI device ID
    QString name;         // Friendly name
    int index;            // Index in enumeration
    int maxChannels;      // Maximum channel count
    int defaultSampleRate;// Default sample rate
    bool isLoopback;      // True if this is a loopback device

    DeviceInfo()
        : index(-1)
        , maxChannels(2)
        , defaultSampleRate(48000)
        , isLoopback(false)
    {}

    DeviceInfo(const QString& id, const QString& name, int index,
               int maxChannels = 2, int sampleRate = 48000, bool isLoopback = false)
        : id(id)
        , name(name)
        , index(index)
        , maxChannels(maxChannels)
        , defaultSampleRate(sampleRate)
        , isLoopback(isLoopback)
    {}

    bool isValid() const { return !id.isEmpty() && index >= 0; }
};

#endif // DEVICEINFO_H
