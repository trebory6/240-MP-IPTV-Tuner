#include "IptvTunerBackend.h"
#include "M3uParser.h"
#include "XmltvParser.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QEventLoop>
#include <QTimer>
#include <QMap>
#include <QDebug>

static const QString kStateFile = QStringLiteral("iptv_tuner_state.json");

IptvTunerBackend::IptvTunerBackend(const QString &appRoot, const QString &dataRoot, QObject *parent)
    : QObject(parent), m_appRoot(appRoot), m_dataRoot(dataRoot)
{
    connect(&m_epgTimer, &QTimer::timeout, this, &IptvTunerBackend::loadGuide);
    connect(&m_weather, &WeatherClient::weatherReady, this, [this](const QVariantMap &d) {
        emit weatherReady(d);
    });
    connect(&m_weather, &WeatherClient::weatherFailed, this, [this](const QString &msg) {
        emit loadFailed(msg);
    });
    scheduleEpgRefresh();
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
