#pragma once
#include <QObject>
#include <QVariant>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <QHash>
#include "ChannelRegistry.h"
#include "EpgIndex.h"
#include "WeatherClient.h"
#include "ChannelMusicPlayer.h"

class IptvTunerBackend : public QObject {
    Q_OBJECT
public:
    static constexpr const char *kModuleId = "com.240mp.iptv_tuner";

    explicit IptvTunerBackend(const QString &appRoot, const QString &dataRoot,
                              QObject *parent = nullptr);
    ~IptvTunerBackend() override;

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

    Q_INVOKABLE void startVirtualChannelClock();
    Q_INVOKABLE void stopVirtualChannelClock();
    Q_INVOKABLE int computeGuideRow(qint64 nowMs = -1) const;
    Q_INVOKABLE void onVirtualChannelTuneIn(const QString &channel);
    Q_INVOKABLE void onVirtualChannelTuneOut();
    Q_INVOKABLE QVariantMap getWeatherData() const;

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
    struct PlaylistCache {
        QString sourceDir;
        QString m3uPath;
        QJsonArray tracks;
        qint64 totalDurationMs = 0;
    };

    QVariantMap loadModuleConfig() const;
    void saveState(const QJsonObject &patch) const;
    QJsonObject loadState() const;
    QByteArray fetchUrl(const QString &url, QString *error);
    void finishLoad(const QString &error = {});
    void scheduleEpgRefresh();
    void emitCategories();

    QString resolveMusicDirectory(const QString &channel) const;
    QString playlistCachePath(const QString &channel) const;
    QString playlistM3uPath(const QString &channel) const;
    PlaylistCache loadPlaylistCache(const QString &channel) const;
    PlaylistCache buildPlaylistCache(const QString &channel);
    PlaylistCache ensurePlaylist(const QString &channel);
    void computeMusicPosition(const QString &channel, qint64 nowMs,
                              int *playlistIndex, double *startSec) const;
    void startMusicForChannel(const QString &channel);
    void loadWeatherCacheFromDisk();
    void scheduleWeatherRefresh();
    void maybeRefreshWeather();
    int guideScrollIntervalMs() const;
    int visibleChannelCount() const;
    bool isMusicEnabled(const QString &channel) const;
    static bool isToggleOn(const QVariant &value);

    QString m_appRoot;
    QString m_dataRoot;
    QNetworkAccessManager m_nam;
    QTimer m_epgTimer;
    QTimer m_weatherTimer;
    WeatherClient m_weather;
    ChannelRegistry m_registry;
    EpgIndex m_epg;
    ChannelMusicPlayer m_musicPlayer;
    int m_activeStreams = 0;
    int m_streamLimit = 1;
    QString m_pendingM3u;
    QString m_pendingEpg;
    bool m_validating = false;

    bool m_clockRunning = false;
    qint64 m_guideScrollEpochMs = 0;
    qint64 m_guideMusicEpochMs = 0;
    qint64 m_weatherMusicEpochMs = 0;
    qint64 m_lastWeatherFetchMs = 0;
    QVariantMap m_weatherCache;
    QString m_tunedVirtualChannel;
    mutable QHash<QString, PlaylistCache> m_playlistCaches;
};
