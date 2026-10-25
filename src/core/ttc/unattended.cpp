// SPDX-License-Identifier: GPL-3.0-or-later
#include "ttc/unattended.h"

#include <QtGlobal>

#if defined(Q_OS_WIN)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <atomic>
#  include <chrono>
#  include <cwchar>
#  include <thread>
#endif

namespace ttc {

#if defined(Q_OS_WIN)
namespace {

BOOL CALLBACK pressOk(HWND hwnd, LPARAM)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    wchar_t cls[32] = {};
    GetClassNameW(hwnd, cls, 32);
    if (pid == GetCurrentProcessId() && wcscmp(cls, L"#32770") == 0 && IsWindowVisible(hwnd))
    {
        wchar_t title[256] = {};
        GetWindowTextW(hwnd, title, 256);
        // Only the SDK's own notices, never one of TT Central's dialogs.
        if (wcsstr(title, L"TeamTalk") || wcsstr(title, L"Trial") || wcsstr(title, L"trial"))
        {
            qInfo("Dismissed TeamTalk SDK notice: %ls", title);
            PostMessageW(hwnd, WM_COMMAND, IDOK, 0);
        }
    }
    return TRUE;
}

} // namespace
#endif

void dismissSdkNoticesInBackground()
{
#if defined(Q_OS_WIN)
    static std::atomic<bool> started{false};
    if (started.exchange(true))
        return;
    std::thread([] {
        for (;;)
        {
            EnumWindows(pressOk, 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }).detach();
#endif
}

} // namespace ttc
