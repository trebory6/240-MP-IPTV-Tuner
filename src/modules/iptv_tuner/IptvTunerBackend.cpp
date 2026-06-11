#include "IptvTunerBackend.h"
#include "M3uParser.h"
#include "XmltvParser.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QProcess>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QEventLoop>
#include <QTimer>
#include <QMap>
#include <QHash>
#include <QMetaType>
#include <QDebug>

static const QString kStateFile = QStringLiteral("iptv_tuner_state.json");

IptvTunerBackend::IptvTunerBackend(const QString &appRoot, const QString &dataRoot, QObject *parent)
    : QObject(parent), m_appRoot(appRoot), m_dataRoot(dataRoot)
{
    connect(&m_epgTimer, &QTimer::timeout, this, &IptvTunerBackend::loadGuide);
    connect(&m_weatherTimer, &QTimer::timeout, this, &IptvTunerBackend::maybeRefreshWeather);
    connect(&m_weather, &WeatherClient::weatherReady, this, [this](const QVariantMap &d) {
        m_weatherCache = d;
        m_lastWeatherFetchMs = QDateTime::currentMSecsSinceEpoch();
        emit weatherReady(d);
    });
    connect(&m_weather, &WeatherClient::weatherFailed, this, [this](const QString &msg) {
        if (!m_weatherCache.isEmpty())
            return;
        emit loadFailed(msg);
    });
    scheduleEpgRefresh();
    loadWeatherCacheFromDisk();
}

IptvTunerBackend::~IptvTunerBackend() {
    stopVirtualChannelClock();
}

QVariantMap IptvTunerBackend::loadModuleConfig() const {
    QFile f(m_dataRoot + "/config.json");
    if (!f.open(QIODevice::ReadOnly)) return {};
    QJsonObject cfg = QJsonDocument::fromJson(f.readAll()).object();
    return cfg["modules"].toObject()[kModuleId].toObject().toVariantMap();
}

QJsonObject IptvTunerBackend::loadState() const {
    QFile f(m_dataRoot + "/" + kStateFile);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

void IptvTunerBackend::saveState(const QJsonObject &patch) const {
    QJsonObject state = loadState();
    for (auto it = patch.begin(); it != patch.end(); ++it)
        state[it.key()] = it.value();
    QFile f(m_dataRoot + "/" + kStateFile);
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(state).toJson(QJsonDocument::Indented));
}

QVariantMap IptvTunerBackend::moduleSettings() const {
    return loadModuleConfig();
}

bool IptvTunerBackend::isConfigured() const {
    const QVariantMap cfg = loadModuleConfig();
    return !cfg.value(QStringLiteral("m3u_url")).toString().isEmpty();
}

QByteArray IptvTunerBackend::fetchUrl(const QString &url, QString *error) {
    const QVariantMap cfg = loadModuleConfig();
    const QString ua = cfg.value(QStringLiteral("user_agent"), QStringLiteral("240-MP IPTV Tuner")).toString();
    const int timeoutMs = cfg.value(QStringLiteral("stream_connect_timeout_sec"), 10).toInt() * 1000;

    QString localPath = url;
    if (localPath.startsWith(QStringLiteral("file://")))
        localPath = QUrl(localPath).toLocalFile();

    if (!url.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
        && !url.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)) {
        QFile f(localPath);
        if (!f.open(QIODevice::ReadOnly)) {
            if (error) *error = QStringLiteral("Cannot open: %1").arg(localPath);
            return {};
        }
        return f.readAll();
    }

    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, ua);
    QEventLoop loop;
    QByteArray body;
    auto *reply = m_nam.get(req);
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();
    if (!timer.isActive()) {
        reply->abort();
        if (error) *error = QStringLiteral("Request timed out");
        reply->deleteLater();
        return {};
    }
    timer.stop();
    if (reply->error() != QNetworkReply::NoError) {
        if (error) *error = reply->errorString();
        reply->deleteLater();
        return {};
    }
    body = reply->readAll();
    reply->deleteLater();
    return body;
}

