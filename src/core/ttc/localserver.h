// TT Central — hosts a TeamTalk server on this computer by running
// BearWare's prebuilt Standard server (tt5srv) with TT Central's settings.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The server's own tt5srv.xml (in the server folder) is the source of truth:
// TT Central edits the settings it manages and leaves the channels, bans and
// history the server stores there untouched. tt5srv reads the file when it
// starts and saves its own changes into it, so settings are only written
// while the server is stopped (a running server is restarted around it).

#pragma once

#include "ttc/serverconfig.h"

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QTimer>

namespace ttc {

class LocalServer : public QObject
{
    Q_OBJECT
public:
    enum class State
    {
        Stopped,
        Starting,     // process started, not listening yet
        Running,      // accepting connections
        Restarting,   // stopped unexpectedly; restarting soon
        Failed,
    };
    Q_ENUM(State)

    LocalServer(QString serverExe, QString workDir, QObject* parent = nullptr);
    ~LocalServer() override;

    /// Where to find tt5srv: TTC_SERVER_EXE, then next to TT Central
    /// (server/tt5srv[.exe] or tt5srv[.exe]), then `fallback`.
    static QString findServerExecutable(const QString& fallback = {});

    QString serverExecutable() const { return m_exe; }
    QString workDir() const { return m_workDir; }
    QString configPath() const;
    QString logPath() const;
    bool hasConfig() const;

    /// The saved settings, or defaults when there are none yet.
    LocalServerConfig config() const;
    /// Validate and save. A running server is stopped, updated and started
    /// again. False (with `error`) when the settings can't be used.
    bool setConfig(const LocalServerConfig& c, QString* error = nullptr);

    State state() const { return m_state; }
    bool isRunning() const { return m_state == State::Starting || m_state == State::Running || m_state == State::Restarting; }
    QString stateText() const;
    QString lastError() const { return m_lastError; }
    /// The server's most recent output, oldest first.
    QStringList logTail() const { return m_log; }

    void start();
    void stop();

    // Test hooks
    void setStartTimeoutMs(int ms) { m_startTimeoutMs = ms; }
    void setRestartDelayMs(int ms) { m_restartDelayMs = ms; }
    qint64 processId() const;

signals:
    void stateChanged(ttc::LocalServer::State state);
    void logLine(const QString& line);

private:
    void setState(State s);
    void launch();
    void probe();
    void onFinished(int code, QProcess::ExitStatus status);
    void appendLog(const QString& text);
    static bool portIsFree(int port, const QString& bindIp);

    QString m_exe;
    QString m_workDir;
    State m_state = State::Stopped;
    QString m_lastError;
    QStringList m_log;
    QString m_partial;
    QProcess* m_proc = nullptr;
    QTimer m_probe;
    QTimer m_restart;
    QDateTime m_startedAt;
    int m_startTimeoutMs = 15000;
    int m_restartDelayMs = 2000;
    bool m_stopping = false;
    QList<QDateTime> m_crashes;
    int m_port = 0;
};

} // namespace ttc
