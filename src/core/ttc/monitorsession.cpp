// SPDX-License-Identifier: GPL-3.0-or-later
#include "ttc/monitorsession.h"

#include <QCoreApplication>

#include <algorithm>

namespace ttc {

namespace {
const char* const kClientName = "TT Central Monitor";
} // namespace

MonitorSession::MonitorSession(MonitorConfig config, std::unique_ptr<TeamTalkClient> client,
                               QObject* parent)
    : QObject(parent)
    , m_config(std::move(config))
    , m_client(std::move(client))
{
    m_pollTimer.setInterval(50);
    connect(&m_pollTimer, &QTimer::timeout, this, &MonitorSession::pollNow);
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &MonitorSession::connectNow);
}

MonitorSession::~MonitorSession()
{
    m_pollTimer.stop();
    m_reconnectTimer.stop();
    if (m_client)
        m_client->disconnect();
}

QString MonitorSession::stateText() const
{
    switch (m_state)
    {
    case State::Stopped: return tr("Stopped");
    case State::Connecting: return tr("Connecting");
    case State::LoggingIn: return tr("Logging in");
    case State::Joining: return tr("Joining channel");
    case State::Online: return tr("Online");
    case State::WaitingToReconnect:
        return m_lastError.isEmpty() ? tr("Waiting to reconnect")
                                     : tr("Waiting to reconnect: %1").arg(m_lastError);
    case State::Failed:
        return m_lastError.isEmpty() ? tr("Failed") : tr("Failed: %1").arg(m_lastError);
    }
    return {};
}

void MonitorSession::setPollInterval(int msec) { m_pollTimer.setInterval(msec); }

void MonitorSession::setReconnectDelays(int firstMsec, int maxMsec)
{
    m_firstDelayMs = std::max(1, firstMsec);
    m_maxDelayMs = std::max(m_firstDelayMs, maxMsec);
}

void MonitorSession::start()
{
    if (m_state != State::Stopped && m_state != State::Failed)
        return;
    const QString err = m_config.validationError();
    if (!err.isEmpty())
    {
        m_lastError = err;
        setState(State::Failed);
        return;
    }
    m_lastError.clear();
    m_attempts = 0;
    m_problemAnnounced = false;
    m_pollTimer.start();
    connectNow();
}

void MonitorSession::stop()
{
    m_reconnectTimer.stop();
    m_pollTimer.stop();
    if (m_client)
        m_client->disconnect();
    m_synced = false;
    m_text.clear();
    setState(State::Stopped);
}

void MonitorSession::pollNow()
{
    TTMessage msg;
    // Bounded so a flood of events can never starve the UI thread.
    for (int i = 0; i < 500 && m_client && m_client->nextMessage(msg, 0); ++i)
        processMessage(msg);
}

void MonitorSession::connectNow()
{
    m_loginCmd = m_joinCmd = 0;
    m_loginFailed = m_joinFailed = m_synced = m_fellBackToRoot = false;
    m_text.clear();
    setState(State::Connecting);
    if (!m_client->connect(m_config.host.trimmed(), m_config.tcpPort, m_config.udpPort, m_config.encrypted))
        scheduleReconnect(true, MonitorEvent::Kind::ConnectFailed, tr("could not start connecting"));
}

void MonitorSession::scheduleReconnect(bool announce, MonitorEvent::Kind kind, const QString& reason)
{
    if (m_client)
        m_client->disconnect();
    m_synced = false;
    m_text.clear();
    if (!reason.isEmpty())
        m_lastError = reason;
    if (announce && !m_problemAnnounced)
    {
        m_problemAnnounced = true;
        notify(kind, {}, {}, {}, reason);
    }
    // Exponential backoff: first, 2x, 4x ... capped at max.
    qint64 delay = m_firstDelayMs;
    for (int i = 0; i < m_attempts && delay < m_maxDelayMs; ++i)
        delay *= 2;
    delay = std::min<qint64>(delay, m_maxDelayMs);
    ++m_attempts;
    setState(State::WaitingToReconnect);
    m_reconnectTimer.start(int(delay));
}

void MonitorSession::setState(State s)
{
    if (m_state == s)
        return;
    m_state = s;
    emit stateChanged(s);
}

bool MonitorSession::isPermanentLoginError(int errorNo)
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

