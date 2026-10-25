// SPDX-License-Identifier: GPL-3.0-or-later
// Integration tests: TT Central's media bot against BearWare's real TeamTalk
// server (tt5srv), with a second real client sending it commands and
// listening for the music it plays into the channel.
#include "serverharness.h"
#include "ttc/media/controlserver.h"
#include "ttc/media/mediabot.h"
#include "ttc/textassembler.h"

#include <QJsonDocument>
#include <QLocalSocket>
#include <QUuid>
#include <QtTest>

using namespace ttc;
using namespace ttc::media;
using namespace ttc::test;

namespace {

/// A real client that remembers what it hears: assembled texts, and how many
/// voice audio blocks arrived from each user.
struct Listener
{
    Driver d;
    TextAssembler assembler;
    QList<AssembledMessage> texts;
    QHash<int, int> audioBlocks;

    bool join()
    {
        return d.waitCommand(d.c->joinChannel(d.c->rootChannelId(), QString()));
    }

    void hearVoiceFrom(int userId)
    {
        TTInstance* tt = d.c->instance();
        // A virtual sound device lets the SDK decode audio with no hardware.
        TT_InitSoundOutputDevice(tt, TT_SOUNDDEVICE_ID_TEAMTALK_VIRTUAL);
        TT_EnableAudioBlockEvent(tt, userId, STREAMTYPE_VOICE, TRUE);
    }

    void pump()
    {
        TTMessage m;
        while (d.c->nextMessage(m, 0))
        {
            if (m.nClientEvent == CLIENTEVENT_CMD_USER_TEXTMSG)
            {
                if (auto done = assembler.add(m.textmessage))
                    texts << *done;
            }
            else if (m.nClientEvent == CLIENTEVENT_USER_AUDIOBLOCK)
            {
                if (AudioBlock* b = TT_AcquireUserAudioBlock(d.c->instance(), STREAMTYPE_VOICE, m.nSource))
                {
                    ++audioBlocks[m.nSource];
                    TT_ReleaseUserAudioBlock(d.c->instance(), b);
                }
            }
        }
    }

    void pm(int to, const QString& text) { d.c->sendTextMessage(MSGTYPE_USER, to, 0, text); }

    /// Wait for a text from `from` containing `needle` (after index `since`).
    bool waitText(int from, const QString& needle, int timeoutMs = 15000, int* since = nullptr)
    {
        int start = since ? *since : 0;
        return waitUntil([&] {
            pump();
            for (int i = start; i < texts.size(); ++i)
                if (texts.at(i).fromUserId == from && texts.at(i).text.contains(needle))
                {
                    if (since)
                        *since = i + 1;
                    return true;
                }
            return false;
        }, timeoutMs);
    }

    QString dump() const
    {
        QStringList l;
        for (const auto& t : texts)
            l << QStringLiteral("[%1] %2").arg(t.fromUserId).arg(t.text.left(80));
        return l.join(QStringLiteral(" | "));
    }
};

} // namespace

class TestIntegrationMediaBot : public QObject
{
    Q_OBJECT

    ServerHarness server;
    std::unique_ptr<MediaBot> bot;

    MediaBotConfig botConfig() const
    {
        MediaBotConfig c;
        c.name = QStringLiteral("Local Test");
        c.host = QStringLiteral("127.0.0.1");
        c.tcpPort = c.udpPort = server.port;
        c.username = QStringLiteral("bot");
        c.password = QStringLiteral("botpw");
        c.nickname = QStringLiteral("Jukebox");
        c.admins = {QStringLiteral("dj")};
        c.channelPrefix = QStringLiteral("!");
        return c;
    }

    std::unique_ptr<MediaBot> makeBot(const MediaBotConfig& cfg)
    {
        auto b = std::make_unique<MediaBot>(cfg, createSdkClient());
        b->setReconnectDelays(300, 1000);
        // Every link plays 4 s of a generated tone instead of going to ffmpeg.
        b->engine()->setDecoderCommand([](const Track&, double start) {
            return QStringList{QStringLiteral(TTC_PCMGEN_PATH), QStringLiteral("--seconds"), QStringLiteral("4"),
                               QStringLiteral("--start"), QString::number(start)};
        });
        return b;
    }

