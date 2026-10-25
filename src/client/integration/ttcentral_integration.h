// TT Central — hooks called from the (patched) qtTeamTalk main window.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Compiled into the client executable. See patches/TeamTalk5/.

#pragma once

class QMainWindow;
class QMenuBar;
struct TTMessage;

namespace ttcentral {

/// Create TT Central's features and add the "TT Central" menu.
/// Called once, right after the main window's UI is set up.
void install(QMainWindow* mainWindow, QMenuBar* menuBar);

/// Every event of the client's own TeamTalk connection.
void onClientMessage(const TTMessage& msg);

} // namespace ttcentral
