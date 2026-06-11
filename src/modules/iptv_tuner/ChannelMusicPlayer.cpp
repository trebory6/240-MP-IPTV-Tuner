#include "ChannelMusicPlayer.h"
#include <QStandardPaths>
#include <QDebug>

ChannelMusicPlayer::ChannelMusicPlayer(QObject *parent) : QObject(parent) {}

ChannelMusicPlayer::~ChannelMusicPlayer() {
    stop();
}

void ChannelMusicPlayer::ensureMpvPath() {
#ifdef Q_OS_MACOS
    const QStringList extraPaths = { QStringLiteral("/opt/homebrew/bin"),
                                     QStringLiteral("/usr/local/bin") };
    const QStringList current = qEnvironmentVariable("PATH").split(QLatin1Char(':'));
    for (const QString &p : extraPaths) {
        if (!current.contains(p))
            qputenv("PATH", (p + QLatin1Char(':') + qEnvironmentVariable("PATH")).toUtf8());
    }
#endif
}

void ChannelMusicPlayer::startAt(const QString &playlistPath, int playlistIndex, double startSec) {
    stop();
    if (playlistPath.isEmpty())
        return;

    ensureMpvPath();
    const QString bin = QStandardPaths::findExecutable(QStringLiteral("mpv"));
    if (bin.isEmpty()) {
        qWarning("[IPTV] mpv not found in PATH — channel music will not play");
        return;
    }

    QStringList args;
    args << playlistPath
         << QStringLiteral("--no-video")
         << QStringLiteral("--loop-playlist=inf")
         << QStringLiteral("--no-terminal")
         << QStringLiteral("--really-quiet")
         << QStringLiteral("--playlist-start=%1").arg(qMax(0, playlistIndex))
         << QStringLiteral("--start=%1").arg(qMax(0.0, startSec));

    m_process = new QProcess(this);
    m_process->start(bin, args);
    qDebug("[IPTV] channel music started: %s index=%d start=%.1f",
           qPrintable(playlistPath), playlistIndex, startSec);
}

void ChannelMusicPlayer::stop() {
    if (!m_process)
        return;
    if (m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        m_process->waitForFinished(1000);
    }
    m_process->deleteLater();
    m_process = nullptr;
}

bool ChannelMusicPlayer::isPlaying() const {
    return m_process && m_process->state() != QProcess::NotRunning;
}
