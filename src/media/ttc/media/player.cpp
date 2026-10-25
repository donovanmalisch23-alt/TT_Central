// SPDX-License-Identifier: GPL-3.0-or-later
#include "ttc/media/player.h"

#include <QPointer>
#include <QRandomGenerator>

#include <algorithm>

namespace ttc::media {

namespace {
constexpr int kHistoryLimit = 200;
} // namespace

Player::Player(PlaybackEngine* engine, Resolver* resolver, QObject* parent)
    : QObject(parent)
    , m_engine(engine)
    , m_resolver(resolver)
{
    m_engine->onFinished = [this](const TrackPtr& t, const QString& reason) { onTrackFinished(t, reason); };
}

QString Player::modesText() const
{
    QString s = tr("volume %1% | repeat %2 | shuffle %3")
                    .arg(volume())
                    .arg(repeatModeName(m_repeat), m_shuffle ? tr("on") : tr("off"));
    if (isPaused())
        s += tr(" | paused");
    return s;
}

void Player::say(const QString& text)
{
    if (announce)
        announce(text);
}

void Player::retire(const TrackPtr& t)
{
    if (!t)
        return;
    m_history.append(t);
    if (m_history.size() > kHistoryLimit)
        m_history.erase(m_history.begin(), m_history.begin() + (m_history.size() - kHistoryLimit));
}

int Player::enqueue(const TrackList& tracks, QString* error)
{
    if (tracks.isEmpty())
        return m_queue.size() + 1;
    if (m_queue.size() + tracks.size() > m_maxQueue)
    {
        if (error)
            *error = tr("The queue is full (room for %1 more).").arg(std::max<qsizetype>(0, m_maxQueue - m_queue.size()));
        return -1;
    }
    const bool startNow = !m_current && m_queue.isEmpty();
    m_queue.append(tracks);
    if (startNow)
    {
        playNext();
        return 0;
    }
    emit changed();
    return int(m_queue.size() - tracks.size() + 1);
}

void Player::playNow(const TrackList& tracks)
{
    ++m_epoch;
    for (int i = int(tracks.size()) - 1; i >= 0; --i)
        m_queue.prepend(tracks.at(i));
    m_engine->stop();
    retire(m_current);
    m_current.reset();
    playNext();
}

TrackPtr Player::skip()
{
    ++m_epoch;
    m_engine->stop();
    retire(m_current);
    m_current.reset();
    playNext();
    return m_current;
}

TrackPtr Player::previous()
{
    if (m_history.isEmpty())
        return nullptr;
    ++m_epoch;
    if (m_current)
        m_queue.prepend(replayable(m_current));
    m_queue.prepend(replayable(m_history.takeLast()));
    m_engine->stop();
    m_current.reset();
    playNext();
    return m_current;
}

void Player::stop()
{
    ++m_epoch;
    m_engine->stop();
    retire(m_current);
    m_current.reset();
    m_loading = false;
    m_queue.clear();
    emit changed();
}

int Player::clear()
{
    const int n = int(m_queue.size());
    m_queue.clear();
    emit changed();
    return n;
}

bool Player::pause()
{
    const bool ok = m_engine->pause();
    if (ok)
        emit changed();
    return ok;
}

bool Player::resume()
{
    const bool ok = m_engine->resume();
    if (ok)
        emit changed();
    return ok;
}

bool Player::togglePause()
{
    if (m_engine->isPaused())
    {
        m_engine->resume();
        emit changed();
        return false;
    }
    const bool paused = m_engine->pause();
    emit changed();
    return paused;
}

bool Player::seekTo(double seconds)
{
    const bool ok = m_engine->seek(seconds);
    if (ok)
        emit changed();
    return ok;
}

bool Player::seekRelative(double delta)
{
    return seekTo(std::max(0.0, m_engine->elapsed() + delta));
}

int Player::setVolume(int percent)
{
    const int v = m_engine->setVolume(percent);
    emit changed();
    return v;
}

void Player::setRepeat(RepeatMode m)
{
    m_repeat = m;
    emit changed();
}

void Player::setShuffle(bool on)
{
    m_shuffle = on;
    emit changed();
}

void Player::shuffleQueue()
{
    auto* rng = QRandomGenerator::global();
    for (qsizetype i = m_queue.size() - 1; i > 0; --i)
        m_queue.swapItemsAt(i, rng->bounded(int(i + 1)));
    emit changed();
}

TrackPtr Player::select(int position, QString* error)
{
    if (m_queue.isEmpty())
    {
        if (error)
            *error = tr("The queue is empty.");
        return nullptr;
    }
    if (position < 1 || position > m_queue.size())
    {
        if (error)
            *error = tr("Pick a number between 1 and %1.").arg(m_queue.size());
        return nullptr;
    }
    ++m_epoch;
    m_queue.erase(m_queue.begin(), m_queue.begin() + (position - 1));
    m_engine->stop();
    retire(m_current);
    m_current.reset();
    playNext();
    return m_current;
}

TrackPtr Player::removeAt(int position)
{
    if (position < 1 || position > m_queue.size())
        return nullptr;
    TrackPtr t = m_queue.takeAt(position - 1);
    emit changed();
    return t;
}

bool Player::move(int from, int to)
{
    const int n = int(m_queue.size());
    if (from < 1 || from > n || to < 1 || to > n)
        return false;
    m_queue.move(from - 1, to - 1);
    emit changed();
    return true;
}

void Player::onTrackFinished(const TrackPtr& t, const QString& reason)
{
    if (m_current != t)
        return; // stop/skip/another play already handled this track
    m_current.reset();
    if (reason == QLatin1String("error"))
    {
        // Most often an expired or blocked stream URL: re-resolve once.
        if (!t->query.isEmpty() && t->retries < 1)
        {
            TrackPtr retry = replayable(t, true);
            retry->retries = t->retries + 1;
            m_queue.prepend(retry);
        }
        else
        {
            say(tr("Couldn't play: %1. Skipping.").arg(t->label()));
            retire(t);
        }
        playNext();
        return;
    }
    if (m_repeat == RepeatMode::Track)
        m_queue.prepend(replayable(t));
    else if (m_repeat == RepeatMode::Queue)
        m_queue.append(replayable(t));
    retire(t);
    playNext();
}

TrackPtr Player::popNext()
{
    if (m_queue.isEmpty())
        return nullptr;
    if (m_shuffle && m_queue.size() > 1)
        return m_queue.takeAt(QRandomGenerator::global()->bounded(int(m_queue.size())));
    return m_queue.takeFirst();
}

void Player::playNext()
{
    m_loading = false;
    while (!m_queue.isEmpty())
    {
        TrackPtr t = popNext();
        if (t->needsResolve || t->playbackUrl.isEmpty())
        {
            m_current = t;
            m_loading = true;
            const quint64 epoch = ++m_epoch;
            emit changed();
            QPointer<Player> self(this);
            m_resolver->resolve(t, [self, epoch, t](bool ok) {
                if (!self || self->m_epoch != epoch || self->m_current != t)
                    return; // superseded by a later action
                self->m_loading = false;
                if (ok && !t->playbackUrl.isEmpty() && self->startTrack(t))
                    return;
                if (!ok || t->playbackUrl.isEmpty())
                    self->say(tr("Skipping (couldn't load): %1").arg(t->label()));
                self->m_current.reset();
                self->retire(t);
                self->playNext();
            });
            return;
        }
        if (startTrack(t))
            return;
    }
    m_current.reset();
    emit changed();
}

bool Player::startTrack(const TrackPtr& t)
{
    QString err;
    if (!m_engine->play(t, &err))
    {
        say(tr("Skipping (playback error): %1").arg(t->label()));
        m_current.reset();
        return false;
    }
    m_current = t;
    m_loading = false;
    say(tr("Now playing: %1").arg(t->label()));
    prefetchNext();
    emit changed();
    return true;
}

void Player::prefetchNext()
{
    if (m_shuffle || m_queue.isEmpty())
        return;
    TrackPtr next = m_queue.first();
    if (next->needsResolve || next->playbackUrl.isEmpty())
        m_resolver->resolve(next, [](bool) {}); // retried (and reported) when due
}

} // namespace ttc::media
