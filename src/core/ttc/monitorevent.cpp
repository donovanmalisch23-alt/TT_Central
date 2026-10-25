// SPDX-License-Identifier: MIT
#include "ttc/monitorevent.h"

#include <QCoreApplication>

namespace ttc {

namespace {
QString tr(const char* s) { return QCoreApplication::translate("ttc::MonitorEvent", s); }
} // namespace

bool MonitorEvent::isConnectionEvent() const
{
    switch (kind)
    {
    case Kind::Online:
    case Kind::ConnectionLost:
    case Kind::ConnectFailed:
    case Kind::LoginFailed:
    case Kind::ChannelJoinFailed:
    case Kind::Kicked:
        return true;
    default:
        return false;
    }
}

QString MonitorEvent::describe() const
{
    const QString who = nickname.isEmpty() ? username : nickname;
    QString body;
    switch (kind)
    {
    case Kind::Online:
        body = channel.isEmpty() ? tr("monitor online")
                                 : tr("monitor online in %1").arg(channel);
        break;
    case Kind::ConnectionLost:
        body = tr("connection lost, reconnecting");
        break;
    case Kind::ConnectFailed:
        body = text.isEmpty() ? tr("unable to connect, retrying")
                              : tr("unable to connect (%1), retrying").arg(text);
        break;
    case Kind::LoginFailed:
        body = tr("login failed: %1").arg(text);
        break;
    case Kind::ChannelJoinFailed:
        body = tr("could not join %1 (%2), monitoring from the root channel").arg(channel, text);
        break;
    case Kind::Kicked:
        if (!channel.isEmpty()) // kicked from its channel only; still on the server
            body = who.isEmpty() ? tr("monitor was kicked from %1").arg(channel)
                                 : tr("monitor was kicked from %1 by %2").arg(channel, who);
        else
            body = who.isEmpty() ? tr("monitor was kicked, reconnecting")
                                 : tr("monitor was kicked by %1, reconnecting").arg(who);
        break;
    case Kind::UserLoggedIn:
        body = tr("%1 logged in").arg(who);
        break;
    case Kind::UserLoggedOut:
        body = tr("%1 logged out").arg(who);
        break;
    case Kind::PrivateMessage:
        body = tr("private message from %1: %2").arg(who, text);
        break;
    case Kind::ChannelMessage:
        body = channel.isEmpty() ? tr("%1: %2").arg(who, text)
                                 : tr("%1 in %2: %3").arg(who, channel, text);
        break;
    case Kind::BroadcastMessage:
        body = tr("broadcast from %1: %2").arg(who, text);
        break;
    }
    return tr("%1: %2").arg(serverName, body);
}

} // namespace ttc
