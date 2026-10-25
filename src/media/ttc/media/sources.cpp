// SPDX-License-Identifier: GPL-3.0-or-later
#include "ttc/media/sources.h"

#include <QDateTime>
#include <QFileInfo>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

namespace ttc::media {

namespace {

const char* const kUserAgent =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0 Safari/537.36";

QString tr(const char* s)
{
    return QObject::tr(s);
}

} // namespace

// ---------------------------------------------------------------------------
// HttpJson

HttpJson::HttpJson(QObject* parent) : QObject(parent), m_nam(new QNetworkAccessManager(this)) {}

static void handleReply(QNetworkReply* reply, HttpJson::Callback cb)
{
    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, cb] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 404)
            return cb({}, tr("That link was not found (404)."));
        if (status == 401 || status == 403)
            return cb({}, tr("The service rejected the request; check the API credentials in the media bot settings."));
        if (status >= 400)
            return cb({}, tr("The service returned HTTP %1.").arg(status));
        if (reply->error() != QNetworkReply::NoError)
            return cb({}, tr("Couldn't reach the service. Check the internet connection."));
        QJsonParseError pe;
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll(), &pe);
        if (pe.error != QJsonParseError::NoError)
            return cb({}, tr("The service sent an unexpected response."));
        cb(doc, {});
    });
}

static QNetworkRequest makeRequest(const QString& url, const QHash<QByteArray, QByteArray>& headers)
{
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, QByteArray(kUserAgent));
    req.setRawHeader("Accept-Language", "en-US,en;q=0.8");
    for (auto it = headers.cbegin(); it != headers.cend(); ++it)
        req.setRawHeader(it.key(), it.value());
    req.setTransferTimeout(15000);
    return req;
}

void HttpJson::get(const QString& url, const QHash<QByteArray, QByteArray>& headers, Callback cb)
{
    handleReply(m_nam->get(makeRequest(url, headers)), std::move(cb));
}

void HttpJson::post(const QString& url, const QHash<QByteArray, QByteArray>& headers, const QByteArray& body,
                    Callback cb)
{
    handleReply(m_nam->post(makeRequest(url, headers), body), std::move(cb));
}

// ---------------------------------------------------------------------------
// Source defaults

void Source::search(const QString&, int, const QString&, TracksCallback cb)
{
    cb({}, tr("%1 can't search.").arg(title()));
}

void Source::resolve(const TrackPtr&, ErrorCallback cb)
{
    cb(tr("%1 can't play its own tracks.").arg(title()));
}

// ---------------------------------------------------------------------------
// yt-dlp sources

YtDlpSource::YtDlpSource(YtDlpRunner* runner, QString name, QString title, QStringList hosts,
                         QString searchPrefix, int maxPlaylist)
    : m_runner(runner)
    , m_name(std::move(name))
    , m_title(std::move(title))
    , m_hosts(std::move(hosts))
    , m_searchPrefix(std::move(searchPrefix))
    , m_maxPlaylist(maxPlaylist)
{
}

bool YtDlpSource::matches(const QString& text) const
{
    const QString h = urlHost(text);
    if (h.isEmpty())
        return false;
    for (const QString& x : m_hosts)
        if (h == x || h.endsWith(QChar('.') + x))
            return true;
    return false;
}

void YtDlpSource::fromUrl(const QString& url, const QString& requester, TracksCallback cb)
{
    const QString name = m_name;
    const int max = m_maxPlaylist;
    m_runner->extract(url.trimmed(), true, max, [name, max, requester, cb](const QJsonObject& info, const QString& err) {
        if (!err.isEmpty())
            return cb({}, err);
        if (info.contains(QStringLiteral("entries")))
        {
            TrackList tracks;
            for (const QJsonValue& v : info.value(QStringLiteral("entries")).toArray())
            {
                if (!v.isObject())
                    continue;
                TrackPtr t = trackFromEntry(v.toObject(), name, requester);
                if (!t->query.isEmpty())
                    tracks << t;
                if (tracks.size() >= max)
                    break;
            }
            if (tracks.isEmpty())
                return cb({}, tr("That playlist is empty or unavailable."));
            return cb(tracks, {});
        }
        // A single item: yt-dlp ran with --flat-playlist, which still resolves
        // single videos fully.
        TrackPtr t = trackFromInfo(info, name, requester);
        if (t->playbackUrl.isEmpty())
        {
            if (t->query.isEmpty())
                return cb({}, tr("No playable audio stream was found."));
            t->needsResolve = true;
        }
        cb({t}, {});
    });
}

