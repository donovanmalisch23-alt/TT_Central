// SPDX-License-Identifier: MIT
#include "ttc/serverconfig.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDomDocument>
#include <QFile>
#include <QHostAddress>
#include <QSaveFile>
#include <QSet>

namespace ttc {

namespace {

QString tr(const char* s) { return QCoreApplication::translate("ttc::ServerConfig", s); }

QDomElement child(QDomElement parent, const QString& name, bool create)
{
    QDomElement e = parent.firstChildElement(name);
    if (e.isNull() && create)
    {
        e = parent.ownerDocument().createElement(name);
        parent.appendChild(e);
    }
    return e;
}

void setText(QDomElement parent, const QString& name, const QString& value)
{
    QDomElement e = child(parent, name, true);
    while (e.hasChildNodes())
        e.removeChild(e.firstChild());
    e.appendChild(parent.ownerDocument().createTextNode(value));
}

QString text(const QDomElement& parent, const QString& name, const QString& def = {})
{
    const QDomElement e = parent.firstChildElement(name);
    return e.isNull() ? def : e.text();
}

QString boolText(bool b) { return b ? QStringLiteral("true") : QStringLiteral("false"); }
bool toBool(const QString& s, bool def) { return s.isEmpty() ? def : (s.trimmed() == QLatin1String("true")); }

QString now() { return QDateTime::currentDateTime().toString(QStringLiteral("yyyy/MM/dd HH:mm")); }

void fillNewUser(QDomElement user, const ServerAccount& a)
{
    setText(user, QStringLiteral("username"), a.username);
    setText(user, QStringLiteral("password"), a.password);
    setText(user, QStringLiteral("user-type"), a.admin ? QStringLiteral("2") : QStringLiteral("1"));
    setText(user, QStringLiteral("user-rights"), a.admin ? QStringLiteral("0") : QString::number(kDefaultUserRights));
    setText(user, QStringLiteral("note"), a.note);
    setText(user, QStringLiteral("userdata"), QStringLiteral("0"));
    setText(user, QStringLiteral("init-channel"), QString());
    setText(user, QStringLiteral("modified-time"), now());
    setText(user, QStringLiteral("last-login-time"), QStringLiteral("1970/01/01 00:00"));
    setText(user, QStringLiteral("audiocodec-bps-limit"), QStringLiteral("0"));
    QDomElement abuse = child(user, QStringLiteral("abuse-prevention"), true);
    setText(abuse, QStringLiteral("commands-limit"), QStringLiteral("0"));
    setText(abuse, QStringLiteral("commands-interval-msec"), QStringLiteral("0"));
    child(user, QStringLiteral("channel-operator"), true);
}

} // namespace

QString LocalServerConfig::validationError() const
{
    if (serverName.trimmed().isEmpty())
        return tr("Enter a server name.");
    if (tcpPort <= 0 || tcpPort > 65535 || udpPort <= 0 || udpPort > 65535)
        return tr("Ports must be between 1 and 65535.");
    if (maxUsers <= 0)
        return tr("The maximum number of users must be at least 1.");
    if (!bindIp.trimmed().isEmpty() && QHostAddress(bindIp.trimmed()).isNull())
        return tr("The bind address is not a valid IP address.");
    QSet<QString> seen;
    for (const ServerAccount& a : accounts)
    {
        const QString name = a.username.trimmed().toLower();
        if (seen.contains(name))
            return tr("The account \"%1\" is listed more than once.").arg(a.username);
        seen.insert(name);
    }
    if (accounts.isEmpty())
        return tr("Add at least one account, or nobody will be able to log in.");
    return {};
}

bool LocalServerConfig::operator==(const LocalServerConfig& o) const
{
    return serverName == o.serverName && motd == o.motd && maxUsers == o.maxUsers &&
           autoSave == o.autoSave && tcpPort == o.tcpPort && udpPort == o.udpPort &&
           bindIp == o.bindIp && upnp == o.upnp && loginAttempts == o.loginAttempts &&
           maxLoginsPerIp == o.maxLoginsPerIp && loginDelayMsec == o.loginDelayMsec &&
           filesRoot == o.filesRoot && maxDiskUsage == o.maxDiskUsage &&
           channelDiskQuota == o.channelDiskQuota && accounts == o.accounts;
}

bool readServerXml(const QString& path, LocalServerConfig& out, QString* error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        if (error) *error = tr("Cannot open %1").arg(path);
        return false;
    }
    QDomDocument doc;
    const auto parsed = doc.setContent(&f);
    if (!parsed)
    {
        if (error) *error = tr("%1 is not valid XML").arg(path);
        return false;
    }
    const QDomElement root = doc.documentElement();
    if (root.tagName() != QLatin1String("teamtalk"))
    {
        if (error) *error = tr("%1 is not a TeamTalk server configuration").arg(path);
        return false;
    }

