// TT Central — starting child processes safely on Windows.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// On Windows a child process inherits every inheritable handle of its
// parent, including the TeamTalk SDK's network sockets and open files. A
// media bot or local server started after you connected to a server would
// then hold your connection open even after you disconnect. These helpers
// limit what a child inherits to its own standard input, output and error.

#pragma once

class QProcess;

namespace ttc {

/// Make `p` (before start()) pass only its own pipes to the child.
/// Does nothing on other platforms.
void limitInheritedHandles(QProcess* p);

} // namespace ttc
