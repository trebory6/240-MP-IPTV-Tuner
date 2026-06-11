#pragma once
#include <QVector>
#include <QVariantMap>
#include <QString>

class ChannelRegistry {
public:
    void setRawChannels(const QVector<QVariantMap> &channels);
    void applySettings(const QVariantMap &moduleConfig);
    QVector<QVariantMap> visibleChannels() const;
    QVariantMap resolveByNumber(const QString &displayNumber) const;
    QVariantMap adjacentChannel(const QString &currentNumber, int direction) const;
    QStringList uniqueGroupTitles() const;

private:
    QVariantMap makeVirtualChannel(const QString &type, const QString &number) const;
    bool isCategoryEnabled(const QString &groupTitle) const;
    void rebuild();

    QVector<QVariantMap> m_raw;
    QVector<QVariantMap> m_channels;
    QVariantMap m_config;
};
