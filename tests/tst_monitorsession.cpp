// SPDX-License-Identifier: GPL-3.0-or-later
#include "fakeclient.h"
#include "ttc/monitorsession.h"

#include <QSignalSpy>
#include <QtTest>

using namespace ttc;
using namespace ttc::test;
using Kind = MonitorEvent::Kind;

class TestMonitorSession : public QObject
{
    Q_OBJECT

    struct Rig
    {
        FakeClient* fake = nullptr;
        std::unique_ptr<MonitorSession> session;
        QList<MonitorEvent> events;
    };

    static MonitorConfig config()
    {
        MonitorConfig c = MonitorConfig::create();
        c.name = QStringLiteral("Home Server");
        c.host = QStringLiteral("tt.example.org");
        c.username = QStringLiteral("me");
        c.password = QStringLiteral("secret");
        c.nickname = QStringLiteral("Watcher");
        c.channelPath = QStringLiteral("/Lobby/");
        return c;
    }

    static std::unique_ptr<Rig> makeRig(const MonitorConfig& cfg = config())
    {
        auto rig = std::make_unique<Rig>();
        auto fake = std::make_unique<FakeClient>();
        rig->fake = fake.get();
        fake->root = 1;
        fake->channelPaths.insert(QStringLiteral("/"), 1);
        fake->channelPaths.insert(QStringLiteral("/Lobby/"), 2);
        fake->channelNames.insert(2, QStringLiteral("Lobby"));
        rig->session = std::make_unique<MonitorSession>(cfg, std::move(fake));
        rig->session->setReconnectDelays(10, 40);
        Rig* raw = rig.get();
        QObject::connect(raw->session.get(), &MonitorSession::event,
                         [raw](const MonitorEvent& e) { raw->events << e; });
        return rig;
    }

    // Drive a rig through connect -> login -> join so it is Online in /Lobby/.
    static void bringOnline(Rig& r, const QVector<int>& lobbyMembers = {})
    {
        r.session->start();
        r.fake->queue.enqueue(blank(CLIENTEVENT_CON_SUCCESS));
        r.session->pollNow();
        const int loginCmd = r.fake->nextCmd;
        r.fake->myId = 7;
        // The server replays everyone already online during login: ignored.
        r.fake->queue.enqueue(userEvent(CLIENTEVENT_CMD_USER_LOGGEDIN, makeUser(20, "Old Timer", "old")));
        r.fake->queue.enqueue(processing(loginCmd, false));
        r.session->pollNow();
        const int joinCmd = r.fake->nextCmd;
        r.fake->myChannel = 2;
        r.fake->channelMembers.insert(2, QVector<int>{7} + lobbyMembers);
        r.fake->queue.enqueue(processing(joinCmd, false));
        r.session->pollNow();
    }

private slots:
    void loginFlowJoinsConfiguredChannel()
    {
        auto r = makeRig();
        bringOnline(*r, {30, 31});
        QCOMPARE(r->session->state(), MonitorSession::State::Online);
        QCOMPARE(r->fake->connects, 1);
        QCOMPARE(r->fake->logins.size(), 1);
        QCOMPARE(r->fake->logins.first(), QStringLiteral("Watcher|me|secret|TT Central Monitor"));
        QCOMPARE(r->fake->joins.size(), 1);
        QCOMPARE(r->fake->joins.first().first, 2);
        // Only the Online notice, not the replayed login burst.
        QCOMPARE(r->events.size(), 1);
        QCOMPARE(r->events.first().kind, Kind::Online);
        QCOMPARE(r->events.first().describe(), QStringLiteral("Home Server: monitor online in Lobby"));
        // Audio from existing channel members is turned off (bandwidth).
        QCOMPARE(r->fake->unsubscribes.size(), 2);
    }

    void reportsLoginsAndLogoutsTaggedWithServer()
    {
        auto r = makeRig();
        bringOnline(*r);
        r->events.clear();
        const User alice = makeUser(40, "Alice", "alice");
        r->fake->queue.enqueue(userEvent(CLIENTEVENT_CMD_USER_LOGGEDIN, alice));
        r->fake->queue.enqueue(userEvent(CLIENTEVENT_CMD_USER_LOGGEDOUT, alice));
        r->session->pollNow();
        QCOMPARE(r->events.size(), 2);
        QCOMPARE(r->events.at(0).describe(), QStringLiteral("Home Server: Alice logged in"));
        QCOMPARE(r->events.at(1).describe(), QStringLiteral("Home Server: Alice logged out"));
        QCOMPARE(r->events.at(0).serverName, QStringLiteral("Home Server"));
    }

