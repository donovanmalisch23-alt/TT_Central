// TT Central media bot — a child process whose stdout is read with blocking
// reads (the decoder: ffmpeg).
// SPDX-License-Identifier: GPL-3.0-or-later
//
// QProcess reads a child's output into an unbounded buffer as soon as it is
// available. The audio feeder needs the opposite: it reads at playback speed,
// and while paused it stops reading so the OS pipe fills and the decoder
// blocks. That back-pressure keeps memory flat for hour-long tracks and makes
// pause free. stderr is drained continuously (keeping only its tail) so the
// child can never block on it.
//
// start() must be called from one thread only (the bot's main thread, which
// also starts every QProcess), so no other child can inherit our pipe.

#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

namespace ttc::media {

class PipeProcess
{
public:
    PipeProcess();
    ~PipeProcess();
    PipeProcess(const PipeProcess&) = delete;
    PipeProcess& operator=(const PipeProcess&) = delete;

    bool start(const QString& program, const QStringList& arguments, QString* error = nullptr);
    bool isStarted() const { return m_started; }

    /// Blocking read of up to n bytes. Returns the number read, 0 at end of
    /// output (or after kill()), -1 on error. Safe to call from a thread
    /// other than the one that calls kill().
    qint64 read(char* buffer, qint64 n);

    /// Terminate the child (idempotent). Unblocks a pending read().
    void kill();

    /// Wait up to msec for the child to exit. Returns its exit code, or -1
    /// if it is still running (or was never started).
    int waitForExit(int msec);

    /// The last few KB the child wrote to stderr.
    QByteArray stderrTail() const;

    /// Quote arguments into one Windows command line (MSVCRT rules).
    static QString windowsCommandLine(const QString& program, const QStringList& arguments);

private:
    void drainStderr();

    struct Native;
    std::unique_ptr<Native> m_native;
    bool m_started = false;
    std::atomic<bool> m_killed{false};
    std::thread m_stderrThread;
    mutable std::mutex m_tailMutex;
    QByteArray m_tail;
};

} // namespace ttc::media
