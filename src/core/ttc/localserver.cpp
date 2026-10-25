// SPDX-License-Identifier: GPL-3.0-or-later
#include "ttc/localserver.h"

#include "ttc/childprocess.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTextStream>
#include <QThread>

#if defined(Q_OS_WIN)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#else
#  include <csignal>
#endif

namespace ttc {

namespace {
constexpr int kLogLines = 500;
constexpr int kMaxCrashes = 5;
constexpr int kCrashWindowSecs = 600;
} // namespace

LocalServer::LocalServer(QString serverExe, QString workDir, QObject* parent)
    : QObject(parent)
    , m_exe(std::move(serverExe))
    , m_workDir(std::move(workDir))
{
    m_probe.setInterval(200);
    connect(&m_probe, &QTimer::timeout, this, &LocalServer::probe);
    m_restart.setSingleShot(true);
    connect(&m_restart, &QTimer::timeout, this, &LocalServer::launch);
}

LocalServer::~LocalServer()
{
    stop();
}

QString LocalServer::findServerExecutable(const QString& fallback)
{
#if defined(Q_OS_WIN)
    const QString name = QStringLiteral("tt5srv.exe");
#else
    const QString name = QStringLiteral("tt5srv");
#endif
    const QString env = qEnvironmentVariable("TTC_SERVER_EXE");
    if (!env.isEmpty())
        return env;
    const QDir app(QCoreApplication::applicationDirPath());
    for (const QString& rel : {QStringLiteral("server/") + name, name})
        if (QFileInfo::exists(app.filePath(rel)))
            return app.filePath(rel);
    if (!fallback.isEmpty() && QFileInfo::exists(fallback))
        return fallback;
    return app.filePath(QStringLiteral("server/") + name);
}

QString LocalServer::configPath() const
{
    return QDir(m_workDir).filePath(QStringLiteral("tt5srv.xml"));
}

QString LocalServer::pidPath() const
{
    return QDir(m_workDir).filePath(QStringLiteral("tt5srv.pid"));
}

namespace {

/// The executable file name of a running process ("" if it isn't running).
QString processImageName(qint64 pid)
{
#if defined(Q_OS_WIN)
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, DWORD(pid));
    if (!h)
        return {};
    wchar_t path[MAX_PATH] = {};
    DWORD len = MAX_PATH;
    DWORD code = 0;
    QString name;
    if (GetExitCodeProcess(h, &code) && code == STILL_ACTIVE && QueryFullProcessImageNameW(h, 0, path, &len))
        name = QFileInfo(QString::fromWCharArray(path, int(len))).fileName();
    CloseHandle(h);
    return name;
#else
    return QFileInfo(QFile::symLinkTarget(QStringLiteral("/proc/%1/exe").arg(pid))).fileName();
#endif
}

void terminateProcess(qint64 pid)
{
#if defined(Q_OS_WIN)
    if (HANDLE h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, DWORD(pid)))
    {
        TerminateProcess(h, 1);
        WaitForSingleObject(h, 5000);
        CloseHandle(h);
    }
#else
    ::kill(pid_t(pid), SIGKILL);
    for (int i = 0; i < 100 && ::kill(pid_t(pid), 0) == 0; ++i)
        QThread::msleep(20);
#endif
}

} // namespace

void LocalServer::reclaimLeftover()
{
    QFile f(pidPath());
    if (!f.open(QIODevice::ReadOnly))
        return;
    const qint64 pid = f.readAll().trimmed().toLongLong();
    f.close();
    QFile::remove(pidPath());
    if (pid <= 0 || (m_proc && m_proc->processId() == pid))
        return;
    const QString name = processImageName(pid);
    if (name.startsWith(QLatin1String("tt5srv"), Qt::CaseInsensitive))
    {
        appendLog(tr("Stopping a server left running by an earlier session.") + QChar('\n'));
        terminateProcess(pid);
    }
}

QString LocalServer::logPath() const
{
    return QDir(m_workDir).filePath(QStringLiteral("tt5srv-console.log"));
}

bool LocalServer::hasConfig() const
{
    return QFileInfo::exists(configPath());
}

LocalServerConfig LocalServer::config() const
{
    LocalServerConfig c;
    if (hasConfig())
        readServerXml(configPath(), c);
    return c;
}

