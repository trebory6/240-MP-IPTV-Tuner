#include "EpgIndex.h"
#include <algorithm>

void EpgIndex::setProgrammes(const QVector<XmltvProgramme> &programmes) {
    m_programmes = programmes;
    std::sort(m_programmes.begin(), m_programmes.end(),
              [](const XmltvProgramme &a, const XmltvProgramme &b) {
                  return a.start < b.start;
              });
}

QVariantList EpgIndex::programmesForChannel(const QString &channelId, const QDateTime &from,
                                            const QDateTime &to) const {
    QVariantList list;
    for (const XmltvProgramme &p : m_programmes) {
        if (p.channelId != channelId) continue;
        if (p.stop < from || p.start > to) continue;
        list.append(xmltvProgrammeToVariant(p));
    }
    return list;
}

QVariantList EpgIndex::allProgrammes() const {
    QVariantList list;
    for (const XmltvProgramme &p : m_programmes)
        list.append(xmltvProgrammeToVariant(p));
    return list;
}
