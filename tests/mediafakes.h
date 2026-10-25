// TT Central tests — fakes for the media bot's engine, resolver, sources and
// bot actions.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ttc/media/commands.h"
#include "ttc/media/player.h"
#include "ttc/media/sources.h"

#include <QList>
#include <QStringList>

namespace ttc::test {

using namespace ttc::media;

class FakeEngine final : public PlaybackEngine
{
public:
    TrackPtr playing;
    bool paused = false;
    int vol = 100;
    int maxVol = 200;
    double pos = 0;
    bool failNextPlay = false;
    QStringList played;   // titles, in order

    bool play(const TrackPtr& t, QString* error) override
    {
        if (failNextPlay)
        {
            failNextPlay = false;
            if (error)
                *error = QStringLiteral("boom");
            return false;
        }
        playing = t;
        paused = false;
        pos = 0;
        played << t->title;
        return true;
    }
    void stop() override { playing.reset(); paused = false; }
    bool pause() override { if (!playing || paused) return false; paused = true; return true; }
    bool resume() override { if (!playing || !paused) return false; paused = false; return true; }
    bool seek(double s) override { if (!playing || playing->isLive) return false; pos = s; return true; }
    int setVolume(int p) override { vol = qBound(0, p, maxVol); return vol; }
    int volume() const override { return vol; }
    bool isPlaying() const override { return bool(playing); }
    bool isPaused() const override { return paused; }
    double elapsed() const override { return pos; }

    /// Simulate the end of the current track.
    void finish(const QString& reason = QStringLiteral("finished"))
    {
        TrackPtr t = playing;
        playing.reset();
        if (t && onFinished)
            onFinished(t, reason);
    }
};

/// Resolves on demand: tests decide when (and whether) each one succeeds.
class FakeResolver final : public Resolver
{
public:
    struct Pending
    {
        TrackPtr track;
        std::function<void(bool)> done;
    };
    QList<Pending> pending;
    bool immediate = false;   // resolve synchronously with success
    int calls = 0;

    void resolve(const TrackPtr& t, std::function<void(bool)> done) override
    {
        ++calls;
        if (immediate || (!t->needsResolve && !t->playbackUrl.isEmpty()))
        {
            t->playbackUrl = QStringLiteral("http://stream/") + t->title;
            t->needsResolve = false;
            done(true);
            return;
        }
        pending.append({t, std::move(done)});
    }
    /// Complete the oldest pending resolution.
    void complete(bool ok)
    {
        Pending p = pending.takeFirst();
        if (ok)
        {
            p.track->playbackUrl = QStringLiteral("http://stream/") + p.track->title;
            p.track->needsResolve = false;
        }
        p.done(ok);
    }
};

inline TrackPtr track(const QString& title, bool resolved = true, double duration = 0)
{
    Track t;
    t.title = title;
    t.source = QStringLiteral("youtube");
    t.query = QStringLiteral("https://youtube.com/watch?v=") + title;
    t.duration = duration;
    if (resolved)
        t.playbackUrl = QStringLiteral("http://stream/") + title;
    else
        t.needsResolve = true;
    return makeTrack(t);
}

class FakeSources final : public MediaSources
{
public:
    QString current = QStringLiteral("youtube");
    QStringList available{QStringLiteral("youtube"), QStringLiteral("soundcloud")};
    TrackList searchResults;
    QString nextError;
    QStringList calls;

    QString service() const override { return current; }
    QStringList services() const override { return available; }
    bool setService(const QString& name, QString* error) override
    {
        if (!available.contains(name.trimmed().toLower()))
        {
            *error = QStringLiteral("Unknown service '%1'.").arg(name);
            return false;
        }
        current = name.trimmed().toLower();
        return true;
    }
    bool isUrl(const QString& t) const override { return t.startsWith(QLatin1String("http")); }
    void fromInput(const QString& text, const QString& requester, TracksCallback cb) override
    {
        calls << QStringLiteral("fromInput:") + text;
        if (!nextError.isEmpty())
            return cb({}, nextError);
        TrackPtr t = track(text);
        t->requestedBy = requester;
        cb({t}, {});
    }
    void playDirect(const QString& url, const QString& requester, TracksCallback cb) override
    {
        calls << QStringLiteral("playDirect:") + url;
        cb({DirectUrlSource::trackFor(url, requester)}, {});
    }
    void search(const QString& query, const QString&, int limit, TracksCallback cb) override
    {
        calls << QStringLiteral("search:%1:%2").arg(query).arg(limit);
        if (!nextError.isEmpty())
            return cb({}, nextError);
        TrackList r = searchResults;
        if (limit > 0 && r.size() > limit)
            r = r.mid(0, limit);
        cb(r, {});
    }
    void updateTools(std::function<void(bool, const QString&)> cb) override { cb(true, QStringLiteral("Updated yt-dlp")); }
};

class FakeBot final : public BotActions
{
public:
    QList<QPair<int, QString>> privateMessages;
    QStringList channelMessages;
    int shutdowns = 0;
    void replyPrivate(int userId, const QString& text) override { privateMessages.append({userId, text}); }
    void sendChannel(const QString& text) override { channelMessages << text; }
    void shutdown() override { ++shutdowns; }
};

} // namespace ttc::test
