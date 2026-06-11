#include "ChannelRegistry.h"
#include <QDebug>
#include <QSet>
#include <algorithm>

static bool categoryEnabled(const QVariantMap &cfg, const QString &key, const QString &groupTitle) {
    if (groupTitle.isEmpty()) return true;
    const QString dotKey = key + QLatin1Char('.') + groupTitle;
    if (cfg.contains(dotKey)) return cfg[dotKey].toBool();
    const QVariant nested = cfg.value(key);
    if (nested.canConvert<QVariantMap>()) {
        const QVariantMap map = nested.toMap();
        if (map.contains(groupTitle)) return map[groupTitle].toBool();
    }
    return true;
}

void ChannelRegistry::setRawChannels(const QVector<QVariantMap> &channels) {
    m_raw = channels;
    rebuild();
}

void ChannelRegistry::applySettings(const QVariantMap &moduleConfig) {
    m_config = moduleConfig;
    rebuild();
}

QVariantMap ChannelRegistry::makeVirtualChannel(const QString &type, const QString &number) const {
    QVariantMap m;
    m["tvgId"] = QString();
    m["displayNumber"] = number;
    bool ok = false;
    m["displayNumberSort"] = number.toDouble(&ok);
    if (!ok) m["displayNumberSort"] = 0.0;
    m["name"] = type == QLatin1String("guide") ? QStringLiteral("TV Guide")
                                               : QStringLiteral("Weather");
    m["streamUrl"] = QString();
    m["logoUrl"] = QString();
    m["groupTitle"] = QString();
    m["isVirtual"] = true;
    m["virtualType"] = type;
    m["hidden"] = false;
    m["sourceNumber"] = number;
    return m;
}

bool ChannelRegistry::isCategoryEnabled(const QString &groupTitle) const {
    return categoryEnabled(m_config, QStringLiteral("enabled_categories"), groupTitle);
}

void ChannelRegistry::rebuild() {
    m_channels.clear();
    const QString mode = m_config.value(QStringLiteral("channel_number_mode"), QStringLiteral("source")).toString();
    const QString guideNum = m_config.value(QStringLiteral("guide_channel_number"), QStringLiteral("00")).toString();
    const QString weatherNum = m_config.value(QStringLiteral("weather_channel_number"), QStringLiteral("000")).toString();

    QVector<QVariantMap> numbered = m_raw;
    if (mode == QLatin1String("chronological")) {
        std::sort(numbered.begin(), numbered.end(), [](const QVariantMap &a, const QVariantMap &b) {
            const double sa = a.value(QStringLiteral("displayNumberSort")).toDouble();
            const double sb = b.value(QStringLiteral("displayNumberSort")).toDouble();
            if (sa != sb) return sa < sb;
            return a.value(QStringLiteral("name")).toString() < b.value(QStringLiteral("name")).toString();
        });
        int n = 1;
        for (QVariantMap &ch : numbered) {
            ch[QStringLiteral("displayNumber")] = QString::number(n);
            ch[QStringLiteral("displayNumberSort")] = static_cast<double>(n);
            ++n;
        }
    }

    QSet<QString> reserved;
    reserved.insert(guideNum);
    reserved.insert(weatherNum);

    for (QVariantMap ch : numbered) {
        const QString num = ch.value(QStringLiteral("displayNumber")).toString();
        const QString group = ch.value(QStringLiteral("groupTitle")).toString();
        if (reserved.contains(num)) {
            ch[QStringLiteral("hidden")] = true;
            qWarning("[ChannelRegistry] M3U channel %s hidden (virtual collision)", qPrintable(num));
        } else if (!isCategoryEnabled(group)) {
            ch[QStringLiteral("hidden")] = true;
        } else {
            ch[QStringLiteral("hidden")] = false;
        }
        m_channels.append(ch);
    }

    m_channels.append(makeVirtualChannel(QStringLiteral("guide"), guideNum));
    m_channels.append(makeVirtualChannel(QStringLiteral("weather"), weatherNum));

    std::sort(m_channels.begin(), m_channels.end(), [](const QVariantMap &a, const QVariantMap &b) {
        return a.value(QStringLiteral("displayNumberSort")).toDouble()
             < b.value(QStringLiteral("displayNumberSort")).toDouble();
    });
}

QVector<QVariantMap> ChannelRegistry::visibleChannels() const {
    QVector<QVariantMap> out;
    for (const QVariantMap &ch : m_channels) {
        if (!ch.value(QStringLiteral("hidden")).toBool())
            out.append(ch);
    }
    return out;
}

QVariantMap ChannelRegistry::resolveByNumber(const QString &displayNumber) const {
    for (const QVariantMap &ch : m_channels) {
        if (ch.value(QStringLiteral("hidden")).toBool()) continue;
        if (ch.value(QStringLiteral("displayNumber")).toString() == displayNumber)
            return ch;
    }
    return {};
}

QVariantMap ChannelRegistry::adjacentChannel(const QString &currentNumber, int direction) const {
    auto visible = visibleChannels();
    int idx = -1;
    for (int i = 0; i < visible.size(); ++i) {
        if (visible[i].value(QStringLiteral("displayNumber")).toString() == currentNumber) {
            idx = i;
            break;
        }
    }
    if (idx < 0) return visible.isEmpty() ? QVariantMap{} : visible.first();
    idx = (idx + direction + visible.size()) % visible.size();
    return visible.at(idx);
}

QStringList ChannelRegistry::uniqueGroupTitles() const {
    QSet<QString> set;
    for (const QVariantMap &ch : m_raw) {
        const QString g = ch.value(QStringLiteral("groupTitle")).toString();
        if (!g.isEmpty()) set.insert(g);
    }
    QStringList list = set.values();
    list.sort();
    return list;
}
