// TT Central — unattended runs on Windows: answer the TeamTalk SDK's own
// message boxes (the one-time "Activating TeamTalk SDK Trial" notice).
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Header-only and Qt-free so ttc-sdkcheck can use it too. Only for tests and
// CI: people running TT Central see BearWare's notices as usual.
//
// Two layers:
//  1. Redirect TeamTalk5.dll's imports of MessageBoxW/MessageBoxA (normal and
//     delay-loaded) to a function that logs the text and answers OK at once.
//  2. A watcher thread that presses the button of any SDK dialog that still
//     appears (BM_CLICK, then WM_CLOSE).

#pragma once

#if defined(_WIN32)

#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <windows.h>
#include <delayimp.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <thread>

namespace ttc::win {

inline void logNotice(const wchar_t* caption, const wchar_t* text)
{
    std::fwprintf(stderr, L"[unattended] TeamTalk SDK notice answered: %ls: %ls\n", caption ? caption : L"",
                  text ? text : L"");
    std::fflush(stderr);
}

inline int answerFor(UINT type)
{
    switch (type & MB_TYPEMASK)
    {
    case MB_YESNO:
    case MB_YESNOCANCEL: return IDYES;
    case MB_RETRYCANCEL: return IDCANCEL;
    case MB_ABORTRETRYIGNORE: return IDIGNORE;
    default: return IDOK;
    }
}

inline int WINAPI quietMessageBoxW(HWND, LPCWSTR text, LPCWSTR caption, UINT type)
{
    logNotice(caption, text);
    return answerFor(type);
}

inline int WINAPI quietMessageBoxA(HWND, LPCSTR text, LPCSTR caption, UINT type)
{
    wchar_t t[1024] = {}, c[256] = {};
    if (text)
        MultiByteToWideChar(CP_ACP, 0, text, -1, t, 1023);
    if (caption)
        MultiByteToWideChar(CP_ACP, 0, caption, -1, c, 255);
    logNotice(c, t);
    return answerFor(type);
}

inline bool patchThunk(ULONG_PTR* slot, void* replacement)
{
    DWORD old = 0;
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &old))
        return false;
    *slot = reinterpret_cast<ULONG_PTR>(replacement);
    VirtualProtect(slot, sizeof(*slot), old, &old);
    return true;
}

inline void* replacementFor(const char* name)
{
    if (std::strcmp(name, "MessageBoxW") == 0)
        return reinterpret_cast<void*>(&quietMessageBoxW);
    if (std::strcmp(name, "MessageBoxA") == 0)
        return reinterpret_cast<void*>(&quietMessageBoxA);
    return nullptr;
}

/// Patch the given module's normal and delay-load imports. Returns how many
/// entries now point at the quiet message boxes.
inline int patchMessageBoxImports(HMODULE module)
{
    if (!module)
        return 0;
    auto* base = reinterpret_cast<BYTE*>(module);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    int patched = 0;

    const IMAGE_DATA_DIRECTORY& imports = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (imports.VirtualAddress)
    {
        for (auto* d = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + imports.VirtualAddress); d->Name; ++d)
        {
            if (_stricmp(reinterpret_cast<char*>(base + d->Name), "user32.dll") != 0)
                continue;
            auto* iat = reinterpret_cast<IMAGE_THUNK_DATA*>(base + d->FirstThunk);
            auto* names = reinterpret_cast<IMAGE_THUNK_DATA*>(
                base + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
            for (; names->u1.AddressOfData; ++names, ++iat)
            {
                if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))
                    continue;
                auto* byName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData);
                if (void* r = replacementFor(reinterpret_cast<char*>(byName->Name)))
                    patched += patchThunk(&iat->u1.Function, r) ? 1 : 0;
            }
        }
    }

    const IMAGE_DATA_DIRECTORY& delay = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT];
    if (delay.VirtualAddress)
    {
        for (auto* d = reinterpret_cast<ImgDelayDescr*>(base + delay.VirtualAddress); d->rvaDLLName; ++d)
        {
            if (_stricmp(reinterpret_cast<char*>(base + d->rvaDLLName), "user32.dll") != 0)
                continue;
            auto* iat = reinterpret_cast<IMAGE_THUNK_DATA*>(base + d->rvaIAT);
            auto* names = reinterpret_cast<IMAGE_THUNK_DATA*>(base + d->rvaINT);
            for (; names->u1.AddressOfData; ++names, ++iat)
            {
                if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))
                    continue;
                auto* byName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData);
                if (void* r = replacementFor(reinterpret_cast<char*>(byName->Name)))
                    patched += patchThunk(&iat->u1.Function, r) ? 1 : 0;
            }
        }
    }
    return patched;
}

inline BOOL CALLBACK pressButton(HWND child, LPARAM)
{
    wchar_t cls[32] = {};
    GetClassNameW(child, cls, 32);
    if (_wcsicmp(cls, L"Button") == 0)
    {
        SendMessageTimeoutW(child, BM_CLICK, 0, 0, SMTO_ABORTIFHUNG, 1000, nullptr);
        return FALSE;
    }
    return TRUE;
}

inline BOOL CALLBACK pressSdkDialog(HWND hwnd, LPARAM)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    wchar_t cls[32] = {};
    GetClassNameW(hwnd, cls, 32);
    if (pid != GetCurrentProcessId() || std::wcscmp(cls, L"#32770") != 0 || !IsWindowVisible(hwnd))
        return TRUE;
    wchar_t title[256] = {};
    GetWindowTextW(hwnd, title, 256);
    if (!std::wcsstr(title, L"TeamTalk") && !std::wcsstr(title, L"Trial") && !std::wcsstr(title, L"trial"))
        return TRUE;
    logNotice(title, L"(dialog; pressing its button)");
    EnumChildWindows(hwnd, pressButton, 0);
    PostMessageW(hwnd, WM_CLOSE, 0, 0);
    return TRUE;
}

/// Answer the TeamTalk SDK's message boxes in this process from now on.
/// Call before the first SDK instance is created. Returns the number of
/// patched imports (0 if TeamTalk5.dll isn't loaded yet).
inline int suppressSdkNotices()
{
    static std::atomic<bool> watcher{false};
    int patched = patchMessageBoxImports(GetModuleHandleW(L"TeamTalk5.dll"));
    patched += patchMessageBoxImports(GetModuleHandleW(L"TeamTalk5Pro.dll"));
    if (!watcher.exchange(true))
    {
        std::thread([] {
            for (;;)
            {
                EnumWindows(pressSdkDialog, 0);
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
            }
        }).detach();
    }
    return patched;
}

} // namespace ttc::win

#endif // _WIN32
