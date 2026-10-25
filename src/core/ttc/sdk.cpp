// SPDX-License-Identifier: MIT
#include "ttc/sdk.h"

#include <QRegularExpression>

#include <algorithm>
#include <cstring>

namespace ttc {

QString fromTT(const TTCHAR* s)
{
    if (!s)
        return {};
#if defined(_WIN32)
    return QString::fromWCharArray(s);
#else
    return QString::fromUtf8(s);
#endif
}

TTStr::TTStr(const QString& s)
#if defined(_WIN32)
    : m_data(s.toStdWString())
#else
    : m_data(s.toStdString())
#endif
{
}

int encodedLength(const QString& s)
{
#if defined(_WIN32)
    return int(s.toStdWString().size());
#else
    return int(s.toUtf8().size());
#endif
}

void copyToTT(const QString& s, TTCHAR* dst, int dstLen)
{
    if (!dst || dstLen <= 0)
        return;
    // Shrink by whole characters until the encoded form fits (dstLen - 1
    // units plus the terminating NUL). Never split a surrogate pair or a
    // UTF-8 sequence.
    QString text = s;
    while (encodedLength(text) > dstLen - 1)
    {
        int cut = text.size() - 1;
        if (cut > 0 && text.at(cut).isLowSurrogate())
            --cut;
        text.truncate(cut);
    }
    TTStr enc(text);
    const auto n = std::min<size_t>(size_t(dstLen - 1), std::char_traits<TTCHAR>::length(enc.c_str()));
    std::char_traits<TTCHAR>::copy(dst, enc.c_str(), n);
    dst[n] = 0;
}

QStringList splitForTextMessage(const QString& text, int maxUnits)
{
    QStringList parts;
    QString rest = text;
    while (!rest.isEmpty())
    {
        if (encodedLength(rest) <= maxUnits)
        {
            parts << rest;
            break;
        }
        // Largest prefix that fits.
        int lo = 1, hi = rest.size();
        while (lo < hi)
        {
            const int mid = (lo + hi + 1) / 2;
            if (encodedLength(rest.left(mid)) <= maxUnits)
                lo = mid;
            else
                hi = mid - 1;
        }
        int cut = lo;
        if (cut < rest.size() && rest.at(cut).isLowSurrogate())
            --cut; // keep surrogate pairs together
        // Prefer breaking at whitespace in the last quarter of the piece.
        const int ws = rest.left(cut).lastIndexOf(QRegularExpression(QStringLiteral("\\s")));
        if (ws > cut * 3 / 4)
            cut = ws + 1;
        parts << rest.left(cut);
        rest = rest.mid(cut);
    }
    return parts;
}

QString errorName(int errorNo)
{
    switch (errorNo)
    {
    case CMDERR_SYNTAX_ERROR: return QStringLiteral("syntax error");
    case CMDERR_UNKNOWN_COMMAND: return QStringLiteral("unknown command");
    case CMDERR_MISSING_PARAMETER: return QStringLiteral("missing parameter");
    case CMDERR_INCOMPATIBLE_PROTOCOLS: return QStringLiteral("incompatible protocol version");
    case CMDERR_UNKNOWN_AUDIOCODEC: return QStringLiteral("unknown audio codec");
    case CMDERR_INVALID_USERNAME: return QStringLiteral("invalid username");
    case CMDERR_INVALID_ACCOUNT: return QStringLiteral("invalid username or password");
    case CMDERR_MAX_SERVER_USERS_EXCEEDED: return QStringLiteral("the server is full");
    case CMDERR_SERVER_BANNED: return QStringLiteral("banned from the server");
    case CMDERR_NOT_AUTHORIZED: return QStringLiteral("not authorized");
    case CMDERR_MAX_LOGINS_PER_IPADDRESS_EXCEEDED: return QStringLiteral("too many logins from this IP address");
    case CMDERR_MAX_CHANNELS_EXCEEDED: return QStringLiteral("too many channels");
    case CMDERR_COMMAND_FLOOD: return QStringLiteral("command flood protection");
    case CMDERR_CHANNEL_BANNED: return QStringLiteral("banned from the channel");
    case CMDERR_LOGINSERVICE_UNAVAILABLE: return QStringLiteral("login service unavailable");
    case CMDERR_INCORRECT_CHANNEL_PASSWORD: return QStringLiteral("incorrect channel password");
    case CMDERR_CHANNEL_NOT_FOUND: return QStringLiteral("channel not found");
    case CMDERR_NOT_LOGGEDIN: return QStringLiteral("not logged in");
    case CMDERR_ALREADY_LOGGEDIN: return QStringLiteral("already logged in");
    case CMDERR_NOT_IN_CHANNEL: return QStringLiteral("not in a channel");
    case CMDERR_ALREADY_IN_CHANNEL: return QStringLiteral("already in the channel");
    case CMDERR_USER_NOT_FOUND: return QStringLiteral("user not found");
    case CMDERR_CHANNEL_HAS_USERS: return QStringLiteral("channel has users");
    default: return QStringLiteral("error %1").arg(errorNo);
    }
}

} // namespace ttc
