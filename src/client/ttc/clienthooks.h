// TT Central — what the host client (qtTeamTalk) provides to TT Central.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// ttc_client never includes qtTeamTalk headers. The client integration file
// fills in these hooks with qtTeamTalk's own settings store, TTS/Prism
// speech, sound packs, toasts, saved servers and preferences dialog.

#pragma once

#include "ttc/notificationcenter.h"

#include <QList>
#include <QString>

class QAction;

#include <functional>

class QSettings;

namespace ttc {

/// A server saved in the client's server list (for "Import saved server").
struct SavedServer
{
    QString name;
    QString host;
    int tcpPort = 10333;
    int udpPort = 10333;
    bool encrypted = false;
    QString username;
    QString password;
    QString nickname;
    QString channel;
    QString channelPassword;
};

struct ClientHooks
{
    QSettings* settings = nullptr;
    NotificationOutputs outputs;
    std::function<QList<SavedServer>()> savedServers;
    std::function<void()> openClientPreferences;
    /// Add a server to the client's server list (e.g. your local server).
    std::function<void(const SavedServer&)> addSavedServer;
    /// The client's own server administration actions (server properties,
    /// accounts, bans, ...), shown again in TT Central's menu.
    std::function<QList<QAction*>()> adminActions;
};

} // namespace ttc
