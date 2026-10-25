// SPDX-License-Identifier: GPL-3.0-or-later
// Unit tests for the media bot: tracks, the player queue, sources, yt-dlp
// output, chat commands, locking and configuration.
#include "mediafakes.h"

#include "ttc/media/botconfig.h"
#include "ttc/media/pipeprocess.h"
#include "ttc/media/ytdlp.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>

using namespace ttc::media;
using namespace ttc::test;

class TestMedia : public QObject
{
    Q_OBJECT

    struct Harness
    {
        FakeEngine engine;
        FakeResolver resolver;
        Player player{&engine, &resolver};
        QStringList announced;
        Harness()
        {
            player.announce = [this](const QString& t) { announced << t; };
        }
    };

    struct Chat
    {
        Harness h;
        FakeSources sources;
        FakeBot bot;
        LockManager lock{&bot};
        Dispatcher d{&h.player, &sources, &bot, &lock, DispatcherSettings{200, 600, QStringLiteral("1.2.3")}};
        QStringList replies;

        void say(const QString& text, int user = 5, bool admin = false, bool priv = true)
        {
            CommandContext ctx;
            ctx.requester = QStringLiteral("Alice");
            ctx.userId = user;
            ctx.isAdmin = admin;
            ctx.isPrivate = priv;
            ctx.reply = [this](const QString& r) { replies << r; };
            d.handle(text, ctx);
        }
        QString last() const { return replies.isEmpty() ? QString() : replies.last(); }
    };

private slots:
    // --- tracks ---------------------------------------------------------
    void formatsDurationsAndLabels()
    {
        QCOMPARE(fmtDuration(0), QStringLiteral("?:??"));
        QCOMPARE(fmtDuration(65), QStringLiteral("1:05"));
        QCOMPARE(fmtDuration(3725), QStringLiteral("1:02:05"));
        Track t;
        t.title = QStringLiteral("Song");
        t.artist = QStringLiteral("Band");
        t.duration = 61;
        QCOMPARE(t.label(), QStringLiteral("Band - Song [1:01]"));
    }

    void parsesTimes()
    {
        double s = 0;
        QVERIFY(parseTime(QStringLiteral("90"), &s) && s == 90);
        QVERIFY(parseTime(QStringLiteral("1:30"), &s) && s == 90);
        QVERIFY(parseTime(QStringLiteral("1:02:03"), &s) && s == 3723);
        QVERIFY(!parseTime(QStringLiteral("1:x"), &s));
        QVERIFY(!parseTime(QStringLiteral("-5"), &s));
        QVERIFY(!parseTime(QStringLiteral("1:2:3:4"), &s));
        QVERIFY(!parseTime(QString(), &s));
    }

    void replayableDropsExpiringUrls()
    {
        TrackPtr yt = track(QStringLiteral("a"));
        TrackPtr copy = replayable(yt);
        QVERIFY(copy != yt);
        QVERIFY(copy->needsResolve && copy->playbackUrl.isEmpty());
        TrackPtr direct = DirectUrlSource::trackFor(QStringLiteral("https://x.org/a.mp3"), {});
        QVERIFY(!replayable(direct)->needsResolve);            // direct links don't expire
        QVERIFY(replayable(direct, true)->needsResolve);       // ...unless forced
    }

    void repeatModes()
    {
        RepeatMode m;
        QVERIFY(parseRepeatMode(QStringLiteral("Track"), &m) && m == RepeatMode::Track);
        QVERIFY(parseRepeatMode(QStringLiteral("all"), &m) && m == RepeatMode::Queue);
        QVERIFY(!parseRepeatMode(QStringLiteral("sometimes"), &m));
    }

    // --- player ---------------------------------------------------------
    void enqueueStartsImmediately()
    {
        Harness h;
        QCOMPARE(h.player.enqueue({track(QStringLiteral("one"))}), 0);
        QCOMPARE(h.engine.played, QStringList{QStringLiteral("one")});
        QVERIFY(h.announced.last().startsWith(QStringLiteral("Now playing: one")));
        QCOMPARE(h.player.enqueue({track(QStringLiteral("two"))}), 1);
        QCOMPARE(h.player.queue().size(), 1);
    }

