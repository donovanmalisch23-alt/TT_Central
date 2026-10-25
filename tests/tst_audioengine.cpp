// SPDX-License-Identifier: GPL-3.0-or-later
// Tests for the media bot's audio engine: real-time pacing, pause, seek,
// stop, volume and decoder failures, with a generated tone standing in for
// ffmpeg (and real ffmpeg too, when it is available).
#include "ttc/media/audioengine.h"
#include "ttc/media/botconfig.h"

#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <atomic>
#include <cstdlib>
#include <mutex>

using namespace ttc::media;

namespace {

class RecordingSink final : public AudioSink
{
public:
    std::atomic<int> frames{0};
    std::atomic<int> ends{0};
    std::atomic<int> peak{0};       // of the latest frame
    std::atomic<int> maxPeak{0};    // of all frames
    std::atomic<int> lastStream{0};
    std::atomic<int> samplesPerFrame{0};

    bool insert(const AudioBlock& b) override
    {
        const auto* s = static_cast<const int16_t*>(b.lpRawAudio);
        int p = 0;
        for (int i = 0; i < b.nSamples * b.nChannels; ++i)
            p = std::max(p, std::abs(int(s[i])));
        peak = p;
        if (p > maxPeak)
            maxPeak = p;
        lastStream = b.nStreamID;
        samplesPerFrame = b.nSamples;
        ++frames;
        return true;
    }
    void end() override { ++ends; }
};

QString pcmgen()
{
    return QStringLiteral(TTC_PCMGEN_PATH);
}

/// playbackUrl "pcm:<seconds>" or "pcm:fail" drives the stand-in decoder.
AudioEngine::DecoderCommand toneDecoder(int amplitude = 8000)
{
    return [amplitude](const Track& t, double start) {
        const QString spec = t.playbackUrl.mid(4);
        if (spec == QLatin1String("fail"))
            return QStringList{pcmgen(), QStringLiteral("--fail")};
        return QStringList{pcmgen(), QStringLiteral("--seconds"), spec, QStringLiteral("--start"),
                           QString::number(start), QStringLiteral("--amplitude"), QString::number(amplitude)};
    };
}

TrackPtr tone(const QString& spec, double duration = 0)
{
    Track t;
    t.title = spec;
    t.playbackUrl = QStringLiteral("pcm:") + spec;
    t.duration = duration;
    return makeTrack(t);
}

bool waitFor(const std::function<bool()>& cond, int ms)
{
    QElapsedTimer t;
    t.start();
    while (!cond())
    {
        if (t.elapsed() > ms)
            return false;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(2);
    }
    return true;
}

} // namespace

class TestAudioEngine : public QObject
{
    Q_OBJECT

    struct Rig
    {
        RecordingSink sink;
        AudioEngine engine{&sink, AudioSettings{}};
        QList<QPair<TrackPtr, QString>> finished;
        Rig()
        {
            engine.setDecoderCommand(toneDecoder());
            engine.onFinished = [this](const TrackPtr& t, const QString& r) { finished.append({t, r}); };
        }
    };

private slots:
    void playsAtRealTimePace()
    {
        Rig r;
        QElapsedTimer clock;
        clock.start();
        QString err;
        QVERIFY2(r.engine.play(tone(QStringLiteral("1.0")), &err), qPrintable(err));
        QVERIFY(r.engine.isPlaying());
        QVERIFY(waitFor([&] { return !r.finished.isEmpty(); }, 5000));
        const qint64 ms = clock.elapsed();
        // 1 s of audio, sent at most 350 ms ahead of real time.
        QVERIFY2(ms >= 550 && ms < 3000, qPrintable(QString::number(ms)));
        QCOMPARE(r.sink.frames.load(), 50);
        QCOMPARE(r.sink.samplesPerFrame.load(), 960);   // 20 ms at 48 kHz
        QCOMPARE(r.finished.first().second, QStringLiteral("finished"));
        QCOMPARE(r.sink.ends.load(), 1);                // the voice stream was ended
        QVERIFY(!r.engine.isPlaying());
    }

