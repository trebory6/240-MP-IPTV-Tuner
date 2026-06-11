#pragma once
#include <QString>
#include <QVector>
#include <QVariantMap>

struct M3uChannel {
    QString tvgId;
    QString name;
    QString streamUrl;
    QString logoUrl;
    QString groupTitle;
    QString sourceNumber;
    QString channelId;
};

struct M3uParseResult {
    QString epgUrlFromHeader;
    QVector<M3uChannel> channels;
    QString error;
};

class M3uParser {
public:
    static M3uParseResult parse(const QByteArray &data);
    static M3uParseResult parseFile(const QString &path);
};

QVariantMap m3uChannelToVariant(const M3uChannel &ch);
