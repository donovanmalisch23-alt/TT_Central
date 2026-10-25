// SPDX-License-Identifier: GPL-3.0-or-later
#include "ttc/media/pipeprocess.h"

#include <QRegularExpression>

#include <chrono>

#if defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#else
#  include <cerrno>
#  include <csignal>
#  include <cstring>
#  include <fcntl.h>
#  include <spawn.h>
#  include <sys/wait.h>
#  include <unistd.h>
#  include <vector>
extern char** environ;
#endif

namespace ttc::media {

namespace {
constexpr int kTailBytes = 4000;
} // namespace

#if defined(_WIN32)

struct PipeProcess::Native
{
    HANDLE process = nullptr;
    HANDLE outRead = nullptr;
    HANDLE errRead = nullptr;
    std::atomic<DWORD> readerThread{0};
};

#else

struct PipeProcess::Native
{
    pid_t pid = -1;
    int outRead = -1;
    int errRead = -1;
    bool reaped = false;
    int exitCode = -1;
    std::mutex waitMutex;
};

#endif

PipeProcess::PipeProcess() : m_native(std::make_unique<Native>()) {}

QString PipeProcess::windowsCommandLine(const QString& program, const QStringList& arguments)
{
    auto quote = [](const QString& arg) {
        if (!arg.isEmpty() && !arg.contains(QRegularExpression(QStringLiteral("[\\s\"]"))))
            return arg;
        QString out = QStringLiteral("\"");
        int backslashes = 0;
        for (QChar c : arg)
        {
            if (c == QChar('\\'))
            {
                ++backslashes;
                continue;
            }
            if (c == QChar('"'))
                out += QString(backslashes * 2 + 1, QChar('\\'));
            else
                out += QString(backslashes, QChar('\\'));
            backslashes = 0;
            out += c;
        }
        out += QString(backslashes * 2, QChar('\\'));
        out += QChar('"');
        return out;
    };
    QStringList parts{quote(program)};
    for (const QString& a : arguments)
        parts << quote(a);
    return parts.join(QChar(' '));
}

#if defined(_WIN32)

static QString lastErrorText()
{
    wchar_t* buf = nullptr;
    const DWORD err = GetLastError();
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, err, 0, reinterpret_cast<wchar_t*>(&buf), 0, nullptr);
    QString text = buf ? QString::fromWCharArray(buf).trimmed() : QStringLiteral("error %1").arg(err);
    if (buf)
        LocalFree(buf);
    return text;
}

bool PipeProcess::start(const QString& program, const QStringList& arguments, QString* error)
{
    if (m_started)
        return false;
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE outWrite = nullptr, errWrite = nullptr;
    if (!CreatePipe(&m_native->outRead, &outWrite, &sa, 1 << 16) ||
        !CreatePipe(&m_native->errRead, &errWrite, &sa, 1 << 12))
    {
        if (error)
            *error = lastErrorText();
        return false;
    }
    SetHandleInformation(m_native->outRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(m_native->errRead, HANDLE_FLAG_INHERIT, 0);
    HANDLE nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = nul;
    si.hStdOutput = outWrite;
    si.hStdError = errWrite;
    PROCESS_INFORMATION pi{};
    std::wstring cmd = windowsCommandLine(program, arguments).toStdWString();
    const BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE,
                                   CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, nullptr, nullptr, &si, &pi);
    const QString err = ok ? QString() : lastErrorText();
    CloseHandle(outWrite);
    CloseHandle(errWrite);
    if (nul != INVALID_HANDLE_VALUE)
        CloseHandle(nul);
    if (!ok)
    {
        if (error)
            *error = QStringLiteral("could not start %1: %2").arg(program, err);
        return false;
    }
    CloseHandle(pi.hThread);
    m_native->process = pi.hProcess;
    m_started = true;
    m_stderrThread = std::thread([this] { drainStderr(); });
    return true;
}

qint64 PipeProcess::read(char* buffer, qint64 n)
{
    if (!m_started || m_killed)
        return 0;
    m_native->readerThread = GetCurrentThreadId();
    for (;;)
    {
        DWORD got = 0;
        const BOOL ok = ReadFile(m_native->outRead, buffer, DWORD(n), &got, nullptr);
        if (ok && got > 0)
        {
            m_native->readerThread = 0;
            return qint64(got);
        }
        if (!ok)
        {
            const DWORD e = GetLastError();
            m_native->readerThread = 0;
            return (e == ERROR_BROKEN_PIPE || e == ERROR_OPERATION_ABORTED || m_killed) ? 0 : -1;
        }
        if (m_killed)
            return 0;
    }
}

void PipeProcess::kill()
{
    if (!m_started || m_killed.exchange(true))
        return;
    TerminateProcess(m_native->process, 1);
    // Belt and braces: if anything else still holds the pipe's write end, a
    // blocked ReadFile would not see the end of the pipe. Cancel it.
    if (const DWORD tid = m_native->readerThread)
    {
        if (HANDLE th = OpenThread(THREAD_TERMINATE, FALSE, tid))
        {
            CancelSynchronousIo(th);
            CloseHandle(th);
        }
    }
}