    void pauseHoldsThePosition()
    {
        Rig r;
        QVERIFY(r.engine.play(tone(QStringLiteral("5")), nullptr));
        QTest::qWait(600);
        QVERIFY(r.engine.pause());
        QVERIFY(!r.engine.pause());
        QVERIFY(r.engine.isPaused());
        QTest::qWait(100);
        const int frozen = r.sink.frames;
        const double at = r.engine.elapsed();
        QTest::qWait(500);
        QCOMPARE(r.sink.frames.load(), frozen);
        QCOMPARE(r.engine.elapsed(), at);
        QVERIFY(r.engine.resume());
        QTest::qWait(400);
        QVERIFY(r.sink.frames > frozen + 10);
        QVERIFY(r.finished.isEmpty());
    }

    void stopIsImmediateAndSilent()
    {
        Rig r;
        QVERIFY(r.engine.play(tone(QStringLiteral("60")), nullptr));
        QTest::qWait(300);
        QElapsedTimer t;
        t.start();
        r.engine.stop();
        QVERIFY2(t.elapsed() < 500, qPrintable(QString::number(t.elapsed())));
        QVERIFY(!r.engine.isPlaying());
        const int frames = r.sink.frames;
        QTest::qWait(300);
        QCOMPARE(r.sink.frames.load(), frames);
        QVERIFY(r.finished.isEmpty());                  // stopping is not "finishing"
        QCOMPARE(r.sink.ends.load(), 1);
    }

    void pausedStopStillEnds()
    {
        Rig r;
        QVERIFY(r.engine.play(tone(QStringLiteral("60")), nullptr));
        QTest::qWait(200);
        r.engine.pause();
        QElapsedTimer t;
        t.start();
        r.engine.stop();
        QVERIFY(t.elapsed() < 500);
    }

    void seekRestartsAtThePosition()
    {
        Rig r;
        QVERIFY(r.engine.play(tone(QStringLiteral("10"), 10), nullptr));
        QTest::qWait(200);
        const int stream = r.sink.lastStream;
        QVERIFY(r.engine.seek(9));
        QVERIFY(r.engine.elapsed() >= 9.0);
        QVERIFY(waitFor([&] { return !r.finished.isEmpty(); }, 5000));
        QCOMPARE(r.finished.first().second, QStringLiteral("finished"));
        QCOMPARE(r.sink.lastStream.load(), stream);     // a seek keeps the stream
        QVERIFY(r.engine.elapsed() >= 9.9);
        // Seeking past the end lands one second before it.
        QVERIFY(r.engine.play(tone(QStringLiteral("10"), 10), nullptr));
        QVERIFY(r.engine.seek(500));
        QVERIFY(qAbs(r.engine.elapsed() - 9.0) < 0.1);
        r.engine.stop();
    }

    void liveStreamsCannotSeek()
    {
        Rig r;
        TrackPtr live = tone(QStringLiteral("5"));
        live->isLive = true;
        QVERIFY(r.engine.play(live, nullptr));
        QVERIFY(!r.engine.seek(2));
        r.engine.stop();
        QVERIFY(!r.engine.seek(2));
    }

    void channelChangeStartsANewStream()
    {
        Rig r;
        QVERIFY(r.engine.play(tone(QStringLiteral("10"), 10), nullptr));
        QTest::qWait(500);
        const int stream = r.sink.lastStream;
        const double before = r.engine.elapsed();
        QVERIFY(r.engine.restartForChannelChange());
        QTest::qWait(100);
        QVERIFY(r.sink.lastStream != stream);
        QVERIFY(r.engine.elapsed() >= before);
        r.engine.stop();
        QVERIFY(!r.engine.restartForChannelChange());
    }

    void volumeScalesAndClamps()
    {
        Rig r;
        QCOMPARE(r.engine.setVolume(50), 50);
        QVERIFY(r.engine.play(tone(QStringLiteral("2")), nullptr));
        QTest::qWait(300);
        QVERIFY2(qAbs(r.sink.peak - 4000) < 100, qPrintable(QString::number(r.sink.peak)));
        QCOMPARE(r.engine.setVolume(999), 200);
        QTest::qWait(500);   // past the lead, frames now carry the new volume
        QVERIFY2(qAbs(r.sink.peak - 16000) < 300, qPrintable(QString::number(r.sink.peak)));
        QCOMPARE(r.engine.setVolume(-5), 0);
        r.engine.stop();
    }