    void finishAdvancesAndRepeats()
    {
        Harness h;
        h.resolver.immediate = true;   // repeated tracks are re-resolved
        h.player.enqueue({track(QStringLiteral("a")), track(QStringLiteral("b"))});
        h.engine.finish();
        QCOMPARE(h.engine.played, (QStringList{QStringLiteral("a"), QStringLiteral("b")}));
        h.player.setRepeat(RepeatMode::Track);
        h.engine.finish();
        QCOMPARE(h.engine.played, (QStringList{QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("b")}));
        h.player.setRepeat(RepeatMode::Queue);
        h.player.enqueue({track(QStringLiteral("c"))});
        h.engine.finish();   // b ends -> requeued at the end; c plays
        QCOMPARE(h.engine.played.last(), QStringLiteral("c"));
        h.engine.finish();   // c ends -> b again
        QCOMPARE(h.engine.played.last(), QStringLiteral("b"));
        h.player.setRepeat(RepeatMode::Off);
        h.player.clear();
        h.engine.finish();
        QVERIFY(!h.player.current());
    }

    void errorReresolvesOnceThenSkips()
    {
        Harness h;
        h.resolver.immediate = true;
        h.player.enqueue({track(QStringLiteral("bad"))});
        h.engine.finish(QStringLiteral("error"));     // retried once
        QCOMPARE(h.engine.played.size(), 2);
        h.engine.finish(QStringLiteral("error"));     // gives up
        QVERIFY(!h.player.current());
        QVERIFY(h.announced.last().contains(QStringLiteral("Couldn't play")));
    }

    void lateCallbackAfterStopIsIgnored()
    {
        Harness h;
        h.player.enqueue({track(QStringLiteral("a")), track(QStringLiteral("b"))});
        TrackPtr a = h.engine.playing;
        h.player.stop();
        h.engine.onFinished(a, QStringLiteral("finished"));
        QVERIFY(!h.player.current());
        QCOMPARE(h.engine.played.size(), 1);
    }

    void skipPreviousSelectRemoveMove()
    {
        Harness h;
        h.player.enqueue({track(QStringLiteral("a")), track(QStringLiteral("b")), track(QStringLiteral("c")),
                          track(QStringLiteral("d"))});
        h.player.skip();
        QCOMPARE(h.player.current()->title, QStringLiteral("b"));
        h.resolver.immediate = true;   // history items are re-resolved
        h.player.previous();
        QCOMPARE(h.player.current()->title, QStringLiteral("a"));
        QCOMPARE(h.player.queue().first()->title, QStringLiteral("b"));
        QVERIFY(h.player.move(3, 1));  // d to the front
        QCOMPARE(h.player.queue().first()->title, QStringLiteral("d"));
        QCOMPARE(h.player.removeAt(1)->title, QStringLiteral("d"));
        QVERIFY(!h.player.removeAt(9));
        h.player.select(2);            // drops b, plays c
        QCOMPARE(h.player.current()->title, QStringLiteral("c"));
        QVERIFY(h.player.queue().isEmpty());
        QString err;
        QVERIFY(!h.player.select(1, &err));
        QVERIFY(!err.isEmpty());
    }

    void resolvesLazilyAndIgnoresSupersededResults()
    {
        Harness h;
        h.player.enqueue({track(QStringLiteral("slow"), false), track(QStringLiteral("next"), false)});
        QVERIFY(h.player.isLoading());
        QCOMPARE(h.player.current()->title, QStringLiteral("slow"));
        QCOMPARE(h.resolver.pending.size(), 1);
        h.player.skip();                                 // user skips while it loads
        QCOMPARE(h.player.current()->title, QStringLiteral("next"));
        h.resolver.complete(true);                       // the stale "slow" result
        QVERIFY(h.engine.played.isEmpty());              // ...must not start anything
        h.resolver.complete(true);                       // "next" resolves
        QCOMPARE(h.engine.played, QStringList{QStringLiteral("next")});
        QVERIFY(!h.player.isLoading());
    }

