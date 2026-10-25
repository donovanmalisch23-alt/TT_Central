// SPDX-License-Identifier: GPL-3.0-or-later
#include "ttc/media/mediabot.h"

#include "ttc/monitorconfig.h"

#include <QJsonArray>

#include <algorithm>

#ifndef TTC_VERSION
#  define TTC_VERSION "dev"
#endif

namespace ttc::media {

namespace {
const char* const kClientName = "TT Central Media Bot";
} // namespace

QString MediaBot::version()
{
    return QStringLiteral(TTC_VERSION);
}

MediaBot::MediaBot(const MediaBotConfig& config, std::unique_ptr<TeamTalkClient> client, QObject* parent)
    : QObject(parent)
    , m_config(config)
    , m_client(std::move(client))
    , m_sink(m_client.get())
    , m_lock(this)
{
    m_engine = new AudioEngine(&m_sink, m_config.audio, this);
    if (!m_config.ffmpegPath.isEmpty())
        m_engine->setFfmpegPath(m_config.ffmpegPath);
    m_sources = new SourceRegistry(m_config.sources, this);
    m_player = new Player(m_engine, m_sources, this);
    m_player->setMaxQueue(m_config.maxQueue);
    m_player->announce = [this](const QString& text) {
        log(text);
        if (m_config.announceNowPlaying)
            sendChannel(text);
    };
    connect(m_player, &Player::changed, this, &MediaBot::statusChanged);

    DispatcherSettings ds;
    ds.maxVolume = m_config.audio.maxVolume;
    ds.maxTrackSeconds = m_config.maxTrackSeconds;
    ds.version = version();
    m_dispatcher = new Dispatcher(m_player, m_sources, this, &m_lock, ds, this);

    m_pollTimer.setInterval(20);
    connect(&m_pollTimer, &QTimer::timeout, this, &MediaBot::pollNow);
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &MediaBot::connectNow);
    const int first = std::max(1, m_config.reconnectSeconds) * 1000;
    setReconnectDelays(first, std::max(first, 60000));
}

MediaBot::~MediaBot()
{
    m_pollTimer.stop();
    m_reconnectTimer.stop();
    m_engine->stop();
    if (m_client)
        m_client->disconnect();
}

void MediaBot::setReconnectDelays(int firstMsec, int maxMsec)
{
    m_firstDelayMs = std::max(1, firstMsec);
    m_maxDelayMs = std::max(m_firstDelayMs, maxMsec);
}

QString MediaBot::stateText() const
{
    switch (m_state)
    {
    case State::Stopped: return tr("Stopped");
    case State::Connecting: return tr("Connecting");
    case State::LoggingIn: return tr("Logging in");
    case State::Joining: return tr("Joining channel");
    case State::Online: return tr("Online");
    case State::WaitingToReconnect:
        return m_lastError.isEmpty() ? tr("Waiting to reconnect") : tr("Waiting to reconnect: %1").arg(m_lastError);
    case State::Failed: return m_lastError.isEmpty() ? tr("Failed") : tr("Failed: %1").arg(m_lastError);
    }
    return {};
}

void MediaBot::log(const QString& text)
{
    emit logLine(text);
}

void MediaBot::start()
{
    if (m_state != State::Stopped && m_state != State::Failed)
        return;
    const QString err = m_config.validationError();
    if (!err.isEmpty())
    {
        m_lastError = err;
        setState(State::Failed);
        log(err);
        return;
    }
    m_lastError.clear();
    m_attempts = 0;
    m_pollTimer.start();
    connectNow();
}

void MediaBot::stop()
{
    m_reconnectTimer.stop();
    m_pollTimer.stop();
    m_player->stop();
    if (m_client)
        m_client->disconnect();
    m_text.clear();
    setState(State::Stopped);
}

void MediaBot::shutdown()
{
    log(tr("Shutting down."));
    // Let the goodbye reply leave before the connection closes.
    QTimer::singleShot(300, this, [this] {
        stop();
        emit shutdownRequested();
    });
}

void MediaBot::setState(State s)
{
    if (m_state == s)
        return;
    m_state = s;
    emit stateChanged(s);
    emit statusChanged();
}