void YtDlpSource::search(const QString& query, int limit, const QString& requester, TracksCallback cb)
{
    limit = qBound(1, limit, 50);
    const QString name = m_name;
    m_runner->extract(QStringLiteral("%1%2:%3").arg(m_searchPrefix).arg(limit).arg(query), true, 0,
                      [name, requester, cb](const QJsonObject& info, const QString& err) {
                          if (!err.isEmpty())
                              return cb({}, err);
                          TrackList tracks;
                          for (const QJsonValue& v : info.value(QStringLiteral("entries")).toArray())
                          {
                              if (!v.isObject())
                                  continue;
                              TrackPtr t = trackFromEntry(v.toObject(), name, requester);
                              if (!t->query.isEmpty())
                                  tracks << t;
                          }
                          cb(tracks, {});
                      });
}

void YtDlpSource::resolve(const TrackPtr& t, ErrorCallback cb)
{
    const QString target = t->query.isEmpty() ? t->url : t->query;
    if (target.isEmpty())
        return cb(tr("This track has nothing to resolve."));
    YtDlpRunner* runner = m_runner;
    const QString name = m_name;
    runner->extract(target, false, 0, [t, cb, runner, name](const QJsonObject& info, const QString& err) {
        if (!err.isEmpty())
            return cb(err);
        QJsonObject chosen = info;
        if (info.contains(QStringLiteral("entries")))
        {
            const QJsonArray entries = info.value(QStringLiteral("entries")).toArray();
            QJsonObject first;
            for (const QJsonValue& v : entries)
                if (v.isObject())
                {
                    first = v.toObject();
                    break;
                }
            if (first.isEmpty())
                return cb(tr("Nothing playable was found."));
            if (first.value(QStringLiteral("url")).toString().isEmpty() ||
                !first.contains(QStringLiteral("formats")))
            {
                // A flat entry: resolve its page.
                runner->extract(entryUrl(first, name), false, 0, [t, cb](const QJsonObject& i2, const QString& e2) {
                    if (!e2.isEmpty())
                        return cb(e2);
                    cb(applyStreamInfo(i2, *t) ? QString() : tr("No playable audio stream was found."));
                });
                return;
            }
            chosen = first;
        }
        cb(applyStreamInfo(chosen, *t) ? QString() : tr("No playable audio stream was found."));
    });
}

GenericSiteSource::GenericSiteSource(YtDlpRunner* runner, int maxPlaylist)
    : YtDlpSource(runner, QStringLiteral("web"), QObject::tr("Web"), {}, {}, maxPlaylist)
{
}

bool GenericSiteSource::matches(const QString& text) const
{
    const QString t = text.trimmed().toLower();
    return t.startsWith(QLatin1String("http://")) || t.startsWith(QLatin1String("https://"));
}

// ---------------------------------------------------------------------------
// Direct URLs and radio

namespace {
const QStringList kMediaExts = {QStringLiteral(".mp3"),  QStringLiteral(".aac"),  QStringLiteral(".m4a"),
                                QStringLiteral(".ogg"),  QStringLiteral(".oga"),  QStringLiteral(".opus"),
                                QStringLiteral(".flac"), QStringLiteral(".wav"),  QStringLiteral(".wma"),
                                QStringLiteral(".mp4"),  QStringLiteral(".m4b"),  QStringLiteral(".webm"),
                                QStringLiteral(".mka")};
const QStringList kStreamHints = {QStringLiteral(".m3u8"), QStringLiteral(".pls"), QStringLiteral(".m3u"),
                                  QStringLiteral("/stream"), QStringLiteral("icecast"),
                                  QStringLiteral("shoutcast"), QStringLiteral(":8000")};

QString extensionOf(const QString& path)
{
    const QString base = path.section(QChar('/'), -1);
    const int dot = base.lastIndexOf(QChar('.'));
    return dot < 0 ? QString() : base.mid(dot).toLower();
}
} // namespace