void IptvTunerBackend::validateSources(const QString &m3uUrl, const QString &epgUrl) {
    m_validating = true;
    m_pendingM3u = m3uUrl;
    m_pendingEpg = epgUrl;

    QString err;
    const QByteArray m3uData = fetchUrl(m3uUrl, &err);
    if (m3uData.isEmpty()) {
        emit sourcesValidated(false, err.isEmpty() ? QStringLiteral("M3U fetch failed") : err);
        m_validating = false;
        return;
    }
    M3uParseResult m3u = M3uParser::parse(m3uData);
    if (!m3u.error.isEmpty()) {
        emit sourcesValidated(false, m3u.error);
        m_validating = false;
        return;
    }

    QString epg = epgUrl;
    if (epg.isEmpty()) epg = m3u.epgUrlFromHeader;
    if (epg.isEmpty()) {
        emit sourcesValidated(false, QStringLiteral("EPG URL required"));
        m_validating = false;
        return;
    }

    const QByteArray epgData = fetchUrl(epg, &err);
    if (epgData.isEmpty()) {
        emit sourcesValidated(false, err.isEmpty() ? QStringLiteral("EPG fetch failed") : err);
        m_validating = false;
        return;
    }

    QVector<QVariantMap> xmlCh;
    QVector<XmltvProgramme> progs;
    if (!XmltvParser::parse(epgData, &err, &xmlCh, &progs)) {
        emit sourcesValidated(false, err);
        m_validating = false;
        return;
    }

    emit sourcesValidated(true, QStringLiteral("OK"));
    m_validating = false;
}

void IptvTunerBackend::finishLoad(const QString &error) {
    if (!error.isEmpty()) {
        emit loadFailed(error);
        return;
    }
    const auto visible = m_registry.visibleChannels();
    QVariantList list;
    for (const QVariantMap &ch : visible)
        list.append(ch);
    emit channelsReady(list);
    QVariantList progs = m_epg.allProgrammes();
    emit guideReady(list, progs);
    emitCategories();
}

void IptvTunerBackend::loadChannels() {
    const QVariantMap cfg = loadModuleConfig();
    m_streamLimit = cfg.value(QStringLiteral("simultaneous_stream_limit"), 1).toInt();
    const QString m3uUrl = cfg.value(QStringLiteral("m3u_url")).toString();
    if (m3uUrl.isEmpty()) {
        emit loadFailed(QStringLiteral("M3U URL not configured"));
        return;
    }

    QString err;
    const QByteArray m3uData = fetchUrl(m3uUrl, &err);
    if (m3uData.isEmpty()) {
        finishLoad(err.isEmpty() ? QStringLiteral("M3U load failed") : err);
        return;
    }

    M3uParseResult m3u = M3uParser::parse(m3uData);
    if (!m3u.error.isEmpty()) {
        finishLoad(m3u.error);
        return;
    }

    QVector<QVariantMap> raw;
    for (const M3uChannel &ch : m3u.channels)
        raw.append(m3uChannelToVariant(ch));

    m_registry.setRawChannels(raw);
    m_registry.applySettings(cfg);

    QFile cache(m_dataRoot + "/iptv_tuner_m3u.cache");
    if (cache.open(QIODevice::WriteOnly)) cache.write(m3uData);

    finishLoad();
}