    LocalServerConfig c;
    const QDomElement g = root.firstChildElement(QStringLiteral("general"));
    c.serverName = text(g, QStringLiteral("server-name"), c.serverName);
    c.motd = text(g, QStringLiteral("motd"));
    c.maxUsers = text(g, QStringLiteral("max-users"), QString::number(c.maxUsers)).toInt();
    c.autoSave = toBool(text(g, QStringLiteral("auto-save")), c.autoSave);
    c.tcpPort = text(g, QStringLiteral("tcpport"), QString::number(c.tcpPort)).toInt();
    c.udpPort = text(g, QStringLiteral("udpport"), QString::number(c.udpPort)).toInt();
    c.bindIp = text(g, QStringLiteral("bind-ip"));
    c.upnp = toBool(text(g, QStringLiteral("upnp")), c.upnp);
    c.loginAttempts = text(g, QStringLiteral("login-attempts"), QStringLiteral("0")).toInt();
    c.maxLoginsPerIp = text(g, QStringLiteral("max-logins-per-ipaddr"), QStringLiteral("0")).toInt();
    c.loginDelayMsec = text(g, QStringLiteral("login-delay-msec"), QStringLiteral("0")).toInt();

    const QDomElement fs = root.firstChildElement(QStringLiteral("file-storage"));
    c.filesRoot = text(fs, QStringLiteral("files-root"));
    c.maxDiskUsage = text(fs, QStringLiteral("max-diskusage"), QStringLiteral("0")).toLongLong();
    c.channelDiskQuota = text(fs, QStringLiteral("channel-diskquota"), QStringLiteral("0")).toLongLong();

    const QDomElement users = root.firstChildElement(QStringLiteral("users"));
    for (QDomElement u = users.firstChildElement(QStringLiteral("user")); !u.isNull();
         u = u.nextSiblingElement(QStringLiteral("user")))
    {
        ServerAccount a;
        a.username = text(u, QStringLiteral("username"));
        a.password = text(u, QStringLiteral("password"));
        a.admin = text(u, QStringLiteral("user-type")).trimmed() == QLatin1String("2");
        a.note = text(u, QStringLiteral("note"));
        c.accounts << a;
    }
    out = c;
    return true;
}