    bool waitOnline(MediaBot* b, int timeoutMs = 20000)
    {
        return waitUntil([&] { return b->state() == MediaBot::State::Online; }, timeoutMs);
    }

    std::unique_ptr<Listener> listener(const QString& user, const QString& pass, const QString& nick)
    {
        auto l = std::make_unique<Listener>();
        if (!l->d.login(server.port, nick, user, pass) || !l->join())
            return nullptr;
        return l;
    }

private slots:
    void initTestCase()
    {
        server.config.serverName = QStringLiteral("TT Central Media Test");
        server.config.accounts = {{QStringLiteral("bot"), QStringLiteral("botpw"), false, {}},
                                  {QStringLiteral("guest"), QStringLiteral("guestpw"), false, {}},
                                  {QStringLiteral("dj"), QStringLiteral("djpw"), false, {}},
                                  {QStringLiteral("admin"), QStringLiteral("adminpw"), true, {}}};
        QString err;
        TTC_STEP("QVERIFY2(server.start(&err), qPrintable(err));");
        QVERIFY2(server.start(&err), qPrintable(err));
        bot = makeBot(botConfig());
        bot->start();
        TTC_STEP("QVERIFY2(waitOnline(bot.get()), qPrintable(bot->stateText()));");
        QVERIFY2(waitOnline(bot.get()), qPrintable(bot->stateText()));
    }

    void cleanupTestCase()
    {
        bot.reset();
        server.stop();
    }

    void answersHelpByPrivateMessage()
    {
        auto guest = listener(QStringLiteral("guest"), QStringLiteral("guestpw"), QStringLiteral("Guest"));
        TTC_STEP("QVERIFY(guest);");
        QVERIFY(guest);
        const int botId = bot->client()->myUserId();
        guest->pm(botId, QStringLiteral("h"));
        TTC_STEP("QVERIFY2(guest->waitText(botId, QStringLiteral(\"TT Central media bot\")");
        QVERIFY2(guest->waitText(botId, QStringLiteral("TT Central media bot")), qPrintable(guest->dump()));
        // The help is longer than one TeamTalk message; it arrives whole.
        TTC_STEP("QVERIFY(guest->texts.last().text.contains(QStringLiteral(\"[Information");
        QVERIFY(guest->texts.last().text.contains(QStringLiteral("[Information]")));
        guest->pm(botId, QStringLiteral("dance"));
        TTC_STEP("QVERIFY(guest->waitText(botId, QStringLiteral(\"Unknown command\")));");
        QVERIFY(guest->waitText(botId, QStringLiteral("Unknown command")));
    }