bool LocalServer::setConfig(const LocalServerConfig& c, QString* error)
{
    const QString err = c.validationError();
    if (!err.isEmpty())
    {
        if (error)
            *error = err;
        return false;
    }
    const bool wasRunning = isRunning();
    if (wasRunning)
        stop();
    if (!QDir().mkpath(m_workDir))
    {
        if (error)
            *error = tr("Can't create the server folder %1.").arg(QDir::toNativeSeparators(m_workDir));
        return false;
    }
    QString werr;
    if (!writeServerXml(configPath(), c, &werr))
    {
        if (error)
            *error = werr;
        return false;
    }
    if (wasRunning)
        start();
    return true;
}

QString LocalServer::stateText() const
{
    switch (m_state)
    {
    case State::Stopped: return tr("Stopped");
    case State::Starting: return tr("Starting");
    case State::Running:
        return tr("Running: accepting connections on port %1").arg(m_port);
    case State::Restarting: return tr("Stopped unexpectedly; restarting");
    case State::Failed: return m_lastError.isEmpty() ? tr("Failed") : tr("Failed: %1").arg(m_lastError);
    }
    return {};
}

qint64 LocalServer::processId() const
{
    return m_proc ? m_proc->processId() : 0;
}

void LocalServer::setState(State s)
{
    if (m_state == s)
        return;
    m_state = s;
    emit stateChanged(s);
}

bool LocalServer::portIsFree(int port, const QString& bindIp)
{
    // Windows lets a program bind one address on a port that another holds
    // on "all addresses" (and the other way round), so check every address
    // the server may use, and whether anything already answers there.
    QList<QHostAddress> addresses{QHostAddress(QHostAddress::Any), QHostAddress(QHostAddress::LocalHost)};
    const QHostAddress bind(bindIp.trimmed());
    if (!bindIp.trimmed().isEmpty() && !bind.isNull())
        addresses << bind;
    for (const QHostAddress& a : std::as_const(addresses))
    {
        QTcpServer probe;
        if (!probe.listen(a, quint16(port)))
            return false;
    }
    QTcpSocket s;
    s.connectToHost(bind.isNull() || bind == QHostAddress(QHostAddress::Any) ? QHostAddress(QHostAddress::LocalHost) : bind,
                    quint16(port));
    if (s.waitForConnected(300))
    {
        s.abort();
        return false;
    }
    return true;
}

void LocalServer::start()
{
    if (isRunning())
        return;
    m_lastError.clear();
    if (!QFileInfo::exists(m_exe))
    {
        m_lastError = tr("the TeamTalk server program was not found (%1)").arg(QDir::toNativeSeparators(m_exe));
        setState(State::Failed);
        return;
    }
    if (!hasConfig())
    {
        m_lastError = tr("the server is not set up yet; add an administrator account and save");
        setState(State::Failed);
        return;
    }
    const LocalServerConfig c = config();
    const QString err = c.validationError();
    if (!err.isEmpty())
    {
        m_lastError = err;
        setState(State::Failed);
        return;
    }
    m_crashes.clear();
    reclaimLeftover();
    launch();
}

void LocalServer::launch()
{
    const LocalServerConfig c = config();
    m_port = c.tcpPort;
    if (!portIsFree(c.tcpPort, c.bindIp))
    {
        m_lastError = tr("port %1 is already in use by another program; choose another port").arg(c.tcpPort);
        setState(State::Failed);
        return;
    }
    m_stopping = false;
    m_partial.clear();
    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::MergedChannels);
    m_proc->setWorkingDirectory(m_workDir);
    limitInheritedHandles(m_proc);
    connect(m_proc, &QProcess::readyRead, this, [this] {
        appendLog(QString::fromLocal8Bit(m_proc->readAll()));
    });
    connect(m_proc, &QProcess::finished, this, &LocalServer::onFinished);
    connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart)
            return;
        m_probe.stop();
        m_lastError = tr("could not start the TeamTalk server: %1").arg(m_proc->errorString());
        m_proc->deleteLater();
        m_proc = nullptr;
        setState(State::Failed);
    });
    QStringList args{QStringLiteral("-nd"), QStringLiteral("-wd"), QDir::toNativeSeparators(m_workDir),
                     QStringLiteral("-verbose")};
    appendLog(tr("Starting the TeamTalk server on port %1.").arg(c.tcpPort) + QChar('\n'));
    setState(State::Starting);
    m_startedAt = QDateTime::currentDateTimeUtc();
    m_proc->start(m_exe, args);
    m_probe.start();
}