void IptvTunerBackend::loadGuide() {
    const QVariantMap cfg = loadModuleConfig();
    QString epgUrl = cfg.value(QStringLiteral("epg_url")).toString();
    if (epgUrl.isEmpty()) {
        QString err;
        const QString m3uUrl = cfg.value(QStringLiteral("m3u_url")).toString();
        const QByteArray m3uData = fetchUrl(m3uUrl, &err);
        if (!m3uData.isEmpty()) {
            M3uParseResult m3u = M3uParser::parse(m3uData);
            epgUrl = m3u.epgUrlFromHeader;
        }
    }
    if (epgUrl.isEmpty()) {
        emit loadFailed(QStringLiteral("EPG URL not configured"));
        return;
    }

    QString err;
    const QByteArray epgData = fetchUrl(epgUrl, &err);
    if (epgData.isEmpty()) {
        emit loadFailed(err.isEmpty() ? QStringLiteral("EPG load failed") : err);
        return;
    }

    QVector<QVariantMap> xmlCh;
    QVector<XmltvProgramme> progs;
    if (!XmltvParser::parse(epgData, &err, &xmlCh, &progs)) {
        emit loadFailed(err);
        return;
    }

    m_epg.setProgrammes(progs);
    QFile cache(m_dataRoot + "/iptv_tuner_epg.cache");
    if (cache.open(QIODevice::WriteOnly)) cache.write(epgData);

    loadChannels();
}

void IptvTunerBackend::refreshData() {
    loadGuide();
}

void IptvTunerBackend::refreshWeather() {
    const QVariantMap cfg = loadModuleConfig();
    m_weather.fetch(cfg.value(QStringLiteral("weather_zip")).toString(),
                      cfg.value(QStringLiteral("weather_api_key")).toString(),
                      m_dataRoot + "/iptv_tuner_weather.json");
}

QVariant IptvTunerBackend::resolveChannel(const QString &displayNumber) {
    QVariantMap ch = m_registry.resolveByNumber(displayNumber);
    emit channelResolved(ch.isEmpty() ? QVariant() : ch);
    return ch;
}

QVariant IptvTunerBackend::adjacentChannel(const QString &currentNumber, int direction) {
    return m_registry.adjacentChannel(currentNumber, direction);
}

QVariantMap IptvTunerBackend::getGuideTheme() {
    const QVariantMap cfg = loadModuleConfig();
    const QString themeId = cfg.value(QStringLiteral("guide_theme"), QStringLiteral("prevue")).toString();
    QString path = m_appRoot + "/modules/iptv_tuner/assets/themes/" + themeId + ".json";
    if (themeId == QLatin1String("custom")) {
        path = m_dataRoot + "/iptv_tuner_custom_theme.json";
    }
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return QVariantMap{
            {QStringLiteral("name"), themeId},
            {QStringLiteral("background"), QStringLiteral("#0A0094")},
            {QStringLiteral("rowAlt"), QStringLiteral("#8480C9")},
            {QStringLiteral("rowPrimary"), QStringLiteral("#0A0094")},
            {QStringLiteral("text"), QStringLiteral("#FFFFFF")},
            {QStringLiteral("accent"), QStringLiteral("#AECFFF")}
        };
    }
    return QJsonDocument::fromJson(f.readAll()).object().toVariantMap();
}

QString IptvTunerBackend::getCategoryColor(const QString &contentType) {
    const QVariantMap cfg = loadModuleConfig();
    QString type = contentType;
    if (type == QLatin1String("movie")) type = QStringLiteral("movies");
    const QString key = QStringLiteral("epg_color_") + type;
    static const QMap<QString, QString> defaults = {
        {QStringLiteral("epg_color_movies"), QStringLiteral("#FFD900")},
        {QStringLiteral("epg_color_series"), QStringLiteral("#AECFFF")},
        {QStringLiteral("epg_color_sports"), QStringLiteral("#4AF626")},
        {QStringLiteral("epg_color_news"), QStringLiteral("#FF6B6B")},
        {QStringLiteral("epg_color_kids"), QStringLiteral("#D48BFF")},
        {QStringLiteral("epg_color_other"), QStringLiteral("#FFFFFF")}
    };
    const QString lookup = defaults.contains(key) ? key : QStringLiteral("epg_color_other");
    return cfg.value(lookup, defaults.value(lookup)).toString();
}

void IptvTunerBackend::emitCategories() {
    QVariantList opts;
    for (const QString &g : m_registry.uniqueGroupTitles()) {
        QVariantMap item;
        item["id"] = g;
        item["label"] = g;
        opts.append(item);
    }
    emit dynamicOptionsReady(QStringLiteral("enabled_categories"), opts);
}

