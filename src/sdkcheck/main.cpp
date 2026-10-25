// ttc-sdkcheck — minimal TeamTalk SDK smoke test.
//
// Prints the version of the loaded TeamTalk 5 SDK. This is the first build
// milestone for TT Central: it proves the SDK headers, import library, and
// runtime DLL are wired up correctly (fetched by scripts/fetch_sdk.py) before
// any of the larger client/server/monitor/media code is built.
//
// TT Central's own code — MIT licensed. The TeamTalk SDK it calls is
// proprietary (c) BearWare.dk; see NOTICE.

#if defined(_WIN32)
#  include <windows.h>  // must precede TeamTalk.h on Windows (WCHAR/HWND)
#endif

#include <cstdio>
#include <cwchar>  // std::wprintf (wide-char output used on Windows)

#include "TeamTalk.h"

int main() {
    const TTCHAR* version = TT_GetVersion();

#if defined(_WIN32)
    // On Windows TTCHAR is WCHAR (UNICODE).
    std::wprintf(L"TeamTalk SDK version: %ls\n", version);
#else
    // Elsewhere TTCHAR is char.
    std::printf("TeamTalk SDK version: %s\n", version);
#endif

    return version != nullptr ? 0 : 1;
}
