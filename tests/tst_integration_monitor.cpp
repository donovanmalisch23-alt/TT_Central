// SPDX-License-Identifier: MIT
// Integration tests: a real TT Central monitor against BearWare's real
// TeamTalk server (tt5srv), with a second real client acting as a user.
#include "serverharness.h"
#include "ttc/monitorsession.h"

#include <QtTest>

using namespace ttc;
using namespace ttc::test;
using Kind = MonitorEvent::Kind;

class TestIntegrationMonitor : public QObject
{
    Q_OBJECT

    struct Watch
    {
        std::unique_ptr<MonitorSession> session;
        QList<MonitorEvent> events;
        Driver* driver = nullptr;

        bool waitEvent(Kind kind, const std::function<bool(const MonitorEvent&)>& pred = {}, int timeoutMs = 15000)
        {
            int seen = 0;
            return waitUntil([&] {
                if (driver) driver->drain();
                for (; seen < events.size(); ++seen)
                    if (events.at(seen).kind == kind && (!pred || pred(events.at(seen))))
                        return true;
                return false;
            }, timeoutMs);
        }
        QString dump() const
        {
            QStringList l;
            for (const auto& e : events) l << e.describe();
            return l.join(QStringLiteral(" | "));
        }
    };

    static LocalServerConfig baseConfig()
    {
        LocalServerConfig c;
        c.serverName = QStringLiteral("TT Central Integration");
        c.accounts = {{QStringLiteral("monitor"), QStringLiteral("monpw"), false, {}},
                      {QStringLiteral("alice"), QStringLiteral("alicepw"), false, {}},
                      {QStringLiteral("admin"), QStringLiteral("adminpw"), true, {}}};
        return c;
    }

    static std::unique_ptr<Watch> makeWatch(int port, Driver* driver)
    {
        MonitorConfig cfg = MonitorConfig::create();
        cfg.name = QStringLiteral("Local Test");
        cfg.host = QStringLiteral("127.0.0.1");
        cfg.tcpPort = cfg.udpPort = port;
        cfg.username = QStringLiteral("monitor");
        cfg.password = QStringLiteral("monpw");
        cfg.nickname = QStringLiteral("Monitor");
        cfg.channelPath = QStringLiteral("/");
        auto w = std::make_unique<Watch>();
        w->driver = driver;
        w->session = std::make_unique<MonitorSession>(cfg, createSdkClient());
        w->session->setPollInterval(10);
        w->session->setReconnectDelays(200, 1000);
        Watch* raw = w.get();
        QObject::connect(raw->session.get(), &MonitorSession::event,
                         [raw](const MonitorEvent& e) { raw->events << e; });
        return w;
    }

private slots:
    void initTestCase()
    {
        if (serverExe().isEmpty() || !QFileInfo::exists(serverExe()))
            QSKIP("TeamTalk server (tt5srv) not available; run scripts/bootstrap.py");
    }