    void ignoresOwnAccountSessions()
    {
        auto r = makeRig();
        bringOnline(*r);
        r->events.clear();
        r->fake->queue.enqueue(userEvent(CLIENTEVENT_CMD_USER_LOGGEDIN, makeUser(41, "Me On Laptop", "ME")));
        r->session->pollNow();
        QVERIFY(r->events.isEmpty());

        MonitorConfig cfg = config();
        cfg.ignoreOwnAccount = false;
        auto r2 = makeRig(cfg);
        bringOnline(*r2);
        r2->events.clear();
        r2->fake->queue.enqueue(userEvent(CLIENTEVENT_CMD_USER_LOGGEDIN, makeUser(41, "Me On Laptop", "me")));
        r2->session->pollNow();
        QCOMPARE(r2->events.size(), 1);
    }

    void reportsPrivateMessagesToTheMonitorOnly()
    {
        auto r = makeRig();
        bringOnline(*r);
        r->fake->users.insert(40, makeUser(40, "Alice", "alice", 2));
        r->events.clear();
        r->fake->queue.enqueue(text(MSGTYPE_USER, 40, "alice", 7, 0, "are you there?"));
        // As the real server delivers it: nToUserID is 0 on the receiving side.
        r->fake->queue.enqueue(text(MSGTYPE_USER, 40, "alice", 0, 0, "delivered as to=0"));
        // An intercepted message between two other users is not reported.
        r->fake->queue.enqueue(text(MSGTYPE_USER, 40, "alice", 99, 0, "not for the monitor"));
        r->session->pollNow();
        QCOMPARE(r->events.size(), 2);
        QCOMPARE(r->events.at(1).text, QStringLiteral("delivered as to=0"));
        QCOMPARE(r->events.first().kind, Kind::PrivateMessage);
        QCOMPARE(r->events.first().describe(),
                 QStringLiteral("Home Server: private message from Alice: are you there?"));
    }

    void reportsChannelMessagesOnlyInItsChannel()
    {
        auto r = makeRig();
        bringOnline(*r);
        r->fake->users.insert(40, makeUser(40, "Alice", "alice", 2));
        r->events.clear();
        r->fake->queue.enqueue(text(MSGTYPE_CHANNEL, 40, "alice", 0, 2, "hello lobby"));
        r->fake->queue.enqueue(text(MSGTYPE_CHANNEL, 40, "alice", 0, 5, "other channel"));
        r->fake->queue.enqueue(text(MSGTYPE_CHANNEL, 7, "me", 0, 2, "my own echo"));
        r->session->pollNow();
        QCOMPARE(r->events.size(), 1);
        QCOMPARE(r->events.first().describe(), QStringLiteral("Home Server: Alice in Lobby: hello lobby"));
    }

    void reassemblesMultiPartMessages()
    {
        auto r = makeRig();
        bringOnline(*r);
        r->fake->users.insert(40, makeUser(40, "Alice", "alice", 2));
        r->events.clear();
        r->fake->queue.enqueue(text(MSGTYPE_USER, 40, "alice", 7, 0, "part one, ", true));
        r->fake->queue.enqueue(text(MSGTYPE_USER, 40, "alice", 7, 0, "part two"));
        r->session->pollNow();
        QCOMPARE(r->events.size(), 1);
        QCOMPARE(r->events.first().text, QStringLiteral("part one, part two"));
    }

    void respectsNotificationToggles()
    {
        MonitorConfig cfg = config();
        cfg.notifyLogins = false;
        cfg.notifyChannelMessages = false;
        cfg.notifyConnection = false;
        auto r = makeRig(cfg);
        bringOnline(*r);
        QVERIFY(r->events.isEmpty()); // no "online" notice either
        r->fake->users.insert(40, makeUser(40, "Alice", "alice", 2));
        r->fake->queue.enqueue(userEvent(CLIENTEVENT_CMD_USER_LOGGEDIN, makeUser(40, "Alice", "alice")));
        r->fake->queue.enqueue(text(MSGTYPE_CHANNEL, 40, "alice", 0, 2, "hi"));
        r->fake->queue.enqueue(text(MSGTYPE_USER, 40, "alice", 7, 0, "pm still on"));
        r->session->pollNow();
        QCOMPARE(r->events.size(), 1);
        QCOMPARE(r->events.first().kind, Kind::PrivateMessage);
    }