void IptvTunerBackend::getCategories() {
    emitCategories();
}

int IptvTunerBackend::activeStreamCount() const {
    return m_activeStreams;
}

bool IptvTunerBackend::beginStream() {
    if (m_activeStreams >= m_streamLimit) return false;
    ++m_activeStreams;
    return true;
}

void IptvTunerBackend::endStream() {
    m_activeStreams = qMax(0, m_activeStreams - 1);
}

void IptvTunerBackend::saveLastChannel(const QString &displayNumber) {
    saveState({{QStringLiteral("last_channel_number"), displayNumber}});
}

QString IptvTunerBackend::lastChannel() const {
    return loadState().value(QStringLiteral("last_channel_number")).toString();
}

bool IptvTunerBackend::isFirstTvSession() const {
    return !loadState().value(QStringLiteral("first_tv_session_complete")).toBool(false);
}

void IptvTunerBackend::markFirstTvSessionComplete() {
    saveState({{QStringLiteral("first_tv_session_complete"), true}});
}

QString IptvTunerBackend::startupChannelNumber() const {
    const QVariantMap cfg = loadModuleConfig();
    const QString mode = cfg.value(QStringLiteral("startup_mode"), QStringLiteral("last_channel")).toString();
    if (!isConfigured()) return {};
    if (isFirstTvSession())
        return cfg.value(QStringLiteral("guide_channel_number"), QStringLiteral("00")).toString();
    if (mode == QLatin1String("guide_channel"))
        return cfg.value(QStringLiteral("guide_channel_number"), QStringLiteral("00")).toString();
    if (mode == QLatin1String("specific_channel"))
        return cfg.value(QStringLiteral("startup_channel_number")).toString();
    const QString last = lastChannel();
    if (!last.isEmpty()) return last;
    return cfg.value(QStringLiteral("guide_channel_number"), QStringLiteral("00")).toString();
}

void IptvTunerBackend::reconfigureSources() {
    emit reconfigureRequested();
}

void IptvTunerBackend::onSettingChanged(const QString &moduleId, const QString &key,
                                        const QVariant &value) {
    Q_UNUSED(value)
    if (moduleId != QLatin1String(kModuleId)) return;
    if (key == QLatin1String("epg_auto_refresh_hours")) {
        scheduleEpgRefresh();
        return;
    }
    if (key == QLatin1String("weather_refresh_minutes")) {
        scheduleWeatherRefresh();
        return;
    }
    if (key == QLatin1String("guide_music_directory") || key == QLatin1String("weather_music_directory")) {
        const QString channel = key.startsWith(QStringLiteral("guide")) ? QStringLiteral("guide")
                                                                        : QStringLiteral("weather");
        m_playlistCaches.remove(channel);
        buildPlaylistCache(channel);
        if (m_tunedVirtualChannel == channel)
            startMusicForChannel(channel);
        return;
    }
    if (key == QLatin1String("guide_music_enabled") || key == QLatin1String("weather_music_enabled")) {
        const QString channel = key.startsWith(QStringLiteral("guide")) ? QStringLiteral("guide")
                                                                        : QStringLiteral("weather");
        if (m_tunedVirtualChannel == channel) {
            if (isMusicEnabled(channel))
                startMusicForChannel(channel);
            else
                m_musicPlayer.stop();
        }
        return;
    }
    if (key.startsWith(QStringLiteral("enabled_categories"))
        || key == QLatin1String("channel_number_mode")
        || key == QLatin1String("guide_channel_number")
        || key == QLatin1String("weather_channel_number")) {
        m_registry.applySettings(loadModuleConfig());
        finishLoad();
        return;
    }
    if (key == QLatin1String("m3u_url") || key == QLatin1String("epg_url"))
        loadGuide();
}

void IptvTunerBackend::scheduleEpgRefresh() {
    m_epgTimer.stop();
    const QVariantMap cfg = loadModuleConfig();
    const int hours = cfg.value(QStringLiteral("epg_auto_refresh_hours"), 12).toInt();
    if (hours > 0)
        m_epgTimer.start(hours * 3600 * 1000);
}

