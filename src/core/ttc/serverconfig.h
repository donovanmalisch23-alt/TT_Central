// TT Central — configuration for a locally hosted TeamTalk server (tt5srv).
// SPDX-License-Identifier: GPL-3.0-or-later
//
// TT Central's server manager edits the server's own XML file (tt5srv.xml).
// The server also stores channels, bans and other state in that file, so the
// writer updates only the settings TT Central manages and preserves every
// other element untouched.

#pragma once

#include <QList>
#include <QString>

namespace ttc {

struct ServerAccount
{
    QString username;
    QString password;
    bool admin = false;
    QString note;

    bool operator==(const ServerAccount& o) const
    {
        return username == o.username && password == o.password && admin == o.admin && note == o.note;
    }
};

struct LocalServerConfig
{
    QString serverName = QStringLiteral("My TeamTalk Server");
    QString motd;
    int maxUsers = 1000;
    bool autoSave = true;
    int tcpPort = 10333;
    int udpPort = 10333;
    QString bindIp;              // empty = all interfaces
    bool upnp = false;
    int loginAttempts = 0;       // max invalid attempts before ban, 0 = off
    int maxLoginsPerIp = 0;      // 0 = unlimited
    int loginDelayMsec = 0;
    QString filesRoot;           // empty = file sharing disabled
    qint64 maxDiskUsage = 0;
    qint64 channelDiskQuota = 0;
    QList<ServerAccount> accounts;

    /// A problem that prevents saving, or an empty string when valid.
    QString validationError() const;

    bool operator==(const LocalServerConfig& o) const;
};

/// TeamTalk user rights given to new default (non-admin) accounts, matching
/// what BearWare's tt5srv setup wizard assigns.
constexpr unsigned int kDefaultUserRights = 12842503u;

/// Read the settings TT Central manages from an existing tt5srv.xml.
/// Returns false (with *error set) if the file cannot be parsed.
bool readServerXml(const QString& path, LocalServerConfig& out, QString* error = nullptr);

/// Write `cfg` into tt5srv.xml, creating it if needed. Elements TT Central does
/// not manage (channels, bans, per-user history, ...) are preserved.
bool writeServerXml(const QString& path, const LocalServerConfig& cfg, QString* error = nullptr);

} // namespace ttc