bool MediaBot::isPermanentLoginError(int errorNo)
{
    switch (errorNo)
    {
    case CMDERR_INVALID_ACCOUNT:
    case CMDERR_INVALID_USERNAME:
    case CMDERR_SERVER_BANNED:
    case CMDERR_INCOMPATIBLE_PROTOCOLS:
        return true;
    default:
        return false;
    }
}

void MediaBot::pollNow()
{
    TTMessage msg;
    for (int i = 0; i < 500 && m_client && m_client->nextMessage(msg, 0); ++i)
        processMessage(msg);
}

void MediaBot::connectNow()
{
    m_loginCmd = m_joinCmd = 0;
    m_loginFailed = m_joinFailed = m_fellBackToRoot = false;
    m_channelId = 0;
    m_text.clear();
    setState(State::Connecting);
    if (!m_client->connect(m_config.host.trimmed(), m_config.tcpPort, m_config.udpPort, m_config.encrypted))
        scheduleReconnect(tr("could not start connecting"));
}

void MediaBot::scheduleReconnect(const QString& reason)
{
    // Keep the queue; pause what is playing until we are back.
    if (m_engine->isPlaying() && !m_engine->isPaused())
    {
        m_engine->pause();
        m_pausedByOutage = true;
    }
    if (m_client)
        m_client->disconnect();
    m_text.clear();
    m_lastError = reason.isEmpty() ? tr("connection lost") : reason;
    qint64 delay = m_firstDelayMs;
    for (int i = 0; i < m_attempts && delay < m_maxDelayMs; ++i)
        delay *= 2;
    delay = std::min<qint64>(delay, m_maxDelayMs);
    ++m_attempts;
    if (m_attempts == 1)
        log(tr("Disconnected from %1 (%2); reconnecting.").arg(m_config.displayName(), m_lastError));
    setState(State::WaitingToReconnect);
    m_reconnectTimer.start(int(delay));
}

void MediaBot::onLoginComplete()
{
    m_client->changeStatus(0, m_config.statusMessage);
    setState(State::Joining);
    const QString path = normalizeChannelPath(m_config.channelPath);
    int chan = (path == QLatin1String("/")) ? m_client->rootChannelId() : m_client->channelIdFromPath(path);
    QString password = m_config.channelPassword;
    if (chan <= 0)
    {
        log(tr("Channel %1 was not found; joining the root channel instead.").arg(path));
        m_fellBackToRoot = true;
        chan = m_client->rootChannelId();
        password.clear();
    }
    m_joinCmd = m_client->joinChannel(chan, password);
    if (m_joinCmd <= 0)
    {
        m_joinFailed = true;
        onJoinComplete();
    }
}

void MediaBot::onJoinComplete()
{
    const int root = m_client->rootChannelId();
    if (m_joinFailed && !m_fellBackToRoot && m_client->myChannelId() != root)
    {
        log(tr("Could not join %1; joining the root channel instead.").arg(m_config.channelPath));
        m_fellBackToRoot = true;
        m_joinFailed = false;
        m_joinCmd = m_client->joinChannel(root, QString());
        if (m_joinCmd > 0)
            return;
    }
    m_attempts = 0;
    m_lastError.clear();
    m_channelId = m_client->myChannelId();
    setState(State::Online);
    log(tr("Online on %1. Private-message the bot 'h' for commands.").arg(m_config.displayName()));
    if (m_everOnline && m_engine->isPlaying())
    {
        // Back after an outage: restart the voice stream where it was.
        const bool keepPaused = m_engine->isPaused() && !m_pausedByOutage;
        m_engine->restartForChannelChange();
        if (keepPaused)
            m_engine->pause();
    }
    m_pausedByOutage = false;
    m_everOnline = true;
}

void MediaBot::onChannelChanged(int channelId)
{
    // Moved to another channel: re-start the voice stream for its codec.
    emit statusChanged();
    if (!m_engine->isPlaying())
        return;
    if (!m_client->channelHasAudioCodec(channelId))
    {
        m_player->stop();
        sendChannel(tr("I was moved to a channel with no voice codec, so I can't play audio here."));
        return;
    }
    const bool paused = m_engine->isPaused();
    m_engine->restartForChannelChange();
    if (paused)
        m_engine->pause();
}

