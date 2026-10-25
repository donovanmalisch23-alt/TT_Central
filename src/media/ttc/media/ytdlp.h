// TT Central media bot — yt-dlp, the audio provider for the whole bot.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ported from Simple TT Media (MIT, sources/ytdlp.py). yt-dlp turns page URLs
// into direct stream URLs, and it is what Spotify/Apple Music tracks are
// matched against. TT Central runs the yt-dlp executable (fetched by
// scripts/fetch_tools.py) and reads its JSON output; YouTube needs a
// JavaScript runtime, so Deno's path is passed when known.

#pragma once

#include "ttc/media/track.h"

#include <QJsonObject>
#include <QObject>
#include <QQueue>

#include <functional>

namespace ttc::media {

class YtDlpRunner final : public QObject
{
    Q_OBJECT
public:
    using Callback = std::function<void(const QJsonObject& info, const QString& error)>;

    explicit YtDlpRunner(QObject* parent = nullptr);

    void setProgram(const QString& path) { m_program = path; }
    QString program() const { return m_program; }
    void setDenoPath(const QString& path) { m_deno = path; }
    QString denoPath() const { return m_deno; }
    void setTimeoutMs(int ms) { m_timeoutMs = ms; }
    void setMaxConcurrent(int n) { m_maxConcurrent = qMax(1, n); }

    /// Run yt-dlp on `target` (a URL, or "ytsearch5:words"). `flat` lists a
    /// playlist's entries without resolving each one.
    void extract(const QString& target, bool flat, int playlistEnd, Callback cb);
    /// Run "yt-dlp -U"; the callback gets its output.
    void update(std::function<void(bool ok, const QString& output)> cb);

    static QStringList arguments(const QString& target, bool flat, int playlistEnd, const QString& denoPath,
                                 int socketTimeoutSeconds = 20);
    /// A short, user-facing version of a yt-dlp error.
    static QString friendlyError(const QString& message);

private:
    struct Job
    {
        QStringList args;
        Callback cb;
    };
    void pump();
    void run(Job job);

    QString m_program = QStringLiteral("yt-dlp");
    QString m_deno;
    int m_timeoutMs = 60000;
    int m_maxConcurrent = 3;
    int m_running = 0;
    QQueue<Job> m_waiting;
};

/// The URL of a flat entry (search result or playlist item).
QString entryUrl(const QJsonObject& entry, const QString& source);
/// A playable-now track from a full info dict.
TrackPtr trackFromInfo(const QJsonObject& info, const QString& source, const QString& requester);
/// A resolve-later track from a flat entry.
TrackPtr trackFromEntry(const QJsonObject& entry, const QString& source, const QString& requester);
/// Copy the stream fields of a full info dict into `t` (url, headers, ...).
bool applyStreamInfo(const QJsonObject& info, Track& t);
/// Pick the entry whose duration is closest to `target` (first when unknown).
QJsonObject bestByDuration(const QList<QJsonObject>& entries, double target);
/// The lower-cased host of a URL ("" when not a URL).
QString urlHost(const QString& url);

} // namespace ttc::media
