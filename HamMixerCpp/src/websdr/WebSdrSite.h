/*
 * WebSdrSite.h
 *
 * WebSDR Site configuration data structure
 * Part of HamMixer CT7BAC
 */

#ifndef WEBSDRSITE_H
#define WEBSDRSITE_H

#include <QString>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>

struct WebSdrSite {
    QString id;             // Unique identifier (e.g., "maasbree")
    QString name;           // Display name in dropdown (e.g., "Maasbree NL")
    QString url;            // Full URL (e.g., "http://sdr.websdrmaasbree.nl:8901/")

    WebSdrSite()
    {}

    WebSdrSite(const QString& id, const QString& name, const QString& url)
        : id(id)
        , name(name)
        , url(url)
    {}

    bool isValid() const {
        return !id.isEmpty() && !url.isEmpty();
    }

    bool operator==(const WebSdrSite& other) const {
        return id == other.id;
    }

    /**
     * Serialize to JSON
     */
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["id"] = id;
        obj["name"] = name;
        obj["url"] = url;
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
