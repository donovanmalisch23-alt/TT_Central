// SPDX-License-Identifier: GPL-3.0-or-later
#include "ttc/unattended.h"

#include <QtGlobal>

#if defined(Q_OS_WIN)
#  include "ttc/sdknotice_win.h"
#endif

namespace ttc {

void dismissSdkNoticesInBackground()
{
#if defined(Q_OS_WIN)
    const int patched = win::suppressSdkNotices();
    qInfo("Unattended run: answering TeamTalk SDK notices (%d imports redirected).", patched);
#endif
}

} // namespace ttc
