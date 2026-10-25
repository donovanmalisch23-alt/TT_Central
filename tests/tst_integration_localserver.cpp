// SPDX-License-Identifier: GPL-3.0-or-later
// Integration tests: TT Central's local server manager running BearWare's
// real TeamTalk server (tt5srv), with a real client connecting to it.
#include "serverharness.h"
#include "ttc/localserver.h"

#include <QSignalSpy>
#include <QtTest>

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <csignal>
#endif

using namespace ttc;
using namespace ttc::test;

class TestIntegrationLocalServer : public QObject
{
    Q_OBJECT

    static LocalServerConfig config(int port)
    {
        LocalServerConfig c;
        c.serverName = QStringLiteral("Managed Server");
        c.tcpPort = c.udpPort = port;
        c.bindIp = QStringLiteral("127.0.0.1");
        c.accounts = {{QStringLiteral("boss"), QStringLiteral("bosspw"), true, {}},
                      {QStringLiteral("guest"), QStringLiteral("guestpw"), false, {}}};
        return c;
    }

    static QString serverNameSeenBy(Driver& d)
    {
        ServerProperties p;
        std::memset(&p, 0, sizeof(p));
        return TT_GetServerProperties(d.c->instance(), &p) ? fromTT(p.szServerName) : QString();
    }

private slots:
    void initTestCase()
    {
        if (serverExe().isEmpty() || !QFileInfo::exists(serverExe()))
            QSKIP("TeamTalk server (tt5srv) not available; run scripts/bootstrap.py");
    }

    void hostsAServerPeopleCanLogInTo()
    {
        QTemporaryDir dir;
        LocalServer server(serverExe(), dir.filePath(QStringLiteral("srv")));
        QString err;
        TTC_STEP("save settings");
        const int port = freePort();
        QVERIFY2(server.setConfig(config(port), &err), qPrintable(err));
        TTC_STEP("start");
        server.start();
        QVERIFY2(waitUntil([&] { return server.state() == LocalServer::State::Running; }, 20000),
                 qPrintable(server.stateText() + QStringLiteral(" | ") + server.logTail().join(QStringLiteral(" / "))));
        QVERIFY(server.stateText().contains(QString::number(port)));

        TTC_STEP("log in as the administrator");
        Driver boss;
        QVERIFY(boss.login(port, QStringLiteral("Boss"), QStringLiteral("boss"), QStringLiteral("bosspw")));
        QVERIFY(TT_GetFlags(boss.c->instance()) & CLIENT_AUTHORIZED);
        QCOMPARE(serverNameSeenBy(boss), QStringLiteral("Managed Server"));
        User me;
        QVERIFY(boss.c->user(boss.c->myUserId(), me));
        QVERIFY(me.uUserType & USERTYPE_ADMIN);
        TTC_STEP("wrong password is refused");
        Driver intruder;
        intruder.login(port, QStringLiteral("X"), QStringLiteral("boss"), QStringLiteral("nope"));
        QVERIFY(!(TT_GetFlags(intruder.c->instance()) & CLIENT_AUTHORIZED));   // refused

        TTC_STEP("changing settings restarts the server");
        LocalServerConfig c = config(port);
        c.serverName = QStringLiteral("Renamed Server");
        QVERIFY2(server.setConfig(c, &err), qPrintable(err));
        QVERIFY(waitUntil([&] { return server.state() == LocalServer::State::Running; }, 20000));
        Driver again;
        QVERIFY(again.login(port, QStringLiteral("Guest"), QStringLiteral("guest"), QStringLiteral("guestpw")));
        QCOMPARE(serverNameSeenBy(again), QStringLiteral("Renamed Server"));

        TTC_STEP("stop");
        server.stop();
        QCOMPARE(server.state(), LocalServer::State::Stopped);
        QTcpSocket probe;
        probe.connectToHost(QHostAddress::LocalHost, quint16(port));
        QVERIFY(!probe.waitForConnected(500));
        QVERIFY(!server.logTail().isEmpty());
    }

    void restartsAfterACrash()
    {
        QTemporaryDir dir;
        LocalServer server(serverExe(), dir.filePath(QStringLiteral("srv")));
        server.setRestartDelayMs(300);
        QString err;
        const int port = freePort();
        QVERIFY2(server.setConfig(config(port), &err), qPrintable(err));
        server.start();
        QVERIFY(waitUntil([&] { return server.state() == LocalServer::State::Running; }, 20000));
        const qint64 pid = server.processId();
        QVERIFY(pid > 0);
        TTC_STEP("kill the server process from outside");
#if defined(_WIN32)
        HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, DWORD(pid));
        QVERIFY(h);
        TerminateProcess(h, 3);
        CloseHandle(h);
#else
        ::kill(pid_t(pid), SIGKILL);
#endif
        QVERIFY(waitUntil([&] { return server.state() == LocalServer::State::Restarting; }, 10000));
        QVERIFY2(waitUntil([&] { return server.state() == LocalServer::State::Running; }, 20000), qPrintable(server.stateText()));
        QVERIFY(server.processId() != pid);
        server.stop();
    }

    void refusesAPortInUse()
    {
        QTemporaryDir dir;
        QTcpServer squatter;
        QVERIFY(squatter.listen(QHostAddress::Any, 0));
        LocalServer server(serverExe(), dir.filePath(QStringLiteral("srv")));
        QString err;
        QVERIFY2(server.setConfig(config(squatter.serverPort()), &err), qPrintable(err));
        server.start();
        QCOMPARE(server.state(), LocalServer::State::Failed);
        QVERIFY(server.stateText().contains(QStringLiteral("already in use")));
    }

    void needsSettingsFirst()
    {
        QTemporaryDir dir;
        LocalServer server(serverExe(), dir.filePath(QStringLiteral("srv")));
        QVERIFY(!server.hasConfig());
        server.start();
        QCOMPARE(server.state(), LocalServer::State::Failed);
        QVERIFY(server.stateText().contains(QStringLiteral("administrator")));
        LocalServerConfig noAdmin = config(10333);
        noAdmin.accounts = {};
        QString err;
        QVERIFY(!server.setConfig(noAdmin, &err));
        QVERIFY(!err.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestIntegrationLocalServer)
#include "tst_integration_localserver.moc"