    void broadcastsAreOptIn()
    {
        auto r = makeRig();
        bringOnline(*r);
        r->events.clear();
        r->fake->queue.enqueue(text(MSGTYPE_BROADCAST, 1, "admin", 0, 0, "server restart soon"));
        r->session->pollNow();
        QVERIFY(r->events.isEmpty());

        MonitorConfig cfg = config();
        cfg.notifyBroadcasts = true;
        auto r2 = makeRig(cfg);
        bringOnline(*r2);
        r2->events.clear();
        r2->fake->queue.enqueue(text(MSGTYPE_BROADCAST, 1, "admin", 0, 0, "server restart soon"));
        r2->session->pollNow();
        QCOMPARE(r2->events.size(), 1);
        QCOMPARE(r2->events.first().describe(), QStringLiteral("Home Server: broadcast from admin: server restart soon"));
    }

    void reconnectsAfterConnectionLossAnnouncingOnce()
    {
        auto r = makeRig();
        bringOnline(*r);
        r->events.clear();
        r->fake->queue.enqueue(blank(CLIENTEVENT_CON_LOST));
        r->session->pollNow();
        QCOMPARE(r->session->state(), MonitorSession::State::WaitingToReconnect);
        QCOMPARE(r->events.size(), 1);
        QCOMPARE(r->events.first().describe(), QStringLiteral("Home Server: connection lost, reconnecting"));

        // The retry fails too: no second announcement.
        QTRY_COMPARE(r->fake->connects, 2);
        r->fake->queue.enqueue(blank(CLIENTEVENT_CON_FAILED));
        r->session->pollNow();
        QCOMPARE(r->events.size(), 1);

        // Third attempt succeeds: back online, announced again.
        QTRY_COMPARE(r->fake->connects, 3);
        r->fake->queue.enqueue(blank(CLIENTEVENT_CON_SUCCESS));
        r->session->pollNow();
        r->fake->queue.enqueue(processing(r->fake->nextCmd, false));
        r->session->pollNow();
        r->fake->myChannel = 2;
        r->fake->queue.enqueue(processing(r->fake->nextCmd, false));
        r->session->pollNow();
        QCOMPARE(r->session->state(), MonitorSession::State::Online);
        QCOMPARE(r->events.size(), 2);
        QCOMPARE(r->events.last().kind, Kind::Online);
    }

    void permanentLoginErrorStopsRetrying()
    {
        auto r = makeRig();
        r->session->start();
        r->fake->queue.enqueue(blank(CLIENTEVENT_CON_SUCCESS));
        r->session->pollNow();
        const int loginCmd = r->fake->nextCmd;
        r->fake->queue.enqueue(cmdError(loginCmd, CMDERR_INVALID_ACCOUNT));
        r->fake->queue.enqueue(processing(loginCmd, false));
        r->session->pollNow();
        QCOMPARE(r->session->state(), MonitorSession::State::Failed);
        QCOMPARE(r->events.size(), 1);
        QCOMPARE(r->events.first().describe(),
                 QStringLiteral("Home Server: login failed: invalid username or password"));
        QTest::qWait(80);
        QCOMPARE(r->fake->connects, 1); // no retry
    }

    void oneLoginPerIpExplainsSeparateAccount()
    {
        auto r = makeRig();
        r->session->start();
        r->fake->queue.enqueue(blank(CLIENTEVENT_CON_SUCCESS));
        r->session->pollNow();
        r->fake->queue.enqueue(cmdError(r->fake->nextCmd, CMDERR_MAX_LOGINS_PER_IPADDRESS_EXCEEDED));
        r->session->pollNow();
        QCOMPARE(r->events.size(), 1);
        QVERIFY(r->events.first().text.contains(QStringLiteral("create a separate account")));
        // Retryable: e.g. it succeeds once your own client logs out.
        QCOMPARE(r->session->state(), MonitorSession::State::WaitingToReconnect);
    }

