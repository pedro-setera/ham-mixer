#include "config/Settings.h"
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QDebug>

Settings::Settings()
{
    // Set default recording directory
    m_recording.directory = getDefaultRecordingDir();

    // Default channel 1 settings (Radio - left)
    m_channel1.volume = 100;
    m_channel1.pan = -100;  // Full left
    m_channel1.delayMs = 300;
    m_channel1.muted = false;

    // Default channel 2 settings (WebSDR - right)
    m_channel2.volume = 100;
    m_channel2.pan = 100;   // Full right
    m_channel2.delayMs = 0;
    m_channel2.muted = false;

    // Default WebSDR sites
    m_webSdrSites = WebSdrSite::defaultSites();
}

QString Settings::getConfigDir()
{
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    // Ensure directory name is HamMixer
    if (!appData.endsWith("HamMixer")) {
        appData = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/HamMixer";
    }
    return appData;
}

QString Settings::getConfigPath()
{
    return getConfigDir() + "/config.json";
}

QString Settings::getDefaultRecordingDir()
{
    // Use executable directory, not current working directory
    return QCoreApplication::applicationDirPath() + "/recordings";
}

bool Settings::load()
{
    QString path = getConfigPath();
    QFile file(path);

    if (!file.exists()) {
        qDebug() << "Config file does not exist, using defaults:" << path;
        return false;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open config file:" << path;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "Invalid config file format";
        return false;
    }

    fromJson(doc.object());
    qDebug() << "Settings loaded from:" << path;
    return true;
}

bool Settings::save()
{
    QString dir = getConfigDir();
    QDir().mkpath(dir);

    QString path = getConfigPath();
    QFile file(path);

    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to save config file:" << path;
        return false;
    }

    QJsonDocument doc(toJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    qDebug() << "Settings saved to:" << path;
    return true;
}

QJsonObject Settings::toJson() const
{
    QJsonObject root;
    root["version"] = m_version;

    // Devices
    QJsonObject devices;
    devices["radio_input"] = m_devices.radioInput;
    devices["system_loopback"] = m_devices.systemLoopback;
    devices["output"] = m_devices.output;
    root["devices"] = devices;

    // Channel 1
    QJsonObject ch1;
    ch1["volume"] = m_channel1.volume;
    ch1["pan"] = m_channel1.pan;
    ch1["delay_ms"] = m_channel1.delayMs;
    ch1["muted"] = m_channel1.muted;
    root["channel1"] = ch1;

    // Channel 2
    QJsonObject ch2;
    ch2["volume"] = m_channel2.volume;
    ch2["pan"] = m_channel2.pan;
    ch2["muted"] = m_channel2.muted;
    root["channel2"] = ch2;

    // Master
    QJsonObject master;
    master["volume"] = m_master.volume;
    master["muted"] = m_master.muted;
    root["master"] = master;

    // Recording
    QJsonObject recording;
    recording["directory"] = m_recording.directory;
    recording["filename_prefix"] = m_recording.filenamePrefix;
    root["recording"] = recording;

    // Window
    QJsonObject window;
    window["x"] = m_window.position.x();
    window["y"] = m_window.position.y();
    window["width"] = m_window.size.width();
    window["height"] = m_window.size.height();
    root["window"] = window;

    // Serial
    QJsonObject serial;
    serial["port"] = m_serial.portName;
    serial["baud_rate"] = m_serial.baudRate;
    serial["auto_connect"] = m_serial.autoConnect;
    root["serial"] = serial;

    // WebSDR
    QJsonObject webSdr;
    webSdr["selected_site"] = m_webSdr.selectedSiteId;
    webSdr["show_browser"] = m_webSdr.showBrowser;
    webSdr["auto_load"] = m_webSdr.autoLoad;

    // WebSDR sites list
    QJsonArray sitesArray;
    for (const WebSdrSite& site : m_webSdrSites) {
        sitesArray.append(site.toJson());
    }
    webSdr["sites"] = sitesArray;

    root["websdr"] = webSdr;

    return root;
}

void Settings::fromJson(const QJsonObject& json)
{
    m_version = json["version"].toString("1.0");

    // Devices
    QJsonObject devices = json["devices"].toObject();
    m_devices.radioInput = devices["radio_input"].toString();
    m_devices.systemLoopback = devices["system_loopback"].toString();
    m_devices.output = devices["output"].toString();

    // Channel 1
    QJsonObject ch1 = json["channel1"].toObject();
    m_channel1.volume = ch1["volume"].toInt(100);
    m_channel1.pan = ch1["pan"].toInt(-100);
    m_channel1.delayMs = ch1["delay_ms"].toInt(300);
    m_channel1.muted = ch1["muted"].toBool(false);

    // Channel 2
    QJsonObject ch2 = json["channel2"].toObject();
    m_channel2.volume = ch2["volume"].toInt(100);
    m_channel2.pan = ch2["pan"].toInt(100);
    m_channel2.muted = ch2["muted"].toBool(false);

    // Master
    QJsonObject master = json["master"].toObject();
    m_master.volume = master["volume"].toInt(80);
    m_master.muted = master["muted"].toBool(false);

    // Recording
    QJsonObject recording = json["recording"].toObject();
    // Always use fresh path next to executable, don't load saved path
    // (saved paths become invalid when exe is moved/copied)
    m_recording.directory = getDefaultRecordingDir();
    m_recording.filenamePrefix = recording["filename_prefix"].toString("HamMixer");

    // Window
    QJsonObject window = json["window"].toObject();
    m_window.position = QPoint(
        window["x"].toInt(100),
        window["y"].toInt(100)
    );
    m_window.size = QSize(
        window["width"].toInt(950),
        window["height"].toInt(550)
    );

    // Serial
    QJsonObject serial = json["serial"].toObject();
    m_serial.portName = serial["port"].toString();
    m_serial.baudRate = serial["baud_rate"].toInt(57600);
    m_serial.autoConnect = serial["auto_connect"].toBool(false);

    // WebSDR
    QJsonObject webSdr = json["websdr"].toObject();
    m_webSdr.selectedSiteId = webSdr["selected_site"].toString("maasbree");
    m_webSdr.showBrowser = webSdr["show_browser"].toBool(true);
    m_webSdr.autoLoad = webSdr["auto_load"].toBool(false);

    // WebSDR sites list
    m_webSdrSites.clear();
    QJsonArray sitesArray = webSdr["sites"].toArray();
    if (sitesArray.isEmpty()) {
        // Use default sites if none configured
        m_webSdrSites = WebSdrSite::defaultSites();
    } else {
        for (const QJsonValue& val : sitesArray) {
            WebSdrSite site = WebSdrSite::fromJson(val.toObject());
            if (site.isValid()) {
                m_webSdrSites.append(site);
            }
        }
    }
}
