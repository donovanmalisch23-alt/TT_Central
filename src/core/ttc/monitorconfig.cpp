// SPDX-License-Identifier: MIT
#include "ttc/monitorconfig.h"

#include <QCoreApplication>
#include <QSettings>
#include <QUuid>

namespace ttc {

namespace {
const char* const kGroup = "ttcentral/monitors";
QString tr(const char* s) { return QCoreApplication::translate("ttc::MonitorConfig", s); }
} // namespace

QString MonitorConfig::displayName() const
{
    const QString n = name.trimmed();
    if (!n.isEmpty())
        return n;
    return host.trimmed().isEmpty() ? tr("Unnamed monitor") : host.trimmed();
}

QString MonitorConfig::validationError() const
{
    if (host.trimmed().isEmpty())
        return tr("Enter the server address.");
    if (tcpPort <= 0 || tcpPort > 65535)
        return tr("The TCP port must be between 1 and 65535.");
    if (udpPort <= 0 || udpPort > 65535)
        return tr("The UDP port must be between 1 and 65535.");
    if (nickname.trimmed().isEmpty())
        return tr("Enter a nickname for the monitor.");
    return {};
}

MonitorConfig MonitorConfig::create()
{
    MonitorConfig c;
    c.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    return c;
}

bool MonitorConfig::operator==(const MonitorConfig& o) const
{
    return id == o.id && name == o.name && enabled == o.enabled && host == o.host &&
           tcpPort == o.tcpPort && udpPort == o.udpPort && encrypted == o.encrypted &&
           username == o.username && password == o.password && nickname == o.nickname &&
           statusMessage == o.statusMessage && channelPath == o.channelPath &&
           channelPassword == o.channelPassword && notifyLogins == o.notifyLogins &&
           notifyLogouts == o.notifyLogouts && notifyPrivateMessages == o.notifyPrivateMessages &&
           notifyChannelMessages == o.notifyChannelMessages && notifyBroadcasts == o.notifyBroadcasts &&
           notifyConnection == o.notifyConnection && ignoreOwnAccount == o.ignoreOwnAccount &&
           receiveAudio == o.receiveAudio;
}

QString oneLoginPerIpNote()
{
    return tr("A monitor can log in with your own account. However, if a TeamTalk "
              "server only allows one login per IP address, create a separate account "
              "for the monitor on that server; otherwise the monitor (or you) will be "
              "refused.");
}

QList<MonitorConfig> loadMonitorConfigs(QSettings& s)
{
    QList<MonitorConfig> list;
    const int n = s.beginReadArray(QLatin1String(kGroup));
    for (int i = 0; i < n; ++i)
    {
        s.setArrayIndex(i);
        MonitorConfig c;
        c.id = s.value("id").toString();
        if (c.id.isEmpty())
            c.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        c.name = s.value("name").toString();
        c.enabled = s.value("enabled", c.enabled).toBool();
        c.host = s.value("host").toString();
        c.tcpPort = s.value("tcpport", c.tcpPort).toInt();
        c.udpPort = s.value("udpport", c.udpPort).toInt();
        c.encrypted = s.value("encrypted", c.encrypted).toBool();
        c.username = s.value("username").toString();
        c.password = s.value("password").toString();
        c.nickname = s.value("nickname", c.nickname).toString();
        c.statusMessage = s.value("statusmsg", c.statusMessage).toString();
        c.channelPath = s.value("channel", c.channelPath).toString();
        c.channelPassword = s.value("chanpassword").toString();
        c.notifyLogins = s.value("notify-logins", c.notifyLogins).toBool();
        c.notifyLogouts = s.value("notify-logouts", c.notifyLogouts).toBool();
        c.notifyPrivateMessages = s.value("notify-private", c.notifyPrivateMessages).toBool();
        c.notifyChannelMessages = s.value("notify-channel", c.notifyChannelMessages).toBool();
        c.notifyBroadcasts = s.value("notify-broadcast", c.notifyBroadcasts).toBool();
        c.notifyConnection = s.value("notify-connection", c.notifyConnection).toBool();
        c.ignoreOwnAccount = s.value("ignore-own-account", c.ignoreOwnAccount).toBool();
        c.receiveAudio = s.value("receive-audio", c.receiveAudio).toBool();
        list << c;
    }
    s.endArray();
    return list;
}

void saveMonitorConfigs(QSettings& s, const QList<MonitorConfig>& monitors)
{
    s.remove(QLatin1String(kGroup));
    s.beginWriteArray(QLatin1String(kGroup), int(monitors.size()));
    for (int i = 0; i < monitors.size(); ++i)
    {
        const MonitorConfig& c = monitors.at(i);
        s.setArrayIndex(i);
        s.setValue("id", c.id);
        s.setValue("name", c.name);
        s.setValue("enabled", c.enabled);
        s.setValue("host", c.host);
        s.setValue("tcpport", c.tcpPort);
        s.setValue("udpport", c.udpPort);
        s.setValue("encrypted", c.encrypted);
        s.setValue("username", c.username);
        s.setValue("password", c.password);
        s.setValue("nickname", c.nickname);
        s.setValue("statusmsg", c.statusMessage);
        s.setValue("channel", c.channelPath);
        s.setValue("chanpassword", c.channelPassword);
        s.setValue("notify-logins", c.notifyLogins);
        s.setValue("notify-logouts", c.notifyLogouts);
        s.setValue("notify-private", c.notifyPrivateMessages);
        s.setValue("notify-channel", c.notifyChannelMessages);
        s.setValue("notify-broadcast", c.notifyBroadcasts);
        s.setValue("notify-connection", c.notifyConnection);
        s.setValue("ignore-own-account", c.ignoreOwnAccount);
        s.setValue("receive-audio", c.receiveAudio);
    }
    s.endArray();
}

} // namespace ttc
