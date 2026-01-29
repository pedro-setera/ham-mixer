/*
 * WebSdrSite.h
 *
 * SDR Site configuration data structure (WebSDR 2.x and KiwiSDR)
 * Part of HamMixer CT7BAC
 */

#ifndef WEBSDRSITE_H
#define WEBSDRSITE_H

#include <QString>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>

/**
 * @brief SDR site type enumeration
 */
enum class SdrSiteType {
    WebSDR,     // WebSDR 2.x (PA3FWM software)
    KiwiSDR     // KiwiSDR receivers
};

struct WebSdrSite {
    QString id;             // Unique identifier (e.g., "maasbree")
    QString name;           // Display name in dropdown (e.g., "Maasbree NL")
    QString url;            // Full URL (e.g., "http://sdr.websdrmaasbree.nl:8901/")
    SdrSiteType type = SdrSiteType::WebSDR;  // Site type (default: WebSDR)
    int port = 0;           // Custom port (0 = use URL default)
    QString password;       // Optional password for protected KiwiSDR sites

    WebSdrSite()
    {}

    WebSdrSite(const QString& id, const QString& name, const QString& url,
               SdrSiteType type = SdrSiteType::WebSDR, int port = 0,
               const QString& password = QString())
        : id(id)
        , name(name)
        , url(url)
        , type(type)
        , port(port)
        , password(password)
    {}

    bool isValid() const {
        return !id.isEmpty() && !url.isEmpty();
    }

    bool operator==(const WebSdrSite& other) const {
        return id == other.id;
    }

    /**
     * Check if this is a KiwiSDR site
     */
    bool isKiwiSDR() const { return type == SdrSiteType::KiwiSDR; }

    /**
     * Check if this is a WebSDR site
     */
    bool isWebSDR() const { return type == SdrSiteType::WebSDR; }

    /**
     * Get the effective URL with port applied
     * @return URL string with custom port if specified
     */
    QString effectiveUrl() const {
        if (port <= 0) return url;

        QUrl parsed(url);
        parsed.setPort(port);
        return parsed.toString();
    }

    /**
     * Serialize to JSON
     */
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["id"] = id;
        obj["name"] = name;
        obj["url"] = url;
        obj["type"] = (type == SdrSiteType::KiwiSDR) ? "kiwisdr" : "websdr";
        if (port > 0) obj["port"] = port;
        if (!password.isEmpty()) obj["password"] = password;
        return obj;
    }

    /**
     * Deserialize from JSON
     */
    static WebSdrSite fromJson(const QJsonObject& obj) {
        WebSdrSite site;
        site.id = obj["id"].toString();
        site.name = obj["name"].toString();
        site.url = obj["url"].toString();

        QString typeStr = obj["type"].toString("websdr");
        site.type = (typeStr == "kiwisdr") ? SdrSiteType::KiwiSDR : SdrSiteType::WebSDR;

        site.port = obj["port"].toInt(0);
        site.password = obj["password"].toString();
        return site;
    }

    /**
     * Get the default list of supported WebSDR sites
     * Used on first run or when config is empty
     */
    static QList<WebSdrSite> defaultSites() {
        QList<WebSdrSite> sites;

        // Maasbree, Netherlands
        sites.append(WebSdrSite(
            "maasbree",
            "Maasbree NL",
            "http://sdr.websdrmaasbree.nl:8901/"
        ));

        // Utah, USA
        sites.append(WebSdrSite(
            "utah",
            "Utah US",
            "http://websdr1.sdrutah.org:8901/index1a.html"
        ));

        // Bordeaux, France
        sites.append(WebSdrSite(
            "bordeaux",
            "Bordeaux FR",
            "http://ham.websdrbordeaux.fr:8000"
        ));

        // Pardinho, Brazil
        sites.append(WebSdrSite(
            "pardinho",
            "Pardinho BR",
            "http://appr.org.br:8901"
        ));

        // Yaroslavl, Russia
        sites.append(WebSdrSite(
            "yaroslavl",
            "Yaroslavl RU",
            "http://websdr.srr-76.ru"
        ));

        // Sparta, Greece
        sites.append(WebSdrSite(
            "sparta",
            "Sparta GR",
            "http://sv3gcb.ddns.net:8905"
        ));

        return sites;
    }

    /**
     * Find a site by its ID in a list
     */
    static WebSdrSite findById(const QList<WebSdrSite>& sites, const QString& id) {
        for (const auto& site : sites) {
            if (site.id == id) {
                return site;
            }
        }
        // Return first site as default
        return sites.isEmpty() ? WebSdrSite() : sites.first();
    }
};

#endif // WEBSDRSITE_H
