// ttc-mediabot — TT Central's media player bot.
// SPDX-License-Identifier: GPL-3.0-or-later
//
//   ttc-mediabot --config mediabot.ini [--control NAME] [--parent-pid PID] [--log FILE]
//
// TT Central starts it as a child process (so a media problem can never take
// the client down) and talks to it over --control. It also runs standalone.

#include "ttc/media/botconfig.h"
#include "ttc/media/controlserver.h"
#include "ttc/media/mediabot.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QTextStream>
#include <QTimer>

#include <csignal>
#include <cstdio>

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <cerrno>
#  include <signal.h>
#endif

using namespace ttc::media;

namespace {

QFile* g_logFile = nullptr;
QMutex g_logMutex;
volatile std::sig_atomic_t g_quit = 0;

void messageHandler(QtMsgType type, const QMessageLogContext&, const QString& msg)
{
    const char* level = type == QtDebugMsg ? "debug" : type == QtInfoMsg ? "info"
                      : type == QtWarningMsg ? "warning" : "error";
    const QByteArray line = QStringLiteral("%1 [%2] %3\n")
                                .arg(QDateTime::currentDateTime().toString(Qt::ISODate), QLatin1String(level), msg)
                                .toUtf8();
    QMutexLocker lock(&g_logMutex);
    std::fwrite(line.constData(), 1, size_t(line.size()), stderr);
    std::fflush(stderr);
    if (g_logFile)
    {
        g_logFile->write(line);
        g_logFile->flush();
    }
}

#if defined(_WIN32)
BOOL WINAPI consoleHandler(DWORD)
{
    g_quit = 1;
    return TRUE;
}
#else
void onSignal(int)
{
    g_quit = 1;
}
#endif

bool processAlive(qint64 pid)
{
#if defined(_WIN32)
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, DWORD(pid));
    if (!h)
        return false;
    const bool alive = WaitForSingleObject(h, 0) == WAIT_TIMEOUT;
    CloseHandle(h);
    return alive;
#else
    return ::kill(pid_t(pid), 0) == 0 || errno == EPERM;
#endif
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("ttc-mediabot"));
    QCoreApplication::setApplicationVersion(MediaBot::version());

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("TT Central media bot: plays music into a TeamTalk channel."));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption configOpt(QStringLiteral("config"), QStringLiteral("Settings file (INI, [mediabot] group)."),
                                 QStringLiteral("file"));
    QCommandLineOption controlOpt(QStringLiteral("control"), QStringLiteral("Local control socket name."),
                                  QStringLiteral("name"));
    QCommandLineOption parentOpt(QStringLiteral("parent-pid"), QStringLiteral("Exit when this process exits."),
                                 QStringLiteral("pid"));
    QCommandLineOption logOpt(QStringLiteral("log"), QStringLiteral("Also write the log to this file."),
                              QStringLiteral("file"));
    QCommandLineOption checkOpt(QStringLiteral("check"),
                                QStringLiteral("Check the settings and helper tools, then exit."));
    parser.addOptions({configOpt, controlOpt, parentOpt, logOpt, checkOpt});
    parser.process(app);

    qInstallMessageHandler(messageHandler);
    if (parser.isSet(logOpt))
    {
        g_logFile = new QFile(parser.value(logOpt));
        if (!g_logFile->open(QIODevice::Append | QIODevice::Text))
        {
            delete g_logFile;
            g_logFile = nullptr;
        }
    }

    if (!parser.isSet(configOpt) || !QFileInfo::exists(parser.value(configOpt)))
    {
        qCritical().noquote() << "A settings file is required: ttc-mediabot --config mediabot.ini";
        return 2;
    }
    QSettings settings(parser.value(configOpt), QSettings::IniFormat);
    MediaBotConfig config = loadMediaBotConfig(settings);

    const QStringList dirs = defaultToolDirs();
    config.ffmpegPath = findTool(config.ffmpegPath, QStringLiteral("ffmpeg"), dirs);
    config.sources.ytdlpPath = findTool(config.sources.ytdlpPath, QStringLiteral("yt-dlp"), dirs);
    config.sources.denoPath = findTool(config.sources.denoPath, QStringLiteral("deno"), dirs);

    const QString err = config.validationError();
    bool toolsOk = true;
    auto report = [&](const char* name, const QString& path, bool required) {
        if (path.isEmpty())
        {
            if (required)
                toolsOk = false;
            qWarning().noquote() << QStringLiteral("%1 was not found%2. Run scripts/fetch_tools.py or set its path "
                                                   "in the media bot settings.")
                                        .arg(QLatin1String(name), required ? QString() : QStringLiteral(" (optional)"));
        }
        else
        {
            qInfo().noquote() << QStringLiteral("%1: %2").arg(QLatin1String(name), path);
        }
    };
    report("ffmpeg", config.ffmpegPath, true);
    report("yt-dlp", config.sources.ytdlpPath, false);
    report("deno", config.sources.denoPath, false);
    if (!err.isEmpty())
    {
        qCritical().noquote() << err;
        return 2;
    }
    if (parser.isSet(checkOpt))
        return toolsOk ? 0 : 3;
    if (config.ffmpegPath.isEmpty())
        config.ffmpegPath = QStringLiteral("ffmpeg");

    MediaBot bot(config, ttc::createSdkClient());
    QObject::connect(&bot, &MediaBot::logLine, [](const QString& line) { qInfo().noquote() << line; });
    QObject::connect(&bot, &MediaBot::shutdownRequested, &app, &QCoreApplication::quit);

    ControlServer* control = nullptr;
    if (parser.isSet(controlOpt))
    {
        control = new ControlServer(&bot, &app);
        QString cerr;
        if (!control->listen(parser.value(controlOpt), &cerr))
        {
            qCritical().noquote() << "Could not open the control connection:" << cerr;
            return 4;
        }
    }

#if defined(_WIN32)
    SetConsoleCtrlHandler(consoleHandler, TRUE);
#else
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
#endif
    const qint64 parentPid = parser.value(parentOpt).toLongLong();
    QTimer watchdog;
    QObject::connect(&watchdog, &QTimer::timeout, &app, [&] {
        if (g_quit || (parentPid > 0 && !processAlive(parentPid)))
        {
            watchdog.stop();
            bot.stop();
            app.quit();
        }
    });
    watchdog.start(500);

    qInfo().noquote() << QStringLiteral("TT Central media bot %1 starting for %2 (search: %3; sources: %4).")
                             .arg(MediaBot::version(), config.displayName(), bot.sources()->service(),
                                  bot.sources()->enabledNames().join(QStringLiteral(", ")));
    bot.start();
    const int rc = app.exec();
    bot.stop();
    delete control;
    return rc;
}