bool DirectUrlSource::matches(const QString& text) const
{
    const QString t = text.trimmed().toLower();
    if (!t.startsWith(QLatin1String("http://")) && !t.startsWith(QLatin1String("https://")))
        return false;
    const QString path = QUrl(t).path();
    if (kMediaExts.contains(extensionOf(path)))
        return true;
    for (const QString& h : kStreamHints)
        if (t.contains(h))
            return true;
    // Radio URLs often carry the codec as a token, e.g. ".../stream-128-mp3".
    const QString base = path.section(QChar('/'), -1);
    for (const char* tag : {"mp3", "aac", "ogg", "opus", "flac"})
        if (base.contains(QLatin1String(tag)))
            return true;
    return false;
}

TrackPtr DirectUrlSource::trackFor(const QString& url, const QString& requester)
{
    const QString u = url.trimmed();
    const QUrl parsed(u);
    const QString path = parsed.path();
    QString name = QUrl::fromPercentEncoding(path.section(QChar('/'), -1).toUtf8());
    if (name.isEmpty())
        name = parsed.host().isEmpty() ? u : parsed.host();
    Track t;
    t.title = name;
    t.playbackUrl = u;
    t.url = u;
    t.query = u;
    t.source = QStringLiteral("url");
    t.requestedBy = requester;
    t.isLive = !kMediaExts.contains(extensionOf(path)); // radio/HLS: no reliable duration or seeking
    return makeTrack(t);
}

void DirectUrlSource::fromUrl(const QString& url, const QString& requester, TracksCallback cb)
{
    cb({trackFor(url, requester)}, {});
}

void DirectUrlSource::resolve(const TrackPtr& t, ErrorCallback cb)
{
    if (t->playbackUrl.isEmpty())
    {
        if (t->query.isEmpty())
            return cb(tr("This track has no URL to play."));
        t->playbackUrl = t->query;
    }
    t->needsResolve = false;
    cb({});
}

// ---------------------------------------------------------------------------
// Spotify

namespace {
const QRegularExpression kSpotifyRe(
    QStringLiteral("(?:open\\.spotify\\.com/(?:intl-[a-z]{2}(?:-[a-z]{2})?/)?|spotify:)"
                   "(track|album|playlist)[/:]([A-Za-z0-9]{10,40})"));
const char* const kSpotifyApi = "https://api.spotify.com/v1";
} // namespace

SpotifySource::SpotifySource(HttpJson* http, QString clientId, QString clientSecret, int maxTracks)
    : m_http(http)
    , m_id(clientId.trimmed())
    , m_secret(clientSecret.trimmed())
    , m_max(maxTracks)
{
}

QPair<QString, QString> SpotifySource::parse(const QString& text)
{
    const auto m = kSpotifyRe.match(text.trimmed());
    if (!m.hasMatch())
        return {};
    return {m.captured(1), m.captured(2)};
}

bool SpotifySource::matches(const QString& text) const
{
    return !parse(text).first.isEmpty();
}

TrackPtr SpotifySource::trackFromApi(const QJsonObject& item, const QString& requester)
{
    QStringList artists;
    for (const QJsonValue& a : item.value(QStringLiteral("artists")).toArray())
    {
        const QString n = a.toObject().value(QStringLiteral("name")).toString();
        if (!n.isEmpty())
            artists << n;
    }
    Track t;
    t.title = item.value(QStringLiteral("name")).toString();
    t.artist = artists.join(QStringLiteral(", "));
    const double ms = item.value(QStringLiteral("duration_ms")).toDouble();
    t.duration = ms > 0 ? ms / 1000.0 : 0;
    t.url = item.value(QStringLiteral("external_urls")).toObject().value(QStringLiteral("spotify")).toString();
    t.query = t.artist.isEmpty() ? t.title : t.artist + QStringLiteral(" - ") + t.title;
    t.source = QStringLiteral("spotify");
    t.requestedBy = requester;
    t.needsResolve = true;
    return makeTrack(t);
}

