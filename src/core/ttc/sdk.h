// TT Central — thin helpers around the TeamTalk 5 SDK C API.
// SPDX-License-Identifier: MIT
//
// The TeamTalk SDK uses TTCHAR strings: UTF-16 (WCHAR) on Windows and UTF-8
// (char) elsewhere. Everything that crosses the SDK boundary goes through the
// helpers here so the rest of TT Central only deals with QString.

#pragma once

#if defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>  // must precede TeamTalk.h on Windows
#endif

#include "TeamTalk.h"

#include <QString>
#include <QStringList>

#include <string>

namespace ttc {

/// Convert an SDK string to QString.
QString fromTT(const TTCHAR* s);

/// An SDK-encoded copy of a QString, for passing `const TTCHAR*` arguments.
class TTStr
{
public:
    explicit TTStr(const QString& s);
    const TTCHAR* c_str() const { return m_data.c_str(); }

private:
    std::basic_string<TTCHAR> m_data;
};

/// Copy a QString into a fixed SDK buffer (e.g. TextMessage::szMessage),
/// truncating on a character boundary and always NUL-terminating.
void copyToTT(const QString& s, TTCHAR* dst, int dstLen = TT_STRLEN);

/// Number of TTCHAR units `s` occupies once encoded for the SDK.
int encodedLength(const QString& s);

/// Split text into pieces that each fit in one SDK text message
/// (TT_STRLEN - 1 encoded units), preferring to break at whitespace.
QStringList splitForTextMessage(const QString& text, int maxUnits = TT_STRLEN - 1);

/// Human-readable name of a client error number (falls back to the number).
QString errorName(int errorNo);

} // namespace ttc
