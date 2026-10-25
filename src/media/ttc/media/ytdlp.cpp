// SPDX-License-Identifier: GPL-3.0-or-later
#include "ttc/media/ytdlp.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>
#include <QUrl>

#include <cmath>
#include <limits>

namespace ttc::media {

YtDlpRunner::YtDlpRunner(QObject* parent) : QObject(parent) {}

QStringList YtDlpRunner::arguments(const QString& target, bool flat, int playlistEnd, const QString& denoPath,
                                   int socketTimeoutSeconds)
{
    QStringList a{QStringLiteral("--ignore-config"), QStringLiteral("-J"), QStringLiteral("--no-warnings"),
                  QStringLiteral("--no-progress"), QStringLiteral("--no-cache-dir"),
                  QStringLiteral("--encoding"), QStringLiteral("utf-8"),
                  QStringLiteral("--socket-timeout"), QString::number(socketTimeoutSeconds),
                  QStringLiteral("-f"), QStringLiteral("bestaudio/best"), QStringLiteral("--no-playlist")};
    if (flat)
        a << QStringLiteral("--flat-playlist");
    if (playlistEnd > 0)
        a << QStringLiteral("--playlist-end") << QString::number(playlistEnd);
    if (!denoPath.isEmpty())
        a << QStringLiteral("--js-runtimes") << (QStringLiteral("deno:") + denoPath);
    a << QStringLiteral("--") << target;
    return a;
}

QString YtDlpRunner::friendlyError(const QString& message)
{
    QString msg = message;
    msg.replace(QStringLiteral("ERROR: "), QString());
    msg = msg.trimmed();
    const QString low = msg.toLower();
    if (low.contains(QLatin1String("unsupported url")))
        return tr("That link isn't supported.");
    if (low.contains(QLatin1String("private")) || low.contains(QLatin1String("unavailable")) ||
        low.contains(QLatin1String("sign in")))
        return tr("That item is private, restricted, or unavailable.");
    if (low.contains(QLatin1String("http error 403")))
        return tr("The service refused the request (HTTP 403). Please try again later.");
    if (low.contains(QLatin1String("timed out")) || low.contains(QLatin1String("timeout")))
        return tr("The service took too long to respond. Please try again.");
    if (msg.isEmpty())
        return tr("The service returned an error.");
    // Keep only the last line: yt-dlp prints the reason there.
    const QStringList lines = msg.split(QChar('\n'), Qt::SkipEmptyParts);
    return lines.isEmpty() ? msg.left(300) : lines.last().trimmed().left(300);
}

void YtDlpRunner::extract(const QString& target, bool flat, int playlistEnd, Callback cb)
{
    m_waiting.enqueue(Job{arguments(target, flat, playlistEnd, m_deno), std::move(cb)});
    pump();
}

void YtDlpRunner::pump()
{
    while (m_running < m_maxConcurrent && !m_waiting.isEmpty())
        run(m_waiting.dequeue());
}

void YtDlpRunner::run(Job job)
{
    ++m_running;
    auto* proc = new QProcess(this);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("PYTHONIOENCODING"), QStringLiteral("utf-8"));
    env.insert(QStringLiteral("PYTHONUTF8"), QStringLiteral("1"));
    proc->setProcessEnvironment(env);
    auto* timer = new QTimer(proc);
    timer->setSingleShot(true);
    auto done = std::make_shared<bool>(false);
    auto cb = std::make_shared<Callback>(std::move(job.cb));
    QPointer<YtDlpRunner> self(this);

    auto finish = [self, proc, done, cb](const QJsonObject& info, const QString& error) {
        if (*done)
            return;
        *done = true;
        proc->deleteLater();
        if (self)
        {
            --self->m_running;
            QTimer::singleShot(0, self, [self] {
                if (self)
                    self->pump();
            });
        }
        (*cb)(info, error);
    };

    connect(timer, &QTimer::timeout, proc, [proc, finish] {
        proc->kill();
        finish({}, tr("The service took too long to respond. Please try again."));
    });
    connect(proc, &QProcess::errorOccurred, proc, [proc, finish, this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            finish({}, tr("yt-dlp could not be started (%1). Check the media bot's tool paths.")
                           .arg(m_program));
        (void)proc;
    });
    connect(proc, &QProcess::finished, proc, [proc, finish](int code, QProcess::ExitStatus status) {
        const QByteArray out = proc->readAllStandardOutput();
        const QString err = QString::fromUtf8(proc->readAllStandardError());
        if (status != QProcess::NormalExit)
        {
            finish({}, tr("yt-dlp stopped unexpectedly."));
            return;
        }
        QJsonParseError pe;
        const QJsonDocument doc = QJsonDocument::fromJson(out, &pe);
        if (code != 0 || pe.error != QJsonParseError::NoError || !doc.isObject())
        {
            finish({}, friendlyError(err.isEmpty() ? QString::fromUtf8(out.left(400)) : err));
            return;
        }
        finish(doc.object(), {});
    });
    proc->start(m_program, job.args);
    timer->start(m_timeoutMs);
}

