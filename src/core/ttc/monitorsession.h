// TT Central — one background monitor: a TeamTalk login stationed in a
// channel that reports logins, logouts, private messages and channel
// messages as MonitorEvents.
// SPDX-License-Identifier: MIT

#pragma once

#include "ttc/monitorconfig.h"
#include "ttc/monitorevent.h"
#include "ttc/teamtalkclient.h"
#include "ttc/textassembler.h"

#include <QObject>
#include <QTimer>

#include <memory>

namespace ttc {

class MonitorSession : public QObject
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

    MonitorSession(MonitorConfig config, std::unique_ptr<TeamTalkClient> client,
                   QObject* parent = nullptr);
    ~MonitorSession() override;

    const MonitorConfig& config() const { return m_config; }
    State state() const { return m_state; }
    /// A short, speakable description of the current state.
    QString stateText() const;
    /// The last failure reason (empty when none).
    QString lastError() const { return m_lastError; }

    void start();
    void stop();

    // --- tuning / test hooks -------------------------------------------
    void setPollInterval(int msec);
    void setReconnectDelays(int firstMsec, int maxMsec);
    /// Handle one SDK event (normally called from the poll timer).
    void processMessage(const TTMessage& msg);
    /// Drain all queued SDK events now.
    void pollNow();

signals:
    void event(const ttc::MonitorEvent& e);
    void stateChanged(ttc::MonitorSession::State state);

private:
    void connectNow();
    void scheduleReconnect(bool announce, MonitorEvent::Kind kind, const QString& reason);
    void setState(State s);
    void onLoginComplete();
    void onJoinComplete();
    void quietChannelAudio();
    void notify(MonitorEvent::Kind kind, const QString& nickname = {}, const QString& username = {},
                const QString& channel = {}, const QString& text = {});
    bool isOwnAccount(const User& u) const;
    QString describeChannel(int channelId);
    static bool isPermanentLoginError(int errorNo);

    MonitorConfig m_config;
    std::unique_ptr<TeamTalkClient> m_client;
    State m_state = State::Stopped;

    QTimer m_pollTimer;
    QTimer m_reconnectTimer;
    int m_firstDelayMs = 5000;
    int m_maxDelayMs = 60000;
    int m_attempts = 0;
    bool m_problemAnnounced = false;   // one notice per outage, not per retry

    int m_loginCmd = 0;
    int m_joinCmd = 0;
    bool m_loginFailed = false;
    bool m_joinFailed = false;
    QString m_joinError;
    bool m_synced = false;             // login finished; user events are live
    bool m_fellBackToRoot = false;
    QString m_lastError;

    TextAssembler m_text;
};

} // namespace ttc