    void skipsWhatCannotLoad()
    {
        Harness h;
        h.player.enqueue({track(QStringLiteral("gone"), false), track(QStringLiteral("ok"))});
        h.resolver.complete(false);
        QCOMPARE(h.engine.played, QStringList{QStringLiteral("ok")});
        QVERIFY(h.announced.join(QChar('\n')).contains(QStringLiteral("couldn't load")));
        h.engine.failNextPlay = true;
        h.player.enqueue({track(QStringLiteral("x")), track(QStringLiteral("y"))});
        h.engine.finish();             // x fails to start -> y plays
        QCOMPARE(h.engine.played.last(), QStringLiteral("y"));
    }

    void prefetchesTheNextTrack()
    {
        Harness h;
        h.player.enqueue({track(QStringLiteral("now")), track(QStringLiteral("soon"), false)});
        QCOMPARE(h.resolver.pending.size(), 1);
        QCOMPARE(h.resolver.pending.first().track->title, QStringLiteral("soon"));
    }

    void queueLimit()
    {
        Harness h;
        h.player.setMaxQueue(2);
        h.player.enqueue({track(QStringLiteral("a"))});
        QString err;
        QCOMPARE(h.player.enqueue({track(QStringLiteral("b")), track(QStringLiteral("c")), track(QStringLiteral("d"))}, &err), -1);
        QVERIFY(err.contains(QStringLiteral("full")));
    }

    // --- sources (no network) --------------------------------------------
    void matchesServiceLinks()
    {
        SourcesSettings s;
        SourceRegistry r(s);
        auto nameFor = [&](const QString& url) {
            Source* src = r.sourceForUrl(url);
            return src ? src->name() : QString();
        };
        QCOMPARE(nameFor(QStringLiteral("https://www.youtube.com/watch?v=abc")), QStringLiteral("youtube"));
        QCOMPARE(nameFor(QStringLiteral("https://youtu.be/abc")), QStringLiteral("youtube"));
        QCOMPARE(nameFor(QStringLiteral("https://soundcloud.com/forss/flickermood")), QStringLiteral("soundcloud"));
        QCOMPARE(nameFor(QStringLiteral("https://open.spotify.com/track/4PTG3Z6ehGkBFwjybzWkR8")), QStringLiteral("spotify"));
        QCOMPARE(nameFor(QStringLiteral("https://music.apple.com/us/album/x/123?i=456")), QStringLiteral("apple"));
        QCOMPARE(nameFor(QStringLiteral("https://cdn.example.com/song.mp3")), QStringLiteral("url"));
        QCOMPARE(nameFor(QStringLiteral("https://ice1.somafm.com/groovesalad-128-mp3")), QStringLiteral("url"));
        QCOMPARE(nameFor(QStringLiteral("https://bandcamp.com/album/x")), QStringLiteral("web"));
        QVERIFY(r.isUrl(QStringLiteral("spotify:track:abc")));
        QVERIFY(!r.isUrl(QStringLiteral("daft punk")));
        QCOMPARE(r.services(), (QStringList{QStringLiteral("youtube"), QStringLiteral("soundcloud"), QStringLiteral("apple")}));
        QString err;
        QVERIFY(r.setService(QStringLiteral("SC"), &err));
        QCOMPARE(r.service(), QStringLiteral("soundcloud"));
        QVERIFY(!r.setService(QStringLiteral("napster"), &err));
        QVERIFY(err.contains(QStringLiteral("Available")));
    }

