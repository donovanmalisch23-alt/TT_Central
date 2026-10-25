// SPDX-License-Identifier: GPL-3.0-or-later
//
// Wires TT Central (ttc_client) into qtTeamTalk: its settings store, its
// TTS/Prism speech, its sound packs, its toasts and its saved servers.
#include "ttcentral_integration.h"

#include "ttc/central.h"
#include "ttc/clienthooks.h"
#include "ttc/monitorevent.h"
#include "ttc/unattended.h"

// qtTeamTalk
#include "common.h"
#include "settings.h"
#include "utilsound.h"
#include "utiltts.h"
#include "utilui.h"

#include <QAction>
#include <QCoreApplication>
#include <QFile>
#include <QTimer>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>

#include <cstdio>
#include <cstdlib>

class QTextToSpeech;
extern NonDefaultSettings* ttSettings;
extern QTextToSpeech* ttSpeech;

namespace ttcentral {

namespace {

ttc::Central* g_central = nullptr;

SoundEvent soundFor(ttc::MonitorEvent::Kind kind)
{
    using K = ttc::MonitorEvent::Kind;
    switch (kind)
    {
    case K::UserLoggedIn: return SOUNDEVENT_USERLOGGEDIN;
    case K::UserLoggedOut: return SOUNDEVENT_USERLOGGEDOUT;
    case K::PrivateMessage: return SOUNDEVENT_USERMSG;
    case K::ChannelMessage: return SOUNDEVENT_CHANNELMSG;
    case K::BroadcastMessage: return SOUNDEVENT_BROADCASTMSG;
    case K::ConnectionLost:
    case K::ConnectFailed:
    case K::LoginFailed:
    case K::Kicked: return SOUNDEVENT_SERVERLOST;
    case K::Online:
    case K::ChannelJoinFailed: return SOUNDEVENT_NONE;
    }
    return SOUNDEVENT_NONE;
}

void speak(const QString& text)
{
#if defined(QT_TEXTTOSPEECH_LIB)
    // The Qt speech engine object only exists while that engine is selected.
    if (ttSettings->value(SETTINGS_TTS_ENGINE, SETTINGS_TTS_ENGINE_DEFAULT).toUInt() == TTSENGINE_QT && !ttSpeech)
        return;
#endif
    addTextToSpeechMessage(text);
}

QList<ttc::SavedServer> savedServers()
{
    QList<ttc::SavedServer> list;
    for (int i = 0;; ++i)
    {
        HostEntry h;
        if (!getServerEntry(i, h, false))
            break;
        ttc::SavedServer s;
        s.name = h.name;
        s.host = h.ipaddr;
        s.tcpPort = h.tcpport;
        s.udpPort = h.udpport;
        s.encrypted = h.encrypted;
        s.username = h.username;
        s.password = h.password;
        s.nickname = h.nickname;
        s.channel = h.channel;
        s.channelPassword = h.chanpasswd;
        list << s;
    }
    return list;
}

void addSavedServer(const ttc::SavedServer& s)
{
    HostEntry h;
    h.name = s.name;
    h.ipaddr = s.host;
    h.tcpport = s.tcpPort;
    h.udpport = s.udpPort;
    h.encrypted = s.encrypted;
    h.username = s.username;
    h.password = s.password;
    h.nickname = s.nickname;
    h.channel = s.channel;
    h.chanpasswd = s.channelPassword;
    for (const ttc::SavedServer& existing : savedServers())
        if (existing.name == s.name && existing.host == s.host && existing.tcpPort == s.tcpPort)
            return; // already saved
    addServerEntry(h);
}

} // namespace

void install(QMainWindow* mainWindow, QMenuBar* menuBar)
{
    if (g_central)
        return;
    ttc::ClientHooks hooks;
    hooks.settings = ttSettings;
    hooks.outputs.speak = &speak;
    hooks.outputs.playSound = [](ttc::MonitorEvent::Kind kind) {
        const SoundEvent e = soundFor(kind);
        if (e != SOUNDEVENT_NONE)
            playSoundEvent(e);
    };
    hooks.outputs.toast = [](const QString& title, const QString& text) {
        showNotification(QStringLiteral("TT Central: %1").arg(title), text);
    };
    hooks.savedServers = &savedServers;
    hooks.addSavedServer = &addSavedServer;
    hooks.adminActions = [mainWindow] {
        QList<QAction*> list;
        for (const char* name : {"actionServerProperties", "actionUserAccounts", "actionBannedUsers",
                                 "actionOnlineUsers", "actionServerStatistics", "actionBroadcastMessage",
                                 "actionSaveConfiguration"})
            if (auto* a = mainWindow->findChild<QAction*>(QLatin1String(name)))
                list << a;
        return list;
    };
    hooks.openClientPreferences = [mainWindow] {
        if (auto* a = mainWindow->findChild<QAction*>(QStringLiteral("actionPreferences")))
            a->trigger();
    };

    g_central = new ttc::Central(hooks, mainWindow);
    QMenu* help = mainWindow->findChild<QMenu*>(QStringLiteral("menuHelp"));
    if (help)
        menuBar->insertMenu(help->menuAction(), g_central->menu());
    else
        menuBar->addMenu(g_central->menu());

    // CI self-test: TTC_SELFTEST=<report file> (or 1 for stderr only).
    const QString selftest = qEnvironmentVariable("TTC_SELFTEST");
    if (!selftest.isEmpty())
    {
        ttc::dismissSdkNoticesInBackground();
        QTimer::singleShot(3000, mainWindow, [mainWindow, menuBar, selftest] {
            const ttc::Central::SelfTestResult r = g_central->runSelfTest(mainWindow, menuBar);
            const QString report = r.lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
            if (selftest != QLatin1String("1"))
            {
                QFile f(selftest);
                if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
                    f.write(report.toUtf8());
            }
            std::fprintf(stderr, "%s", report.toUtf8().constData());
            std::fflush(stderr);
            const int code = r.ok ? 0 : 1;
            // Quit; force it if a first-run dialog keeps the event loop busy.
            QTimer::singleShot(4000, [code] { std::_Exit(code); });
            QCoreApplication::exit(code);
        });
    }
}

void onClientMessage(const TTMessage& msg)
{
    if (!g_central)
        return;
    if (msg.nClientEvent == CLIENTEVENT_CMD_MYSELF_LOGGEDIN)
        g_central->onClientLoggedIn();
}

} // namespace ttcentral
