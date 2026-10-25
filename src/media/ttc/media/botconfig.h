// TT Central media bot — configuration (an INI file written by TT Central's
// media bot settings, or by hand for a standalone bot).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ttc/media/audioengine.h"
#include "ttc/media/sources.h"

#include <QSettings>
#include <QString>
#include <QStringList>

namespace ttc::media {

struct MediaBotConfig
{
    // Server and account
    QString name;              // shown in TT Central; defaults to the host
    QString host;
    int tcpPort = 10333;
    int udpPort = 10333;
    bool encrypted = false;
    QString username;
    QString password;
    QString nickname = QStringLiteral("MediaBot");
    QString statusMessage = QStringLiteral("Send me a private message with 'h' for help");
    QString channelPath = QStringLiteral("/");
    QString channelPassword;

    // Behaviour
    QStringList admins;        // TeamTalk usernames; server admins always count
    QString channelPrefix;     // commands in the channel need this prefix; empty = PM only
    bool announceNowPlaying = true;
    int reconnectSeconds = 5;
    int maxTrackSeconds = 0;   // 0 = unlimited
    int maxQueue = 1000;
    bool autoStart = false;    // TT Central starts the bot when it starts

    AudioSettings audio;
    SourcesSettings sources;
    QString ffmpegPath;        // empty = find it (tools folder, then PATH)

    QString displayName() const;
    /// Empty when usable; otherwise a sentence saying what to fix.
    QString validationError() const;
    bool operator==(const MediaBotConfig& o) const;
    bool operator!=(const MediaBotConfig& o) const { return !(*this == o); }
};

/// Read/write the [mediabot] group. Secrets can also come from the
/// environment: TTC_MEDIABOT_PASSWORD, TTC_MEDIABOT_CHANNEL_PASSWORD,
/// SPOTIFY_CLIENT_ID and SPOTIFY_CLIENT_SECRET win over the file.
MediaBotConfig loadMediaBotConfig(QSettings& s, bool applyEnvironment = true);
void saveMediaBotConfig(QSettings& s, const MediaBotConfig& c);

/// Locate a helper tool: the configured path if it exists, then the tools
/// folders (`<dir>/<name>[.exe]`, `<dir>/<name>/<name>[.exe]`,
/// `<dir>/<name>/bin/<name>[.exe]`), then PATH. Empty when not found.
QString findTool(const QString& configured, const QString& name, const QStringList& toolDirs);

/// The tools folders searched by default: TTC_TOOLS_DIR, then `tools` next to
/// the executable and one and two levels up (portable and development layouts).
QStringList defaultToolDirs();

} // namespace ttc::media