    void decoderFailureIsAnError()
    {
        Rig r;
        QVERIFY(r.engine.play(tone(QStringLiteral("fail")), nullptr));
        QVERIFY(waitFor([&] { return !r.finished.isEmpty(); }, 5000));
        QCOMPARE(r.finished.first().second, QStringLiteral("error"));
        QVERIFY(r.engine.lastError().contains(QStringLiteral("simulated")));
        QCOMPARE(r.sink.frames.load(), 0);
        QCOMPARE(r.sink.ends.load(), 0);   // no stream was started, none ended
    }

    void missingDecoderFailsToStart()
    {
        Rig r;
        r.engine.setDecoderCommand([](const Track&, double) {
            return QStringList{QStringLiteral("/no/such/decoder-ttc")};
        });
        QString err;
        QVERIFY(!r.engine.play(tone(QStringLiteral("1")), &err));
        QVERIFY(!err.isEmpty());
        QVERIFY(!r.engine.isPlaying());
    }

    void playReplacesTheCurrentTrack()
    {
        Rig r;
        QVERIFY(r.engine.play(tone(QStringLiteral("30")), nullptr));
        QTest::qWait(200);
        const int stream = r.sink.lastStream;
        QVERIFY(r.engine.play(tone(QStringLiteral("0.5")), nullptr));
        QVERIFY(waitFor([&] { return !r.finished.isEmpty(); }, 5000));
        QCOMPARE(r.finished.size(), 1);                 // only the second one finished
        QCOMPARE(r.finished.first().first->title, QStringLiteral("0.5"));
        QVERIFY(r.sink.lastStream != stream);
    }

    void decodesWithRealFfmpeg()
    {
        QString ffmpeg = qEnvironmentVariable("TTC_FFMPEG");
        if (ffmpeg.isEmpty())
            ffmpeg = findTool({}, QStringLiteral("ffmpeg"), defaultToolDirs() << QStringLiteral(TTC_SOURCE_TOOLS_DIR));
        if (ffmpeg.isEmpty())
            QSKIP("ffmpeg not found (set TTC_FFMPEG or run scripts/fetch_tools.py)");

        // A 1.5 s, 44.1 kHz mono WAV: ffmpeg must resample it to 48 kHz stereo.
        QTemporaryDir dir;
        const QString wav = dir.filePath(QStringLiteral("tone file.wav"));
        {
            const int rate = 44100, n = rate * 3 / 2;
            QByteArray pcm(n * 2, 0);
            auto* s = reinterpret_cast<int16_t*>(pcm.data());
            for (int i = 0; i < n; ++i)
                s[i] = int16_t(8000 * std::sin(2 * M_PI * 440 * i / double(rate)));
            QFile f(wav);
            QVERIFY(f.open(QIODevice::WriteOnly));
            auto u32 = [&](quint32 v) { f.write(reinterpret_cast<const char*>(&v), 4); };
            auto u16 = [&](quint16 v) { f.write(reinterpret_cast<const char*>(&v), 2); };
            f.write("RIFF"); u32(36 + quint32(pcm.size())); f.write("WAVEfmt ");
            u32(16); u16(1); u16(1); u32(rate); u32(rate * 2); u16(2); u16(16);
            f.write("data"); u32(quint32(pcm.size())); f.write(pcm);
        }
        RecordingSink sink;
        AudioEngine engine(&sink, AudioSettings{});
        engine.setFfmpegPath(ffmpeg);
        QString reason;
        engine.onFinished = [&](const TrackPtr&, const QString& r) { reason = r; };
        Track t;
        t.title = QStringLiteral("wav");
        t.playbackUrl = wav;
        QString err;
        QVERIFY2(engine.play(makeTrack(t), &err), qPrintable(err));
        QVERIFY(waitFor([&] { return !reason.isEmpty(); }, 10000));
        QCOMPARE(reason, QStringLiteral("finished"));
        QVERIFY2(qAbs(sink.frames - 75) <= 2, qPrintable(QString::number(sink.frames)));
        // Mono upmixed to stereo comes out about 3 dB lower (8000 -> ~5660).
        QVERIFY2(sink.maxPeak > 5000, qPrintable(QString::number(sink.maxPeak)));
    }
};

QTEST_GUILESS_MAIN(TestAudioEngine)
#include "tst_audioengine.moc"
