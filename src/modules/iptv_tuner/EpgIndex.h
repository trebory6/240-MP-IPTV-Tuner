#pragma once
#include <QVector>
#include <QVariantMap>
#include <QDateTime>
#include "XmltvParser.h"

class EpgIndex {
public:
    void setProgrammes(const QVector<XmltvProgramme> &programmes);
    QVariantList programmesForChannel(const QString &channelId, const QDateTime &from,
                                      const QDateTime &to) const;
    QVariantList allProgrammes() const;

private:
    QVector<XmltvProgramme> m_programmes;
};
