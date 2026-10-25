// SPDX-License-Identifier: GPL-3.0-or-later
// Integration tests: TT Central's media bot controller running the real
// ttc-mediabot program against BearWare's real TeamTalk server.
#include "serverharness.h"
#include "ttc/mediabotcontroller.h"

#include <QSettings>
#include <QtTest>

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <csignal>
#endif

using namespace ttc;
using namespace ttc::test;

class TestIntegrationController : public QObject
{
    Q_OBJECT

    ServerHarness server;

    media::MediaBotConfig config() const
    {
        media::MediaBotConfig c;
        c.name = QStringLiteral("Controller Test");
        c.host = QStringLiteral("127.0.0.1");
        c.tcpPort = c.udpPort = server.port;
        c.username = QStringLiteral("bot");
        c.password = QStringLiteral("botpw");
        c.nickname = QStringLiteral("ControlledBot");
        c.reconnectSeconds = 1;
        return c;
    }

    static bool online(const MediaBotController& b)
    {
        return b.state() == MediaBotController::State::Running && b.status().value(QStringLiteral("online")).toBool();
    }

private slots:
    void initTestCase()
    {
        if (serverExe().isEmpty() || !QFileInfo::exists(serverExe()))
            QSKIP("TeamTalk server (tt5srv) not available; run scripts/bootstrap.py");
        qputenv("TTC_UNATTENDED", "1");   // the bot process presses OK on the SDK's trial notice
        server.config.serverName = QStringLiteral("Controller Test Server");
        server.config.accounts = {{QStringLiteral("bot"), QStringLiteral("botpw"), false, {}},
                                  {QStringLiteral("admin"), QStringLiteral("adminpw"), true, {}}};
        QString err;
        QVERIFY2(server.start(&err), qPrintable(err));
    }

    void cleanupTestCase() { server.stop(); }

    void runsTheRealBot()
    {
        QTemporaryDir dir;
        QSettings s(dir.filePath(QStringLiteral("TTCentral.ini")), QSettings::IniFormat);
        MediaBotController bot(&s, QStringLiteral(TTC_MEDIABOT_EXE));
        QStringList log;
        connect(&bot, &MediaBotController::logLine, this, [&](const QString& l) { log << l; });
        bot.setConfig(config());
        QVERIFY(bot.isConfigured());

        TTC_STEP("start the bot");
        bot.start();
        QVERIFY2(waitUntil([&] { return online(bot); }, 30000), qPrintable(bot.stateText()));
        QVERIFY(bot.stateText().contains(QStringLiteral("Online on Controller Test")));
        QVERIFY(QFileInfo::exists(dir.filePath(QStringLiteral("mediabot.ini"))));

        TTC_STEP("a command and its reply");
        QString reply;
        bot.command(QStringLiteral("v 30"), [&](const QString& r) { reply = r; });
        QVERIFY(waitUntil([&] { return !reply.isEmpty(); }, 10000));
        QCOMPARE(reply, QStringLiteral("Volume set to 30%."));
        QVERIFY(waitUntil([&] { return bot.status().value(QStringLiteral("volume")).toInt() == 30; }, 5000));

        TTC_STEP("a crash is recovered");
        const qint64 pid = bot.processId();
        QVERIFY(pid > 0);
#if defined(_WIN32)
        HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, DWORD(pid));
        QVERIFY(h);
        TerminateProcess(h, 3);
        CloseHandle(h);
#else
        ::kill(pid_t(pid), SIGKILL);
#endif
        QVERIFY(waitUntil([&] { return bot.state() == MediaBotController::State::Restarting; }, 10000));
        QVERIFY2(waitUntil([&] { return online(bot); }, 30000), qPrintable(bot.stateText()));
        QVERIFY(bot.processId() != pid);

        TTC_STEP("changing settings restarts it with them");
        media::MediaBotConfig c = config();
        c.nickname = QStringLiteral("Renamed");
        bot.setConfig(c);
        QVERIFY2(waitUntil([&] { return online(bot); }, 30000), qPrintable(bot.stateText()));
        QSettings written(dir.filePath(QStringLiteral("mediabot.ini")), QSettings::IniFormat);
        QCOMPARE(written.value(QStringLiteral("mediabot/nickname")).toString(), QStringLiteral("Renamed"));

        TTC_STEP("stop");
        const qint64 last = bot.processId();
        bot.stop();
        QCOMPARE(bot.state(), MediaBotController::State::Stopped);
        QVERIFY(bot.status().isEmpty());
#if defined(_WIN32)
        HANDLE gone = OpenProcess(SYNCHRONIZE, FALSE, DWORD(last));
        QVERIFY(!gone || WaitForSingleObject(gone, 5000) == WAIT_OBJECT_0);
        if (gone)
            CloseHandle(gone);
#else
        QVERIFY(waitUntil([&] { return ::kill(pid_t(last), 0) != 0; }, 5000));
#endif
    }

    void quitFromChatStopsCleanly()
    {
        QTemporaryDir dir;
        QSettings s(dir.filePath(QStringLiteral("TTCentral.ini")), QSettings::IniFormat);
        MediaBotController bot(&s, QStringLiteral(TTC_MEDIABOT_EXE));
        bot.setConfig(config());
        bot.start();
        QVERIFY2(waitUntil([&] { return online(bot); }, 30000), qPrintable(bot.stateText()));
        bot.command(QStringLiteral("quit"));   // TT Central's window counts as an administrator
        QVERIFY(waitUntil([&] { return bot.state() == MediaBotController::State::Stopped; }, 15000));
    }
};

QTEST_GUILESS_MAIN(TestIntegrationController)
#include "tst_integration_controller.moc"