    void reportsRealServerActivity()
    {
        ServerHarness server;
        server.config = baseConfig();
        QString err;
        QVERIFY2(server.start(&err), qPrintable(err));

        Driver alice;
        auto w = makeWatch(server.port, &alice);
        w->session->start();
        QVERIFY2(w->waitEvent(Kind::Online), qPrintable(w->dump()));
        QCOMPARE(w->events.last().describe(), QStringLiteral("Local Test: monitor online in the root channel"));

        // Login is reported, tagged with the server.
        QVERIFY(alice.login(server.port, QStringLiteral("Alice"), QStringLiteral("alice"), QStringLiteral("alicepw")));
        QVERIFY2(w->waitEvent(Kind::UserLoggedIn, [](const MonitorEvent& e) { return e.nickname == QLatin1String("Alice"); }),
                 qPrintable(w->dump()));
        QCOMPARE(w->events.last().describe(), QStringLiteral("Local Test: Alice logged in"));

        // Channel message in the monitor's channel.
        const int root = alice.c->rootChannelId();
        QVERIFY(alice.waitCommand(alice.c->joinChannel(root, QString())));
        alice.c->sendTextMessage(MSGTYPE_CHANNEL, 0, root, QStringLiteral("hello from alice"));
        QVERIFY2(w->waitEvent(Kind::ChannelMessage, [](const MonitorEvent& e) { return e.text == QLatin1String("hello from alice"); }),
                 qPrintable(w->dump()));
        QCOMPARE(w->events.last().describe(), QStringLiteral("Local Test: Alice in the root channel: hello from alice"));

        // Private message to the monitor, including a long multi-part one.
        int monitorId = 0;
        for (int id : alice.c->channelUsers(root))
        {
            User u;
            if (alice.c->user(id, u) && fromTT(u.szNickname) == QLatin1String("Monitor"))
                monitorId = id;
        }
        QVERIFY(monitorId > 0);
        alice.c->sendTextMessage(MSGTYPE_USER, monitorId, 0, QStringLiteral("are you watching?"));
        QVERIFY2(w->waitEvent(Kind::PrivateMessage, [](const MonitorEvent& e) { return e.text == QLatin1String("are you watching?"); }),
                 qPrintable(w->dump()));

        QString longText;
        for (int i = 0; i < 120; ++i)
            longText += QStringLiteral("segment-%1 ").arg(i);
        QVERIFY(encodedLength(longText) > TT_STRLEN); // forces several SDK parts
        alice.c->sendTextMessage(MSGTYPE_USER, monitorId, 0, longText);
        QVERIFY2(w->waitEvent(Kind::PrivateMessage, [&](const MonitorEvent& e) { return e.text == longText; }),
                 qPrintable(w->dump().right(400)));

        // Logout is reported.
        alice.c->disconnect();
        QVERIFY2(w->waitEvent(Kind::UserLoggedOut, [](const MonitorEvent& e) { return e.nickname == QLatin1String("Alice"); }),
                 qPrintable(w->dump()));

        w->session->stop();
    }

    void explainsOneLoginPerIpLimit()
    {
        ServerHarness server;
        server.config = baseConfig();
        server.config.maxLoginsPerIp = 1;
        QString err;
        QVERIFY2(server.start(&err), qPrintable(err));

        // "You" are already logged in from this IP address...
        Driver you;
        QVERIFY(you.login(server.port, QStringLiteral("You"), QStringLiteral("alice"), QStringLiteral("alicepw")));

        // ...so a monitor from the same address is refused, with guidance.
        auto w = makeWatch(server.port, &you);
        w->session->start();
        QVERIFY2(w->waitEvent(Kind::LoginFailed), qPrintable(w->dump()));
        QVERIFY2(w->events.last().text.contains(QStringLiteral("create a separate account")),
                 qPrintable(w->events.last().describe()));
        w->session->stop();
    }

    void reconnectsAfterServerRestart()
    {
        ServerHarness server;
        server.config = baseConfig();
        QString err;
        QVERIFY2(server.start(&err), qPrintable(err));

        auto w = makeWatch(server.port, nullptr);
        w->session->start();
        QVERIFY2(w->waitEvent(Kind::Online), qPrintable(w->dump()));

        server.stop();
        QVERIFY2(w->waitEvent(Kind::ConnectionLost), qPrintable(w->dump()));

        QVERIFY2(server.restart(&err), qPrintable(err));
        const int onlineBefore = int(std::count_if(w->events.cbegin(), w->events.cend(),
                                                   [](const MonitorEvent& e) { return e.kind == Kind::Online; }));
        QVERIFY2(waitUntil([&] {
            return std::count_if(w->events.cbegin(), w->events.cend(),
                                 [](const MonitorEvent& e) { return e.kind == Kind::Online; }) > onlineBefore;
        }, 20000), qPrintable(w->dump()));
        QCOMPARE(w->session->state(), MonitorSession::State::Online);
        w->session->stop();
    }
};

QTEST_MAIN(TestIntegrationMonitor)
#include "tst_integration_monitor.moc"
