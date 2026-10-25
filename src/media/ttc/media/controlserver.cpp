// SPDX-License-Identifier: GPL-3.0-or-later
#include "ttc/media/controlserver.h"

#include "ttc/media/mediabot.h"

#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QPointer>

namespace ttc::media {

namespace {
constexpr qint64 kMaxLine = 64 * 1024;
} // namespace

ControlServer::ControlServer(MediaBot* bot, QObject* parent)
    : QObject(parent)
    , m_bot(bot)
    , m_server(new QLocalServer(this))
{
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    connect(m_server, &QLocalServer::newConnection, this, &ControlServer::onNewConnection);

    // Coalesce bursts of changes into at most ~4 status messages a second.
    m_statusThrottle.setSingleShot(true);
    m_statusThrottle.setInterval(250);
    connect(&m_statusThrottle, &QTimer::timeout, this, &ControlServer::pushStatus);
    connect(m_bot, &MediaBot::statusChanged, this, [this] {
        if (!m_statusThrottle.isActive())
            m_statusThrottle.start();
    });
    // Position updates while playing.
    m_tick.setInterval(1000);
    connect(&m_tick, &QTimer::timeout, this, [this] {
        if (m_bot->player()->isPlaying() && !m_bot->player()->isPaused())
            pushStatus();
    });
    m_tick.start();
    connect(m_bot, &MediaBot::logLine, this, [this](const QString& text) {
        m_recentLog << text;
        if (m_recentLog.size() > 50)
            m_recentLog.removeFirst();
        broadcast({{QStringLiteral("type"), QStringLiteral("log")}, {QStringLiteral("text"), text}});
    });
}

ControlServer::~ControlServer() = default;

bool ControlServer::listen(const QString& name, QString* error)
{
    QLocalServer::removeServer(name);
    if (!m_server->listen(name))
    {
        if (error)
            *error = m_server->errorString();
        return false;
    }
    return true;
}

QByteArray ControlServer::encode(const QJsonObject& o)
{
    return QJsonDocument(o).toJson(QJsonDocument::Compact) + '\n';
}

void ControlServer::onNewConnection()
{
    while (QLocalSocket* s = m_server->nextPendingConnection())
    {
        m_clients << s;
        connect(s, &QLocalSocket::readyRead, this, [this, s] { onReadyRead(s); });
        connect(s, &QLocalSocket::disconnected, this, [this, s] {
            m_clients.removeAll(s);
            s->deleteLater();
        });
        send(s, {{QStringLiteral("type"), QStringLiteral("hello")}, {QStringLiteral("version"), MediaBot::version()}});
        for (const QString& line : std::as_const(m_recentLog))
            send(s, {{QStringLiteral("type"), QStringLiteral("log")}, {QStringLiteral("text"), line}});
        QJsonObject st = m_bot->status();
        st.insert(QStringLiteral("type"), QStringLiteral("status"));
        send(s, st);
    }
}

void ControlServer::onReadyRead(QLocalSocket* s)
{
    while (s->canReadLine())
    {
        const QByteArray line = s->readLine(kMaxLine).trimmed();
        if (line.isEmpty())
            continue;
        const QJsonDocument doc = QJsonDocument::fromJson(line);
        if (doc.isObject())
            handle(s, doc.object());
    }
    if (s->bytesAvailable() > kMaxLine)
        s->disconnectFromServer(); // not a well-behaved client
}

void ControlServer::handle(QLocalSocket* s, const QJsonObject& msg)
{
    const QString cmd = msg.value(QStringLiteral("cmd")).toString();
    if (cmd == QLatin1String("command"))
    {
        const QJsonValue id = msg.value(QStringLiteral("id"));
        QPointer<QLocalSocket> guard(s);
        m_bot->runControlCommand(msg.value(QStringLiteral("text")).toString(), [this, guard, id](const QString& text) {
            if (guard)
                send(guard, {{QStringLiteral("type"), QStringLiteral("reply")}, {QStringLiteral("id"), id},
                             {QStringLiteral("text"), text}});
        });
    }
    else if (cmd == QLatin1String("status"))
    {
        QJsonObject st = m_bot->status();
        st.insert(QStringLiteral("type"), QStringLiteral("status"));
        send(s, st);
    }
    else if (cmd == QLatin1String("shutdown"))
    {
        m_bot->shutdown();
    }
}

void ControlServer::send(QLocalSocket* s, const QJsonObject& o)
{
    if (s->state() == QLocalSocket::ConnectedState)
        s->write(encode(o));
}

void ControlServer::broadcast(const QJsonObject& o)
{
    const QByteArray data = encode(o);
    for (QLocalSocket* s : std::as_const(m_clients))
        if (s->state() == QLocalSocket::ConnectedState)
            s->write(data);
}

void ControlServer::pushStatus()
{
    if (m_clients.isEmpty())
        return;
    QJsonObject st = m_bot->status();
    st.insert(QStringLiteral("type"), QStringLiteral("status"));
    broadcast(st);
}

} // namespace ttc::media
