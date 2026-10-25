// SPDX-License-Identifier: GPL-3.0-or-later
#include "ttc/mediabotcontroller.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QSettings>
#include <QUuid>

namespace ttc {

namespace {
constexpr int kMaxCrashes = 5;
constexpr int kCrashWindowSecs = 600;
} // namespace

MediaBotController::MediaBotController(QSettings* settings, QString botExecutable, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_exe(botExecutable.isEmpty() ? findBotExecutable() : botExecutable)
{
    if (m_settings)
        m_config = media::loadMediaBotConfig(*m_settings, false);
    m_connectRetry.setInterval(200);
    connect(&m_connectRetry, &QTimer::timeout, this, &MediaBotController::connectControl);
    m_restartTimer.setSingleShot(true);
    connect(&m_restartTimer, &QTimer::timeout, this, &MediaBotController::launch);
}

MediaBotController::~MediaBotController()
{
    stop();
}

QString MediaBotController::findBotExecutable()
{
#if defined(Q_OS_WIN)
    const QString name = QStringLiteral("ttc-mediabot.exe");
#else
    const QString name = QStringLiteral("ttc-mediabot");
#endif
    const QString env = qEnvironmentVariable("TTC_MEDIABOT_EXE");
    if (!env.isEmpty())
        return env;
    const QDir app(QCoreApplication::applicationDirPath());
    for (const QString& rel : {name, QStringLiteral("bin/") + name})
        if (QFileInfo::exists(app.filePath(rel)))
            return app.filePath(rel);
    return app.filePath(name);
}

QString MediaBotController::stateText() const
{
    switch (m_state)
    {
    case State::Stopped: return tr("Stopped");
    case State::Starting: return tr("Starting");
    case State::Running:
    {
        const QString bot = m_status.value(QStringLiteral("stateText")).toString();
        if (m_status.value(QStringLiteral("online")).toBool())
        {
            const QString chan = m_status.value(QStringLiteral("channel")).toString();
            return chan.isEmpty() ? tr("Online on %1").arg(m_status.value(QStringLiteral("server")).toString())
                                  : tr("Online on %1, in %2").arg(m_status.value(QStringLiteral("server")).toString(), chan);
        }
        return bot.isEmpty() ? tr("Running") : bot;
    }
    case State::Restarting: return tr("Stopped unexpectedly; restarting");
    case State::Failed: return m_lastError.isEmpty() ? tr("Failed") : tr("Failed: %1").arg(m_lastError);
    }
    return {};
}

QString MediaBotController::runtimeConfigPath() const
{
    const QString base = m_settings ? QFileInfo(m_settings->fileName()).absolutePath()
                                    : QDir::tempPath();
    return QDir(base).filePath(QStringLiteral("mediabot.ini"));
}

QString MediaBotController::logPath() const
{
    return QFileInfo(runtimeConfigPath()).absoluteDir().filePath(QStringLiteral("mediabot.log"));
}

void MediaBotController::setConfig(const media::MediaBotConfig& c)
{
    const bool changed = c != m_config;
    m_config = c;
    if (m_settings)
    {
        media::saveMediaBotConfig(*m_settings, c);
        m_settings->sync();
    }
    if (changed && isRunning())
    {
        stop();
        start();
    }
}

void MediaBotController::setState(State s)
{
    if (m_state == s)
        return;
    m_state = s;
    emit stateChanged(s);
}

void MediaBotController::start()
{
    if (isRunning())
        return;
    const QString err = m_config.validationError();
    if (!err.isEmpty())
    {
        m_lastError = err;
        setState(State::Failed);
        return;
    }
    if (!QFileInfo::exists(m_exe))
    {
        m_lastError = tr("the media bot program was not found (%1)").arg(QDir::toNativeSeparators(m_exe));
        setState(State::Failed);
        return;
    }
    m_crashes.clear();
    m_lastError.clear();
    launch();
}

void MediaBotController::launch()
{
    m_stopping = false;
    {
        QFile::remove(runtimeConfigPath());
        QSettings out(runtimeConfigPath(), QSettings::IniFormat);
        media::saveMediaBotConfig(out, m_config);
        out.sync();
        if (out.status() != QSettings::NoError)
        {
            m_lastError = tr("could not write %1").arg(QDir::toNativeSeparators(runtimeConfigPath()));
            setState(State::Failed);
            return;
        }
    }
    m_controlName = QStringLiteral("ttc-mediabot-") + QUuid::createUuid().toString(QUuid::Id128);
    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::MergedChannels);
    m_proc->setWorkingDirectory(QFileInfo(m_exe).absolutePath());
    connect(m_proc, &QProcess::finished, this, &MediaBotController::onProcessFinished);
    connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
        {
            m_lastError = tr("could not start the media bot: %1").arg(m_proc->errorString());
            m_proc->deleteLater();
            m_proc = nullptr;
            setState(State::Failed);
        }
    });
    // The bot also logs to a file; keep its console output drained.
    connect(m_proc, &QProcess::readyRead, this, [this] { m_proc->readAll(); });
    setState(State::Starting);
    m_proc->start(m_exe, {QStringLiteral("--config"), runtimeConfigPath(), QStringLiteral("--control"), m_controlName,
                          QStringLiteral("--parent-pid"), QString::number(QCoreApplication::applicationPid()),
                          QStringLiteral("--log"), logPath()});
    m_connectAttempts = 0;
    m_connectRetry.start();
}

