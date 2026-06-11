#pragma once
#include <QObject>
#include <QVariant>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QJsonObject>
#include "ChannelRegistry.h"
#include "EpgIndex.h"
#include "WeatherClient.h"

class IptvTunerBackend : public QObject {
    Q_OBJECT
public:
    static constexpr const char *kModuleId = "com.240mp.iptv_tuner";

    explicit IptvTunerBackend(const QString &appRoot, const QString &dataRoot,
                              QObject *parent = nullptr);

    Q_INVOKABLE void validateSources(const QString &m3uUrl, const QString &epgUrl);
    Q_INVOKABLE void loadChannels();
    Q_INVOKABLE void loadGuide();
    Q_INVOKABLE void refreshWeather();
    Q_INVOKABLE QVariant resolveChannel(const QString &displayNumber);
    Q_INVOKABLE QVariant adjacentChannel(const QString &currentNumber, int direction);
    Q_INVOKABLE QVariantMap getGuideTheme();
    Q_INVOKABLE QString getCategoryColor(const QString &contentType);
    Q_INVOKABLE void getCategories();
    Q_INVOKABLE int activeStreamCount() const;
    Q_INVOKABLE void saveLastChannel(const QString &displayNumber);
    Q_INVOKABLE QString lastChannel() const;
    Q_INVOKABLE bool isFirstTvSession() const;
    Q_INVOKABLE void markFirstTvSessionComplete();
    Q_INVOKABLE bool isConfigured() const;
    Q_INVOKABLE QString startupChannelNumber() const;
    Q_INVOKABLE void reconfigureSources();
    Q_INVOKABLE void refreshData();
    Q_INVOKABLE bool beginStream();
    Q_INVOKABLE void endStream();
    Q_INVOKABLE QVariantMap moduleSettings() const;

public slots:
    void onSettingChanged(const QString &moduleId, const QString &key, const QVariant &value);

signals:
    void sourcesValidated(bool ok, const QString &message);
    void channelsReady(const QVariant &channels);
    void guideReady(const QVariant &channels, const QVariant &programmes);
    void channelResolved(const QVariant &channel);
    void loadFailed(const QString &message);
    void weatherReady(const QVariant &data);
    void dynamicOptionsReady(const QString &key, const QVariant &options);
    void reconfigureRequested();

private:
    QVariantMap loadModuleConfig() const;
    void saveState(const QJsonObject &patch) const;
    QJsonObject loadState() const;
    QByteArray fetchUrl(const QString &url, QString *error);
    void finishLoad(const QString &error = {});
    void scheduleEpgRefresh();
    void emitCategories();

    QString m_appRoot;
    QString m_dataRoot;
    QNetworkAccessManager m_nam;
    QTimer m_epgTimer;
    WeatherClient m_weather;
    ChannelRegistry m_registry;
    EpgIndex m_epg;
    int m_activeStreams = 0;
    int m_streamLimit = 1;
    QString m_pendingM3u;
    QString m_pendingEpg;
    bool m_validating = false;
};
