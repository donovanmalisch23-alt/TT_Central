// TT Central — automated accessibility checks for our dialogs.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A screen reader announces a control by its accessible name. This audit
// walks a widget tree through Qt's accessibility layer (what NVDA/JAWS see)
// and reports every keyboard-focusable control that would be announced
// without a name, plus buttons/checkboxes without text.

#pragma once

#include <QStringList>

class QWidget;

namespace ttc {

/// Problems found under `root` (empty when everything is labelled).
QStringList auditAccessibility(QWidget* root);

/// Give every field in the QFormLayouts under `root` the accessible name of
/// its visible label (without the '&' mnemonic and trailing ':'), including
/// the text field inside spin boxes, which screen readers may focus.
void nameFormFields(QWidget* root);

} // namespace ttc
