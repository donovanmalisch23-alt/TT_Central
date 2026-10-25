// TT Central media bot — chat commands.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ported from Simple TT Media (MIT, commands/ and lock.py). Users send
// commands to the bot in private messages (or, with a prefix, in its
// channel); TT Central's media window sends the same commands over the
// control connection. Every reply goes back the way the command came.

#pragma once

#include "ttc/media/player.h"
#include "ttc/media/sources.h"

#include <QHash>
#include <QObject>

#include <functional>

namespace ttc::media {

/// What commands may do to the bot itself.
class BotActions
{
public:
    virtual ~BotActions() = default;
    virtual void replyPrivate(int userId, const QString& text) = 0;
    virtual void sendChannel(const QString& text) = 0;
    virtual void shutdown() = 0;
};

struct CommandContext
{
    QString requester;     // the sender's nickname
    int userId = 0;        // 0 for TT Central's own control connection
    bool isAdmin = false;
    bool isPrivate = true;
    QString arg;
    std::function<void(const QString&)> reply;
};

struct Command
{
    QString name;
    QStringList aliases;
    bool admin = false;
    QString usage;
    QString summary;
    QString help;
    QString group;
    std::function<void(const CommandContext&)> run;
};

/// Lock / unlock the bot. Admins lock it with a password they choose (asked
/// for privately); while locked every other command is refused and anyone
/// may unlock it with that password. The password lives in memory only.
class LockManager
{
public:
    explicit LockManager(BotActions* bot) : m_bot(bot) {}
    bool locked() const { return m_locked; }
    bool hasPending(int userId) const { return m_pending.contains(userId); }
    void request(const CommandContext& ctx);
    void submitPassword(int userId, const QString& text);

private:
    struct Pending
    {
        bool setting = false;
        QString nickname;
        std::function<void(const QString&)> reply;
    };
    BotActions* m_bot;
    bool m_locked = false;
    QString m_password;
    QHash<int, Pending> m_pending;
};

struct DispatcherSettings
{
    int maxVolume = 200;
    int maxTrackSeconds = 0;   // 0 = unlimited
    QString version;
};

class Dispatcher final : public QObject
{
    Q_OBJECT
public:
    Dispatcher(Player* player, MediaSources* sources, BotActions* bot, LockManager* lock,
               const DispatcherSettings& settings, QObject* parent = nullptr);

    /// Parse and run one command line. Unknown commands get a clear reply.
    void handle(const QString& text, const CommandContext& base);

    const QList<Command>& commands() const { return m_commands; }
    const Command* lookup(const QString& name) const;
    LockManager* lockManager() const { return m_lock; }

private:
    void registerAll();
    void add(Command c);
    void enqueueAndReport(const CommandContext& ctx, const TrackList& tracks);
    bool rejectTooLong(const CommandContext& ctx, const TrackList& tracks);
    void selectResult(const CommandContext& ctx, int number);
    void seekRelative(const CommandContext& ctx, int sign);
    QString trackLine(int i, const Track& t) const;
    QString helpText(const CommandContext& ctx) const;

    Player* m_player;
    MediaSources* m_sources;
    BotActions* m_bot;
    LockManager* m_lock;
    DispatcherSettings m_settings;
    QList<Command> m_commands;
    QHash<QString, int> m_byKey;
    QHash<int, TrackList> m_results;   // last search results per user
};

} // namespace ttc::media
