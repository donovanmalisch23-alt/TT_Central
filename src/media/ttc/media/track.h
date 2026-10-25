// TT Central media bot — one playable item, and the repeat modes.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ported from Simple TT Media (MIT, models.py).

#pragma once

#include <QList>
#include <QMap>
#include <QString>

#include <memory>

namespace ttc::media {

/// One playable item. `playbackUrl` is what the decoder opens (a direct media
/// URL or a local path); `query` re-resolves the item later (a page URL or
/// "artist - title"), because stream URLs from services expire.
struct Track
{
    QString title;
    QString playbackUrl;
    QString source = QStringLiteral("unknown");
    QString artist;
    double duration = 0;          // seconds; 0 = unknown
    QString url;                  // human-facing page link
    QString query;                // re-resolvable description
    QString requestedBy;
    QMap<QString, QString> httpHeaders;
    bool needsResolve = false;    // fetch playbackUrl just before playing
    bool isLive = false;          // radio / live streams: no seeking
    int retries = 0;              // re-resolve attempts after errors

    /// "Artist - Title [m:ss]".
    QString label() const;
};

using TrackPtr = std::shared_ptr<Track>;
using TrackList = QList<TrackPtr>;

TrackPtr makeTrack(const Track& t);

/// A copy that is safe to queue again. Service stream URLs may have expired,
/// so a re-resolvable track (or any track when `force`) is marked to resolve.
TrackPtr replayable(const TrackPtr& t, bool force = false);

enum class RepeatMode
{
    Off,
    Track,
    Queue,
};

QString repeatModeName(RepeatMode m);
bool parseRepeatMode(const QString& text, RepeatMode* out);

/// "m:ss" or "h:mm:ss"; "?:??" when unknown.
QString fmtDuration(double seconds);

/// Parse "90", "1:30" or "1:02:03" into seconds. False when malformed.
bool parseTime(const QString& text, double* seconds);

} // namespace ttc::media
