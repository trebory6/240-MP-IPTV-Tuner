#include "M3uParser.h"
#include <QFile>
#include <QRegularExpression>

static QString parseAttr(const QString &line, const QString &key) {
    QRegularExpression re(QStringLiteral(R"(%1="([^"]*)")").arg(QRegularExpression::escape(key)));
    auto m = re.match(line);
    return m.hasMatch() ? m.captured(1) : QString();
}

QVariantMap m3uChannelToVariant(const M3uChannel &ch) {
    QVariantMap m;
    m["tvgId"] = ch.tvgId;
    m["name"] = ch.name;
    m["streamUrl"] = ch.streamUrl;
    m["logoUrl"] = ch.logoUrl;
    m["groupTitle"] = ch.groupTitle;
    m["sourceNumber"] = ch.sourceNumber;
    m["channelId"] = ch.channelId;
    m["isVirtual"] = false;
    m["virtualType"] = QString();
    m["hidden"] = false;
    bool ok = false;
    m["displayNumberSort"] = ch.sourceNumber.toDouble(&ok);
    if (!ok) m["displayNumberSort"] = 0.0;
    m["displayNumber"] = ch.sourceNumber.isEmpty() ? QString() : ch.sourceNumber;
    return m;
}

M3uParseResult M3uParser::parse(const QByteArray &data) {
    M3uParseResult result;
    const QString text = QString::fromUtf8(data);
    const QStringList lines = text.split(QRegularExpression(QStringLiteral("[\r\n]+")),
                                         Qt::SkipEmptyParts);

    QString pendingExtinf;
    for (const QString &rawLine : lines) {
        QString line = rawLine.trimmed();
        if (line.isEmpty()) continue;

        if (line.startsWith(QStringLiteral("#EXTM3U"))) {
            QString epg = parseAttr(line, QStringLiteral("url-tvg"));
            if (epg.isEmpty()) epg = parseAttr(line, QStringLiteral("x-tvg-url"));
            result.epgUrlFromHeader = epg;
            continue;
        }

        if (line.startsWith(QStringLiteral("#EXTINF:"))) {
            pendingExtinf = line;
            continue;
        }

        if (line.startsWith('#')) continue;

        if (!pendingExtinf.isEmpty()) {
            M3uChannel ch;
            ch.tvgId = parseAttr(pendingExtinf, QStringLiteral("tvg-id"));
            ch.logoUrl = parseAttr(pendingExtinf, QStringLiteral("tvg-logo"));
            ch.groupTitle = parseAttr(pendingExtinf, QStringLiteral("group-title"));
            ch.channelId = parseAttr(pendingExtinf, QStringLiteral("channel-id"));
            ch.sourceNumber = parseAttr(pendingExtinf, QStringLiteral("tvg-chno"));
            if (ch.sourceNumber.isEmpty())
                ch.sourceNumber = parseAttr(pendingExtinf, QStringLiteral("channel-number"));
            ch.name = parseAttr(pendingExtinf, QStringLiteral("tvg-name"));
            int comma = pendingExtinf.lastIndexOf(',');
            if (comma >= 0) {
                QString after = pendingExtinf.mid(comma + 1).trimmed();
                if (!after.isEmpty()) ch.name = after;
            }
            ch.streamUrl = line;
            result.channels.append(ch);
            pendingExtinf.clear();
        }
    }

    if (result.channels.isEmpty() && result.error.isEmpty())
        result.error = QStringLiteral("No channels found in M3U");
    return result;
}

M3uParseResult M3uParser::parseFile(const QString &path) {
    M3uParseResult result;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        result.error = QStringLiteral("Cannot open M3U: %1").arg(path);
        return result;
    }
    return parse(f.readAll());
}
