// TT Central — configuration of one background monitor.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QString>

class QSettings;

namespace ttc {

struct MonitorConfig
{
    QString id;               // stable identity (UUID)
    QString name;             // label used in notifications, e.g. "Home Server"
    bool enabled = true;

    QString host;
    int tcpPort = 10333;
    int udpPort = 10333;
    bool encrypted = false;

    QString username;         // may be your own account (see oneLoginPerIpNote())
    QString password;
    QString nickname = QStringLiteral("Monitor");
    QString statusMessage = QStringLiteral("TT Central monitor");

    QString channelPath = QStringLiteral("/");
    QString channelPassword;

    // What to notify about.
    bool notifyLogins = true;
    bool notifyLogouts = true;
    bool notifyPrivateMessages = true;
    bool notifyChannelMessages = true;
    bool notifyBroadcasts = false;
    bool notifyConnection = true;     // online / lost / login failed

    /// Skip login/logout notices for this monitor's own account (when it
    /// shares your account, those are your own sessions).
    bool ignoreOwnAccount = true;
    /// Receive channel audio. Off by default to save bandwidth: a monitor
    /// only needs text and presence events.
    bool receiveAudio = false;

    /// Label shown in every notification ("which server it came from").
    QString displayName() const;
    /// A problem that prevents starting, or an empty string when valid.
    QString validationError() const;
    /// Create a fresh config with a new id.
    static MonitorConfig create();

    bool operator==(const MonitorConfig& o) const;
    bool operator!=(const MonitorConfig& o) const { return !(*this == o); }
};

/// Canonical TeamTalk channel path: "/" for the root channel, otherwise
/// "/Parent/Child/" (accepts "Lobby", "/Lobby", "Lobby/" ...).
QString normalizeChannelPath(const QString& path);

/// The guidance shown in Settings → Monitors about sharing your account.
QString oneLoginPerIpNote();

/// Persist the monitor list under the group "ttcentral/monitors".
QList<MonitorConfig> loadMonitorConfigs(QSettings& settings);
void saveMonitorConfigs(QSettings& settings, const QList<MonitorConfig>& monitors);

} // namespace ttc