static qint64 probeAudioDurationMs(const QString &path) {
    const QString ffprobe = QStandardPaths::findExecutable(QStringLiteral("ffprobe"));
    if (ffprobe.isEmpty())
        return 180000;
    QProcess proc;
    proc.start(ffprobe, {
        QStringLiteral("-v"), QStringLiteral("error"),
        QStringLiteral("-show_entries"), QStringLiteral("format=duration"),
        QStringLiteral("-of"), QStringLiteral("default=noprint_wrappers=1:nokey=1"),
        path
    });
    if (!proc.waitForFinished(5000) || proc.exitCode() != 0)
        return 180000;
    bool ok = false;
    const double sec = proc.readAllStandardOutput().trimmed().toDouble(&ok);
    return (ok && sec > 0) ? qint64(sec * 1000.0) : 180000;
}

static QStringList listMp3Files(const QString &dirPath) {
    QDir dir(dirPath);
    if (!dir.exists())
        return {};
    QStringList files;
    const auto entries = dir.entryInfoList({QStringLiteral("*.mp3"), QStringLiteral("*.MP3")},
                                           QDir::Files, QDir::Name);
    for (const QFileInfo &fi : entries)
        files.append(fi.absoluteFilePath());
    return files;
}

bool IptvTunerBackend::isToggleOn(const QVariant &value) {
    if (value.typeId() == QMetaType::Bool)
        return value.toBool();
    const QString s = value.toString().toUpper();
    return s == QLatin1String("ON") || s == QLatin1String("TRUE") || s == QLatin1String("1");
}

bool IptvTunerBackend::isMusicEnabled(const QString &channel) const {
    const QVariantMap cfg = loadModuleConfig();
    const QString key = channel == QLatin1String("weather") ? QStringLiteral("weather_music_enabled")
                                                               : QStringLiteral("guide_music_enabled");
    return isToggleOn(cfg.value(key, QStringLiteral("ON")));
}

int IptvTunerBackend::guideScrollIntervalMs() const {
    const QVariantMap cfg = loadModuleConfig();
    const int ms = cfg.value(QStringLiteral("guide_scroll_interval_ms"), 3000).toInt();
    return qMax(500, ms);
}

int IptvTunerBackend::visibleChannelCount() const {
    return m_registry.visibleChannels().size();
}

QString IptvTunerBackend::resolveMusicDirectory(const QString &channel) const {
    const QVariantMap cfg = loadModuleConfig();
    const QString settingKey = channel == QLatin1String("weather")
        ? QStringLiteral("weather_music_directory")
        : QStringLiteral("guide_music_directory");
    const QString custom = cfg.value(settingKey).toString().trimmed();
    if (!custom.isEmpty() && QDir(custom).exists() && !listMp3Files(custom).isEmpty())
        return QDir(custom).absolutePath();

    const QString dataDir = m_dataRoot + QStringLiteral("/iptv_tuner/music/") + channel;
    if (QDir(dataDir).exists() && !listMp3Files(dataDir).isEmpty())
        return QDir(dataDir).absolutePath();

    const QString bundled = m_appRoot + QStringLiteral("/modules/iptv_tuner/assets/music/") + channel;
    if (QDir(bundled).exists())
        return QDir(bundled).absolutePath();
    return {};
}

QString IptvTunerBackend::playlistCachePath(const QString &channel) const {
    return m_dataRoot + QStringLiteral("/iptv_tuner_") + channel + QStringLiteral("_playlist.json");
}

QString IptvTunerBackend::playlistM3uPath(const QString &channel) const {
    return m_dataRoot + QStringLiteral("/iptv_tuner_") + channel + QStringLiteral("_playlist.m3u");
}