    void disabledSourcesStayOut()
    {
        SourcesSettings s;
        s.youtube = false;
        s.directUrl = false;
        SourceRegistry r(s);
        QCOMPARE(r.service(), QStringLiteral("soundcloud"));
        QCOMPARE(r.sourceForUrl(QStringLiteral("https://youtu.be/x"))->name(), QStringLiteral("web"));
        QString err;
        r.playDirect(QStringLiteral("https://x.org/a.mp3"), {}, [&](const TrackList&, const QString& e) { err = e; });
        QVERIFY(err.contains(QStringLiteral("turned off")));
        // Spotify tracks can't be matched without YouTube.
        TrackPtr sp = track(QStringLiteral("x"), false);
        sp->source = QStringLiteral("spotify");
        bool ok = true;
        r.resolve(sp, [&](bool v) { ok = v; });
        QVERIFY(!ok);
    }

    void parsesSpotifyAndAppleLinks()
    {
        QCOMPARE(SpotifySource::parse(QStringLiteral("https://open.spotify.com/track/4PTG3Z6ehGkBFwjybzWkR8")),
                 qMakePair(QStringLiteral("track"), QStringLiteral("4PTG3Z6ehGkBFwjybzWkR8")));
        QCOMPARE(SpotifySource::parse(QStringLiteral("https://open.spotify.com/intl-de/album/6XzHyAWCSRYVWTcqBkkeMh")).first,
                 QStringLiteral("album"));
        QCOMPARE(SpotifySource::parse(QStringLiteral("spotify:playlist:37i9dQZF1DXcBWIGoYBM5M")).first,
                 QStringLiteral("playlist"));
        QVERIFY(SpotifySource::parse(QStringLiteral("https://example.com")).first.isEmpty());
        QCOMPARE(AppleMusicSource::country(QStringLiteral("https://music.apple.com/gb/album/x/1")), QStringLiteral("gb"));
        QCOMPARE(AppleMusicSource::country(QStringLiteral("https://music.apple.com/album/x/1")), QStringLiteral("us"));

        HttpJson http;
        SpotifySource sp(&http, {}, {}, 100);
        QString err;
        sp.fromUrl(QStringLiteral("https://open.spotify.com/album/6XzHyAWCSRYVWTcqBkkeMh"), {},
                   [&](const TrackList&, const QString& e) { err = e; });
        QVERIFY(err.contains(QStringLiteral("developer app")));
        AppleMusicSource am(&http, 100);
        am.fromUrl(QStringLiteral("https://music.apple.com/us/playlist/x/pl.123"), {},
                   [&](const TrackList&, const QString& e) { err = e; });
        QVERIFY(err.contains(QStringLiteral("playlists")));
    }

    void mapsMetadataToTracks()
    {
        const QJsonObject spotify = QJsonDocument::fromJson("{\"name\":\"Get Lucky\",\"duration_ms\":248000, \"artists\":[{\"name\":\"Daft Punk\"},{\"name\":\"Pharrell Williams\"}], \"external_urls\":{\"spotify\":\"https://open.spotify.com/track/x\"}}").object();
        TrackPtr t = SpotifySource::trackFromApi(spotify, QStringLiteral("bob"));
        QCOMPARE(t->query, QStringLiteral("Daft Punk, Pharrell Williams - Get Lucky"));
        QCOMPARE(t->duration, 248.0);
        QVERIFY(t->needsResolve);
        const QJsonObject apple = QJsonDocument::fromJson("{\"trackName\":\"One\",\"artistName\":\"U2\", \"trackTimeMillis\":276000,\"trackViewUrl\":\"https://music.apple.com/x\"}").object();
        TrackPtr a = AppleMusicSource::trackFromResult(apple, {});
        QCOMPARE(a->label(), QStringLiteral("U2 - One [4:36]"));
        QCOMPARE(a->source, QStringLiteral("apple"));
    }

    void directLinksAndRadio()
    {
        TrackPtr song = DirectUrlSource::trackFor(QStringLiteral("https://cdn.example.com/My%20Song.mp3"), {});
        QVERIFY(!song->isLive);
        QCOMPARE(song->title, QStringLiteral("My Song.mp3"));
        TrackPtr radio = DirectUrlSource::trackFor(QStringLiteral("https://ice1.somafm.com/groovesalad-128-mp3"), {});
        QVERIFY(radio->isLive);
    }

