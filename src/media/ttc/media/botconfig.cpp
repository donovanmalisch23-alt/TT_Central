// SPDX-License-Identifier: GPL-3.0-or-later
#include "ttc/media/botconfig.h"

#include "ttc/monitorconfig.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHostAddress>
#include <QStandardPaths>

namespace ttc::media {

QString MediaBotConfig::displayName() const
{
    if (!name.trimmed().isEmpty())
        return name.trimmed();
    return host.trimmed();
}

QString MediaBotConfig::validationError() const
{
    if (host.trimmed().isEmpty())
        return QObject::tr("Enter the server address for the media bot.");
    if (tcpPort < 1 || tcpPort > 65535 || udpPort < 1 || udpPort > 65535)
        return QObject::tr("Ports must be between 1 and 65535.");
    if (username.trimmed().isEmpty())
        return QObject::tr("Enter the username of the media bot's account.");
    if (nickname.trimmed().isEmpty())
        return QObject::tr("Enter a nickname for the media bot.");
    if (audio.defaultVolume < 0 || audio.defaultVolume > audio.maxVolume)
        return QObject::tr("The default volume must be between 0 and the maximum volume.");
    if (!sources.youtube && !sources.soundcloud && !sources.appleMusic && !sources.directUrl && !sources.spotify)
        return QObject::tr("Turn on at least one media source.");
    return {};
}

bool MediaBotConfig::operator==(const MediaBotConfig& o) const
{
    return name == o.name && host == o.host && tcpPort == o.tcpPort && udpPort == o.udpPort &&
           encrypted == o.encrypted && username == o.username && password == o.password &&
           nickname == o.nickname && statusMessage == o.statusMessage && channelPath == o.channelPath &&
           channelPassword == o.channelPassword && admins == o.admins && channelPrefix == o.channelPrefix &&
           announceNowPlaying == o.announceNowPlaying && reconnectSeconds == o.reconnectSeconds &&
           maxTrackSeconds == o.maxTrackSeconds && maxQueue == o.maxQueue && autoStart == o.autoStart &&
           audio.defaultVolume == o.audio.defaultVolume && audio.maxVolume == o.audio.maxVolume &&
           audio.leadMs == o.audio.leadMs && sources.youtube == o.sources.youtube &&
           sources.soundcloud == o.sources.soundcloud && sources.spotify == o.sources.spotify &&
           sources.spotifyClientId == o.sources.spotifyClientId &&
           sources.spotifyClientSecret == o.sources.spotifyClientSecret &&
           sources.appleMusic == o.sources.appleMusic && sources.directUrl == o.sources.directUrl &&
           sources.maxSearchResults == o.sources.maxSearchResults &&
           sources.maxPlaylistTracks == o.sources.maxPlaylistTracks && sources.ytdlpPath == o.sources.ytdlpPath &&
           sources.denoPath == o.sources.denoPath && ffmpegPath == o.ffmpegPath;
}

MediaBotConfig loadMediaBotConfig(QSettings& s, bool applyEnvironment)
{
    MediaBotConfig c;
    s.beginGroup(QStringLiteral("mediabot"));
    c.name = s.value(QStringLiteral("name"), c.name).toString();
    c.host = s.value(QStringLiteral("host"), c.host).toString();
    c.tcpPort = s.value(QStringLiteral("tcp-port"), c.tcpPort).toInt();
    c.udpPort = s.value(QStringLiteral("udp-port"), c.udpPort).toInt();
    c.encrypted = s.value(QStringLiteral("encrypted"), c.encrypted).toBool();
    c.username = s.value(QStringLiteral("username"), c.username).toString();
    c.password = s.value(QStringLiteral("password"), c.password).toString();
    c.nickname = s.value(QStringLiteral("nickname"), c.nickname).toString();
    c.statusMessage = s.value(QStringLiteral("status-message"), c.statusMessage).toString();
    c.channelPath = normalizeChannelPath(s.value(QStringLiteral("channel"), c.channelPath).toString());
    c.channelPassword = s.value(QStringLiteral("channel-password"), c.channelPassword).toString();
    c.admins = s.value(QStringLiteral("admins")).toStringList();
    c.admins.removeAll(QString());
    c.channelPrefix = s.value(QStringLiteral("channel-prefix"), c.channelPrefix).toString();
    c.announceNowPlaying = s.value(QStringLiteral("announce-now-playing"), c.announceNowPlaying).toBool();
    c.reconnectSeconds = qBound(1, s.value(QStringLiteral("reconnect-seconds"), c.reconnectSeconds).toInt(), 300);
    c.maxTrackSeconds = qMax(0, s.value(QStringLiteral("max-track-seconds"), c.maxTrackSeconds).toInt());
    c.maxQueue = qBound(1, s.value(QStringLiteral("max-queue"), c.maxQueue).toInt(), 10000);
    c.autoStart = s.value(QStringLiteral("auto-start"), c.autoStart).toBool();
    c.audio.defaultVolume = s.value(QStringLiteral("default-volume"), c.audio.defaultVolume).toInt();
    c.audio.maxVolume = qBound(1, s.value(QStringLiteral("max-volume"), c.audio.maxVolume).toInt(), 400);
    c.audio.leadMs = qBound(100, s.value(QStringLiteral("lead-ms"), c.audio.leadMs).toInt(), 2000);
    c.sources.youtube = s.value(QStringLiteral("youtube"), c.sources.youtube).toBool();
    c.sources.soundcloud = s.value(QStringLiteral("soundcloud"), c.sources.soundcloud).toBool();
    c.sources.spotify = s.value(QStringLiteral("spotify"), c.sources.spotify).toBool();
    c.sources.spotifyClientId = s.value(QStringLiteral("spotify-client-id")).toString();
    c.sources.spotifyClientSecret = s.value(QStringLiteral("spotify-client-secret")).toString();
    c.sources.appleMusic = s.value(QStringLiteral("apple-music"), c.sources.appleMusic).toBool();
    c.sources.directUrl = s.value(QStringLiteral("direct-urls"), c.sources.directUrl).toBool();
    c.sources.maxSearchResults = qBound(1, s.value(QStringLiteral("max-search-results"), c.sources.maxSearchResults).toInt(), 50);
    c.sources.maxPlaylistTracks = qBound(1, s.value(QStringLiteral("max-playlist-tracks"), c.sources.maxPlaylistTracks).toInt(), 5000);
    c.sources.ytdlpPath = s.value(QStringLiteral("ytdlp-path")).toString();
    c.sources.denoPath = s.value(QStringLiteral("deno-path")).toString();
    c.ffmpegPath = s.value(QStringLiteral("ffmpeg-path")).toString();
    s.endGroup();

    if (applyEnvironment)
    {
        auto env = [](const char* name, QString& target) {
            const QByteArray v = qgetenv(name);
            if (!v.isEmpty())
                target = QString::fromLocal8Bit(v);
        };
        env("TTC_MEDIABOT_PASSWORD", c.password);
        env("TTC_MEDIABOT_CHANNEL_PASSWORD", c.channelPassword);
        env("SPOTIFY_CLIENT_ID", c.sources.spotifyClientId);
        env("SPOTIFY_CLIENT_SECRET", c.sources.spotifyClientSecret);
    }
    return c;
}

void saveMediaBotConfig(QSettings& s, const MediaBotConfig& c)
{
    s.beginGroup(QStringLiteral("mediabot"));
    s.setValue(QStringLiteral("name"), c.name);
    s.setValue(QStringLiteral("host"), c.host.trimmed());
    s.setValue(QStringLiteral("tcp-port"), c.tcpPort);
    s.setValue(QStringLiteral("udp-port"), c.udpPort);
    s.setValue(QStringLiteral("encrypted"), c.encrypted);
    s.setValue(QStringLiteral("username"), c.username.trimmed());
    s.setValue(QStringLiteral("password"), c.password);
    s.setValue(QStringLiteral("nickname"), c.nickname);
    s.setValue(QStringLiteral("status-message"), c.statusMessage);
    s.setValue(QStringLiteral("channel"), normalizeChannelPath(c.channelPath));
    s.setValue(QStringLiteral("channel-password"), c.channelPassword);
    s.setValue(QStringLiteral("admins"), c.admins);
    s.setValue(QStringLiteral("channel-prefix"), c.channelPrefix);
    s.setValue(QStringLiteral("announce-now-playing"), c.announceNowPlaying);
    s.setValue(QStringLiteral("reconnect-seconds"), c.reconnectSeconds);
    s.setValue(QStringLiteral("max-track-seconds"), c.maxTrackSeconds);
    s.setValue(QStringLiteral("max-queue"), c.maxQueue);
    s.setValue(QStringLiteral("auto-start"), c.autoStart);
    s.setValue(QStringLiteral("default-volume"), c.audio.defaultVolume);
    s.setValue(QStringLiteral("max-volume"), c.audio.maxVolume);
    s.setValue(QStringLiteral("lead-ms"), c.audio.leadMs);
    s.setValue(QStringLiteral("youtube"), c.sources.youtube);
    s.setValue(QStringLiteral("soundcloud"), c.sources.soundcloud);
    s.setValue(QStringLiteral("spotify"), c.sources.spotify);
    s.setValue(QStringLiteral("spotify-client-id"), c.sources.spotifyClientId);
    s.setValue(QStringLiteral("spotify-client-secret"), c.sources.spotifyClientSecret);
    s.setValue(QStringLiteral("apple-music"), c.sources.appleMusic);
    s.setValue(QStringLiteral("direct-urls"), c.sources.directUrl);
    s.setValue(QStringLiteral("max-search-results"), c.sources.maxSearchResults);
    s.setValue(QStringLiteral("max-playlist-tracks"), c.sources.maxPlaylistTracks);
    s.setValue(QStringLiteral("ytdlp-path"), c.sources.ytdlpPath);
    s.setValue(QStringLiteral("deno-path"), c.sources.denoPath);
    s.setValue(QStringLiteral("ffmpeg-path"), c.ffmpegPath);
    s.endGroup();
}

QString findTool(const QString& configured, const QString& name, const QStringList& toolDirs)
{
    if (!configured.trimmed().isEmpty() && QFileInfo(configured.trimmed()).isFile())
        return QFileInfo(configured.trimmed()).absoluteFilePath();
#if defined(Q_OS_WIN)
    const QString exe = name + QStringLiteral(".exe");
#else
    const QString exe = name;
#endif
    for (const QString& dir : toolDirs)
    {
        for (const QString& rel : {exe, name + QChar('/') + exe, name + QStringLiteral("/bin/") + exe})
        {
            const QFileInfo fi(QDir(dir).filePath(rel));
            if (fi.isFile())
                return fi.absoluteFilePath();
        }
    }
    return QStandardPaths::findExecutable(name);
}

QStringList defaultToolDirs()
{
    QStringList dirs;
    const QByteArray env = qgetenv("TTC_TOOLS_DIR");
    if (!env.isEmpty())
        dirs << QString::fromLocal8Bit(env);
    const QDir app(QCoreApplication::applicationDirPath());
    dirs << app.filePath(QStringLiteral("tools")) << app.filePath(QStringLiteral("../tools"))
         << app.filePath(QStringLiteral("../../tools"));
    return dirs;
}

} // namespace ttc::media
