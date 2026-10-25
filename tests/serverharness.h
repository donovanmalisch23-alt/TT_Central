// TT Central tests — runs BearWare's real TeamTalk server (tt5srv) for
// integration tests, configured through TT Central's own config writer.
// SPDX-License-Identifier: MIT
#pragma once

#include "ttc/serverconfig.h"
#include "ttc/teamtalkclient.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QProcess>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QThread>

#include <functional>
#include <memory>

namespace ttc::test {

inline QString serverExe()
{
#ifdef TTC_SERVER_EXE_PATH
    return QStringLiteral(TTC_SERVER_EXE_PATH);
#else
    return qEnvironmentVariable("TTC_SERVER_EXE");
#endif
}

inline int freePort()
{
    QTcpServer probe;
    probe.listen(QHostAddress::LocalHost, 0);
    return probe.serverPort();
}

/// Pump Qt events (so MonitorSession timers run) until `done` or timeout.
inline bool waitUntil(const std::function<bool()>& done, int timeoutMs)
{
    QElapsedTimer t;
    t.start();
    while (!done())
    {
        if (t.elapsed() > timeoutMs)
            return false;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(5);
    }
    return true;
}

class ServerHarness
{
public:
    LocalServerConfig config;
    int port = 0;

    bool start(QString* error)
    {
        if (!QFileInfo::exists(serverExe()))
        {
            *error = QStringLiteral("tt5srv not found at '%1'").arg(serverExe());
            return false;
        }
        if (port == 0)
            port = freePort();
        config.tcpPort = config.udpPort = port;
        if (!writeServerXml(m_dir.filePath(QStringLiteral("tt5srv.xml")), config, error))
            return false;
        return launch(error);
    }

    bool restart(QString* error)
    {
        stop();
        return launch(error);
    }

    void stop()
    {
        if (m_proc && m_proc->state() != QProcess::NotRunning)
        {
            m_proc->kill();
            m_proc->waitForFinished(5000);
        }
    }

    ~ServerHarness() { stop(); }

private:
    bool launch(QString* error)
    {
        m_proc = std::make_unique<QProcess>();
        m_proc->setProcessChannelMode(QProcess::MergedChannels);
        m_proc->setWorkingDirectory(m_dir.path());
        m_proc->start(serverExe(), {QStringLiteral("-nd"), QStringLiteral("-wd"), m_dir.path(),
                                    QStringLiteral("-ip"), QStringLiteral("127.0.0.1"),
                                    QStringLiteral("-tcpport"), QString::number(port),
                                    QStringLiteral("-udpport"), QString::number(port)});
        if (!m_proc->waitForStarted(10000))
        {
            *error = QStringLiteral("tt5srv did not start: %1").arg(m_proc->errorString());
            return false;
        }
        const bool up = waitUntil([this] {
            QTcpSocket s;
            s.connectToHost(QHostAddress::LocalHost, quint16(port));
            return s.waitForConnected(200);
        }, 15000);
        if (!up)
            *error = QStringLiteral("tt5srv never listened on %1: %2")
                         .arg(port).arg(QString::fromLocal8Bit(m_proc->readAll()));
        return up;
    }

    QTemporaryDir m_dir;
    std::unique_ptr<QProcess> m_proc;
};

/// A second, real client that plays "another user" in integration tests.
class Driver
{
public:
    std::unique_ptr<TeamTalkClient> c = createSdkClient();

    bool waitFor(ClientEvent ev, int timeoutMs, const std::function<bool(const TTMessage&)>& pred = {},
                 TTMessage* out = nullptr)
    {
        bool hit = false;
        waitUntil([&] {
            TTMessage m;
            while (!hit && c->nextMessage(m, 0))
                if (m.nClientEvent == ev && (!pred || pred(m)))
                {
                    hit = true;
                    if (out) *out = m;
                }
            return hit;
        }, timeoutMs);
        return hit;
    }

    bool waitCommand(int cmdId, int timeoutMs = 10000)
    {
        return cmdId > 0 && waitFor(CLIENTEVENT_CMD_PROCESSING, timeoutMs,
                                    [cmdId](const TTMessage& m) { return m.nSource == cmdId && !m.bActive; });
    }

    bool login(int port, const QString& nick, const QString& user, const QString& pass)
    {
        if (!c->connect(QStringLiteral("127.0.0.1"), port, port, false))
            return false;
        if (!waitFor(CLIENTEVENT_CON_SUCCESS, 10000))
            return false;
        return waitCommand(c->login(nick, user, pass, QStringLiteral("ttc-test-driver")));
    }

    /// Keep the driver's own SDK queue drained while waiting on something else.
    void drain()
    {
        TTMessage m;
        while (c->nextMessage(m, 0)) {}
    }
};

} // namespace ttc::test
