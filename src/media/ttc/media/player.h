// TT Central media bot — the playback queue and its policy.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ported from Simple TT Media (MIT, player.py). The player owns the queue,
// history and the repeat/shuffle policy and drives a PlaybackEngine. Tracks
// from metadata-only services are resolved just before they play (and the
// next one is prefetched), so a long queue never pins thousands of stream
// URLs that would expire. Resolution is asynchronous; every transport action
// starts a new "epoch" so a late resolution can never start the wrong track.

#pragma once

#include "ttc/media/audioengine.h"
#include "ttc/media/track.h"

#include <QObject>

#include <functional>

namespace ttc::media {

class Resolver
{
public:
    virtual ~Resolver() = default;
    /// Fill t->playbackUrl; call done(true) on success. May finish later.
    virtual void resolve(const TrackPtr& t, std::function<void(bool ok)> done) = 0;
};

class Player final : public QObject
{
    Q_OBJECT
public:
    Player(PlaybackEngine* engine, Resolver* resolver, QObject* parent = nullptr);

    /// Channel announcements ("Now playing: ...").
    std::function<void(const QString&)> announce;

    void setMaxQueue(int n) { m_maxQueue = n; }
    int maxQueue() const { return m_maxQueue; }

    /// The track playing, or being loaded to play.
    TrackPtr current() const { return m_current; }
    bool isLoading() const { return m_loading; }
    RepeatMode repeat() const { return m_repeat; }
    bool shuffle() const { return m_shuffle; }
    bool isPlaying() const { return m_engine->isPlaying(); }
    bool isPaused() const { return m_engine->isPaused(); }
    int volume() const { return m_engine->volume(); }
    double elapsed() const { return m_engine->elapsed(); }
    const TrackList& queue() const { return m_queue; }
    const TrackList& history() const { return m_history; }
    QString modesText() const;

    /// Add tracks, starting playback when idle. Returns the 1-based queue
    /// position of the first added track, 0 if it began playing now, or -1
    /// (with `error`) when the queue has no room.
    int enqueue(const TrackList& tracks, QString* error = nullptr);
    /// Replace playback with these tracks now.
    void playNow(const TrackList& tracks);

    TrackPtr skip();
    /// Plays the previous track; nullptr when there is none.
    TrackPtr previous();
    void stop();
    int clear();
    bool pause();
    bool resume();
    /// Returns true when now paused.
    bool togglePause();
    bool seekTo(double seconds);
    bool seekRelative(double delta);
    int setVolume(int percent);
    void setRepeat(RepeatMode m);
    void setShuffle(bool on);
    void shuffleQueue();
    /// Jump to a queued track by 1-based position, dropping those before it.
    TrackPtr select(int position, QString* error = nullptr);
    /// Remove one queued track by 1-based position.
    TrackPtr removeAt(int position);
    /// Move a queued track from one 1-based position to another.
    bool move(int from, int to);

signals:
    /// Anything a listener might show changed (track, queue, modes).
    void changed();

private:
    void onTrackFinished(const TrackPtr& t, const QString& reason);
    void playNext();
    bool startTrack(const TrackPtr& t);
    void prefetchNext();
    TrackPtr popNext();
    void retire(const TrackPtr& t);
    void say(const QString& text);

    PlaybackEngine* m_engine;
    Resolver* m_resolver;
    TrackList m_queue;
    TrackList m_history;
    TrackPtr m_current;
    bool m_loading = false;
    RepeatMode m_repeat = RepeatMode::Off;
    bool m_shuffle = false;
    int m_maxQueue = 1000;
    quint64 m_epoch = 0;
};

} // namespace ttc::media