bool MediaBot::isAdmin(int userId, const QString& username)
{
    for (const QString& a : m_config.admins)
        if (!a.trimmed().isEmpty() && a.trimmed().compare(username, Qt::CaseInsensitive) == 0)
            return true;
    User u;
    return m_client->user(userId, u) && (u.uUserType & USERTYPE_ADMIN);
}

void MediaBot::replyPrivate(int userId, const QString& text)
{
    if (userId > 0 && m_state == State::Online)
        m_client->sendTextMessage(MSGTYPE_USER, userId, 0, text);
}

void MediaBot::sendChannel(const QString& text)
{
    const int cid = m_client->myChannelId();
    if (cid > 0 && m_state == State::Online)
        m_client->sendTextMessage(MSGTYPE_CHANNEL, 0, cid, text);
}

void MediaBot::onText(const AssembledMessage& t)
{
    const int me = m_client->myUserId();
    if (t.fromUserId == me)
        return;
    QString nick = t.fromUsername;
    User u;
    if (m_client->user(t.fromUserId, u) && !fromTT(u.szNickname).isEmpty())
        nick = fromTT(u.szNickname);
    if (nick.isEmpty())
        nick = tr("user %1").arg(t.fromUserId);

    CommandContext ctx;
    ctx.requester = nick;
    ctx.userId = t.fromUserId;
    QString text = t.text.trimmed();
    if (t.type == MSGTYPE_USER)
    {
        if (t.toUserId != 0 && t.toUserId != me)
            return; // an admin's copy of someone else's message
        ctx.isPrivate = true;
        const int uid = t.fromUserId;
        ctx.reply = [this, uid](const QString& r) { replyPrivate(uid, r); };
    }
    else if (t.type == MSGTYPE_CHANNEL)
    {
        const QString prefix = m_config.channelPrefix;
        if (prefix.isEmpty() || t.channelId != m_client->myChannelId() || !text.startsWith(prefix))
            return;
        text = text.mid(prefix.size()).trimmed();
        ctx.isPrivate = false;
        ctx.reply = [this](const QString& r) { sendChannel(r); };
    }
    else
    {
        return;
    }
    if (text.isEmpty())
        return;
    ctx.isAdmin = isAdmin(t.fromUserId, t.fromUsername);
    m_dispatcher->handle(text, ctx);
}

void MediaBot::runControlCommand(const QString& text, std::function<void(const QString&)> reply)
{
    CommandContext ctx;
    ctx.requester = QStringLiteral("TT Central");
    ctx.userId = 0;
    ctx.isAdmin = true;
    ctx.isPrivate = true;
    ctx.reply = std::move(reply);
    m_dispatcher->handle(text, ctx);
}

static QJsonObject trackJson(const Track& t)
{
    return QJsonObject{{QStringLiteral("title"), t.title},
                       {QStringLiteral("artist"), t.artist},
                       {QStringLiteral("label"), t.label()},
                       {QStringLiteral("duration"), t.duration},
                       {QStringLiteral("url"), t.url},
                       {QStringLiteral("source"), t.source},
                       {QStringLiteral("requestedBy"), t.requestedBy},
                       {QStringLiteral("live"), t.isLive}};
}

QJsonObject MediaBot::status() const
{
    QJsonObject o;
    o.insert(QStringLiteral("state"), int(m_state));
    o.insert(QStringLiteral("online"), m_state == State::Online);
    o.insert(QStringLiteral("stateText"), stateText());
    o.insert(QStringLiteral("server"), m_config.displayName());
    QString channel;
    if (m_state == State::Online)
    {
        const int cid = m_client->myChannelId();
        channel = cid == m_client->rootChannelId() ? tr("root channel") : m_client->channelName(cid);
    }
    o.insert(QStringLiteral("channel"), channel);
    if (const TrackPtr cur = m_player->current())
    {
        QJsonObject now = trackJson(*cur);
        now.insert(QStringLiteral("elapsed"), m_player->isLoading() ? 0.0 : m_player->elapsed());
        now.insert(QStringLiteral("loading"), m_player->isLoading());
        o.insert(QStringLiteral("current"), now);
    }
    QJsonArray q;
    const TrackList& queue = m_player->queue();
    for (int i = 0; i < queue.size() && i < 200; ++i)
        q.append(trackJson(*queue.at(i)));
    o.insert(QStringLiteral("queue"), q);
    o.insert(QStringLiteral("queueLength"), int(queue.size()));
    o.insert(QStringLiteral("paused"), m_player->isPaused());
    o.insert(QStringLiteral("playing"), m_player->isPlaying());
    o.insert(QStringLiteral("volume"), m_player->volume());
    o.insert(QStringLiteral("maxVolume"), m_config.audio.maxVolume);
    o.insert(QStringLiteral("repeat"), repeatModeName(m_player->repeat()));
    o.insert(QStringLiteral("shuffle"), m_player->shuffle());
    o.insert(QStringLiteral("locked"), m_lock.locked());
    o.insert(QStringLiteral("service"), m_sources->service());
    return o;
}

