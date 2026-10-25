// TT Central media bot — decode a track with ffmpeg and transmit it as voice.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ported from Simple TT Media (MIT, teamtalk/audio.py).
//
// TT_InsertAudioBlock lets an application push raw PCM into its voice
// stream, so the bot plays anything ffmpeg can decode without a virtual audio
// cable. ffmpeg writes signed 16-bit PCM to a pipe; a feeder thread reads one
// frame at a time, applies the volume and inserts it, staying only `leadMs`
// ahead of real time. A short lead keeps stop/skip responsive, because the
// SDK keeps playing whatever is already queued.
//
// Pause stops reading: the pipe fills and ffmpeg blocks, freezing the decode
// position until resume. All controls are called from the bot's main thread;
// "finished" is delivered there too.

#pragma once

#include "ttc/media/track.h"
#include "ttc/sdk.h"

#include <QObject>
#include <QStringList>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace ttc {
class TeamTalkClient;
}

namespace ttc::media {

class PipeProcess;

/// What the player needs from an engine (AudioEngine, or a fake in tests).
class PlaybackEngine
{
public:
    virtual ~PlaybackEngine() = default;

    /// Start playing `t` from the beginning. False (with `error`) when the
    /// decoder could not be started.
    virtual bool play(const TrackPtr& t, QString* error) = 0;
    virtual void stop() = 0;
    virtual bool pause() = 0;
    virtual bool resume() = 0;
    virtual bool seek(double seconds) = 0;
    /// Clamp and set the volume; returns the value applied.
    virtual int setVolume(int percent) = 0;
    virtual int volume() const = 0;
    virtual bool isPlaying() const = 0;
    virtual bool isPaused() const = 0;
    virtual double elapsed() const = 0;

    /// Called when a track ends by itself: reason "finished" or "error".
    std::function<void(const TrackPtr&, const QString& reason)> onFinished;
};

/// Where decoded frames go (the SDK in the bot, a recorder in tests).
class AudioSink
{
public:
    virtual ~AudioSink() = default;
    /// Called from the feeder thread. False when the SDK queue is full.
    virtual bool insert(const AudioBlock& block) = 0;
    /// End the current audio input (main thread).
    virtual void end() = 0;
};

/// Sends frames to a TeamTalk client as voice.
class SdkAudioSink final : public AudioSink
{
public:
    explicit SdkAudioSink(TeamTalkClient* client) : m_client(client) {}
    bool insert(const AudioBlock& block) override;
    void end() override;

private:
    TeamTalkClient* m_client;
};

struct AudioSettings
{
    int sampleRate = 48000;
    int channels = 2;
    int frameMs = 20;          // PCM frame size fed to the SDK
    int leadMs = 350;          // how far ahead of real time we buffer
    int defaultVolume = 100;   // percent
    int maxVolume = 200;       // percent ceiling users may set
};

class AudioEngine final : public QObject, public PlaybackEngine
{
    Q_OBJECT
public:
    /// Builds the decoder command line: program first, then its arguments.
    using DecoderCommand = std::function<QStringList(const Track& track, double startSeconds)>;

    AudioEngine(AudioSink* sink, const AudioSettings& settings, QObject* parent = nullptr);
    ~AudioEngine() override;

    void setFfmpegPath(const QString& path) { m_ffmpeg = path; }
    QString ffmpegPath() const { return m_ffmpeg; }
    /// Replace ffmpeg (tests feed generated PCM).
    void setDecoderCommand(DecoderCommand cmd) { m_command = std::move(cmd); }

    static QStringList ffmpegCommand(const QString& ffmpeg, const Track& track, double startSeconds,
                                     int sampleRate, int channels);

    bool play(const TrackPtr& t, QString* error) override;
    void stop() override;
    bool pause() override;
    bool resume() override;
    bool seek(double seconds) override;
    int setVolume(int percent) override;
    int volume() const override { return m_volume.load(); }
    bool isPlaying() const override { return !m_stopped; }
    bool isPaused() const override;
    double elapsed() const override;

    TrackPtr track() const { return m_track; }
    /// The decoder's last error output (after a failed track).
    QString lastError() const { return m_lastError; }
    int streamId() const { return m_streamId; }

    /// Re-start the voice stream at the same position after the bot was moved
    /// to another channel (the new channel's codec may use another frame size).
    bool restartForChannelChange();

private:
    bool begin(const TrackPtr& t, double startSeconds, bool newStream, QString* error);
    void stopFeeder();
    void endInput();
    void feed(int gen, std::shared_ptr<PipeProcess> proc);
    void onFeederDone(int gen, const QString& reason, const QString& error);

    AudioSink* m_sink;
    AudioSettings m_settings;
    int m_frameSamples;
    int m_frameBytes;
    QString m_ffmpeg = QStringLiteral("ffmpeg");
    DecoderCommand m_command;

    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    int m_gen = 0;             // guarded by m_mutex
    bool m_paused = false;     // guarded by m_mutex

    bool m_stopped = true;     // main thread only
    TrackPtr m_track;
    double m_seekBase = 0;
    int m_streamId = 0;
    std::atomic<int> m_volume{100};
    std::atomic<qint64> m_framesSent{0};
    std::atomic<bool> m_inputActive{false};
    std::shared_ptr<PipeProcess> m_proc;
    std::thread m_feeder;
    QString m_lastError;
};

} // namespace ttc::media