    void playsMusicIntoTheChannel()
    {
        auto guest = listener(QStringLiteral("guest"), QStringLiteral("guestpw"), QStringLiteral("Guest"));
        TTC_STEP("QVERIFY(guest);");
        QVERIFY(guest);
        const int botId = bot->client()->myUserId();
        guest->hearVoiceFrom(botId);
        int seen = 0;
        guest->pm(botId, QStringLiteral("url http://127.0.0.1/tone.mp3"));
        // Everyone in the channel hears what starts...
        TTC_STEP("QVERIFY2(guest->waitText(botId, QStringLiteral(\"Now playing: tone.mp3\"");
        QVERIFY2(guest->waitText(botId, QStringLiteral("Now playing: tone.mp3"), 15000, &seen), qPrintable(guest->dump()));
        // ...and the bot's voice stream carries it.
        TTC_STEP("QVERIFY2(waitUntil([&] { guest->pump(); return guest->audioBlocks.valu");
        QVERIFY2(waitUntil([&] { guest->pump(); return guest->audioBlocks.value(botId) > 20; }, 10000),
                 qPrintable(QStringLiteral("audio blocks: %1").arg(guest->audioBlocks.value(botId))));
        guest->pm(botId, QStringLiteral("np"));
        TTC_STEP("QVERIFY(guest->waitText(botId, QStringLiteral(\"Position:\"), 15000, &se");
        QVERIFY(guest->waitText(botId, QStringLiteral("Position:"), 15000, &seen));
        guest->pm(botId, QStringLiteral("pause"));
        TTC_STEP("QVERIFY(guest->waitText(botId, QStringLiteral(\"Paused.\"), 15000, &seen");
        QVERIFY(guest->waitText(botId, QStringLiteral("Paused."), 15000, &seen));
        QTest::qWait(1500);   // let what was already queued play out
        guest->pump();
        const int paused = guest->audioBlocks.value(botId);
        QTest::qWait(1000);
        guest->pump();
        TTC_STEP("QVERIFY2(guest->audioBlocks.value(botId) <= paused + 2,");
        QVERIFY2(guest->audioBlocks.value(botId) <= paused + 2,
                 qPrintable(QStringLiteral("%1 -> %2").arg(paused).arg(guest->audioBlocks.value(botId))));
        guest->pm(botId, QStringLiteral("resume"));
        TTC_STEP("QVERIFY(guest->waitText(botId, QStringLiteral(\"Resumed.\"), 15000, &see");
        QVERIFY(guest->waitText(botId, QStringLiteral("Resumed."), 15000, &seen));
        TTC_STEP("QVERIFY(waitUntil([&] { guest->pump(); return guest->audioBlocks.value");
        QVERIFY(waitUntil([&] { guest->pump(); return guest->audioBlocks.value(botId) > paused + 20; }, 10000));
        guest->pm(botId, QStringLiteral("stop"));
        TTC_STEP("QVERIFY(guest->waitText(botId, QStringLiteral(\"Stopped\"), 15000, &seen");
        QVERIFY(guest->waitText(botId, QStringLiteral("Stopped"), 15000, &seen));
        TTC_STEP("QVERIFY(!bot->player()->current());");
        QVERIFY(!bot->player()->current());
    }

    void channelCommandsNeedThePrefix()
    {
        auto guest = listener(QStringLiteral("guest"), QStringLiteral("guestpw"), QStringLiteral("Guest"));
        TTC_STEP("QVERIFY(guest);");
        QVERIFY(guest);
        const int botId = bot->client()->myUserId();
        const int chan = guest->d.c->myChannelId();
        guest->d.c->sendTextMessage(MSGTYPE_CHANNEL, 0, chan, QStringLiteral("v"));   // ordinary chat
        guest->d.c->sendTextMessage(MSGTYPE_CHANNEL, 0, chan, QStringLiteral("!v"));
        TTC_STEP("QVERIFY2(guest->waitText(botId, QStringLiteral(\"Volume is\")), qPrintab");
        QVERIFY2(guest->waitText(botId, QStringLiteral("Volume is")), qPrintable(guest->dump()));
        int replies = 0;
        for (const auto& t : guest->texts)
            replies += (t.fromUserId == botId && t.type == MSGTYPE_CHANNEL) ? 1 : 0;
        TTC_STEP("QCOMPARE(replies, 1);");
        QCOMPARE(replies, 1);
    }