void SpotifySource::withToken(std::function<void(const QString&, const QString&)> cb)
{
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (!m_token.isEmpty() && now < m_tokenExpiry - 60)
        return cb(m_token, {});
    const QByteArray auth = (m_id + QChar(':') + m_secret).toUtf8().toBase64();
    m_http->post(QStringLiteral("https://accounts.spotify.com/api/token"),
                 {{"Authorization", "Basic " + auth}, {"Content-Type", "application/x-www-form-urlencoded"}},
                 "grant_type=client_credentials", [this, cb](const QJsonDocument& doc, const QString& err) {
                     if (!err.isEmpty())
                         return cb({}, err);
                     m_token = doc.object().value(QStringLiteral("access_token")).toString();
                     m_tokenExpiry = QDateTime::currentSecsSinceEpoch() +
                                     doc.object().value(QStringLiteral("expires_in")).toInt(3600);
                     if (m_token.isEmpty())
                         return cb({}, tr("Spotify did not return an access token; check the client ID and secret."));
                     cb(m_token, {});
                 });
}

void SpotifySource::paged(const QString& url, const QString& token, std::shared_ptr<QList<QJsonObject>> items,
                          std::function<void(const QString&)> done)
{
    if (url.isEmpty() || items->size() >= m_max)
        return done({});
    m_http->get(url, {{"Authorization", "Bearer " + token.toUtf8()}},
                [this, token, items, done](const QJsonDocument& doc, const QString& err) {
                    if (!err.isEmpty())
                        return done(err);
                    for (const QJsonValue& v : doc.object().value(QStringLiteral("items")).toArray())
                        if (v.isObject())
                            items->append(v.toObject());
                    paged(doc.object().value(QStringLiteral("next")).toString(), token, items, done);
                });
}

void SpotifySource::fromUrl(const QString& url, const QString& requester, TracksCallback cb)
{
    const auto [kind, id] = parse(url);
    if (kind.isEmpty())
        return cb({}, tr("That doesn't look like a Spotify link."));
    if (!hasCredentials())
    {
        if (kind != QLatin1String("track"))
            return cb({}, tr("Spotify albums and playlists need a free Spotify developer app. Add its client ID "
                             "and secret in TT Central's media bot settings."));
        // Single tracks work without credentials through the public oEmbed API.
        const QString page = QStringLiteral("https://open.spotify.com/track/") + id;
        m_http->get(QStringLiteral("https://open.spotify.com/oembed?url=") +
                        QString::fromUtf8(QUrl::toPercentEncoding(page)),
                    {}, [page, requester, cb](const QJsonDocument& doc, const QString& err) {
                        if (!err.isEmpty())
                            return cb({}, err);
                        const QString title = doc.object().value(QStringLiteral("title")).toString().trimmed();
                        if (title.isEmpty())
                            return cb({}, tr("Spotify didn't return details for that track."));
                        Track t;
                        t.title = title;
                        t.source = QStringLiteral("spotify");
                        t.url = page;
                        t.query = title;
                        t.requestedBy = requester;
                        t.needsResolve = true;
                        cb({makeTrack(t)}, {});
                    });
        return;
    }
    const int max = m_max;
    withToken([this, kind = kind, id = id, requester, cb, max](const QString& token, const QString& err) {
        if (!err.isEmpty())
            return cb({}, err);
        const QString api = QLatin1String(kSpotifyApi);
        if (kind == QLatin1String("track"))
        {
            m_http->get(api + QStringLiteral("/tracks/") + id, {{"Authorization", "Bearer " + token.toUtf8()}},
                        [requester, cb](const QJsonDocument& doc, const QString& e) {
                            if (!e.isEmpty())
                                return cb({}, e);
                            TrackPtr t = trackFromApi(doc.object(), requester);
                            if (t->title.isEmpty())
                                return cb({}, tr("No playable tracks were found in that Spotify link."));
                            cb({t}, {});
                        });
            return;
        }
        auto items = std::make_shared<QList<QJsonObject>>();
        const bool album = kind == QLatin1String("album");
        const QString first = album ? api + QStringLiteral("/albums/") + id + QStringLiteral("/tracks?limit=50")
                                    : api + QStringLiteral("/playlists/") + id + QStringLiteral("/tracks?limit=100");
        paged(first, token, items, [items, album, requester, cb, max](const QString& e) {
            if (!e.isEmpty())
                return cb({}, e);
            TrackList tracks;
            for (const QJsonObject& it : *items)
            {
                const QJsonObject track = album ? it : it.value(QStringLiteral("track")).toObject();
                if (track.value(QStringLiteral("name")).toString().isEmpty())
                    continue;
                tracks << trackFromApi(track, requester);
                if (tracks.size() >= max)
                    break;
            }
            if (tracks.isEmpty())
                return cb({}, tr("No playable tracks were found in that Spotify link."));
            cb(tracks, {});
        });
    });
}