IptvTunerBackend::PlaylistCache IptvTunerBackend::loadPlaylistCache(const QString &channel) const {
    PlaylistCache cache;
    QFile f(playlistCachePath(channel));
    if (!f.open(QIODevice::ReadOnly))
        return cache;
    const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
    cache.sourceDir = obj[QStringLiteral("sourceDir")].toString();
    cache.m3uPath = obj[QStringLiteral("m3uPath")].toString();
    cache.tracks = obj[QStringLiteral("tracks")].toArray();
    cache.totalDurationMs = qint64(obj[QStringLiteral("totalDurationMs")].toDouble());
    return cache;
}

IptvTunerBackend::PlaylistCache IptvTunerBackend::buildPlaylistCache(const QString &channel) {
    PlaylistCache cache;
    cache.sourceDir = resolveMusicDirectory(channel);
    if (cache.sourceDir.isEmpty())
        return cache;

    const QStringList files = listMp3Files(cache.sourceDir);
    if (files.isEmpty())
        return cache;

    QJsonArray tracks;
    qint64 total = 0;
    for (const QString &path : files) {
        const qint64 dur = probeAudioDurationMs(path);
        QJsonObject track;
        track[QStringLiteral("path")] = path;
        track[QStringLiteral("durationMs")] = double(dur);
        tracks.append(track);
        total += dur;
    }

    cache.tracks = tracks;
    cache.totalDurationMs = total;
    cache.m3uPath = playlistM3uPath(channel);

    QFile m3u(cache.m3uPath);
    if (m3u.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m3u.write("#EXTM3U\n");
        for (const QString &path : files)
            m3u.write(path.toUtf8() + '\n');
    }

    QJsonObject obj;
    obj[QStringLiteral("sourceDir")] = cache.sourceDir;
    obj[QStringLiteral("m3uPath")] = cache.m3uPath;
    obj[QStringLiteral("tracks")] = cache.tracks;
    obj[QStringLiteral("totalDurationMs")] = double(cache.totalDurationMs);
    obj[QStringLiteral("builtAtMs")] = double(QDateTime::currentMSecsSinceEpoch());
    QFile json(playlistCachePath(channel));
    if (json.open(QIODevice::WriteOnly | QIODevice::Truncate))
        json.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));

    m_playlistCaches[channel] = cache;
    return cache;
}

IptvTunerBackend::PlaylistCache IptvTunerBackend::ensurePlaylist(const QString &channel) {
    const QString dir = resolveMusicDirectory(channel);
    if (m_playlistCaches.contains(channel)) {
        const PlaylistCache &cached = m_playlistCaches[channel];
        if (cached.sourceDir == dir && !cached.tracks.isEmpty())
            return cached;
    }
    const PlaylistCache disk = loadPlaylistCache(channel);
    if (!disk.tracks.isEmpty() && disk.sourceDir == dir && QFile::exists(disk.m3uPath)) {
        m_playlistCaches[channel] = disk;
        return disk;
    }
    return buildPlaylistCache(channel);
}

void IptvTunerBackend::computeMusicPosition(const QString &channel, qint64 nowMs,
                                             int *playlistIndex, double *startSec) const {
    if (playlistIndex) *playlistIndex = 0;
    if (startSec) *startSec = 0.0;

    const qint64 epoch = channel == QLatin1String("weather") ? m_weatherMusicEpochMs
                                                               : m_guideMusicEpochMs;
    if (epoch <= 0)
        return;

    PlaylistCache cache;
    if (m_playlistCaches.contains(channel))
        cache = m_playlistCaches[channel];
    else
        cache = loadPlaylistCache(channel);

    if (cache.tracks.isEmpty() || cache.totalDurationMs <= 0)
        return;

    const qint64 elapsed = qMax<qint64>(0, nowMs - epoch);
    qint64 posInLoop = elapsed % cache.totalDurationMs;
    int idx = 0;
    for (const QJsonValue &v : cache.tracks) {
        const QJsonObject track = v.toObject();
        const qint64 dur = qint64(track[QStringLiteral("durationMs")].toDouble());
        if (dur <= 0)
            continue;
        if (posInLoop < dur) {
            if (playlistIndex) *playlistIndex = idx;
            if (startSec) *startSec = double(posInLoop) / 1000.0;
            return;
        }
        posInLoop -= dur;
        ++idx;
    }
}

