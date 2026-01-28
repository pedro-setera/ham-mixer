#ifndef SETTINGS_H
#define SETTINGS_H

#include <QString>
#include <QJsonObject>
#include <QPoint>
#include <QSize>
#include <QList>

#include "websdr/WebSdrSite.h"

/**
 * @brief Application settings manager
 *
 * Handles loading/saving application configuration to JSON file
 * in the user's AppData directory.
 */
class Settings {
public:
    // Device settings
    struct DeviceSettings {
        QString radioInput;
        QString systemLoopback;
        QString output;
    };

    // Channel settings
    struct ChannelSettings {
        int volume = 100;     // 0-150
        int pan = 0;          // -100 to +100
        int delayMs = 300;    // 0-600 (channel 1 only)
        bool muted = false;
    };

    // Master settings
    struct MasterSettings {
        int volume = 80;      // 0-100
        bool muted = false;
    };

    // Recording settings
    struct RecordingSettings {
        QString directory;
        QString filenamePrefix = "HamMixer";
    };

    // Window settings
    struct WindowSettings {
        QPoint position = QPoint(100, 100);
        QSize size = QSize(950, 550);
    };

    // Serial/CI-V settings
    struct SerialSettings {
        QString portName;
        int baudRate = 57600;
        bool autoConnect = false;
    };

    // WebSDR settings
    struct WebSdrSettings {
        QString selectedSiteId = "maasbree";
        bool showBrowser = true;
        bool autoLoad = false;
    };

    Settings();
    ~Settings() = default;

    /**
     * @brief Load settings from file
     * @return true if successful
     */
    bool load();

    /**
     * @brief Save settings to file
     * @return true if successful
     */
    bool save();

    /**
     * @brief Get config file path
     */
    static QString getConfigPath();

    /**
     * @brief Get config directory
     */
    static QString getConfigDir();

    /**
     * @brief Get default recording directory
     */
    static QString getDefaultRecordingDir();

    // Accessors
    DeviceSettings& devices() { return m_devices; }
    const DeviceSettings& devices() const { return m_devices; }

    ChannelSettings& channel1() { return m_channel1; }
    const ChannelSettings& channel1() const { return m_channel1; }

    ChannelSettings& channel2() { return m_channel2; }
    const ChannelSettings& channel2() const { return m_channel2; }

    MasterSettings& master() { return m_master; }
    const MasterSettings& master() const { return m_master; }

    RecordingSettings& recording() { return m_recording; }
    const RecordingSettings& recording() const { return m_recording; }

    WindowSettings& window() { return m_window; }
    const WindowSettings& window() const { return m_window; }

    SerialSettings& serial() { return m_serial; }
    const SerialSettings& serial() const { return m_serial; }

    WebSdrSettings& webSdr() { return m_webSdr; }
    const WebSdrSettings& webSdr() const { return m_webSdr; }

    // WebSDR sites list
    QList<WebSdrSite> webSdrSites() const { return m_webSdrSites; }
    void setWebSdrSites(const QList<WebSdrSite>& sites) { m_webSdrSites = sites; }

private:
    DeviceSettings m_devices;
    ChannelSettings m_channel1;
    ChannelSettings m_channel2;
    MasterSettings m_master;
    RecordingSettings m_recording;
    WindowSettings m_window;
    SerialSettings m_serial;
    WebSdrSettings m_webSdr;
    QList<WebSdrSite> m_webSdrSites;

    QString m_version = "1.2";

    // JSON helpers
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& json);
};

#endif // SETTINGS_H