bool writeServerXml(const QString& path, const LocalServerConfig& cfg, QString* error)
{
    QDomDocument doc;
    {
        QFile in(path);
        if (in.exists() && in.open(QIODevice::ReadOnly))
        {
            const auto parsed = doc.setContent(&in);
            if (!parsed || doc.documentElement().tagName() != QLatin1String("teamtalk"))
            {
                if (error) *error = tr("%1 exists but is not a TeamTalk server configuration").arg(path);
                return false;
            }
        }
    }
    if (doc.documentElement().isNull())
    {
        doc.appendChild(doc.createProcessingInstruction(QStringLiteral("xml"),
                                                        QStringLiteral("version=\"1.0\" encoding=\"UTF-8\"")));
        QDomElement root = doc.createElement(QStringLiteral("teamtalk"));
        root.setAttribute(QStringLiteral("version"), QStringLiteral("5.3"));
        doc.appendChild(root);
    }
    QDomElement root = doc.documentElement();

    QDomElement g = child(root, QStringLiteral("general"), true);
    setText(g, QStringLiteral("server-name"), cfg.serverName);
    setText(g, QStringLiteral("motd"), cfg.motd);
    setText(g, QStringLiteral("max-users"), QString::number(cfg.maxUsers));
    setText(g, QStringLiteral("auto-save"), boolText(cfg.autoSave));
    setText(g, QStringLiteral("tcpport"), QString::number(cfg.tcpPort));
    setText(g, QStringLiteral("udpport"), QString::number(cfg.udpPort));
    setText(g, QStringLiteral("upnp"), boolText(cfg.upnp));
    setText(g, QStringLiteral("login-attempts"), QString::number(cfg.loginAttempts));
    setText(g, QStringLiteral("max-logins-per-ipaddr"), QString::number(cfg.maxLoginsPerIp));
    setText(g, QStringLiteral("login-delay-msec"), QString::number(cfg.loginDelayMsec));
    if (cfg.bindIp.trimmed().isEmpty())
    {
        const QDomElement b = g.firstChildElement(QStringLiteral("bind-ip"));
        if (!b.isNull())
            g.removeChild(b);
    }
    else
    {
        setText(g, QStringLiteral("bind-ip"), cfg.bindIp.trimmed());
    }

    QDomElement fs = child(root, QStringLiteral("file-storage"), true);
    setText(fs, QStringLiteral("files-root"), cfg.filesRoot);
    setText(fs, QStringLiteral("max-diskusage"), QString::number(cfg.maxDiskUsage));
    setText(fs, QStringLiteral("channel-diskquota"), QString::number(cfg.channelDiskQuota));

    // Accounts: update existing <user> entries in place (keeping the
    // server's history fields), add new ones, remove deleted ones.
    QDomElement users = child(root, QStringLiteral("users"), true);
    QSet<QString> wanted;
    for (const ServerAccount& a : cfg.accounts)
        wanted.insert(a.username);
    for (QDomElement u = users.firstChildElement(QStringLiteral("user")); !u.isNull();)
    {
        QDomElement next = u.nextSiblingElement(QStringLiteral("user"));
        if (!wanted.contains(text(u, QStringLiteral("username"))))
            users.removeChild(u);
        u = next;
    }
    for (const ServerAccount& a : cfg.accounts)
    {
        QDomElement found;
        for (QDomElement u = users.firstChildElement(QStringLiteral("user")); !u.isNull();
             u = u.nextSiblingElement(QStringLiteral("user")))
            if (text(u, QStringLiteral("username")) == a.username)
            {
                found = u;
                break;
            }
        if (found.isNull())
        {
            QDomElement u = doc.createElement(QStringLiteral("user"));
            users.appendChild(u);
            fillNewUser(u, a);
            continue;
        }
        const bool changed = text(found, QStringLiteral("password")) != a.password ||
                             (text(found, QStringLiteral("user-type")).trimmed() == QLatin1String("2")) != a.admin ||
                             text(found, QStringLiteral("note")) != a.note;
        setText(found, QStringLiteral("password"), a.password);
        setText(found, QStringLiteral("note"), a.note);
        if ((text(found, QStringLiteral("user-type")).trimmed() == QLatin1String("2")) != a.admin)
        {
            setText(found, QStringLiteral("user-type"), a.admin ? QStringLiteral("2") : QStringLiteral("1"));
            setText(found, QStringLiteral("user-rights"),
                    a.admin ? QStringLiteral("0") : QString::number(kDefaultUserRights));
        }
        if (changed)
            setText(found, QStringLiteral("modified-time"), now());
    }

    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        if (error) *error = tr("Cannot write %1").arg(path);
        return false;
    }
    out.write(doc.toByteArray(4));
    if (!out.commit())
    {
        if (error) *error = tr("Cannot save %1").arg(path);
        return false;
    }
    return true;
}

} // namespace ttc