    // --- yt-dlp output --------------------------------------------------
    void readsYtDlpJson()
    {
        const QJsonObject info = QJsonDocument::fromJson("{\"title\":\"Clip\",\"uploader\":\"Chan\",\"duration\":200, \"url\":\"https://rr.googlevideo.com/x\",\"webpage_url\":\"https://www.youtube.com/watch?v=abc\", \"http_headers\":{\"User-Agent\":\"UA\"},\"live_status\":\"not_live\"}").object();
        TrackPtr t = trackFromInfo(info, QStringLiteral("youtube"), QStringLiteral("me"));
        QCOMPARE(t->artist, QStringLiteral("Chan"));
        QCOMPARE(t->playbackUrl, QStringLiteral("https://rr.googlevideo.com/x"));
        QCOMPARE(t->httpHeaders.value(QStringLiteral("User-Agent")), QStringLiteral("UA"));
        QVERIFY(!t->isLive && !t->needsResolve);

        const QJsonObject entry = QJsonDocument::fromJson("{\"id\":\"xyz\",\"title\":\"E\",\"duration\":10}").object();
        TrackPtr e = trackFromEntry(entry, QStringLiteral("youtube"), {});
        QCOMPARE(e->query, QStringLiteral("https://www.youtube.com/watch?v=xyz"));
        QVERIFY(e->needsResolve);

        Track r;
        QVERIFY(applyStreamInfo(info, r));
        QCOMPARE(r.duration, 200.0);
        QVERIFY(!applyStreamInfo(QJsonObject{}, r));

        QList<QJsonObject> entries;
        for (int d : {100, 245, 400})
            entries << QJsonObject{{QStringLiteral("duration"), d}};
        QCOMPARE(bestByDuration(entries, 248).value(QStringLiteral("duration")).toInt(), 245);
        QCOMPARE(bestByDuration(entries, 0).value(QStringLiteral("duration")).toInt(), 100);
    }

    void buildsYtDlpArguments()
    {
        const QStringList a = YtDlpRunner::arguments(QStringLiteral("-weird"), true, 50, QStringLiteral("C:/deno.exe"));
        QVERIFY(a.contains(QStringLiteral("-J")));
        QVERIFY(a.contains(QStringLiteral("--flat-playlist")));
        QCOMPARE(a.at(a.indexOf(QStringLiteral("--playlist-end")) + 1), QStringLiteral("50"));
        QCOMPARE(a.at(a.indexOf(QStringLiteral("--js-runtimes")) + 1), QStringLiteral("deno:C:/deno.exe"));
        // The target always follows "--", so it can't be read as an option.
        QCOMPARE(a.last(), QStringLiteral("-weird"));
        QCOMPARE(a.at(a.size() - 2), QStringLiteral("--"));
        QVERIFY(YtDlpRunner::friendlyError(QStringLiteral("ERROR: [youtube] x: Private video")).contains(QStringLiteral("private")));
        QVERIFY(YtDlpRunner::friendlyError(QStringLiteral("ERROR: Unsupported URL: x")).contains(QStringLiteral("supported")));
    }

    void ffmpegCommandLine()
    {
        Track t;
        t.playbackUrl = QStringLiteral("https://host/a.webm");
        t.httpHeaders.insert(QStringLiteral("Referer"), QStringLiteral("https://x"));
        const QStringList c = AudioEngine::ffmpegCommand(QStringLiteral("ffmpeg"), t, 12.5, 48000, 2);
        QCOMPARE(c.first(), QStringLiteral("ffmpeg"));
        QCOMPARE(c.at(c.indexOf(QStringLiteral("-ss")) + 1), QStringLiteral("12.500"));
        QCOMPARE(c.at(c.indexOf(QStringLiteral("-headers")) + 1), QStringLiteral("Referer: https://x\r\n"));
        QVERIFY(c.contains(QStringLiteral("-reconnect")));
        QCOMPARE(c.at(c.indexOf(QStringLiteral("-i")) + 1), t.playbackUrl);
        QCOMPARE(c.last(), QStringLiteral("pipe:1"));
        t.playbackUrl = QStringLiteral("C:/music/a.flac");
        QVERIFY(!AudioEngine::ffmpegCommand(QStringLiteral("ffmpeg"), t, 0, 48000, 2).contains(QStringLiteral("-reconnect")));
    }

