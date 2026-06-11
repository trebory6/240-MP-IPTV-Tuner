#pragma once
#include <QString>
#include <QVector>
#include <QDateTime>
#include <QVariantMap>

struct XmltvProgramme {
    QString channelId;
    QDateTime start;
    QDateTime stop;
    QString title;
    QString subTitle;
    QString description;
    QStringList categories;
    QString iconUrl;
    QString contentType;
};

class XmltvParser {
public:
    static bool parse(const QByteArray &data, QString *error,
                      QVector<QVariantMap> *channelsOut,
                      QVector<XmltvProgramme> *programmesOut);
    static bool parseFile(const QString &path, QString *error,
                          QVector<QVariantMap> *channelsOut,
                          QVector<XmltvProgramme> *programmesOut);
};

QVariantMap xmltvProgrammeToVariant(const XmltvProgramme &p);