// ---------------------------------------------------------------------------
// Apple Music

namespace {
const QRegularExpression kAppleCountry(QStringLiteral("music\\.apple\\.com/([a-z]{2})/"),
                                       QRegularExpression::CaseInsensitiveOption);
const QRegularExpression kAppleSongParam(QStringLiteral("[?&]i=(\\d+)"));
const QRegularExpression kAppleAlbum(QStringLiteral("/album/(?:[^/?#]+/)?(\\d+)"));
const QRegularExpression kAppleSong(QStringLiteral("/song/(?:[^/?#]+/)?(\\d+)"));
} // namespace

AppleMusicSource::AppleMusicSource(HttpJson* http, int maxTracks) : m_http(http), m_max(maxTracks) {}

bool AppleMusicSource::matches(const QString& text) const
{
    const QString t = text.toLower();
    return t.contains(QLatin1String("music.apple.com/")) || t.contains(QLatin1String("itunes.apple.com/"));
}

QString AppleMusicSource::country(const QString& url)
{
    const auto m = kAppleCountry.match(url);
    return m.hasMatch() ? m.captured(1).toLower() : QStringLiteral("us");
}

TrackPtr AppleMusicSource::trackFromResult(const QJsonObject& r, const QString& requester)
{
    Track t;
    t.title = r.value(QStringLiteral("trackName")).toString();
    t.artist = r.value(QStringLiteral("artistName")).toString();
    const double ms = r.value(QStringLiteral("trackTimeMillis")).toDouble();
    t.duration = ms > 0 ? ms / 1000.0 : 0;
    t.url = r.value(QStringLiteral("trackViewUrl")).toString();
    t.query = t.artist.isEmpty() ? t.title : t.artist + QStringLiteral(" - ") + t.title;
    t.source = QStringLiteral("apple");
    t.requestedBy = requester;
    t.needsResolve = true;
    return makeTrack(t);
}

void AppleMusicSource::fromUrl(const QString& url, const QString& requester, TracksCallback cb)
{
    if (url.toLower().contains(QLatin1String("/playlist/")))
        return cb({}, tr("Apple Music playlists aren't supported (Apple only exposes them to its developer API). "
                         "Song and album links work."));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("country"), country(url));
    auto song = kAppleSongParam.match(url);
    if (!song.hasMatch())
        song = kAppleSong.match(url);
    if (song.hasMatch())
    {
        q.addQueryItem(QStringLiteral("id"), song.captured(1));
    }
    else
    {
        const auto album = kAppleAlbum.match(url);
        if (!album.hasMatch())
            return cb({}, tr("That Apple Music link isn't a song or album."));
        q.addQueryItem(QStringLiteral("id"), album.captured(1));
        q.addQueryItem(QStringLiteral("entity"), QStringLiteral("song"));
        q.addQueryItem(QStringLiteral("limit"), QStringLiteral("200"));
    }
    const int max = m_max;
    m_http->get(QStringLiteral("https://itunes.apple.com/lookup?") + q.toString(QUrl::FullyEncoded), {},
                [requester, cb, max](const QJsonDocument& doc, const QString& err) {
                    if (!err.isEmpty())
                        return cb({}, err);
                    TrackList tracks;
                    for (const QJsonValue& v : doc.object().value(QStringLiteral("results")).toArray())
                    {
                        const QJsonObject r = v.toObject();
                        if (r.value(QStringLiteral("wrapperType")).toString() == QLatin1String("track") ||
                            r.value(QStringLiteral("kind")).toString() == QLatin1String("song"))
                            tracks << trackFromResult(r, requester);
                        if (tracks.size() >= max)
                            break;
                    }
                    if (tracks.isEmpty())
                        return cb({}, tr("Apple Music returned no tracks for that link."));
                    cb(tracks, {});
                });
}