    void adminCommandsNeedAnAdmin()
    {
        auto guest = listener(QStringLiteral("guest"), QStringLiteral("guestpw"), QStringLiteral("Guest"));
        auto dj = listener(QStringLiteral("dj"), QStringLiteral("djpw"), QStringLiteral("DJ"));
        auto admin = listener(QStringLiteral("admin"), QStringLiteral("adminpw"), QStringLiteral("Admin"));
        TTC_STEP("QVERIFY(guest && dj && admin);");
        QVERIFY(guest && dj && admin);
        const int botId = bot->client()->myUserId();
        guest->pm(botId, QStringLiteral("say hello"));
        TTC_STEP("QVERIFY(guest->waitText(botId, QStringLiteral(\"administrators only\")))");
        QVERIFY(guest->waitText(botId, QStringLiteral("administrators only")));
        dj->pm(botId, QStringLiteral("say listed in the admins setting"));        // in config.admins
        TTC_STEP("QVERIFY2(guest->waitText(botId, QStringLiteral(\"listed in the admins s");
        QVERIFY2(guest->waitText(botId, QStringLiteral("listed in the admins setting")), qPrintable(guest->dump()));
        admin->pm(botId, QStringLiteral("say a server administrator"));          // USERTYPE_ADMIN
        TTC_STEP("QVERIFY(guest->waitText(botId, QStringLiteral(\"a server administrator\"");
        QVERIFY(guest->waitText(botId, QStringLiteral("a server administrator")));
    }

    void lockedBotRefusesCommands()
    {
        auto guest = listener(QStringLiteral("guest"), QStringLiteral("guestpw"), QStringLiteral("Guest"));
        auto dj = listener(QStringLiteral("dj"), QStringLiteral("djpw"), QStringLiteral("DJ"));
        TTC_STEP("QVERIFY(guest && dj);");
        QVERIFY(guest && dj);
        const int botId = bot->client()->myUserId();
        dj->pm(botId, QStringLiteral("lock"));
        TTC_STEP("QVERIFY(dj->waitText(botId, QStringLiteral(\"password you want to set\")");
        QVERIFY(dj->waitText(botId, QStringLiteral("password you want to set")));
        dj->pm(botId, QStringLiteral("letmein"));
        TTC_STEP("QVERIFY(guest->waitText(botId, QStringLiteral(\"locked by DJ\")));");
        QVERIFY(guest->waitText(botId, QStringLiteral("locked by DJ")));
        guest->pm(botId, QStringLiteral("p anything"));
        TTC_STEP("QVERIFY(guest->waitText(botId, QStringLiteral(\"The bot is locked\")));");
        QVERIFY(guest->waitText(botId, QStringLiteral("The bot is locked")));
        guest->pm(botId, QStringLiteral("lock"));
        TTC_STEP("QVERIFY(guest->waitText(botId, QStringLiteral(\"unlock password\")));");
        QVERIFY(guest->waitText(botId, QStringLiteral("unlock password")));
        guest->pm(botId, QStringLiteral("letmein"));
        TTC_STEP("QVERIFY(guest->waitText(botId, QStringLiteral(\"now unlocked\")));");
        QVERIFY(guest->waitText(botId, QStringLiteral("now unlocked")));
    }