void LocalServer::probe()
{
    if (!m_proc || m_proc->state() == QProcess::NotRunning)
    {
        m_probe.stop();
        return;
    }
    QTcpSocket s;
    s.connectToHost(QHostAddress::LocalHost, quint16(m_port));
    if (s.waitForConnected(150))
    {
        s.abort();
        m_probe.stop();
        appendLog(tr("The server is accepting connections.") + QChar('\n'));
        QFile pid(pidPath());
        if (pid.open(QIODevice::WriteOnly | QIODevice::Truncate))
            pid.write(QByteArray::number(m_proc->processId()));
        setState(State::Running);
        return;
    }
    if (m_startedAt.msecsTo(QDateTime::currentDateTimeUtc()) > m_startTimeoutMs)
    {
        m_probe.stop();
        m_lastError = tr("the server did not start listening on port %1").arg(m_port);
        stop();
        setState(State::Failed);
    }
}

void LocalServer::stop()
{
    m_restart.stop();
    m_probe.stop();
    m_stopping = true;
    if (m_proc)
    {
        if (m_proc->state() != QProcess::NotRunning)
        {
            // tt5srv has no clean shutdown command; it saves its own changes
            // as they happen (auto-save), so stopping it is safe.
            m_proc->kill();
            m_proc->waitForFinished(5000);
            appendLog(tr("The server was stopped.") + QChar('\n'));
        }
        m_proc->disconnect(this);
        m_proc->deleteLater();
        m_proc = nullptr;
    }
    QFile::remove(pidPath());
    if (m_state != State::Failed)
        setState(State::Stopped);
}

void LocalServer::onFinished(int code, QProcess::ExitStatus status)
{
    if (m_stopping)
        return;
    m_probe.stop();
    appendLog(QString::fromLocal8Bit(m_proc->readAll()));
    m_proc->deleteLater();
    m_proc = nullptr;
    const bool early = m_state == State::Starting;
    const QString last = m_log.isEmpty() ? QString() : m_log.last();
    if (early)
    {
        // It never came up: retrying won't help (bad settings, missing files).
        m_lastError = status == QProcess::CrashExit ? tr("the server crashed while starting")
                                                    : tr("the server stopped while starting (exit code %1): %2")
                                                          .arg(code)
                                                          .arg(last);
        setState(State::Failed);
        return;
    }
    const QDateTime now = QDateTime::currentDateTimeUtc();
    m_crashes.append(now);
    while (!m_crashes.isEmpty() && m_crashes.first().secsTo(now) > kCrashWindowSecs)
        m_crashes.removeFirst();
    if (m_crashes.size() > kMaxCrashes)
    {
        m_lastError = tr("the server keeps stopping; see its log");
        setState(State::Failed);
        return;
    }
    appendLog(tr("The server stopped unexpectedly; restarting it.") + QChar('\n'));
    setState(State::Restarting);
    m_restart.start(m_restartDelayMs);
}

void LocalServer::appendLog(const QString& text)
{
    if (text.isEmpty())
        return;
    m_partial += text;
    m_partial.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    const int lastNl = m_partial.lastIndexOf(QChar('\n'));
    if (lastNl < 0)
        return;
    const QStringList lines = m_partial.left(lastNl).split(QChar('\n'));
    m_partial = m_partial.mid(lastNl + 1);
    QFile f(logPath());
    const bool file = QDir().mkpath(m_workDir) && f.open(QIODevice::Append | QIODevice::Text);
    QTextStream out(&f);
    for (const QString& raw : lines)
    {
        const QString line = raw.trimmed();
        if (line.isEmpty())
            continue;
        m_log << line;
        if (file)
            out << QDateTime::currentDateTime().toString(Qt::ISODate) << ' ' << line << '\n';
        emit logLine(line);
    }
    while (m_log.size() > kLogLines)
        m_log.removeFirst();
}

} // namespace ttc