    void quotesWindowsCommandLines()
    {
        QCOMPARE(PipeProcess::windowsCommandLine(QStringLiteral("C:/Program Files/ff.exe"),
                                                 {QStringLiteral("-i"), QStringLiteral("a b"), QString(),
                                                  QStringLiteral("say \"hi\""), QStringLiteral("dir\\")}),
                 QStringLiteral("\"C:/Program Files/ff.exe\" -i \"a b\" \"\" \"say \\\"hi\\\"\" dir\\"));
        QCOMPARE(PipeProcess::windowsCommandLine(QStringLiteral("x"), {QStringLiteral("a b\\")}),
                 QStringLiteral("x \"a b\\\\\""));
    }

    // --- commands -------------------------------------------------------
    void unknownCommandsAndHelp()
    {
        Chat c;
        c.say(QStringLiteral("dance"));
        QVERIFY(c.last().contains(QStringLiteral("Unknown command")));
        c.say(QStringLiteral("h"));
        QVERIFY(c.last().contains(QStringLiteral("[Playback]")));
        QVERIFY(!c.last().contains(QStringLiteral("say:")));      // admin commands hidden
        c.say(QStringLiteral("h"), 5, true);
        QVERIFY(c.last().contains(QStringLiteral("say:")));
        c.say(QStringLiteral("help seek"));
        QVERIFY(c.last().contains(QStringLiteral("Usage: seek")));
        c.say(QStringLiteral("about"));
        QVERIFY(c.last().contains(QStringLiteral("1.2.3")));
    }

    void playSearchSelect()
    {
        Chat c;
        c.sources.searchResults = {track(QStringLiteral("r1")), track(QStringLiteral("r2")), track(QStringLiteral("r3"))};
        c.say(QStringLiteral("p"));
        QVERIFY(c.last().contains(QStringLiteral("Usage")));
        c.say(QStringLiteral("p daft punk"));
        QCOMPARE(c.sources.calls.last(), QStringLiteral("search:daft punk:1"));
        QCOMPARE(c.h.engine.played, QStringList{QStringLiteral("r1")});
        c.say(QStringLiteral("s something"));
        QVERIFY(c.last().contains(QStringLiteral("2. r2")));
        c.say(QStringLiteral("sel 3"));
        QVERIFY(c.last().contains(QStringLiteral("#1: r3")));
        c.say(QStringLiteral("p 2"));
        QVERIFY(c.last().contains(QStringLiteral("#2: r2")));
        c.say(QStringLiteral("sel 9"));
        QVERIFY(c.last().contains(QStringLiteral("between 1 and 3")));
        c.say(QStringLiteral("p https://youtu.be/x"));
        QCOMPARE(c.sources.calls.last(), QStringLiteral("fromInput:https://youtu.be/x"));
        c.say(QStringLiteral("url nope"));
        QVERIFY(c.last().contains(QStringLiteral("isn't a URL")));
        c.say(QStringLiteral("url https://radio.example/stream"));
        QCOMPARE(c.sources.calls.last(), QStringLiteral("playDirect:https://radio.example/stream"));
        c.sources.nextError = QStringLiteral("Nothing found");
        c.say(QStringLiteral("p zzz"));
        QCOMPARE(c.last(), QStringLiteral("Nothing found"));
    }

