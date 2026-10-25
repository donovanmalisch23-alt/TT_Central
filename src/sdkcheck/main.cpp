// ttc-sdkcheck — minimal TeamTalk SDK smoke test.
//
// Prints the version of the loaded TeamTalk 5 SDK, proving the SDK headers,
// import library and runtime DLL (fetched by scripts/fetch_sdk.py) are wired
// up correctly.
//
//   ttc-sdkcheck                   print the SDK version
//   ttc-sdkcheck --activate-trial  also create one client instance, which
//                                  activates the SDK's 30-day trial
//
// On Windows the trial SDK shows a one-time "Activating TeamTalk SDK Trial"
// message box the first time a client instance is created on a computer (it
// then remembers this in the registry). Unattended machines such as CI
// runners have nobody to press OK, so --activate-trial presses it and prints
// the message instead. People running TT Central see the notice as usual.
//
// SPDX-License-Identifier: GPL-3.0-or-later (see NOTICE). The TeamTalk SDK it
// calls is proprietary (c) BearWare.dk; see NOTICE.

#if defined(_WIN32)
#  include <windows.h>  // must precede TeamTalk.h on Windows (WCHAR/HWND)
#endif

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cwchar>  // std::wprintf (wide-char output used on Windows)
#include <thread>

#include "TeamTalk.h"

#if defined(_WIN32)
#  include "../core/ttc/sdknotice_win.h"
#endif

int main(int argc, char** argv)
{
    const TTCHAR* version = TT_GetVersion();
#if defined(_WIN32)
    std::wprintf(L"TeamTalk SDK version: %ls\n", version);
#else
    std::printf("TeamTalk SDK version: %s\n", version);
#endif
    std::fflush(stdout);
    if (!version)
        return 1;

    const bool activate = argc > 1 && std::strcmp(argv[1], "--activate-trial") == 0;
    if (!activate)
        return 0;

#if defined(_WIN32)
    const int patched = ttc::win::suppressSdkNotices();
    std::wprintf(L"Answering the SDK's notices automatically (%d imports redirected).\n", patched);
    std::fflush(stdout);
#endif
    TTInstance* tt = TT_InitTeamTalkPoll();
    const bool ok = tt != nullptr;
    if (tt)
        TT_CloseTeamTalk(tt);
#if defined(_WIN32)
    std::wprintf(L"Client instance %ls.\n", ok ? L"created" : L"FAILED");
#else
    std::printf("Client instance %s.\n", ok ? "created" : "FAILED");
#endif
    return ok ? 0 : 1;
}
