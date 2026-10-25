// SPDX-License-Identifier: GPL-3.0-or-later
#include "ttc/media/audioengine.h"

#include "ttc/media/pipeprocess.h"
#include "ttc/teamtalkclient.h"

#include <QMetaObject>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <deque>
#include <vector>

namespace ttc::media {

using Clock = std::chrono::steady_clock;

bool SdkAudioSink::insert(const AudioBlock& block)
{
    return m_client->insertAudioBlock(&block);
}

void SdkAudioSink::end()
{
    m_client->insertAudioBlock(nullptr);
}

AudioEngine::AudioEngine(AudioSink* sink, const AudioSettings& settings, QObject* parent)
    : QObject(parent)
    , m_sink(sink)
    , m_settings(settings)
{
    m_settings.channels = std::clamp(m_settings.channels, 1, 2);
    m_settings.frameMs = std::clamp(m_settings.frameMs, 10, 100);
    m_settings.leadMs = std::clamp(m_settings.leadMs, 0, 5000);
    m_settings.maxVolume = std::max(1, m_settings.maxVolume);
    m_frameSamples = m_settings.sampleRate * m_settings.frameMs / 1000;
    m_frameBytes = m_frameSamples * m_settings.channels * 2;
    m_volume = std::clamp(m_settings.defaultVolume, 0, m_settings.maxVolume);
}

AudioEngine::~AudioEngine()
{
    stopFeeder();
}

QStringList AudioEngine::ffmpegCommand(const QString& ffmpeg, const Track& track, double startSeconds,
                                       int sampleRate, int channels)
{
    QStringList cmd{ffmpeg, QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("error"),
                    QStringLiteral("-nostdin")};
    if (startSeconds > 0)
        cmd << QStringLiteral("-ss") << QString::number(startSeconds, 'f', 3);
    if (!track.httpHeaders.isEmpty())
    {
        QString headers;
        for (auto it = track.httpHeaders.cbegin(); it != track.httpHeaders.cend(); ++it)
            headers += it.key() + QStringLiteral(": ") + it.value() + QStringLiteral("\r\n");
        cmd << QStringLiteral("-headers") << headers;
    }
    const QString url = track.playbackUrl;
    if (url.startsWith(QLatin1String("http://"), Qt::CaseInsensitive) ||
        url.startsWith(QLatin1String("https://"), Qt::CaseInsensitive))
        cmd << QStringLiteral("-reconnect") << QStringLiteral("1") << QStringLiteral("-reconnect_streamed")
            << QStringLiteral("1") << QStringLiteral("-reconnect_delay_max") << QStringLiteral("5");
    cmd << QStringLiteral("-i") << url << QStringLiteral("-vn") << QStringLiteral("-f") << QStringLiteral("s16le")
        << QStringLiteral("-ac") << QString::number(channels) << QStringLiteral("-ar") << QString::number(sampleRate)
        << QStringLiteral("pipe:1");
    return cmd;
}

bool AudioEngine::play(const TrackPtr& t, QString* error)
{
    stopFeeder();
    return begin(t, 0.0, true, error);
}

bool AudioEngine::begin(const TrackPtr& t, double startSeconds, bool newStream, QString* error)
{
    const QStringList cmd = m_command ? m_command(*t, startSeconds)
                                      : ffmpegCommand(m_ffmpeg, *t, startSeconds, m_settings.sampleRate,
                                                      m_settings.channels);
    auto proc = std::make_shared<PipeProcess>();
    QString err;
    if (cmd.isEmpty() || !proc->start(cmd.first(), cmd.mid(1), &err))
    {
        m_track.reset();
        m_stopped = true;
        m_lastError = err.isEmpty() ? tr("no decoder") : err;
        if (error)
            *error = m_lastError;
        return false;
    }
    int gen;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        gen = ++m_gen;
        m_paused = false;
    }
    m_track = t;
    m_stopped = false;
    m_seekBase = startSeconds;
    m_framesSent = 0;
    m_lastError.clear();
    if (newStream)
        m_streamId = (m_streamId % 2000000) + 1;
    m_proc = proc;
    m_feeder = std::thread([this, gen, proc] { feed(gen, proc); });
    return true;
}

void AudioEngine::stopFeeder()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_gen;
        m_paused = false;
    }
    m_cv.notify_all();
    m_stopped = true;
    if (m_proc)
        m_proc->kill();
    if (m_feeder.joinable())
        m_feeder.join();
    m_proc.reset();
    endInput();
}

void AudioEngine::endInput()
{
    // Only end a voice stream that is actually active; ending one otherwise
    // trips a harmless assertion inside the SDK.
    if (m_inputActive.exchange(false))
        m_sink->end();
}

void AudioEngine::stop()
{
    stopFeeder();
    m_track.reset();
}

bool AudioEngine::pause()
{
    if (m_stopped)
        return false;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_paused)
        return false;
    m_paused = true;
    return true;
}

bool AudioEngine::resume()
{
    if (m_stopped)
        return false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_paused)
            return false;
        m_paused = false;
    }
    m_cv.notify_all();
    return true;
}