void YtDlpRunner::update(std::function<void(bool, const QString&)> cb)
{
    auto* proc = new QProcess(this);
    proc->setProcessChannelMode(QProcess::MergedChannels);
    auto called = std::make_shared<bool>(false);
    auto call = [proc, cb, called](bool ok, const QString& text) {
        if (*called)
            return;
        *called = true;
        proc->deleteLater();
        cb(ok, text);
    };
    connect(proc, &QProcess::errorOccurred, proc, [call, this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            call(false, tr("yt-dlp could not be started (%1).").arg(m_program));
    });
    connect(proc, &QProcess::finished, proc, [proc, call](int code, QProcess::ExitStatus) {
        const QString text = QString::fromUtf8(proc->readAll()).trimmed();
        call(code == 0, text);
    });
    proc->start(m_program, {QStringLiteral("--ignore-config"), QStringLiteral("-U")});
    QTimer::singleShot(180000, proc, [proc, call] {
        proc->kill();
        call(false, tr("The update took too long."));
    });
}

QString urlHost(const QString& url)
{
    const QUrl u(url.trimmed());
    if (!u.isValid() || u.scheme().isEmpty())
        return {};
    return u.host().toLower();
}

QString entryUrl(const QJsonObject& entry, const QString& source)
{
    QString url = entry.value(QStringLiteral("webpage_url")).toString();
    if (url.isEmpty())
        url = entry.value(QStringLiteral("url")).toString();
    const bool yt = source == QLatin1String("youtube");
    if (!url.isEmpty() && !url.startsWith(QLatin1String("http")) && yt)
        url = QStringLiteral("https://www.youtube.com/watch?v=") + url;
    if (url.isEmpty() && yt && !entry.value(QStringLiteral("id")).toString().isEmpty())
        url = QStringLiteral("https://www.youtube.com/watch?v=") + entry.value(QStringLiteral("id")).toString();
    return url;
}

static QString firstString(const QJsonObject& o, std::initializer_list<const char*> keys)
{
    for (const char* k : keys)
    {
        const QString v = o.value(QLatin1String(k)).toString();
        if (!v.isEmpty())
            return v;
    }
    return {};
}

static bool isLiveInfo(const QJsonObject& o)
{
    return o.value(QStringLiteral("is_live")).toBool() ||
           o.value(QStringLiteral("live_status")).toString() == QLatin1String("is_live");
}

static QMap<QString, QString> headersOf(const QJsonObject& o)
{
    QMap<QString, QString> h;
    const QJsonObject hdr = o.value(QStringLiteral("http_headers")).toObject();
    for (auto it = hdr.begin(); it != hdr.end(); ++it)
        h.insert(it.key(), it.value().toString());
    return h;
}

TrackPtr trackFromInfo(const QJsonObject& info, const QString& source, const QString& requester)
{
    Track t;
    t.title = firstString(info, {"track", "title"});
    if (t.title.isEmpty())
        t.title = QObject::tr("Unknown title");
    t.artist = firstString(info, {"artist", "uploader", "channel"});
    t.playbackUrl = info.value(QStringLiteral("url")).toString();
    t.httpHeaders = headersOf(info);
    t.duration = info.value(QStringLiteral("duration")).toDouble();
    t.url = firstString(info, {"webpage_url", "original_url"});
    t.query = t.url;
    t.source = source;
    t.requestedBy = requester;
    t.isLive = isLiveInfo(info);
    return makeTrack(t);
}

TrackPtr trackFromEntry(const QJsonObject& entry, const QString& source, const QString& requester)
{
    Track t;
    t.title = entry.value(QStringLiteral("title")).toString();
    if (t.title.isEmpty())
        t.title = QObject::tr("Unknown title");
    t.artist = firstString(entry, {"uploader", "channel"});
    t.duration = entry.value(QStringLiteral("duration")).toDouble();
    t.url = entryUrl(entry, source);
    t.query = t.url;
    t.source = source;
    t.requestedBy = requester;
    t.needsResolve = true;
    t.isLive = entry.value(QStringLiteral("live_status")).toString() == QLatin1String("is_live");
    return makeTrack(t);
}

bool applyStreamInfo(const QJsonObject& info, Track& t)
{
    const QString url = info.value(QStringLiteral("url")).toString();
    if (url.isEmpty())
        return false;
    t.playbackUrl = url;
    t.httpHeaders = headersOf(info);
    if (!(t.duration > 0))
        t.duration = info.value(QStringLiteral("duration")).toDouble();
    t.isLive = isLiveInfo(info);
    if (t.url.isEmpty())
        t.url = info.value(QStringLiteral("webpage_url")).toString();
    t.needsResolve = false;
    return true;
}

QJsonObject bestByDuration(const QList<QJsonObject>& entries, double target)
{
    if (entries.isEmpty())
        return {};
    if (!(target > 0))
        return entries.first();
    QJsonObject best = entries.first();
    double bestDiff = std::numeric_limits<double>::max();
    for (const QJsonObject& e : entries)
    {
        const double d = e.value(QStringLiteral("duration")).toDouble();
        const double diff = std::fabs((d > 0 ? d : 1e9) - target);
        if (diff < bestDiff)
        {
            bestDiff = diff;
            best = e;
        }
    }
    return best;
}

} // namespace ttc::media
