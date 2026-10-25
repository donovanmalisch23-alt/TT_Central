// TT Central media bot — where tracks come from.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ported from Simple TT Media (MIT, sources/). Routing:
//  * a URL goes to the first source that matches it (services first, then
//    direct media and radio, then any other site yt-dlp understands);
//  * free text is searched on the selected service (YouTube by default);
//  * before playback, resolve() fills a track's stream URL. Spotify and
//    Apple Music only provide track information (their audio is DRM
//    protected), so their tracks play the closest match found on YouTube.
// Everything is asynchronous and runs on the bot's main thread.

#pragma once

#include "ttc/media/player.h"
#include "ttc/media/track.h"
#include "ttc/media/ytdlp.h"

#include <QHash>
#include <QJsonDocument>
#include <QObject>

#include <functional>
#include <memory>
#include <vector>

class QNetworkAccessManager;

namespace ttc::media {

using TracksCallback = std::function<void(const TrackList& tracks, const QString& error)>;
using ErrorCallback = std::function<void(const QString& error)>; // empty = success

/// What the command handlers need (SourceRegistry, or a fake in tests).
class MediaSources
{
public:
    virtual ~MediaSources() = default;
    virtual QString service() const = 0;
    virtual QStringList services() const = 0;
    virtual bool setService(const QString& name, QString* error) = 0;
    virtual bool isUrl(const QString& text) const = 0;
    /// Expand a link into tracks, or search the current service for text.
    virtual void fromInput(const QString& text, const QString& requester, TracksCallback cb) = 0;
    /// Treat a URL as a direct media/radio stream.
    virtual void playDirect(const QString& url, const QString& requester, TracksCallback cb) = 0;
    virtual void search(const QString& query, const QString& requester, int limit, TracksCallback cb) = 0;
    /// Update the downloader (yt-dlp -U).
    virtual void updateTools(std::function<void(bool ok, const QString& output)> cb) = 0;
};

/// Small asynchronous JSON-over-HTTP helper for the metadata services.
class HttpJson final : public QObject
{
    Q_OBJECT
public:
    explicit HttpJson(QObject* parent = nullptr);
    using Callback = std::function<void(const QJsonDocument& doc, const QString& error)>;
    void get(const QString& url, const QHash<QByteArray, QByteArray>& headers, Callback cb);
    void post(const QString& url, const QHash<QByteArray, QByteArray>& headers, const QByteArray& body, Callback cb);

private:
    QNetworkAccessManager* m_nam;
};

class Source
{
public:
    virtual ~Source() = default;
    virtual QString name() const = 0;
    virtual QString title() const = 0;
    virtual bool searchable() const { return false; }
    /// True when this source handles `text` (a URL).
    virtual bool matches(const QString& text) const = 0;
    virtual void fromUrl(const QString& url, const QString& requester, TracksCallback cb) = 0;
    virtual void search(const QString& query, int limit, const QString& requester, TracksCallback cb);
    /// Fill t->playbackUrl. Only sources that can play their own tracks.
    virtual bool canResolve() const { return false; }
    virtual void resolve(const TrackPtr& t, ErrorCallback cb);
};

/// YouTube, SoundCloud and any other site yt-dlp understands.
class YtDlpSource : public Source
{
public:
    YtDlpSource(YtDlpRunner* runner, QString name, QString title, QStringList hosts, QString searchPrefix,
                int maxPlaylist);
    QString name() const override { return m_name; }
    QString title() const override { return m_title; }
    bool searchable() const override { return !m_searchPrefix.isEmpty(); }
    bool matches(const QString& text) const override;
    void fromUrl(const QString& url, const QString& requester, TracksCallback cb) override;
    void search(const QString& query, int limit, const QString& requester, TracksCallback cb) override;
    bool canResolve() const override { return true; }
    void resolve(const TrackPtr& t, ErrorCallback cb) override;

protected:
    YtDlpRunner* m_runner;
    QString m_name, m_title;
    QStringList m_hosts;
    QString m_searchPrefix;
    int m_maxPlaylist;
};

/// Any other http(s) URL (Bandcamp, Vimeo, Mixcloud, ...). Keep it last.
class GenericSiteSource final : public YtDlpSource
{
public:
    GenericSiteSource(YtDlpRunner* runner, int maxPlaylist);
    bool matches(const QString& text) const override;
};

/// Direct media files and internet radio (anything ffmpeg can open).
class DirectUrlSource final : public Source
{
public:
    QString name() const override { return QStringLiteral("url"); }
    QString title() const override { return QObject::tr("Direct URL / radio"); }
    bool matches(const QString& text) const override;
    void fromUrl(const QString& url, const QString& requester, TracksCallback cb) override;
    bool canResolve() const override { return true; }
    void resolve(const TrackPtr& t, ErrorCallback cb) override;
    /// The track a direct link becomes (synchronous; for tests).
    static TrackPtr trackFor(const QString& url, const QString& requester);
};

/// Spotify links (track information only).
class SpotifySource final : public Source
{
public:
    SpotifySource(HttpJson* http, QString clientId, QString clientSecret, int maxTracks);
    QString name() const override { return QStringLiteral("spotify"); }
    QString title() const override { return QStringLiteral("Spotify"); }
    bool matches(const QString& text) const override;
    void fromUrl(const QString& url, const QString& requester, TracksCallback cb) override;
    bool hasCredentials() const { return !m_id.isEmpty() && !m_secret.isEmpty(); }

