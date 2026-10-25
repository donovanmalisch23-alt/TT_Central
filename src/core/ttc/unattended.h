// TT Central — support for unattended runs (CI, automated self-tests).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace ttc {

/// On Windows, the trial TeamTalk SDK shows a one-time "Activating TeamTalk
/// SDK Trial" message box. In unattended runs nobody can press OK, so this
/// starts a background watcher that presses it (and logs the message). Use
/// only for tests and the CI self-test. Does nothing on other platforms.
void dismissSdkNoticesInBackground();

} // namespace ttc
