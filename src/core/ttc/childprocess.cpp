// SPDX-License-Identifier: GPL-3.0-or-later
#include "ttc/childprocess.h"

#include <QProcess>

#if defined(Q_OS_WIN)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <vector>
#endif

namespace ttc {

void limitInheritedHandles(QProcess* p)
{
#if defined(Q_OS_WIN)
    p->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
        // CreateProcess runs right after this, on the same thread, so the
        // extended startup information can live in thread-local storage.
        thread_local STARTUPINFOEXW ex;
        thread_local std::vector<char> attributes;
        thread_local HANDLE handles[3];

        STARTUPINFOW* si = args->startupInfo;
        int n = 0;
        if (si->dwFlags & STARTF_USESTDHANDLES)
        {
            for (HANDLE h : {si->hStdInput, si->hStdOutput, si->hStdError})
            {
                if (h == nullptr || h == INVALID_HANDLE_VALUE)
                    continue;
                bool seen = false;
                for (int i = 0; i < n; ++i)
                    seen = seen || handles[i] == h;
                DWORD flags = 0;
                if (!seen && GetHandleInformation(h, &flags) && (flags & HANDLE_FLAG_INHERIT))
                    handles[n++] = h;
            }
        }
        if (n == 0)
        {
            args->inheritHandles = false;
            return;
        }
        SIZE_T size = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
        attributes.assign(size, 0);
        auto* list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
        if (!InitializeProcThreadAttributeList(list, 1, 0, &size) ||
            !UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles, SIZE_T(n) * sizeof(HANDLE),
                                       nullptr, nullptr))
            return; // fall back to Qt's default
        ex = {};
        ex.StartupInfo = *si;
        ex.StartupInfo.cb = sizeof(ex);
        ex.lpAttributeList = list;
        args->startupInfo = &ex.StartupInfo;
        args->flags |= EXTENDED_STARTUPINFO_PRESENT;
    });
#else
    (void)p;
#endif
}

} // namespace ttc