    void controlConnection()
    {
        ControlServer control(bot.get());
        const QString name = QStringLiteral("ttc-test-") + QUuid::createUuid().toString(QUuid::Id128);
        QString err;
        TTC_STEP("QVERIFY2(control.listen(name, &err), qPrintable(err));");
        QVERIFY2(control.listen(name, &err), qPrintable(err));
        QLocalSocket s;
        s.connectToServer(name);
        TTC_STEP("QVERIFY(s.waitForConnected(5000));");
        QVERIFY(s.waitForConnected(5000));
        QList<QJsonObject> got;
        auto read = [&] {
            while (s.canReadLine())
                got << QJsonDocument::fromJson(s.readLine()).object();
        };
        auto waitType = [&](const QString& type, const std::function<bool(const QJsonObject&)>& pred) {
            return waitUntil([&] {
                read();
                for (const auto& o : got)
                    if (o.value(QStringLiteral("type")).toString() == type && (!pred || pred(o)))
                        return true;
                return false;
            }, 10000);
        };
        TTC_STEP("QVERIFY(waitType(QStringLiteral(\"hello\"), {}));");
        QVERIFY(waitType(QStringLiteral("hello"), {}));
        TTC_STEP("QVERIFY(waitType(QStringLiteral(\"status\"), [](const QJsonObject& o) { ");
        QVERIFY(waitType(QStringLiteral("status"), [](const QJsonObject& o) { return o.value(QStringLiteral("online")).toBool(); }));
        s.write("{\"cmd\":\"command\",\"id\":41,\"text\":\"v 40\"}" "\n");
        TTC_STEP("QVERIFY(waitType(QStringLiteral(\"reply\"), [](const QJsonObject& o) {");
        QVERIFY(waitType(QStringLiteral("reply"), [](const QJsonObject& o) {
            return o.value(QStringLiteral("id")).toInt() == 41 && o.value(QStringLiteral("text")).toString() == QLatin1String("Volume set to 40%.");
        }));
        TTC_STEP("QVERIFY(waitType(QStringLiteral(\"status\"), [](const QJsonObject& o) { ");
        QVERIFY(waitType(QStringLiteral("status"), [](const QJsonObject& o) { return o.value(QStringLiteral("volume")).toInt() == 40; }));
        s.write("not json\n");   // ignored, not fatal
        s.write("{\"cmd\":\"command\",\"id\":42,\"text\":\"url http://127.0.0.1/ctl.ogg\"}" "\n");
        TTC_STEP("QVERIFY(waitType(QStringLiteral(\"status\"), [](const QJsonObject& o) {");
        QVERIFY(waitType(QStringLiteral("status"), [](const QJsonObject& o) {
            return o.value(QStringLiteral("current")).toObject().value(QStringLiteral("title")).toString() == QLatin1String("ctl.ogg");
        }));
        TTC_STEP("QVERIFY(waitType(QStringLiteral(\"log\"), [](const QJsonObject& o) {");
        QVERIFY(waitType(QStringLiteral("log"), [](const QJsonObject& o) {
            return o.value(QStringLiteral("text")).toString().startsWith(QLatin1String("Now playing"));
        }));
        bot->player()->stop();
        bot->player()->setVolume(100);
    }

    void reconnectsAfterServerRestart()
    {
        bot->player()->enqueue({DirectUrlSource::trackFor(QStringLiteral("http://127.0.0.1/long.mp3"), {})});
        TTC_STEP("QVERIFY(bot->player()->isPlaying());");
        QVERIFY(bot->player()->isPlaying());
        QString err;
        TTC_STEP("QVERIFY2(server.restart(&err), qPrintable(err));");
        QVERIFY2(server.restart(&err), qPrintable(err));
        TTC_STEP("QVERIFY(waitUntil([&] { return bot->state() != MediaBot::State::Online");
        QVERIFY(waitUntil([&] { return bot->state() != MediaBot::State::Online; }, 15000));
        TTC_STEP("QVERIFY2(waitOnline(bot.get(), 30000), qPrintable(bot->stateText()));");
        QVERIFY2(waitOnline(bot.get(), 30000), qPrintable(bot->stateText()));
        // The queue survived the outage and playback carries on.
        TTC_STEP("QVERIFY(bot->player()->current());");
        QVERIFY(bot->player()->current());
        TTC_STEP("QVERIFY(waitUntil([&] { return bot->player()->isPlaying() && !bot->pla");
        QVERIFY(waitUntil([&] { return bot->player()->isPlaying() && !bot->player()->isPaused(); }, 5000));
        bot->player()->stop();
    }

    void wrongPasswordStopsRetrying()
    {
        MediaBotConfig cfg = botConfig();
        cfg.password = QStringLiteral("nope");
        auto b = makeBot(cfg);
        b->start();
        TTC_STEP("QVERIFY(waitUntil([&] { return b->state() == MediaBot::State::Failed; ");
        QVERIFY(waitUntil([&] { return b->state() == MediaBot::State::Failed; }, 15000));
        TTC_STEP("QVERIFY(b->stateText().contains(QStringLiteral(\"invalid\")));");
        QVERIFY(b->stateText().contains(QStringLiteral("invalid")));
    }
};

QTEST_GUILESS_MAIN(TestIntegrationMediaBot)
#include "tst_integration_mediabot.moc"
