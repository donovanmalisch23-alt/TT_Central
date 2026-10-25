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
namespace {

std::atomic<int> g_dismissed{0};

BOOL CALLBACK collectText(HWND child, LPARAM out)
{
    wchar_t cls[32] = {};
    GetClassNameW(child, cls, 32);
    if (wcscmp(cls, L"Static") == 0)
    {
        wchar_t text[512] = {};
        GetWindowTextW(child, text, 512);
        if (text[0])
            std::wprintf(L"    %ls\n", text);
    }
    (void)out;
    return TRUE;
}

BOOL CALLBACK dismissDialog(HWND hwnd, LPARAM)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    wchar_t cls[32] = {};
    GetClassNameW(hwnd, cls, 32);
    if (pid == GetCurrentProcessId() && wcscmp(cls, L"#32770") == 0 && IsWindowVisible(hwnd))
    {
        wchar_t title[256] = {};
        GetWindowTextW(hwnd, title, 256);
        std::wprintf(L"SDK notice dismissed: %ls\n", title);
        EnumChildWindows(hwnd, collectText, 0);
        std::fflush(stdout);
        PostMessageW(hwnd, WM_COMMAND, IDOK, 0);
        ++g_dismissed;
    }
    return TRUE;
}

} // namespace
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
    std::atomic<bool> done{false};
    std::thread watcher([&done] {
        while (!done)
        {
            EnumWindows(dismissDialog, 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    });
#endif
    TTInstance* tt = TT_InitTeamTalkPoll();
    const bool ok = tt != nullptr;
    if (tt)
        TT_CloseTeamTalk(tt);
#if defined(_WIN32)
    done = true;
    watcher.join();
    std::wprintf(L"Client instance %ls (%d notice(s) dismissed).\n", ok ? L"created" : L"FAILED", g_dismissed.load());
#else
    std::printf("Client instance %s.\n", ok ? "created" : "FAILED");
#endif
    return ok ? 0 : 1;
}