void IptvTunerBackend::startMusicForChannel(const QString &channel) {
    if (!isMusicEnabled(channel)) {
        m_musicPlayer.stop();
        return;
    }
    const PlaylistCache cache = ensurePlaylist(channel);
    if (cache.tracks.isEmpty() || cache.m3uPath.isEmpty()) {
        m_musicPlayer.stop();
        return;
    }
    int index = 0;
    double startSec = 0.0;
    computeMusicPosition(channel, QDateTime::currentMSecsSinceEpoch(), &index, &startSec);
    m_musicPlayer.startAt(cache.m3uPath, index, startSec);
}

void IptvTunerBackend::loadWeatherCacheFromDisk() {
    const QString path = m_dataRoot + QStringLiteral("/iptv_tuner_weather.json");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return;
    m_weatherCache = QJsonDocument::fromJson(f.readAll()).object().toVariantMap();
    m_lastWeatherFetchMs = QFileInfo(path).lastModified().toMSecsSinceEpoch();
}

void IptvTunerBackend::scheduleWeatherRefresh() {
    m_weatherTimer.stop();
    if (!m_clockRunning)
        return;
    const QVariantMap cfg = loadModuleConfig();
    const int minutes = cfg.value(QStringLiteral("weather_refresh_minutes"), 30).toInt();
    if (minutes > 0)
        m_weatherTimer.start(qMax(1, minutes) * 60 * 1000);
}

void IptvTunerBackend::maybeRefreshWeather() {
    const QVariantMap cfg = loadModuleConfig();
    const QString zip = cfg.value(QStringLiteral("weather_zip")).toString();
    const QString apiKey = cfg.value(QStringLiteral("weather_api_key")).toString();
    if (zip.isEmpty() || apiKey.isEmpty())
        return;

    const int minutes = cfg.value(QStringLiteral("weather_refresh_minutes"), 30).toInt();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_lastWeatherFetchMs > 0 && (now - m_lastWeatherFetchMs) < qint64(minutes) * 60 * 1000)
        return;
    refreshWeather();
}

void IptvTunerBackend::startVirtualChannelClock() {
    if (m_clockRunning)
        return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_guideScrollEpochMs = now;
    m_guideMusicEpochMs = now;
    m_weatherMusicEpochMs = now;
    m_clockRunning = true;
    ensurePlaylist(QStringLiteral("guide"));
    ensurePlaylist(QStringLiteral("weather"));
    scheduleWeatherRefresh();
    maybeRefreshWeather();
}

void IptvTunerBackend::stopVirtualChannelClock() {
    m_musicPlayer.stop();
    m_weatherTimer.stop();
    m_clockRunning = false;
    m_guideScrollEpochMs = 0;
    m_guideMusicEpochMs = 0;
    m_weatherMusicEpochMs = 0;
    m_tunedVirtualChannel.clear();
    m_playlistCaches.clear();
}

int IptvTunerBackend::computeGuideRow(qint64 nowMs) const {
    if (nowMs < 0)
        nowMs = QDateTime::currentMSecsSinceEpoch();
    if (m_guideScrollEpochMs <= 0)
        return 0;
    const int n = visibleChannelCount();
    if (n <= 0)
        return 0;
    const qint64 elapsed = qMax<qint64>(0, nowMs - m_guideScrollEpochMs);
    const int interval = guideScrollIntervalMs();
    return int((elapsed / interval) % n);
}

void IptvTunerBackend::onVirtualChannelTuneIn(const QString &channel) {
    if (channel != QLatin1String("guide") && channel != QLatin1String("weather"))
        return;
    m_tunedVirtualChannel = channel;
    startMusicForChannel(channel);
}

void IptvTunerBackend::onVirtualChannelTuneOut() {
    m_tunedVirtualChannel.clear();
    m_musicPlayer.stop();
}

QVariantMap IptvTunerBackend::getWeatherData() const {
    return m_weatherCache;
}
