#include "WeatherClient.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonArray>

WeatherClient::WeatherClient(QObject *parent) : QObject(parent) {}

void WeatherClient::fetch(const QString &zip, const QString &apiKey, const QString &cachePath) {
    m_cachePath = cachePath;
    if (zip.isEmpty() || apiKey.isEmpty()) {
        emit weatherFailed(QStringLiteral("ZIP code and API key required"));
        return;
    }

    QUrl url(QStringLiteral("https://api.openweathermap.org/data/2.5/weather"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("zip"), zip + QStringLiteral(",us"));
    q.addQueryItem(QStringLiteral("appid"), apiKey);
    q.addQueryItem(QStringLiteral("units"), QStringLiteral("imperial"));
    url.setQuery(q);

    QNetworkRequest req(url);
    auto *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit weatherFailed(reply->errorString());
            return;
        }
        QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        QVariantMap data;
        data["city"] = obj["name"].toString();
        const QJsonObject main = obj["main"].toObject();
        data["temp"] = main["temp"].toDouble();
        data["humidity"] = main["humidity"].toInt();
        const QJsonArray weather = obj["weather"].toArray();
        if (!weather.isEmpty()) {
            data["description"] = weather.first().toObject()["description"].toString();
            data["icon"] = weather.first().toObject()["icon"].toString();
        }
        data["wind"] = obj["wind"].toObject()["speed"].toDouble();

        if (!m_cachePath.isEmpty()) {
            QFile f(m_cachePath);
            if (f.open(QIODevice::WriteOnly))
                f.write(QJsonDocument(QJsonObject::fromVariantMap(data)).toJson());
        }
        emit weatherReady(data);
    });
}
