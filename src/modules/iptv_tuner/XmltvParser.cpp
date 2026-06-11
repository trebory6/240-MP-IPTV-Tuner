#include "XmltvParser.h"
#include <QFile>
#include <QXmlStreamReader>
#include <QRegularExpression>

static QDateTime parseXmltvTime(const QString &s) {
    // YYYYMMDDhhmmss ±ZZZZ
    if (s.length() < 15) return {};
    const QString core = s.left(14);
    QDateTime dt = QDateTime::fromString(core, QStringLiteral("yyyyMMddHHmmss"));
    if (s.length() >= 20) {
        const QString tz = s.mid(15).trimmed();
        static const QRegularExpression re(QStringLiteral(R"(([+-])(\d{2})(\d{2}))"));
        auto m = re.match(tz);
        if (m.hasMatch()) {
            int sign = m.captured(1) == QLatin1Char('-') ? -1 : 1;
            int offMin = sign * (m.captured(2).toInt() * 60 + m.captured(3).toInt());
            dt.setOffsetFromUtc(offMin * 60);
        }
    }
    return dt;
}

static QString deriveContentType(const QStringList &categories) {
    for (const QString &c : categories) {
        const QString l = c.toLower();
        if (l.contains(QStringLiteral("movie"))) return QStringLiteral("movies");
        if (l.contains(QStringLiteral("sport"))) return QStringLiteral("sports");
        if (l.contains(QStringLiteral("news"))) return QStringLiteral("news");
        if (l.contains(QStringLiteral("kid")) || l.contains(QStringLiteral("child")))
            return QStringLiteral("kids");
        if (l.contains(QStringLiteral("series")) || l.contains(QStringLiteral("drama"))
            || l.contains(QStringLiteral("comedy")) || l.contains(QStringLiteral("action")))
            return QStringLiteral("series");
    }
    return QStringLiteral("other");
}

QVariantMap xmltvProgrammeToVariant(const XmltvProgramme &p) {
    QVariantMap m;
    m["channelId"] = p.channelId;
    m["start"] = p.start;
    m["stop"] = p.stop;
    m["title"] = p.title;
    m["subTitle"] = p.subTitle;
    m["description"] = p.description;
    m["categories"] = p.categories;
    m["iconUrl"] = p.iconUrl;
    m["contentType"] = p.contentType;
    return m;
}

bool XmltvParser::parse(const QByteArray &data, QString *error,
                        QVector<QVariantMap> *channelsOut,
                        QVector<XmltvProgramme> *programmesOut) {
    QXmlStreamReader xml(data);
    XmltvProgramme currentProg;
    bool inProgramme = false;
    QString currentChannelId;
    QStringList displayNames;
    QString channelIcon;

    auto flushChannel = [&]() {
        if (currentChannelId.isEmpty()) return;
        QVariantMap ch;
        ch["id"] = currentChannelId;
        ch["displayNames"] = displayNames;
        ch["iconUrl"] = channelIcon;
        channelsOut->append(ch);
        currentChannelId.clear();
        displayNames.clear();
        channelIcon.clear();
    };

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const QString name = xml.name().toString();
            if (name == QLatin1String("channel")) {
                flushChannel();
                currentChannelId = xml.attributes().value(QStringLiteral("id")).toString();
            } else if (name == QLatin1String("display-name") && !currentChannelId.isEmpty() && !inProgramme) {
                displayNames.append(xml.readElementText());
            } else if (name == QLatin1String("icon") && !currentChannelId.isEmpty() && !inProgramme) {
                channelIcon = xml.attributes().value(QStringLiteral("src")).toString();
            } else if (name == QLatin1String("programme")) {
                inProgramme = true;
                currentProg = {};
                currentProg.channelId = xml.attributes().value(QStringLiteral("channel")).toString();
                currentProg.start = parseXmltvTime(xml.attributes().value(QStringLiteral("start")).toString());
                currentProg.stop = parseXmltvTime(xml.attributes().value(QStringLiteral("stop")).toString());
            } else if (inProgramme) {
                if (name == QLatin1String("title"))
                    currentProg.title = xml.readElementText();
                else if (name == QLatin1String("sub-title"))
                    currentProg.subTitle = xml.readElementText();
                else if (name == QLatin1String("desc"))
                    currentProg.description = xml.readElementText();
                else if (name == QLatin1String("category"))
                    currentProg.categories.append(xml.readElementText());
                else if (name == QLatin1String("icon"))
                    currentProg.iconUrl = xml.attributes().value(QStringLiteral("src")).toString();
            }
        } else if (xml.isEndElement()) {
            const QString name = xml.name().toString();
            if (name == QLatin1String("channel")) {
                flushChannel();
            } else if (name == QLatin1String("programme") && inProgramme) {
                currentProg.contentType = deriveContentType(currentProg.categories);
                programmesOut->append(currentProg);
                inProgramme = false;
            }
        }
    }

    flushChannel();

    if (xml.hasError()) {
        if (error) *error = xml.errorString();
        return false;
    }
    return true;
}

bool XmltvParser::parseFile(const QString &path, QString *error,
                            QVector<QVariantMap> *channelsOut,
                            QVector<XmltvProgramme> *programmesOut) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("Cannot open XMLTV: %1").arg(path);
        return false;
    }
    return parse(f.readAll(), error, channelsOut, programmesOut);
}
