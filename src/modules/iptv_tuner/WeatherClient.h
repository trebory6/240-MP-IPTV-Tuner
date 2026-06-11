#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QString>
#include <QVariantMap>

class WeatherClient : public QObject {
    Q_OBJECT
public:
    explicit WeatherClient(QObject *parent = nullptr);

    void fetch(const QString &zip, const QString &apiKey, const QString &cachePath);

signals:
    void weatherReady(const QVariantMap &data);
    void weatherFailed(const QString &message);

private:
    QNetworkAccessManager m_nam;
    QString m_cachePath;
};
