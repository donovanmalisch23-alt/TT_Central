// TT Central tests — a stand-in decoder: writes a sine tone as signed 16-bit
// PCM to stdout, like "ffmpeg ... -f s16le pipe:1".
// SPDX-License-Identifier: GPL-3.0-or-later
//
//   pcmgen [--seconds S] [--start S] [--rate R] [--channels C] [--amplitude A]
//          [--fail]   (print an error and exit 1 without any audio)
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#if defined(_WIN32)
#  include <fcntl.h>
#  include <io.h>
#endif

int main(int argc, char** argv)
{
    double seconds = 2, start = 0;
    int rate = 48000, channels = 2, amplitude = 8000;
    bool fail = false;
    for (int i = 1; i < argc; ++i)
    {
        const char* a = argv[i];
        auto next = [&](double& v) { if (i + 1 < argc) v = std::atof(argv[++i]); };
        double d = 0;
        if (!std::strcmp(a, "--seconds")) next(seconds);
        else if (!std::strcmp(a, "--start")) next(start);
        else if (!std::strcmp(a, "--rate")) { next(d); rate = int(d); }
        else if (!std::strcmp(a, "--channels")) { next(d); channels = int(d); }
        else if (!std::strcmp(a, "--amplitude")) { next(d); amplitude = int(d); }
        else if (!std::strcmp(a, "--fail")) fail = true;
    }
    if (fail)
    {
        std::fprintf(stderr, "pcmgen: simulated decoder failure\n");
        return 1;
    }
#if defined(_WIN32)
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    const long long total = (long long)((seconds - start) * rate);
    std::vector<short> buf;
    const int chunk = rate / 50; // 20 ms
    for (long long n = 0; n < total; n += chunk)
    {
        const long long count = (total - n < chunk) ? total - n : chunk;
        buf.assign(size_t(count * channels), 0);
        for (long long k = 0; k < count; ++k)
        {
            const double t = double(n + k) / rate + start;
            const short v = short(amplitude * std::sin(2 * 3.14159265358979 * 440.0 * t));
            for (int c = 0; c < channels; ++c)
                buf[size_t(k * channels + c)] = v;
        }
        if (std::fwrite(buf.data(), sizeof(short), buf.size(), stdout) != buf.size())
            return 0; // reader went away
    }
    std::fflush(stdout);
    return 0;
}