bool MonitorSession::isOwnAccount(const User& u) const
{
    const QString mine = m_config.username.trimmed();
    return m_config.ignoreOwnAccount && !mine.isEmpty() &&
           fromTT(u.szUsername).compare(mine, Qt::CaseInsensitive) == 0;
}

QString MonitorSession::describeChannel(int channelId)
{
    if (channelId <= 0)
        return {};
    if (channelId == m_client->rootChannelId())
        return tr("the root channel");
    const QString name = m_client->channelName(channelId);
    return name.isEmpty() ? m_client->channelPath(channelId) : name;
}

void MonitorSession::notify(MonitorEvent::Kind kind, const QString& nickname, const QString& username,
                            const QString& channel, const QString& text)
{
    MonitorEvent e;
    e.kind = kind;
    e.monitorId = m_config.id;
    e.serverName = m_config.displayName();
    e.nickname = nickname;
    e.username = username;
    e.channel = channel;
    e.text = text;
    if (e.isConnectionEvent() && !m_config.notifyConnection)
        return;
    emit event(e);
}

void MonitorSession::onLoginComplete()
{
    m_synced = true;
    m_client->changeStatus(0, m_config.statusMessage);
    setState(State::Joining);

    const QString path = normalizeChannelPath(m_config.channelPath);
    int chan = (path == QLatin1String("/")) ? m_client->rootChannelId()
                                            : m_client->channelIdFromPath(path);
    QString password = m_config.channelPassword;
    if (chan <= 0)
    {
        m_fellBackToRoot = true;
        m_joinError = tr("channel not found");
        chan = m_client->rootChannelId();
        password.clear();
    }
    m_joinFailed = false;
    m_joinCmd = m_client->joinChannel(chan, password);
    if (m_joinCmd <= 0)
    {
        m_joinFailed = true;
        m_joinError = tr("join request failed");
        onJoinComplete();
    }
}

void MonitorSession::onJoinComplete()
{
    const int root = m_client->rootChannelId();
    if (m_joinFailed && !m_fellBackToRoot && m_client->myChannelId() != root)
    {
        // Configured channel refused us: station in the root channel instead.
        m_fellBackToRoot = true;
        m_joinFailed = false;
        m_joinCmd = m_client->joinChannel(root, QString());
        if (m_joinCmd > 0)
            return;
    }

    m_attempts = 0;
    m_problemAnnounced = false;
    m_lastError.clear();
    setState(State::Online);

    if (m_fellBackToRoot)
        notify(MonitorEvent::Kind::ChannelJoinFailed, {}, {}, m_config.channelPath, m_joinError);
    notify(MonitorEvent::Kind::Online, {}, {}, describeChannel(m_client->myChannelId()));
    quietChannelAudio();
}

void MonitorSession::quietChannelAudio()
{
    if (m_config.receiveAudio)
        return;
    const int me = m_client->myUserId();
    const Subscriptions media = Subscriptions(SUBSCRIBE_VOICE | SUBSCRIBE_VIDEOCAPTURE |
                                              SUBSCRIBE_DESKTOP | SUBSCRIBE_MEDIAFILE);
    for (int id : m_client->channelUsers(m_client->myChannelId()))
        if (id != me)
            m_client->unsubscribe(id, media);
}