int PipeProcess::waitForExit(int msec)
{
    if (!m_started)
        return -1;
    if (WaitForSingleObject(m_native->process, DWORD(msec < 0 ? INFINITE : msec)) != WAIT_OBJECT_0)
        return -1;
    DWORD code = 0;
    GetExitCodeProcess(m_native->process, &code);
    return int(code);
}

void PipeProcess::drainStderr()
{
    char buf[1024];
    for (;;)
    {
        DWORD got = 0;
        if (!ReadFile(m_native->errRead, buf, sizeof(buf), &got, nullptr) || got == 0)
            break;
        std::lock_guard<std::mutex> lock(m_tailMutex);
        m_tail.append(buf, int(got));
        if (m_tail.size() > kTailBytes)
            m_tail.remove(0, m_tail.size() - kTailBytes);
    }
}

PipeProcess::~PipeProcess()
{
    if (m_started)
    {
        kill();
        WaitForSingleObject(m_native->process, 5000);
        if (m_stderrThread.joinable())
            m_stderrThread.join();
        CloseHandle(m_native->process);
    }
    if (m_native->outRead)
        CloseHandle(m_native->outRead);
    if (m_native->errRead)
        CloseHandle(m_native->errRead);
}

#else // POSIX

bool PipeProcess::start(const QString& program, const QStringList& arguments, QString* error)
{
    if (m_started)
        return false;
    int out[2], err[2];
    if (::pipe(out) != 0)
    {
        if (error)
            *error = QString::fromLocal8Bit(std::strerror(errno));
        return false;
    }
    if (::pipe(err) != 0)
    {
        ::close(out[0]);
        ::close(out[1]);
        if (error)
            *error = QString::fromLocal8Bit(std::strerror(errno));
        return false;
    }
    for (int fd : {out[0], out[1], err[0], err[1]})
        ::fcntl(fd, F_SETFD, FD_CLOEXEC);

    std::vector<QByteArray> storage;
    storage.push_back(program.toLocal8Bit());
    for (const QString& a : arguments)
        storage.push_back(a.toLocal8Bit());
    std::vector<char*> argv;
    for (QByteArray& b : storage)
        argv.push_back(b.data());
    argv.push_back(nullptr);

    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_addopen(&fa, 0, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_adddup2(&fa, out[1], 1);
    posix_spawn_file_actions_adddup2(&fa, err[1], 2);
    pid_t pid = -1;
    const int rc = posix_spawnp(&pid, argv[0], &fa, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&fa);
    ::close(out[1]);
    ::close(err[1]);
    if (rc != 0)
    {
        ::close(out[0]);
        ::close(err[0]);
        if (error)
            *error = QStringLiteral("could not start %1: %2").arg(program, QString::fromLocal8Bit(std::strerror(rc)));
        return false;
    }
    m_native->pid = pid;
    m_native->outRead = out[0];
    m_native->errRead = err[0];
    m_started = true;
    m_stderrThread = std::thread([this] { drainStderr(); });
    return true;
}

qint64 PipeProcess::read(char* buffer, qint64 n)
{
    if (!m_started || m_killed)
        return 0;
    for (;;)
    {
        const ssize_t got = ::read(m_native->outRead, buffer, size_t(n));
        if (got >= 0)
            return qint64(got);
        if (errno != EINTR)
            return m_killed ? 0 : -1;
    }
}

void PipeProcess::kill()
{
    if (!m_started || m_killed.exchange(true))
        return;
    std::lock_guard<std::mutex> lock(m_native->waitMutex);
    if (!m_native->reaped)
        ::kill(m_native->pid, SIGKILL);
}

int PipeProcess::waitForExit(int msec)
{
    if (!m_started)
        return -1;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(msec < 0 ? 1 << 30 : msec);
    for (;;)
    {
        {
            std::lock_guard<std::mutex> lock(m_native->waitMutex);
            if (m_native->reaped)
                return m_native->exitCode;
            int status = 0;
            const pid_t r = ::waitpid(m_native->pid, &status, WNOHANG);
            if (r == m_native->pid)
            {
                m_native->reaped = true;
                m_native->exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
                return m_native->exitCode;
            }
            if (r < 0 && errno != EINTR)
            {
                m_native->reaped = true;
                return m_native->exitCode;
            }
        }
        if (std::chrono::steady_clock::now() >= deadline)
            return -1;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

void PipeProcess::drainStderr()
{
    char buf[1024];
    for (;;)
    {
        const ssize_t got = ::read(m_native->errRead, buf, sizeof(buf));
        if (got < 0 && errno == EINTR)
            continue;
        if (got <= 0)
            break;
        std::lock_guard<std::mutex> lock(m_tailMutex);
        m_tail.append(buf, int(got));
        if (m_tail.size() > kTailBytes)
            m_tail.remove(0, m_tail.size() - kTailBytes);
    }
}

PipeProcess::~PipeProcess()
{
    if (m_started)
    {
        kill();
        waitForExit(5000);
        if (m_stderrThread.joinable())
            m_stderrThread.join();
    }
    if (m_native->outRead >= 0)
        ::close(m_native->outRead);
    if (m_native->errRead >= 0)
        ::close(m_native->errRead);
}

#endif

QByteArray PipeProcess::stderrTail() const
{
    std::lock_guard<std::mutex> lock(m_tailMutex);
    return m_tail;
}

} // namespace ttc::media
