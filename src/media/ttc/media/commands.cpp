// SPDX-License-Identifier: GPL-3.0-or-later
#include "ttc/media/commands.h"

#include <QPointer>
#include <QRegularExpression>

#include <algorithm>

namespace ttc::media {

namespace {
QString tr(const char* s)
{
    return QObject::tr(s);
}

std::function<void(const QString&)> privateReply(BotActions* bot, const CommandContext& ctx)
{
    if (ctx.userId <= 0 || ctx.isPrivate)
        return ctx.reply;
    const int uid = ctx.userId;
    return [bot, uid](const QString& t) { bot->replyPrivate(uid, t); };
}
} // namespace

// ---------------------------------------------------------------------------
// LockManager

void LockManager::request(const CommandContext& ctx)
{
    auto privately = privateReply(m_bot, ctx);
    if (!m_locked)
    {
        if (!ctx.isAdmin)
        {
            ctx.reply(tr("You are not allowed to lock the bot; only administrators can."));
            return;
        }
        m_pending.insert(ctx.userId, Pending{true, ctx.requester, privately});
        privately(tr("You are about to LOCK the bot. Reply here with the password you want to set "
                     "(or send 'cancel' to abort)."));
    }
    else
    {
        m_pending.insert(ctx.userId, Pending{false, ctx.requester, privately});
        privately(tr("The bot is LOCKED. Reply here with the unlock password (or send 'cancel' to abort)."));
    }
    if (!ctx.isPrivate)
        ctx.reply(m_locked ? tr("Check your private messages to enter the unlock password.")
                           : tr("Check your private messages to set the lock password."));
}

void LockManager::submitPassword(int userId, const QString& text)
{
    if (!m_pending.contains(userId))
        return;
    const Pending p = m_pending.take(userId);
    const QString password = text.trimmed();
    if (password.compare(QLatin1String("cancel"), Qt::CaseInsensitive) == 0)
    {
        p.reply(tr("Cancelled; nothing changed."));
        return;
    }
    if (p.setting)
    {
        if (password.isEmpty())
        {
            p.reply(tr("That password was empty, so the bot was not locked. Send 'lock' to try again."));
            return;
        }
        m_password = password;
        m_locked = true;
        p.reply(tr("Done: the bot is now locked."));
        m_bot->sendChannel(tr("The bot has been locked by %1. Commands are disabled until it is unlocked.")
                               .arg(p.nickname));
        return;
    }
    if (!m_password.isEmpty() && password == m_password)
    {
        m_locked = false;
        m_password.clear();
        p.reply(tr("Correct: the bot is now unlocked."));
        m_bot->sendChannel(tr("The bot has been unlocked by %1.").arg(p.nickname));
    }
    else
    {
        p.reply(tr("Incorrect password. The bot is still locked. Send 'lock' to try again."));
    }
}

// ---------------------------------------------------------------------------
// Dispatcher

Dispatcher::Dispatcher(Player* player, MediaSources* sources, BotActions* bot, LockManager* lock,
                       const DispatcherSettings& settings, QObject* parent)
    : QObject(parent)
    , m_player(player)
    , m_sources(sources)
    , m_bot(bot)
    , m_lock(lock)
    , m_settings(settings)
{
    registerAll();
}

void Dispatcher::add(Command c)
{
    const int index = int(m_commands.size());
    QStringList keys = c.aliases;
    keys.prepend(c.name);
    for (const QString& k : keys)
    {
        Q_ASSERT_X(!m_byKey.contains(k.toLower()), "Dispatcher", "duplicate command key");
        m_byKey.insert(k.toLower(), index);
    }
    m_commands << std::move(c);
}

const Command* Dispatcher::lookup(const QString& name) const
{
    const auto it = m_byKey.constFind(name.trimmed().toLower());
    return it == m_byKey.constEnd() ? nullptr : &m_commands.at(*it);
}

void Dispatcher::handle(const QString& text, const CommandContext& base)
{
    const QString line = text.trimmed();
    if (line.isEmpty())
        return;
    // A private message from someone we asked for a password is the
    // password, not a command.
    if (base.isPrivate && m_lock && m_lock->hasPending(base.userId))
    {
        m_lock->submitPassword(base.userId, line);
        return;
    }
    const int space = line.indexOf(QRegularExpression(QStringLiteral("\\s")));
    const QString name = space < 0 ? line : line.left(space);
    CommandContext ctx = base;
    ctx.arg = space < 0 ? QString() : line.mid(space + 1).trimmed();
    const Command* cmd = lookup(name);
    if (m_lock && m_lock->locked() && !(cmd && cmd->name == QLatin1String("lock")))
    {
        ctx.reply(tr("The bot is locked. Send 'lock' and the unlock password to use it again."));
        return;
    }
    if (!cmd)
    {
        ctx.reply(tr("Unknown command: '%1'. Send 'h' for the list of commands.").arg(name));
        return;
    }
    if (cmd->admin && !ctx.isAdmin)
    {
        ctx.reply(tr("That command is for administrators only."));
        return;
    }
    cmd->run(ctx);
}

QString Dispatcher::trackLine(int i, const Track& t) const
{
    QString s = QStringLiteral("%1. %2").arg(i).arg(t.label());
    if (!t.requestedBy.isEmpty())
        s += QStringLiteral(" (%1)").arg(t.requestedBy);
    return s;
}

bool Dispatcher::rejectTooLong(const CommandContext& ctx, const TrackList& tracks)
{
    const int limit = m_settings.maxTrackSeconds;
    if (limit <= 0)
        return false;
    for (const TrackPtr& t : tracks)
    {
        if (t->duration > limit && !t->isLive)
        {
            ctx.reply(tr("'%1' is %2 long; the limit is %3.")
                          .arg(t->title, fmtDuration(t->duration), fmtDuration(limit)));
            return true;
        }
    }
    return false;
}

void Dispatcher::enqueueAndReport(const CommandContext& ctx, const TrackList& tracks)
{
    QString err;
    const int pos = m_player->enqueue(tracks, &err);
    if (pos < 0)
    {
        ctx.reply(err);
        return;
    }
    if (tracks.size() > 1)
    {
        double total = 0;
        for (const TrackPtr& t : tracks)
            total += t->duration;
        ctx.reply(total > 0 ? tr("Added %1 tracks to the queue (about %2).").arg(tracks.size()).arg(fmtDuration(total))
                            : tr("Added %1 tracks to the queue.").arg(tracks.size()));
    }
    else if (pos == 0)
    {
        // The channel hears "Now playing"; tell the requester privately too
        // when announcements go elsewhere.
        if (ctx.isPrivate)
            ctx.reply(tr("Playing: %1").arg(tracks.first()->label()));
    }
    else
    {
        ctx.reply(tr("Added to the queue at #%1: %2").arg(pos).arg(tracks.first()->label()));
    }
}

void Dispatcher::selectResult(const CommandContext& ctx, int number)
{
    const TrackList results = m_results.value(ctx.userId);
    if (results.isEmpty())
    {
        ctx.reply(tr("You have no search results yet. Use 's <words>' first."));
        return;
    }
    if (number < 1 || number > results.size())
    {
        ctx.reply(tr("Pick a number between 1 and %1.").arg(results.size()));
        return;
    }
    // Copy, so choosing the same result twice queues two separate items.
    const TrackList chosen{makeTrack(*results.at(number - 1))};
    if (rejectTooLong(ctx, chosen))
        return;
    enqueueAndReport(ctx, chosen);
}

void Dispatcher::seekRelative(const CommandContext& ctx, int sign)
{
    const TrackPtr cur = m_player->current();
    if (!cur || !m_player->isPlaying())
    {
        ctx.reply(tr("Nothing is playing."));
        return;
    }
    double step = 15;
    if (!ctx.arg.isEmpty())
    {
        bool ok = false;
        step = ctx.arg.toDouble(&ok);
        if (!ok || step < 0)
        {
            ctx.reply(tr("Usage: ff [seconds] or rw [seconds]"));
            return;
        }
    }
    if (cur->isLive)
    {
        ctx.reply(tr("Can't seek in a live stream."));
        return;
    }
    m_player->seekRelative(sign * step);
    ctx.reply(tr("Now at %1.").arg(fmtDuration(m_player->elapsed())));
}

QString Dispatcher::helpText(const CommandContext& ctx) const
{
    QStringList lines{tr("TT Central media bot. Send these to me in a private message:")};
    for (const char* group : {"Playback", "Queue", "Information", "Access", "Admin"})
    {
        QStringList entries;
        for (const Command& c : m_commands)
        {
            if (c.group != QLatin1String(group) || (c.admin && !ctx.isAdmin))
                continue;
            QString names = c.name;
            if (!c.aliases.isEmpty())
                names += QChar('/') + c.aliases.first();
            entries << QStringLiteral("  %1: %2").arg(names, c.summary);
        }
        if (entries.isEmpty())
            continue;
        lines << QString() << QStringLiteral("[%1]").arg(tr(group));
        lines << entries;
    }
    lines << QString() << tr("Tip: 'h <command>' shows details and usage for one command.");
    return lines.join(QChar('\n'));
}

void Dispatcher::registerAll()
{
    QPointer<Dispatcher> self(this);
    Player* player = m_player;
    MediaSources* sources = m_sources;
    BotActions* bot = m_bot;
    const int maxVolume = m_settings.maxVolume;

    // --- playback ---------------------------------------------------------
    add({QStringLiteral("p"), {QStringLiteral("play")}, false, tr("p <words or link>"),
         tr("Play or queue a search or link (no argument toggles pause)"),
         tr("Searches the current service for your words and queues the top hit, or plays/queues a pasted link "
            "(track, playlist or album). With no argument it pauses or resumes. A bare number plays that item "
            "from your last search."),
         QStringLiteral("Playback"), [=](const CommandContext& ctx) {
             if (ctx.arg.isEmpty())
             {
                 if (!player->current())
                 {
                     ctx.reply(tr("Usage: p <search words, or a link>. Example: p daft punk get lucky"));
                     return;
                 }
                 ctx.reply(player->togglePause() ? tr("Paused.") : tr("Resumed."));
                 return;
             }
             bool isNumber = false;
             const int number = ctx.arg.toInt(&isNumber);
             if (isNumber)
             {
                 self->selectResult(ctx, number);
                 return;
             }
             const bool link = sources->isUrl(ctx.arg);
             ctx.reply(link ? tr("Loading the link...") : tr("Searching %1...").arg(sources->service()));
             auto cb = [self, ctx](const TrackList& tracks, const QString& err) {
                 if (!self)
                     return;
                 if (!err.isEmpty())
                     return ctx.reply(err);
                 if (self->rejectTooLong(ctx, tracks))
                     return;
                 self->enqueueAndReport(ctx, tracks);
             };
             if (link)
                 sources->fromInput(ctx.arg, ctx.requester, cb);
             else
                 sources->search(ctx.arg, ctx.requester, 1, [cb](const TrackList& tracks, const QString& err) {
                     cb(tracks.isEmpty() ? tracks : TrackList{tracks.first()}, err);
                 });
         }});

    add({QStringLiteral("url"), {}, false, tr("url <link>"), tr("Play a direct media or radio stream link"),
         tr("Plays a direct audio file URL or an internet radio stream."), QStringLiteral("Playback"),
         [=](const CommandContext& ctx) {
             if (ctx.arg.isEmpty())
                 return ctx.reply(tr("Usage: url <direct media or stream link>"));
             if (!sources->isUrl(ctx.arg))
                 return ctx.reply(tr("That isn't a URL. Use 'p' to search by words."));
             sources->playDirect(ctx.arg, ctx.requester, [self, ctx](const TrackList& tracks, const QString& err) {
                 if (!self)
                     return;
                 if (!err.isEmpty())
                     return ctx.reply(err);
                 self->enqueueAndReport(ctx, tracks);
             });
         }});

    add({QStringLiteral("s"), {QStringLiteral("search"), QStringLiteral("find")}, false, tr("s <words>"),
         tr("Search and show a numbered list to choose from"),
         tr("Searches the current service and lists results. Play one with 'p <number>' or 'sel <number>'."),
         QStringLiteral("Playback"), [=](const CommandContext& ctx) {
             if (ctx.arg.isEmpty())
                 return ctx.reply(tr("Usage: s <words>. Searches %1.").arg(sources->service()));
             const QString service = sources->service();
             sources->search(ctx.arg, ctx.requester, 0, [self, ctx, service](const TrackList& tracks,
                                                                             const QString& err) {
                 if (!self)
                     return;
                 if (!err.isEmpty())
                     return ctx.reply(err);
                 self->m_results.insert(ctx.userId, tracks);
                 QStringList lines{tr("Top results on %1 (send 'p N' to play one):").arg(service)};
                 for (int i = 0; i < tracks.size() && i < 20; ++i)
                     lines << self->trackLine(i + 1, *tracks.at(i));
                 ctx.reply(lines.join(QChar('\n')));
             });
         }});

    add({QStringLiteral("sel"), {QStringLiteral("select")}, false, tr("sel <number>"),
         tr("Play a numbered result from your last search"), {}, QStringLiteral("Playback"),
         [=](const CommandContext& ctx) {
             bool ok = false;
             const int n = ctx.arg.toInt(&ok);
             if (!ok)
                 return ctx.reply(tr("Usage: sel <number from the search results>"));
             self->selectResult(ctx, n);
         }});

    add({QStringLiteral("stop"), {}, false, {}, tr("Stop playback and clear the queue"), {},
         QStringLiteral("Playback"), [=](const CommandContext& ctx) {
             player->stop();
             ctx.reply(tr("Stopped playback and cleared the queue."));
         }});

    add({QStringLiteral("pause"), {}, false, {}, tr("Pause playback"), {}, QStringLiteral("Playback"),
         [=](const CommandContext& ctx) {
             ctx.reply(player->pause() ? tr("Paused.") : tr("Nothing is playing to pause."));
         }});

    add({QStringLiteral("resume"), {}, false, {}, tr("Resume playback"), {}, QStringLiteral("Playback"),
         [=](const CommandContext& ctx) { ctx.reply(player->resume() ? tr("Resumed.") : tr("Nothing is paused.")); }});

    add({QStringLiteral("pp"), {QStringLiteral("playpause")}, false, {}, tr("Toggle pause and resume"), {},
         QStringLiteral("Playback"), [=](const CommandContext& ctx) {
             if (!player->current())
                 return ctx.reply(tr("Nothing is playing."));
             ctx.reply(player->togglePause() ? tr("Paused.") : tr("Resumed."));
         }});

    add({QStringLiteral("next"), {QStringLiteral("skip"), QStringLiteral("n")}, false, {},
         tr("Skip to the next track"), {}, QStringLiteral("Playback"), [=](const CommandContext& ctx) {
             if (!player->current())
                 return ctx.reply(tr("Nothing is playing."));
             if (!player->skip())
                 ctx.reply(tr("Skipped. The queue is now empty."));
         }});

    add({QStringLiteral("prev"), {QStringLiteral("previous"), QStringLiteral("b")}, false, {},
         tr("Play the previous track"), {}, QStringLiteral("Playback"), [=](const CommandContext& ctx) {
             if (player->history().isEmpty())
                 return ctx.reply(tr("There is no previous track."));
             player->previous();
         }});

    add({QStringLiteral("v"), {QStringLiteral("vol"), QStringLiteral("volume")}, false, tr("v [0-%1]").arg(maxVolume),
         tr("Show or set the volume (percent)"), {}, QStringLiteral("Playback"), [=](const CommandContext& ctx) {
             if (ctx.arg.isEmpty())
                 return ctx.reply(tr("Volume is %1%.").arg(player->volume()));
             QString raw = ctx.arg.trimmed();
             if (raw.endsWith(QChar('%')))
                 raw.chop(1);
             bool ok = false;
             const int want = raw.toInt(&ok);
             if (!ok)
                 return ctx.reply(tr("Usage: v <0-%1>").arg(maxVolume));
             const int now = player->setVolume(want);
             ctx.reply(want > maxVolume ? tr("Volume set to %1% (capped at %2%).").arg(now).arg(maxVolume)
                                        : tr("Volume set to %1%.").arg(now));
         }});

    add({QStringLiteral("seek"), {}, false, tr("seek <m:ss or seconds>"), tr("Jump to a position in the current track"),
         {}, QStringLiteral("Playback"), [=](const CommandContext& ctx) {
             const TrackPtr cur = player->current();
             if (!cur || !player->isPlaying())
                 return ctx.reply(tr("Nothing is playing."));
             if (cur->isLive)
                 return ctx.reply(tr("Can't seek in a live stream."));
             double secs = 0;
             if (ctx.arg.isEmpty())
                 return ctx.reply(tr("Give a time, e.g. 'seek 1:30' or 'seek 90'."));
             if (!parseTime(ctx.arg, &secs))
                 return ctx.reply(tr("Time must be seconds, m:ss, or h:mm:ss."));
             if (player->seekTo(secs))
                 ctx.reply(tr("Seeked to %1.").arg(fmtDuration(player->elapsed())));
             else
                 ctx.reply(tr("Couldn't seek."));
         }});

    add({QStringLiteral("ff"), {QStringLiteral("fwd")}, false, tr("ff [seconds]"), tr("Skip forward (default 15 seconds)"),
         {}, QStringLiteral("Playback"), [=](const CommandContext& ctx) { self->seekRelative(ctx, +1); }});
    add({QStringLiteral("rw"), {QStringLiteral("rewind")}, false, tr("rw [seconds]"),
         tr("Skip backward (default 15 seconds)"), {}, QStringLiteral("Playback"),
         [=](const CommandContext& ctx) { self->seekRelative(ctx, -1); }});

    // --- queue --------------------------------------------------------------
    add({QStringLiteral("repeat"), {QStringLiteral("loop")}, false, tr("repeat <off|track|queue>"), tr("Set the repeat mode"),
         {}, QStringLiteral("Queue"), [=](const CommandContext& ctx) {
             if (ctx.arg.isEmpty())
                 return ctx.reply(tr("Repeat is '%1'. Use: repeat off, track, or queue.")
                                      .arg(repeatModeName(player->repeat())));
             RepeatMode m;
             if (!parseRepeatMode(ctx.arg, &m))
                 return ctx.reply(tr("Repeat must be: off, track, or queue."));
             player->setRepeat(m);
             ctx.reply(tr("Repeat set to '%1'.").arg(repeatModeName(m)));
         }});

    add({QStringLiteral("shuffle"), {QStringLiteral("sh")}, false, tr("shuffle [on|off|queue]"),
         tr("Toggle shuffle, or shuffle the queue now"), {}, QStringLiteral("Queue"), [=](const CommandContext& ctx) {
             const QString a = ctx.arg.trimmed().toLower();
             if (a == QLatin1String("on") || a == QLatin1String("off"))
             {
                 player->setShuffle(a == QLatin1String("on"));
                 ctx.reply(a == QLatin1String("on") ? tr("Shuffle on.") : tr("Shuffle off."));
             }
             else if (a.isEmpty() || a == QLatin1String("toggle"))
             {
                 player->setShuffle(!player->shuffle());
                 ctx.reply(player->shuffle() ? tr("Shuffle on.") : tr("Shuffle off."));
             }
             else if (a == QLatin1String("queue") || a == QLatin1String("now"))
             {
                 player->shuffleQueue();
                 ctx.reply(tr("Shuffled the current queue."));
             }
             else
             {
                 ctx.reply(tr("Usage: shuffle [on|off|queue]"));
             }
         }});

    add({QStringLiteral("q"), {QStringLiteral("queue")}, false, {}, tr("Show the queue and what's playing"), {},
         QStringLiteral("Queue"), [=](const CommandContext& ctx) {
             QStringList lines;
             if (const TrackPtr cur = player->current())
             {
                 const QString tag = player->isLoading() ? tr("Loading") : player->isPaused() ? tr("Now playing (paused)")
                                                                                              : tr("Now playing");
                 lines << QStringLiteral("%1: %2").arg(tag, cur->label());
             }
             const TrackList& items = player->queue();
             if (!items.isEmpty())
             {
                 double total = 0;
                 for (const TrackPtr& t : items)
                     total += t->duration;
                 lines << (total > 0 ? tr("Up next (%1, about %2):").arg(items.size()).arg(fmtDuration(total))
                                     : tr("Up next (%1):").arg(items.size()));
                 for (int i = 0; i < items.size() && i < 25; ++i)
                     lines << self->trackLine(i + 1, *items.at(i));
                 if (items.size() > 25)
                     lines << tr("...and %1 more.").arg(items.size() - 25);
             }
             ctx.reply(lines.isEmpty() ? tr("The queue is empty. Add something with 'p <words or link>'.")
                                       : lines.join(QChar('\n')));
         }});

    add({QStringLiteral("clear"), {QStringLiteral("cl")}, false, {}, tr("Clear the queue (keep playing)"), {},
         QStringLiteral("Queue"), [=](const CommandContext& ctx) {
             const int n = player->clear();
             ctx.reply(n ? tr("Cleared %1 track(s) from the queue.").arg(n) : tr("The queue was already empty."));
         }});

    add({QStringLiteral("go"), {QStringLiteral("jump")}, false, tr("go <number>"),
         tr("Play a queued track now (skips the ones before it)"), {}, QStringLiteral("Queue"),
         [=](const CommandContext& ctx) {
             bool ok = false;
             const int n = ctx.arg.toInt(&ok);
             if (!ok)
                 return ctx.reply(tr("Usage: go <queue number>"));
             QString err;
             player->select(n, &err);
             if (!err.isEmpty())
                 ctx.reply(err);
         }});

    add({QStringLiteral("rm"), {QStringLiteral("remove")}, false, tr("rm <number>"), tr("Remove a track from the queue"),
         {}, QStringLiteral("Queue"), [=](const CommandContext& ctx) {
             bool ok = false;
             const int n = ctx.arg.toInt(&ok);
             if (!ok)
                 return ctx.reply(tr("Usage: rm <queue number>"));
             const TrackPtr t = player->removeAt(n);
             ctx.reply(t ? tr("Removed: %1").arg(t->label())
                         : tr("There is no track %1 in the queue.").arg(n));
         }});

    add({QStringLiteral("mv"), {QStringLiteral("move")}, false, tr("mv <from> <to>"), tr("Move a track within the queue"),
         {}, QStringLiteral("Queue"), [=](const CommandContext& ctx) {
             const QStringList parts = ctx.arg.split(QChar(' '), Qt::SkipEmptyParts);
             bool a = false, b = false;
             const int from = parts.value(0).toInt(&a), to = parts.value(1).toInt(&b);
             if (parts.size() != 2 || !a || !b)
                 return ctx.reply(tr("Usage: mv <from> <to>, for example 'mv 5 1'."));
             ctx.reply(player->move(from, to) ? tr("Moved track %1 to position %2.").arg(from).arg(to)
                                              : tr("Both positions must be between 1 and %1.").arg(player->queue().size()));
         }});

    // --- information ------------------------------------------------------
    add({QStringLiteral("np"), {QStringLiteral("now"), QStringLiteral("nowplaying")}, false, {},
         tr("Show the current track and position"), {}, QStringLiteral("Information"), [=](const CommandContext& ctx) {
             const TrackPtr cur = player->current();
             if (!cur)
                 return ctx.reply(tr("Nothing is playing right now."));
             if (player->isLoading())
                 return ctx.reply(tr("Loading: %1").arg(cur->label()));
             const QString dur = cur->duration > 0 ? fmtDuration(cur->duration) : cur->isLive ? tr("live") : tr("?:??");
             QString body = tr("Now playing: %1").arg(cur->label());
             if (!cur->requestedBy.isEmpty())
                 body += tr(" | requested by %1").arg(cur->requestedBy);
             body += QChar('\n') + tr("Position: %1 / %2 | %3").arg(fmtDuration(player->elapsed()), dur, player->modesText());
             if (!cur->url.isEmpty())
                 body += QChar('\n') + tr("Link: %1").arg(cur->url);
             ctx.reply(body);
         }});

    add({QStringLiteral("sv"), {QStringLiteral("service")}, false, tr("sv [name]"),
         tr("Show or set which service free-text searches"), {}, QStringLiteral("Information"),
         [=](const CommandContext& ctx) {
             if (ctx.arg.isEmpty())
                 return ctx.reply(tr("Search service: %1. Available: %2. Change with: sv <name>")
                                      .arg(sources->service(), sources->services().join(QStringLiteral(", "))));
             QString err;
             if (!sources->setService(ctx.arg, &err))
                 return ctx.reply(err);
             ctx.reply(tr("Search service set to %1.").arg(sources->service()));
         }});

    add({QStringLiteral("services"), {}, false, {}, tr("List searchable services"), {}, QStringLiteral("Information"),
         [=](const CommandContext& ctx) {
             ctx.reply(tr("Services you can search: %1\nCurrently searching: %2\nYou can also paste YouTube, "
                          "SoundCloud, Spotify, Apple Music, or direct stream links to 'p' or 'url'.")
                           .arg(sources->services().join(QStringLiteral(", ")), sources->service()));
         }});

    add({QStringLiteral("h"), {QStringLiteral("help"), QStringLiteral("?"), QStringLiteral("commands")}, false,
         tr("h [command]"), tr("Show this help (or details for one command)"), {}, QStringLiteral("Information"),
         [=](const CommandContext& ctx) {
             if (ctx.arg.isEmpty())
                 return ctx.reply(self->helpText(ctx));
             const Command* c = self->lookup(ctx.arg);
             if (!c)
                 return ctx.reply(tr("No such command: '%1'.").arg(ctx.arg));
             QString text = (QStringList{c->name} + c->aliases).join(QChar('/'));
             if (!c->usage.isEmpty())
                 text += QChar('\n') + tr("Usage: %1").arg(c->usage);
             text += QChar('\n') + (c->help.isEmpty() ? c->summary : c->help);
             if (c->admin)
                 text += QChar('\n') + tr("(administrators only)");
             ctx.reply(text);
         }});

    const QString version = m_settings.version;
    add({QStringLiteral("about"), {QStringLiteral("info")}, false, {}, tr("About this bot"), {},
         QStringLiteral("Information"), [=](const CommandContext& ctx) {
             ctx.reply(tr("TT Central media bot %1, a native port of Simple TT Media.\nSearching: %2. Links: "
                          "YouTube, SoundCloud, Spotify, Apple Music and direct streams.\nSend 'h' for the list of "
                          "commands, or 'h <command>' for details.")
                           .arg(version, sources->service()));
         }});

    // --- access / admin -----------------------------------------------------
    add({QStringLiteral("lock"), {}, false, QStringLiteral("lock"),
         tr("Lock the bot (admins), or unlock it with the password"),
         tr("Admins lock the bot: you'll be asked privately for a password to set, then commands are disabled for "
            "everyone. While locked, anyone can send 'lock' and will be asked privately for that password to unlock "
            "it. Non-admins cannot lock an unlocked bot."),
         QStringLiteral("Access"), [=](const CommandContext& ctx) {
             if (!self->m_lock)
                 return ctx.reply(tr("Locking is not available."));
             self->m_lock->request(ctx);
         }});

    add({QStringLiteral("say"), {}, true, tr("say <message>"), tr("Make the bot post a message in its channel"), {},
         QStringLiteral("Admin"), [=](const CommandContext& ctx) {
             if (ctx.arg.isEmpty())
                 return ctx.reply(tr("Usage: say <message to post in the channel>"));
             bot->sendChannel(ctx.arg);
             ctx.reply(tr("Posted to the channel."));
         }});

    add({QStringLiteral("update"), {}, true, {}, tr("Update the media downloader (yt-dlp)"),
         tr("Runs 'yt-dlp -U'. Use it when YouTube links stop working."), QStringLiteral("Admin"),
         [=](const CommandContext& ctx) {
             ctx.reply(tr("Updating yt-dlp..."));
             sources->updateTools([ctx](bool ok, const QString& out) {
                 const QStringList lines = out.split(QChar('\n'), Qt::SkipEmptyParts);
                 const QString last = lines.isEmpty() ? out : lines.last().trimmed();
                 ctx.reply((ok ? tr("Update finished: %1") : tr("Update failed: %1")).arg(last.left(300)));
             });
         }});

    add({QStringLiteral("quit"), {QStringLiteral("shutdown")}, true, {}, tr("Shut the bot down"), {},
         QStringLiteral("Admin"), [=](const CommandContext& ctx) {
             ctx.reply(tr("Shutting down. Goodbye!"));
             player->stop();
             bot->shutdown();
         }});
}

} // namespace ttc::media