    void rejectsTracksOverTheLengthLimit()
    {
        Chat c;   // limit: 600 s
        c.sources.searchResults = {track(QStringLiteral("long"), true, 3600)};
        c.say(QStringLiteral("p long mix"));
        QVERIFY(c.last().contains(QStringLiteral("the limit is 10:00")));
        QVERIFY(c.h.engine.played.isEmpty());
    }

    void transportCommands()
    {
        Chat c;
        c.say(QStringLiteral("next"));
        QVERIFY(c.last().contains(QStringLiteral("Nothing is playing")));
        c.sources.searchResults = {track(QStringLiteral("a"), true, 300)};
        c.say(QStringLiteral("p a"));
        c.say(QStringLiteral("pause"));
        QCOMPARE(c.last(), QStringLiteral("Paused."));
        c.say(QStringLiteral("pp"));
        QCOMPARE(c.last(), QStringLiteral("Resumed."));
        c.say(QStringLiteral("seek 1:30"));
        QVERIFY(c.last().contains(QStringLiteral("1:30")));
        c.say(QStringLiteral("seek soon"));
        QVERIFY(c.last().contains(QStringLiteral("m:ss")));
        c.say(QStringLiteral("ff 10"));
        QVERIFY(c.last().contains(QStringLiteral("1:40")));
        c.say(QStringLiteral("v"));
        QCOMPARE(c.last(), QStringLiteral("Volume is 100%."));
        c.say(QStringLiteral("v 250"));
        QVERIFY(c.last().contains(QStringLiteral("capped at 200")));
        c.say(QStringLiteral("v loud"));
        QVERIFY(c.last().contains(QStringLiteral("Usage")));
        c.say(QStringLiteral("repeat track"));
        QCOMPARE(c.h.player.repeat(), RepeatMode::Track);
        c.say(QStringLiteral("shuffle"));
        QVERIFY(c.h.player.shuffle());
        c.say(QStringLiteral("np"));
        QVERIFY(c.last().contains(QStringLiteral("Now playing: a")));
        QVERIFY(c.last().contains(QStringLiteral("requested by Alice")) || c.last().contains(QStringLiteral("Position")));
        c.say(QStringLiteral("q"));
        QVERIFY(c.last().contains(QStringLiteral("Now playing")));
        c.say(QStringLiteral("stop"));
        QVERIFY(!c.h.player.current());
        c.say(QStringLiteral("q"));
        QVERIFY(c.last().contains(QStringLiteral("empty")));
    }

    void serviceCommands()
    {
        Chat c;
        c.say(QStringLiteral("sv"));
        QVERIFY(c.last().contains(QStringLiteral("youtube, soundcloud")));
        c.say(QStringLiteral("sv soundcloud"));
        QCOMPARE(c.sources.current, QStringLiteral("soundcloud"));
        c.say(QStringLiteral("sv napster"));
        QVERIFY(c.last().contains(QStringLiteral("Unknown service")));
    }

    void adminCommands()
    {
        Chat c;
        c.say(QStringLiteral("say hello"));
        QVERIFY(c.last().contains(QStringLiteral("administrators only")));
        QVERIFY(c.bot.channelMessages.isEmpty());
        c.say(QStringLiteral("say hello all"), 5, true);
        QCOMPARE(c.bot.channelMessages.last(), QStringLiteral("hello all"));
        c.say(QStringLiteral("update"), 5, true);
        QVERIFY(c.last().contains(QStringLiteral("Updated yt-dlp")));
        c.say(QStringLiteral("quit"), 5, true);
        QCOMPARE(c.bot.shutdowns, 1);
    }