    void missingChannelFallsBackToRoot()
    {
        MonitorConfig cfg = config();
        cfg.channelPath = QStringLiteral("/Nowhere/");
        auto r = makeRig(cfg);
        r->session->start();
        r->fake->queue.enqueue(blank(CLIENTEVENT_CON_SUCCESS));
        r->session->pollNow();
        r->fake->queue.enqueue(processing(r->fake->nextCmd, false));
        r->session->pollNow();
        QCOMPARE(r->fake->joins.size(), 1);
        QCOMPARE(r->fake->joins.first().first, 1); // root
        r->fake->myChannel = 1;
        r->fake->queue.enqueue(processing(r->fake->nextCmd, false));
        r->session->pollNow();
        QCOMPARE(r->session->state(), MonitorSession::State::Online);
        QCOMPARE(r->events.size(), 2);
        QCOMPARE(r->events.at(0).kind, Kind::ChannelJoinFailed);
        QCOMPARE(r->events.at(1).describe(), QStringLiteral("Home Server: monitor online in the root channel"));
    }

    void refusedChannelFallsBackToRoot()
    {
        auto r = makeRig();
        r->session->start();
        r->fake->queue.enqueue(blank(CLIENTEVENT_CON_SUCCESS));
        r->session->pollNow();
        r->fake->queue.enqueue(processing(r->fake->nextCmd, false));
        r->session->pollNow();
        const int joinCmd = r->fake->nextCmd;
        r->fake->queue.enqueue(cmdError(joinCmd, CMDERR_INCORRECT_CHANNEL_PASSWORD));
        r->fake->queue.enqueue(processing(joinCmd, false));
        r->session->pollNow();
        QCOMPARE(r->fake->joins.size(), 2);
        QCOMPARE(r->fake->joins.at(1).first, 1);
        r->fake->myChannel = 1;
        r->fake->queue.enqueue(processing(r->fake->nextCmd, false));
        r->session->pollNow();
        QCOMPARE(r->session->state(), MonitorSession::State::Online);
        QCOMPARE(r->events.first().kind, Kind::ChannelJoinFailed);
        QVERIFY(r->events.first().describe().contains(QStringLiteral("incorrect channel password")));
    }

    void kickFromChannelKeepsServerConnection()
    {
        auto r = makeRig();
        bringOnline(*r);
        r->events.clear();
        TTMessage kick = blank(CLIENTEVENT_CMD_MYSELF_KICKED, 2);
        kick.ttType = __USER;
        kick.user = makeUser(50, "Op", "op");
        r->fake->queue.enqueue(kick);
        r->session->pollNow();
        QCOMPARE(r->events.size(), 1);
        QCOMPARE(r->events.first().describe(), QStringLiteral("Home Server: monitor was kicked from Lobby by Op"));
        QCOMPARE(r->session->state(), MonitorSession::State::Online);
    }

    void kickFromServerReconnects()
    {
        auto r = makeRig();
        bringOnline(*r);
        r->events.clear();
        r->fake->queue.enqueue(blank(CLIENTEVENT_CMD_MYSELF_KICKED, 0));
        r->fake->queue.enqueue(blank(CLIENTEVENT_CON_LOST)); // follows a server kick
        r->session->pollNow();
        QCOMPARE(r->events.size(), 1);
        QCOMPARE(r->events.first().describe(), QStringLiteral("Home Server: monitor was kicked, reconnecting"));
        QCOMPARE(r->session->state(), MonitorSession::State::WaitingToReconnect);
        QTRY_COMPARE(r->fake->connects, 2);
    }

    void invalidConfigFailsWithoutConnecting()
    {
        MonitorConfig cfg = config();
        cfg.host.clear();
        auto r = makeRig(cfg);
        r->session->start();
        QCOMPARE(r->session->state(), MonitorSession::State::Failed);
        QCOMPARE(r->fake->connects, 0);
        QVERIFY(r->session->stateText().contains(QStringLiteral("server address")));
    }

    void stopDisconnectsAndStaysQuiet()
    {
        auto r = makeRig();
        bringOnline(*r);
        r->events.clear();
        r->session->stop();
        QCOMPARE(r->session->state(), MonitorSession::State::Stopped);
        QVERIFY(r->fake->disconnects >= 1);
        QTest::qWait(60);
        QCOMPARE(r->fake->connects, 1);
        QVERIFY(r->events.isEmpty());
    }
};

QTEST_MAIN(TestMonitorSession)
#include "tst_monitorsession.moc"