void MonitorSession::processMessage(const TTMessage& msg)
{
    switch (msg.nClientEvent)
    {
    case CLIENTEVENT_CON_SUCCESS:
        setState(State::LoggingIn);
        m_loginFailed = false;
        m_loginCmd = m_client->login(m_config.nickname, m_config.username, m_config.password,
                                     QLatin1String(kClientName));
        if (m_loginCmd <= 0)
            scheduleReconnect(true, MonitorEvent::Kind::ConnectFailed, tr("login could not be sent"));
        break;

    case CLIENTEVENT_CON_FAILED:
        scheduleReconnect(true, MonitorEvent::Kind::ConnectFailed, {});
        break;

    case CLIENTEVENT_CON_CRYPT_ERROR:
        scheduleReconnect(true, MonitorEvent::Kind::ConnectFailed,
                          tr("encryption error; check the Encrypted setting"));
        break;

    case CLIENTEVENT_CON_LOST:
        scheduleReconnect(true, MonitorEvent::Kind::ConnectionLost, {});
        break;

    case CLIENTEVENT_CMD_MYSELF_LOGGEDOUT:
        if (m_state != State::Stopped)
            scheduleReconnect(true, MonitorEvent::Kind::ConnectionLost, {});
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
                          tr("This server allows only one login per IP address; create a separate account for this monitor.");
            if (isPermanentLoginError(errNo))
            {
                m_client->disconnect();
                m_lastError = reason;
                m_reconnectTimer.stop();
                setState(State::Failed);
                notify(MonitorEvent::Kind::LoginFailed, {}, {}, {}, reason);
            }
            else
            {
                scheduleReconnect(true, MonitorEvent::Kind::LoginFailed, reason);
            }
        }
        else if (msg.nSource == m_joinCmd && m_joinCmd > 0)
        {
            m_joinFailed = true;
            m_joinError = errorName(errNo);
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
    {
        const QString by = (msg.ttType == __USER) ? fromTT(msg.user.szNickname) : QString();
        if (msg.nSource > 0)
        {
            // Kicked from the channel only: stay on the server.
            notify(MonitorEvent::Kind::Kicked, by, {}, describeChannel(msg.nSource));
        }
        else
        {
            m_problemAnnounced = true;
            notify(MonitorEvent::Kind::Kicked, by);
            scheduleReconnect(false, MonitorEvent::Kind::Kicked, tr("kicked from the server"));
        }
        break;
    }

    case CLIENTEVENT_CMD_USER_LOGGEDIN:
        if (m_synced && m_config.notifyLogins && msg.user.nUserID != m_client->myUserId() &&
            !isOwnAccount(msg.user))
            notify(MonitorEvent::Kind::UserLoggedIn, fromTT(msg.user.szNickname), fromTT(msg.user.szUsername));
        break;

    case CLIENTEVENT_CMD_USER_LOGGEDOUT:
        m_text.clearUser(msg.user.nUserID);
        if (m_synced && m_config.notifyLogouts && msg.user.nUserID != m_client->myUserId() &&
            !isOwnAccount(msg.user))
            notify(MonitorEvent::Kind::UserLoggedOut, fromTT(msg.user.szNickname), fromTT(msg.user.szUsername));
        break;

    case CLIENTEVENT_CMD_USER_JOINED:
        if (!m_config.receiveAudio && m_state == State::Online &&
            msg.user.nChannelID == m_client->myChannelId() && msg.user.nUserID != m_client->myUserId())
            m_client->unsubscribe(msg.user.nUserID,
                                  Subscriptions(SUBSCRIBE_VOICE | SUBSCRIBE_VIDEOCAPTURE |
                                                SUBSCRIBE_DESKTOP | SUBSCRIBE_MEDIAFILE));
        break;

    case CLIENTEVENT_CMD_USER_TEXTMSG:
    {
        const auto done = m_text.add(msg.textmessage);
        if (!done || !m_synced)
            break;
        const int me = m_client->myUserId();
        if (done->fromUserId == me)
            break;
        QString nick;
        User from;
        if (m_client->user(done->fromUserId, from))
            nick = fromTT(from.szNickname);
        if (nick.isEmpty())
            nick = done->fromUsername;

        switch (done->type)
        {
        case MSGTYPE_USER:
            // The server delivers private messages only to their recipient and
            // leaves nToUserID as 0 on the receiving side. A different, nonzero
            // id means an admin intercept of someone else's message: ignore it.
            if (m_config.notifyPrivateMessages && (done->toUserId == 0 || done->toUserId == me))
                notify(MonitorEvent::Kind::PrivateMessage, nick, done->fromUsername, {}, done->text);
            break;
        case MSGTYPE_CHANNEL:
            if (m_config.notifyChannelMessages && done->channelId == m_client->myChannelId())
                notify(MonitorEvent::Kind::ChannelMessage, nick, done->fromUsername,
                       describeChannel(done->channelId), done->text);
            break;
        case MSGTYPE_BROADCAST:
            if (m_config.notifyBroadcasts)
                notify(MonitorEvent::Kind::BroadcastMessage, nick, done->fromUsername, {}, done->text);
            break;
        default:
            break;
        }
        break;
    }

    default:
        break;
    }
}

} // namespace ttc