    void lockAndUnlock()
    {
        Chat c;
        c.say(QStringLiteral("lock"));                    // not an admin
        QVERIFY(c.last().contains(QStringLiteral("not allowed")));
        c.say(QStringLiteral("lock"), 5, true);
        QVERIFY(c.last().contains(QStringLiteral("password you want to set")));
        c.say(QStringLiteral("s3cret"), 5, true);         // the password, not a command
        QVERIFY(c.lock.locked());
        QVERIFY(c.bot.channelMessages.last().contains(QStringLiteral("locked by Alice")));
        c.say(QStringLiteral("p music"), 9);
        QVERIFY(c.last().contains(QStringLiteral("locked")));
        c.say(QStringLiteral("lock"), 9);                 // anyone may try to unlock
        c.say(QStringLiteral("wrong"), 9);
        QVERIFY(c.last().contains(QStringLiteral("Incorrect")));
        QVERIFY(c.lock.locked());
        c.say(QStringLiteral("lock"), 9);
        c.say(QStringLiteral("cancel"), 9);
        QVERIFY(c.last().contains(QStringLiteral("Cancelled")));
        c.say(QStringLiteral("lock"), 9);
        c.say(QStringLiteral("s3cret"), 9);
        QVERIFY(!c.lock.locked());
    }

    void lockFromChannelAsksPrivately()
    {
        Chat c;
        c.say(QStringLiteral("lock"), 5, true, false);    // typed in the channel
        QVERIFY(c.last().contains(QStringLiteral("private messages")));
        QCOMPARE(c.bot.privateMessages.size(), 1);
        QCOMPARE(c.bot.privateMessages.first().first, 5);
    }

    // --- configuration ----------------------------------------------------
    void savesAndLoadsConfig()
    {
        QTemporaryDir dir;
        QSettings s(dir.filePath(QStringLiteral("bot.ini")), QSettings::IniFormat);
        MediaBotConfig c;
        c.host = QStringLiteral("tt.example.org");
        c.username = QStringLiteral("bot");
        c.password = QStringLiteral("pw");
        c.channelPath = QStringLiteral("Music");
        c.admins = {QStringLiteral("alice"), QStringLiteral("bob")};
        c.channelPrefix = QStringLiteral("!");
        c.audio.defaultVolume = 60;
        c.sources.soundcloud = false;
        c.sources.spotifyClientId = QStringLiteral("id");
        saveMediaBotConfig(s, c);
        s.sync();
        QSettings s2(dir.filePath(QStringLiteral("bot.ini")), QSettings::IniFormat);
        MediaBotConfig back = loadMediaBotConfig(s2, false);
        c.channelPath = QStringLiteral("/Music/");
        QVERIFY(back == c);
        QVERIFY(back.validationError().isEmpty());
        back.username.clear();
        QVERIFY(!back.validationError().isEmpty());
    }

    void environmentOverridesSecrets()
    {
        QTemporaryDir dir;
        QSettings s(dir.filePath(QStringLiteral("bot.ini")), QSettings::IniFormat);
        MediaBotConfig c;
        c.password = QStringLiteral("file");
        saveMediaBotConfig(s, c);
        qputenv("TTC_MEDIABOT_PASSWORD", "env");
        QCOMPARE(loadMediaBotConfig(s).password, QStringLiteral("env"));
        QCOMPARE(loadMediaBotConfig(s, false).password, QStringLiteral("file"));
        qunsetenv("TTC_MEDIABOT_PASSWORD");
    }

    void findsTools()
    {
        QTemporaryDir dir;
#if defined(Q_OS_WIN)
        const QString exe = QStringLiteral("ffmpeg.exe");
#else
        const QString exe = QStringLiteral("ffmpeg");
#endif
        QDir(dir.path()).mkpath(QStringLiteral("ffmpeg/bin"));
        QFile f(dir.filePath(QStringLiteral("ffmpeg/bin/") + exe));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();
        QCOMPARE(QFileInfo(findTool({}, QStringLiteral("ffmpeg"), {dir.path()})).canonicalFilePath(),
                 QFileInfo(f.fileName()).canonicalFilePath());
        QCOMPARE(findTool(f.fileName(), QStringLiteral("x"), {}), QFileInfo(f.fileName()).absoluteFilePath());
        QVERIFY(findTool({}, QStringLiteral("no-such-tool-ttc"), {dir.path()}).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestMedia)
#include "tst_media.moc"
