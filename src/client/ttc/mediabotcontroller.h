// TT Central — runs the media bot (ttc-mediabot) as a child process and talks
// to it over its control connection.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The bot is a separate process so that a problem in media decoding can never
// take the client down. Its settings live in TT Central's settings file
// ([mediabot] group); when the bot starts they are copied to mediabot.ini next
// to it. If the bot exits unexpectedly it is restarted with a growing delay,
// at most five times in ten minutes.

#pragma once

#include "ttc/media/botconfig.h"

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QTimer>

#include <functional>

class QLocalSocket;
class QSettings;

namespace ttc {

class MediaBotController : public QObject
{
    Q_OBJECT
public:
    enum class State
    {
        Stopped,
        Starting,     // process started, control connection not up yet
        Running,      // connected to the bot
        Restarting,   // exited unexpectedly; restarting soon
        Failed,       // could not start, or kept exiting
    };
    Q_ENUM(State)

    /// `botExecutable` empty = look next to TT Central (see findBotExecutable).
    MediaBotController(QSettings* settings, QString botExecutable = {}, QObject* parent = nullptr);
    ~MediaBotController() override;

    static QString findBotExecutable();

    media::MediaBotConfig config() const { return m_config; }
    /// Save new settings; a running bot restarts with them when they changed.
    void setConfig(const media::MediaBotConfig& c);
    bool isConfigured() const { return !m_config.host.trimmed().isEmpty(); }

    State state() const { return m_state; }
    bool isRunning() const { return m_state == State::Starting || m_state == State::Running || m_state == State::Restarting; }
    QString stateText() const;
    QString lastError() const { return m_lastError; }
    /// The bot's latest status (see ControlServer / MediaBot::status()).
    QJsonObject status() const { return m_status; }
    QString logPath() const;
    QString botExecutable() const { return m_exe; }

    void start();
    void stop();

    /// Send a chat command as TT Central (an administrator). The reply, if
    /// any, goes to `reply` and to replyReceived().
    void command(const QString& text, std::function<void(const QString&)> reply = {});

signals:
    void stateChanged(ttc::MediaBotController::State state);
    void statusChanged(const QJsonObject& status);
    void replyReceived(const QString& text);
    void logLine(const QString& text);

private:
    void setState(State s);
    void launch();
    void connectControl();
    void onReadyRead();
    void onProcessFinished(int code, QProcess::ExitStatus status);
    void send(const QJsonObject& o);
    QString runtimeConfigPath() const;

    QSettings* m_settings;
    QString m_exe;
    media::MediaBotConfig m_config;
    State m_state = State::Stopped;
    QString m_lastError;
    QJsonObject m_status;

    QProcess* m_proc = nullptr;
    QLocalSocket* m_socket = nullptr;
    QString m_controlName;
    QTimer m_connectRetry;
    QTimer m_restartTimer;
    int m_connectAttempts = 0;
    bool m_stopping = false;
    QList<QDateTime> m_crashes;
    int m_nextId = 1;
    QHash<int, std::function<void(const QString&)>> m_pending;
};

} // namespace ttc