bool AudioEngine::isPaused() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_paused && !m_stopped;
}

int AudioEngine::setVolume(int percent)
{
    const int v = std::clamp(percent, 0, m_settings.maxVolume);
    m_volume = v;
    return v;
}

double AudioEngine::elapsed() const
{
    return m_seekBase + double(m_framesSent.load()) * m_settings.frameMs / 1000.0;
}

bool AudioEngine::seek(double seconds)
{
    if (m_stopped || !m_track || m_track->isLive)
        return false;
    TrackPtr t = m_track;
    seconds = std::max(0.0, seconds);
    if (t->duration > 0 && seconds >= t->duration)
        seconds = std::max(0.0, t->duration - 1.0);
    // Fully stop and join the current feeder first, so only one feeder ever
    // runs at a time.
    stopFeeder();
    return begin(t, seconds, false, nullptr);
}

bool AudioEngine::restartForChannelChange()
{
    if (m_stopped || !m_track)
        return false;
    TrackPtr t = m_track;
    const double pos = t->isLive ? 0.0 : elapsed();
    stopFeeder();
    return begin(t, pos, true, nullptr);
}

namespace {

qint64 readExact(PipeProcess& p, char* buf, qint64 n)
{
    qint64 got = 0;
    while (got < n)
    {
        const qint64 r = p.read(buf + got, n - got);
        if (r <= 0)
            break;
        got += r;
    }
    return got;
}

void scale(const char* in, int16_t* out, int samples, int volumePercent)
{
    std::memcpy(out, in, size_t(samples) * 2);
    if (volumePercent == 100)
        return;
    const int gain = volumePercent; // fixed point: /100
    for (int i = 0; i < samples; ++i)
    {
        const int v = int(out[i]) * gain / 100;
        out[i] = int16_t(std::clamp(v, -32768, 32767));
    }
}

} // namespace

void AudioEngine::feed(int gen, std::shared_ptr<PipeProcess> proc)
{
    const auto frame = std::chrono::microseconds(m_settings.frameMs * 1000);
    const auto lead = std::chrono::milliseconds(m_settings.leadMs);
    const int samplesPerFrame = m_frameSamples * m_settings.channels;
    const auto start = Clock::now();
    Clock::duration paused{0};
    std::vector<char> raw(static_cast<size_t>(m_frameBytes));
    // Keep recently inserted buffers alive in case the SDK reads them late.
    std::deque<std::vector<int16_t>> keep;
    qint64 frames = 0;
    QString reason = QStringLiteral("finished");
    QString error;

    auto send = [&](const char* data) {
        keep.emplace_back(size_t(samplesPerFrame));
        if (keep.size() > 64)
            keep.pop_front();
        std::vector<int16_t>& pcm = keep.back();
        scale(data, pcm.data(), samplesPerFrame, m_volume.load());
        AudioBlock block;
        std::memset(&block, 0, sizeof(block));
        block.nStreamID = m_streamId;
        block.nSampleRate = m_settings.sampleRate;
        block.nChannels = m_settings.channels;
        block.lpRawAudio = pcm.data();
        block.nSamples = m_frameSamples;
        block.uStreamTypes = STREAMTYPE_VOICE;
        if (!m_sink->insert(block))
        {
            // Queue full: wait one frame and retry once, never drop.
            std::this_thread::sleep_for(frame);
            m_sink->insert(block);
        }
        m_inputActive = true;
    };

    for (;;)
    {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (m_gen != gen)
                return;
            if (m_paused)
            {
                const auto p0 = Clock::now();
                m_cv.wait(lock, [&] { return !m_paused || m_gen != gen; });
                paused += Clock::now() - p0;
                if (m_gen != gen)
                    return;
            }
        }
        const qint64 got = readExact(*proc, raw.data(), m_frameBytes);
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_gen != gen)
                return;
        }
        if (got < m_frameBytes)
        {
            if (got > 0)
            {
                std::fill(raw.begin() + got, raw.end(), 0);
                send(raw.data());
                ++frames;
            }
            // Natural end or decoder failure? Look at the exit code.
            const int rc = proc->waitForExit(3000);
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); // let stderr catch up
            error = QString::fromUtf8(proc->stderrTail()).trimmed();
            if (rc != 0 && frames < 25) // failed before ~0.5 s of audio
                reason = QStringLiteral("error");
            break;
        }
        send(raw.data());
        ++frames;
        ++m_framesSent;
        const auto target = start + paused + frames * frame - lead;
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_cv.wait_until(lock, target, [&] { return m_gen != gen; }))
            return;
    }
    QMetaObject::invokeMethod(
        this, [this, gen, reason, error] { onFeederDone(gen, reason, error); }, Qt::QueuedConnection);
}

void AudioEngine::onFeederDone(int gen, const QString& reason, const QString& error)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_gen != gen)
            return;
    }
    if (m_stopped)
        return;
    TrackPtr t = m_track;
    stopFeeder();
    m_lastError = error;
    if (t && onFinished)
        onFinished(t, reason);
}

} // namespace ttc::media
