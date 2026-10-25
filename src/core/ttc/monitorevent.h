// TT Central — an event raised by a background monitor.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QString>

namespace ttc {

struct MonitorEvent
{
    enum class Kind
    {
        Online,            // logged in and stationed in its channel
        ConnectionLost,    // dropped; reconnecting
        ConnectFailed,     // could not reach the server; retrying
        LoginFailed,       // the server refused the login (text = reason)
        ChannelJoinFailed, // could not join the configured channel; using root
        Kicked,            // kicked off the server; reconnecting
        UserLoggedIn,
        UserLoggedOut,
        PrivateMessage,    // a private message sent to the monitor
        ChannelMessage,    // a message in the monitor's channel
        BroadcastMessage,  // a server-wide broadcast
    };

    Kind kind = Kind::Online;
    QString monitorId;
    QString serverName;    // the monitor's display name ("which server")
    QString nickname;      // the user involved, if any
    QString username;
    QString channel;       // channel name/path, if relevant
    QString text;          // message text or failure reason
    QDateTime time = QDateTime::currentDateTime();

    /// Whether this event is about the monitor's own connection.
    bool isConnectionEvent() const;
    /// The sentence used for speech, braille, the log and toasts, always
    /// starting with the server name, e.g. "Home Server: Alice logged in".
    QString describe() const;
};

} // namespace ttc

Q_DECLARE_METATYPE(ttc::MonitorEvent)
