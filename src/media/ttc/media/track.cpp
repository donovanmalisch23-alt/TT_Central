// SPDX-License-Identifier: GPL-3.0-or-later
#include "ttc/media/track.h"

#include <QStringList>

namespace ttc::media {

QString Track::label() const
{
    const QString who = artist.trimmed();
    QString name = who.isEmpty() ? title : who + QStringLiteral(" - ") + title;
    if (duration > 0)
        name += QStringLiteral(" [%1]").arg(fmtDuration(duration));
    return name;
}

TrackPtr makeTrack(const Track& t)
{
    return std::make_shared<Track>(t);
}

TrackPtr replayable(const TrackPtr& t, bool force)
{
    auto copy = std::make_shared<Track>(*t);
    static const QStringList resolvable = {QStringLiteral("youtube"), QStringLiteral("soundcloud"),
                                           QStringLiteral("spotify"), QStringLiteral("apple"),
                                           QStringLiteral("web")};
    if (!copy->query.isEmpty() && (force || resolvable.contains(copy->source)))
    {
        copy->needsResolve = true;
        copy->playbackUrl.clear();
        copy->httpHeaders.clear();
    }
    return copy;
}

QString repeatModeName(RepeatMode m)
{
    switch (m)
    {
    case RepeatMode::Off: return QStringLiteral("off");
    case RepeatMode::Track: return QStringLiteral("track");
    case RepeatMode::Queue: return QStringLiteral("queue");
    }
    return {};
}

bool parseRepeatMode(const QString& text, RepeatMode* out)
{
    const QString t = text.trimmed().toLower();
    if (t == QLatin1String("off") || t == QLatin1String("none"))
        *out = RepeatMode::Off;
    else if (t == QLatin1String("track") || t == QLatin1String("one") || t == QLatin1String("song"))
        *out = RepeatMode::Track;
    else if (t == QLatin1String("queue") || t == QLatin1String("all"))
        *out = RepeatMode::Queue;
    else
        return false;
    return true;
}

QString fmtDuration(double seconds)
{
    if (!(seconds > 0))
        return QStringLiteral("?:??");
    const qint64 s = qint64(seconds);
    const qint64 h = s / 3600, m = (s % 3600) / 60, sec = s % 60;
    if (h)
        return QStringLiteral("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0'));
    return QStringLiteral("%1:%2").arg(m).arg(sec, 2, 10, QChar('0'));
}

bool parseTime(const QString& text, double* seconds)
{
    const QStringList parts = text.trimmed().split(QChar(':'));
    if (parts.isEmpty() || parts.size() > 3)
        return false;
    qint64 total = 0;
    for (const QString& p : parts)
    {
        if (p.isEmpty())
            return false;
        bool ok = false;
        const int v = p.toInt(&ok);
        if (!ok || v < 0)
            return false;
        for (QChar c : p)
            if (!c.isDigit())
                return false;
        total = total * 60 + v;
    }
    *seconds = double(total);
    return true;
}

} // namespace ttc::media
