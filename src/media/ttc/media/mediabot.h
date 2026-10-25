// TT Central media bot — the bot account: connects to the server, stays in
// its channel, takes commands by private message and plays music as voice.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ttc/media/audioengine.h"
#include "ttc/media/botconfig.h"
#include "ttc/media/commands.h"
#include "ttc/media/player.h"
#include "ttc/media/sources.h"
#include "ttc/teamtalkclient.h"
#include "ttc/textassembler.h"

#include <QJsonObject>
#include <QObject>
#include <QTimer>

#include <memory>

namespace ttc::media {

class MediaBot final : public QObject, public BotActions
{
    Q_OBJECT
public:
    enum class State
    {
        Stopped,
        Connecting,
        LoggingIn,
        Joining,
        Online,
        WaitingToReconnect,
        Failed,   // login refused for a reason retrying will not fix
    };
    Q_ENUM(State)

    static QString version();

    MediaBot(const MediaBotConfig& config, std::unique_ptr<TeamTalkClient> client, QObject* parent = nullptr);
    ~MediaBot() override;

    const MediaBotConfig& config() const { return m_config; }
    State state() const { return m_state; }
    QString stateText() const;
    QString lastError() const { return m_lastError; }

    void start();
    void stop();

    Player* player() const { return m_player; }
    AudioEngine* engine() const { return m_engine; }
    SourceRegistry* sources() const { return m_sources; }
    Dispatcher* dispatcher() const { return m_dispatcher; }
    TeamTalkClient* client() const { return m_client.get(); }

    /// Run a command from TT Central's media window (as an administrator).
    void runControlCommand(const QString& text, std::function<void(const QString&)> reply);

    /// Everything the media window shows, as JSON.
    QJsonObject status() const;

    // BotActions
    void replyPrivate(int userId, const QString& text) override;
    void sendChannel(const QString& text) override;
    void shutdown() override;

    // --- tuning / test hooks -------------------------------------------
    void setPollInterval(int msec) { m_pollTimer.setInterval(msec); }
    void setReconnectDelays(int firstMsec, int maxMsec);
    void processMessage(const TTMessage& msg);
    void pollNow();

signals:
    void stateChanged(ttc::media::MediaBot::State state);
    /// The status JSON changed (track, queue, volume, state ...).
    void statusChanged();
    /// A line for the log ("Now playing ...", connection problems, commands).
    void logLine(const QString& text);
    void shutdownRequested();

private:
    void connectNow();
    void scheduleReconnect(const QString& reason);
    void setState(State s);
    void onLoginComplete();
    void onJoinComplete();
    void onChannelChanged(int channelId);
    void onText(const AssembledMessage& t);
    bool isAdmin(int userId, const QString& username);
    void log(const QString& text);
    static bool isPermanentLoginError(int errorNo);

    MediaBotConfig m_config;
    std::unique_ptr<TeamTalkClient> m_client;
    SdkAudioSink m_sink;
    AudioEngine* m_engine;
    SourceRegistry* m_sources;
    Player* m_player;
    LockManager m_lock;
    Dispatcher* m_dispatcher;

    State m_state = State::Stopped;
    QTimer m_pollTimer;
    QTimer m_reconnectTimer;
    int m_firstDelayMs = 5000;
    int m_maxDelayMs = 60000;
    int m_attempts = 0;
    int m_loginCmd = 0;
    int m_joinCmd = 0;
    bool m_loginFailed = false;
    bool m_joinFailed = false;
    bool m_fellBackToRoot = false;
    bool m_everOnline = false;
    bool m_pausedByOutage = false;
    int m_channelId = 0;
    QString m_lastError;
    TextAssembler m_text;
};

} // namespace ttc::media