    /// ("track"|"album"|"playlist", id), or empty strings when not Spotify.
    static QPair<QString, QString> parse(const QString& text);
    static TrackPtr trackFromApi(const QJsonObject& item, const QString& requester);

private:
    void withToken(std::function<void(const QString& token, const QString& error)> cb);
    void paged(const QString& url, const QString& token, std::shared_ptr<QList<QJsonObject>> items,
               std::function<void(const QString& error)> done);

    HttpJson* m_http;
    QString m_id, m_secret;
    int m_max;
    QString m_token;
    qint64 m_tokenExpiry = 0;
};

/// Apple Music song/album links and search, via Apple's public iTunes API.
class AppleMusicSource final : public Source
{
public:
    AppleMusicSource(HttpJson* http, int maxTracks);
    QString name() const override { return QStringLiteral("apple"); }
    QString title() const override { return QStringLiteral("Apple Music"); }
    bool searchable() const override { return true; }
    bool matches(const QString& text) const override;
    void fromUrl(const QString& url, const QString& requester, TracksCallback cb) override;
    void search(const QString& query, int limit, const QString& requester, TracksCallback cb) override;

    static QString country(const QString& url);
    static TrackPtr trackFromResult(const QJsonObject& r, const QString& requester);

private:
    HttpJson* m_http;
    int m_max;
};

struct SourcesSettings
{
    bool youtube = true;
    bool soundcloud = true;
    bool spotify = true;
    QString spotifyClientId;
    QString spotifyClientSecret;
    bool appleMusic = true;
    bool directUrl = true;
    int maxSearchResults = 20;
    int maxPlaylistTracks = 500;
    QString ytdlpPath;
    QString denoPath;
};

class SourceRegistry final : public QObject, public MediaSources, public Resolver
{
    Q_OBJECT
public:
    explicit SourceRegistry(const SourcesSettings& settings, QObject* parent = nullptr);
    ~SourceRegistry() override;

    QString service() const override { return m_service; }
    QStringList services() const override;
    bool setService(const QString& name, QString* error) override;
    bool isUrl(const QString& text) const override;
    void fromInput(const QString& text, const QString& requester, TracksCallback cb) override;
    void playDirect(const QString& url, const QString& requester, TracksCallback cb) override;
    void search(const QString& query, const QString& requester, int limit, TracksCallback cb) override;
    void updateTools(std::function<void(bool ok, const QString& output)> cb) override;
    void resolve(const TrackPtr& t, std::function<void(bool ok)> done) override;

    QStringList enabledNames() const;
    YtDlpRunner* runner() const { return m_runner; }
    Source* sourceForUrl(const QString& url) const;

private:
    void resolveViaYouTube(const TrackPtr& t, ErrorCallback cb);
    void finishResolve(const TrackPtr& t, bool ok);

    SourcesSettings m_settings;
    YtDlpRunner* m_runner;
    HttpJson* m_http;
    std::vector<std::unique_ptr<Source>> m_owned;
    QList<Source*> m_urlSources;
    QList<QPair<QString, Source*>> m_searchable;
    Source* m_youtube = nullptr;
    QString m_service;
    QHash<Track*, QList<std::function<void(bool)>>> m_inflight;
};

} // namespace ttc::media
