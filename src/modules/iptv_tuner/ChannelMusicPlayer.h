#pragma once
#include <QObject>
#include <QProcess>

class ChannelMusicPlayer : public QObject {
    Q_OBJECT
public:
    explicit ChannelMusicPlayer(QObject *parent = nullptr);
    ~ChannelMusicPlayer() override;

    void startAt(const QString &playlistPath, int playlistIndex, double startSec);
    void stop();
    bool isPlaying() const;

private:
    void ensureMpvPath();

    QProcess *m_process = nullptr;
};