void MediaBotController::connectControl()
{
    if (!m_proc || m_proc->state() == QProcess::NotRunning)
    {
        m_connectRetry.stop();
        return;
    }
    if (++m_connectAttempts > 75) // ~15 s
    {
        m_connectRetry.stop();
        m_lastError = tr("the media bot did not respond");
        stop();
        setState(State::Failed);
        return;
    }
    if (m_socket)
        m_socket->deleteLater();
    m_socket = new QLocalSocket(this);
    connect(m_socket, &QLocalSocket::connected, this, [this] {
        m_connectRetry.stop();
        setState(State::Running);
    });
    connect(m_socket, &QLocalSocket::readyRead, this, &MediaBotController::onReadyRead);
    m_socket->connectToServer(m_controlName);
}

void MediaBotController::onReadyRead()
{
    while (m_socket && m_socket->canReadLine())
    {
        const QJsonObject o = QJsonDocument::fromJson(m_socket->readLine()).object();
        const QString type = o.value(QStringLiteral("type")).toString();
        if (type == QLatin1String("status"))
        {
            m_status = o;
            emit statusChanged(o);
            emit stateChanged(m_state); // stateText() includes the bot's own state
        }
        else if (type == QLatin1String("reply"))
        {
            const QString text = o.value(QStringLiteral("text")).toString();
            const int id = o.value(QStringLiteral("id")).toInt();
            if (auto cb = m_pending.value(id))
                cb(text);
            emit replyReceived(text);
        }
        else if (type == QLatin1String("log"))
        {
            emit logLine(o.value(QStringLiteral("text")).toString());
        }
    }
}

void MediaBotController::send(const QJsonObject& o)
{
    if (m_socket && m_socket->state() == QLocalSocket::ConnectedState)
        m_socket->write(QJsonDocument(o).toJson(QJsonDocument::Compact) + '\n');
}

void MediaBotController::command(const QString& text, std::function<void(const QString&)> reply)
{
    if (m_state != State::Running)
    {
        const QString msg = tr("The media bot is not running.");
        if (reply)
            reply(msg);
        emit replyReceived(msg);
        return;
    }
    const int id = m_nextId++;
    if (reply)
        m_pending.insert(id, std::move(reply));
    if (m_pending.size() > 200)
        m_pending.remove(m_pending.keys().first());
    send({{QStringLiteral("cmd"), QStringLiteral("command")}, {QStringLiteral("id"), id}, {QStringLiteral("text"), text}});
}

void MediaBotController::stop()
{
    m_restartTimer.stop();
    m_connectRetry.stop();
    m_stopping = true;
    if (m_proc && m_proc->state() != QProcess::NotRunning)
    {
        send({{QStringLiteral("cmd"), QStringLiteral("shutdown")}});
        if (m_socket)
            m_socket->flush();
        if (!m_proc->waitForFinished(3000))
        {
            m_proc->kill();
            m_proc->waitForFinished(2000);
        }
    }
    if (m_socket)
    {
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    if (m_proc)
    {
        m_proc->disconnect(this);
        m_proc->deleteLater();
        m_proc = nullptr;
    }
    m_pending.clear();
    m_status = {};
    emit statusChanged(m_status);
    if (m_state != State::Failed)
        setState(State::Stopped);
}

void MediaBotController::onProcessFinished(int code, QProcess::ExitStatus status)
{
    if (m_stopping)
        return;
    if (m_socket)
    {
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    m_proc->deleteLater();
    m_proc = nullptr;
    m_status = {};
    emit statusChanged(m_status);
    if (status == QProcess::NormalExit && code == 0)
    {
        setState(State::Stopped); // asked to quit (for example, an admin sent "quit")
        return;
    }
    if (status == QProcess::NormalExit && code == 2)
    {
        m_lastError = tr("the media bot's settings are incomplete; see %1").arg(QDir::toNativeSeparators(logPath()));
        setState(State::Failed);
        return;
    }
    const QDateTime now = QDateTime::currentDateTimeUtc();
    m_crashes.append(now);
    while (!m_crashes.isEmpty() && m_crashes.first().secsTo(now) > kCrashWindowSecs)
        m_crashes.removeFirst();
    if (m_crashes.size() > kMaxCrashes)
    {
        m_lastError = tr("the media bot keeps stopping; see %1").arg(QDir::toNativeSeparators(logPath()));
        setState(State::Failed);
        return;
    }
    setState(State::Restarting);
    m_restartTimer.start(1000 * (1 << std::min(int(m_crashes.size()), 5)));
}

} // namespace ttc