void MediaBot::processMessage(const TTMessage& msg)
{
    switch (msg.nClientEvent)
    {
    case CLIENTEVENT_CON_SUCCESS:
        setState(State::LoggingIn);
        m_loginFailed = false;
        m_loginCmd = m_client->login(m_config.nickname, m_config.username, m_config.password,
                                     QLatin1String(kClientName));
        if (m_loginCmd <= 0)
            scheduleReconnect(tr("login could not be sent"));
        break;

    case CLIENTEVENT_CON_FAILED:
        scheduleReconnect(tr("unable to connect"));
        break;

    case CLIENTEVENT_CON_CRYPT_ERROR:
        scheduleReconnect(tr("encryption error; check the Encrypted setting"));
        break;

    case CLIENTEVENT_CON_LOST:
        scheduleReconnect(tr("connection lost"));
        break;

    case CLIENTEVENT_CMD_MYSELF_LOGGEDOUT:
        if (m_state != State::Stopped)
            scheduleReconnect(tr("logged out by the server"));
        break;

    case CLIENTEVENT_CMD_ERROR:
    {
        const int errNo = msg.clienterrormsg.nErrorNo;
        if (msg.nSource == m_loginCmd && m_loginCmd > 0)
        {
            m_loginFailed = true;
            QString reason = errorName(errNo);
            if (errNo == CMDERR_MAX_LOGINS_PER_IPADDRESS_EXCEEDED)
                reason += QLatin1String(". ") +
                          tr("This server allows only one login per IP address; give the media bot its own account "
                             "or run it from another computer.");
            if (isPermanentLoginError(errNo))
            {
                m_client->disconnect();
                m_lastError = reason;
                m_reconnectTimer.stop();
                setState(State::Failed);
                log(tr("Login to %1 failed: %2").arg(m_config.displayName(), reason));
            }
            else
            {
                scheduleReconnect(reason);
            }
        }
        else if (msg.nSource == m_joinCmd && m_joinCmd > 0)
        {
            m_joinFailed = true;
        }
        break;
    }

    case CLIENTEVENT_CMD_PROCESSING:
        if (!msg.bActive)
        {
            if (msg.nSource == m_loginCmd && m_loginCmd > 0 && !m_loginFailed && m_state == State::LoggingIn)
                onLoginComplete();
            else if (msg.nSource == m_joinCmd && m_joinCmd > 0 && m_state == State::Joining)
                onJoinComplete();
        }
        break;

    case CLIENTEVENT_CMD_MYSELF_KICKED:
        if (msg.nSource > 0)
        {
            log(tr("The bot was kicked from its channel."));
            emit statusChanged();
        }
        else
        {
            scheduleReconnect(tr("kicked from the server"));
        }
        break;

    case CLIENTEVENT_CMD_USER_JOINED:
        if (msg.user.nUserID == m_client->myUserId() && m_state == State::Online)
        {
            const int newCid = msg.user.nChannelID;
            const int oldCid = m_channelId;
            m_channelId = newCid;
            if (oldCid && newCid > 0 && newCid != oldCid)
            {
                log(tr("The bot was moved to another channel."));
                onChannelChanged(newCid);
            }
        }
        break;

    case CLIENTEVENT_CMD_USER_LOGGEDOUT:
        m_text.clearUser(msg.user.nUserID);
        break;

    case CLIENTEVENT_CMD_USER_TEXTMSG:
        if (m_state == State::Online)
        {
            if (const auto done = m_text.add(msg.textmessage))
                onText(*done);
        }
        break;

    default:
        break;
    }
}

} // namespace ttc::media