void AppleMusicSource::search(const QString& query, int limit, const QString& requester, TracksCallback cb)
{
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("term"), query);
    q.addQueryItem(QStringLiteral("entity"), QStringLiteral("song"));
    q.addQueryItem(QStringLiteral("limit"), QString::number(qBound(1, limit, 50)));
    m_http->get(QStringLiteral("https://itunes.apple.com/search?") + q.toString(QUrl::FullyEncoded), {},
                [requester, cb](const QJsonDocument& doc, const QString& err) {
                    if (!err.isEmpty())
                        return cb({}, err);
                    TrackList tracks;
                    for (const QJsonValue& v : doc.object().value(QStringLiteral("results")).toArray())
                    {
                        const QJsonObject r = v.toObject();
                        if (!r.value(QStringLiteral("trackName")).toString().isEmpty())
                            tracks << trackFromResult(r, requester);
                    }
                    cb(tracks, {});
                });
}

// ---------------------------------------------------------------------------
// Registry

SourceRegistry::SourceRegistry(const SourcesSettings& settings, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_runner(new YtDlpRunner(this))
    , m_http(new HttpJson(this))
{
    if (!settings.ytdlpPath.isEmpty())
        m_runner->setProgram(settings.ytdlpPath);
    m_runner->setDenoPath(settings.denoPath);
    const int maxPl = qMax(1, settings.maxPlaylistTracks);

    auto own = [this](Source* s) {
        m_owned.emplace_back(s);
        return s;
    };
    if (settings.youtube)
    {
        m_youtube = own(new YtDlpSource(m_runner, QStringLiteral("youtube"), QStringLiteral("YouTube"),
                                        {QStringLiteral("youtube.com"), QStringLiteral("youtu.be"),
                                         QStringLiteral("youtube-nocookie.com")},
                                        QStringLiteral("ytsearch"), maxPl));
        m_urlSources << m_youtube;
        m_searchable << qMakePair(QStringLiteral("youtube"), m_youtube);
    }
    if (settings.soundcloud)
    {
        Source* sc = own(new YtDlpSource(m_runner, QStringLiteral("soundcloud"), QStringLiteral("SoundCloud"),
                                         {QStringLiteral("soundcloud.com"), QStringLiteral("snd.sc")},
                                         QStringLiteral("scsearch"), maxPl));
        m_urlSources << sc;
        m_searchable << qMakePair(QStringLiteral("soundcloud"), sc);
    }
    if (settings.spotify)
        m_urlSources << own(new SpotifySource(m_http, settings.spotifyClientId, settings.spotifyClientSecret, maxPl));
    if (settings.appleMusic)
    {
        Source* am = own(new AppleMusicSource(m_http, maxPl));
        m_urlSources << am;
        m_searchable << qMakePair(QStringLiteral("apple"), am);
    }
    if (settings.directUrl)
        m_urlSources << own(new DirectUrlSource());
    // The generic yt-dlp fallback claims any other http(s) URL: keep it last.
    m_urlSources << own(new GenericSiteSource(m_runner, maxPl));

    m_service = m_searchable.isEmpty() ? QString() : m_searchable.first().first;
}

SourceRegistry::~SourceRegistry() = default;

QStringList SourceRegistry::services() const
{
    QStringList names;
    for (const auto& p : m_searchable)
        names << p.first;
    return names;
}

bool SourceRegistry::setService(const QString& name, QString* error)
{
    const QString n = name.trimmed().toLower();
    QString canonical = n;
    if (n == QLatin1String("yt"))
        canonical = QStringLiteral("youtube");
    else if (n == QLatin1String("sc"))
        canonical = QStringLiteral("soundcloud");
    else if (n == QLatin1String("am") || n == QLatin1String("applemusic") || n == QLatin1String("itunes"))
        canonical = QStringLiteral("apple");
    if (!services().contains(canonical))
    {
        if (error)
            *error = tr("Unknown service '%1'. Available: %2.")
                         .arg(n, services().isEmpty() ? tr("none") : services().join(QStringLiteral(", ")));
        return false;
    }
    m_service = canonical;
    return true;
}

QStringList SourceRegistry::enabledNames() const
{
    QStringList seen;
    for (Source* s : m_urlSources)
        if (!seen.contains(s->name()) && s->name() != QLatin1String("web"))
            seen << s->name();
    return seen;
}

bool SourceRegistry::isUrl(const QString& text) const
{
    const QString t = text.trimmed().toLower();
    return t.startsWith(QLatin1String("http://")) || t.startsWith(QLatin1String("https://")) ||
           t.startsWith(QLatin1String("spotify:"));
}

Source* SourceRegistry::sourceForUrl(const QString& url) const
{
    for (Source* s : m_urlSources)
        if (s->matches(url))
            return s;
    return nullptr;
}

void SourceRegistry::fromInput(const QString& text, const QString& requester, TracksCallback cb)
{
    const QString t = text.trimmed();
    if (t.isEmpty())
        return cb({}, tr("Nothing to play."));
    if (isUrl(t))
    {
        if (Source* s = sourceForUrl(t))
            return s->fromUrl(t, requester, cb);
        return cb({}, tr("No source could handle that link."));
    }
    search(t, requester, 0, cb);
}

void SourceRegistry::playDirect(const QString& url, const QString& requester, TracksCallback cb)
{
    for (Source* s : m_urlSources)
        if (s->name() == QLatin1String("url"))
            return s->fromUrl(url, requester, cb);
    cb({}, tr("Direct URL playback is turned off in the media bot settings."));
}

void SourceRegistry::search(const QString& query, const QString& requester, int limit, TracksCallback cb)
{
    Source* src = nullptr;
    QString title;
    for (const auto& p : m_searchable)
        if (p.first == m_service)
        {
            src = p.second;
            title = p.second->title();
        }
    if (!src)
        return cb({}, tr("No search service is enabled."));
    const int n = limit > 0 ? limit : qMax(1, m_settings.maxSearchResults);
    src->search(query, n, requester, [cb, title, query](const TrackList& tracks, const QString& err) {
        if (!err.isEmpty())
            return cb({}, err);
        if (tracks.isEmpty())
            return cb({}, tr("Nothing found on %1 for: %2").arg(title, query));
        cb(tracks, {});
    });
}

void SourceRegistry::updateTools(std::function<void(bool, const QString&)> cb)
{
    m_runner->update(std::move(cb));
}

void SourceRegistry::resolve(const TrackPtr& t, std::function<void(bool)> done)
{
    if (!t->needsResolve && !t->playbackUrl.isEmpty())
        return done(true);
    // One resolution per track at a time; later callers wait for it.
    auto it = m_inflight.find(t.get());
    if (it != m_inflight.end())
    {
        it->append(std::move(done));
        return;
    }
    m_inflight.insert(t.get(), {std::move(done)});
    QPointer<SourceRegistry> self(this);
    auto finished = [self, t](const QString& err) {
        if (self)
            self->finishResolve(t, err.isEmpty() && !t->playbackUrl.isEmpty());
    };
    Source* src = nullptr;
    for (Source* s : m_urlSources)
        if (s->name() == t->source && s->canResolve())
            src = s;
    if (src)
        src->resolve(t, finished);
    else
        resolveViaYouTube(t, finished); // metadata-only (Spotify/Apple) or unknown
}

void SourceRegistry::finishResolve(const TrackPtr& t, bool ok)
{
    const QList<std::function<void(bool)>> waiting = m_inflight.take(t.get());
    for (const auto& cb : waiting)
        cb(ok);
}

void SourceRegistry::resolveViaYouTube(const TrackPtr& t, ErrorCallback cb)
{
    if (!m_youtube)
        return cb(tr("Can't play %1 tracks while YouTube is turned off.").arg(t->source));
    const QString query = !t->query.isEmpty() ? t->query
                          : t->artist.isEmpty() ? t->title
                                                : t->artist + QStringLiteral(" - ") + t->title;
    m_runner->extract(QStringLiteral("ytsearch5:") + query, false, 0, [t, cb](const QJsonObject& info,
                                                                              const QString& err) {
        if (!err.isEmpty())
            return cb(err);
        QList<QJsonObject> entries;
        for (const QJsonValue& v : info.value(QStringLiteral("entries")).toArray())
            if (v.isObject() && !v.toObject().value(QStringLiteral("url")).toString().isEmpty())
                entries << v.toObject();
        if (entries.isEmpty())
            return cb(tr("No match was found on YouTube."));
        const QJsonObject best = bestByDuration(entries, t->duration);
        const QString page = best.value(QStringLiteral("webpage_url")).toString();
        if (!applyStreamInfo(best, *t))
            return cb(tr("No playable audio stream was found."));
        if (t->url.isEmpty())
            t->url = page;
        cb({});
    });
}

} // namespace ttc::media
