// TT Central — the first-run welcome: what TT Central adds, and where to
// start setting each part up.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>

class QCheckBox;

namespace ttc {

class WelcomeDialog : public QDialog
{
    Q_OBJECT
public:
    enum Choice { None = 0, SetUpMonitor, SetUpMediaBot, SetUpServer, OpenGuide };

    explicit WelcomeDialog(QWidget* parent = nullptr);
    Choice choice() const { return m_choice; }
    bool showAgain() const;

    /// Settings key: true once the welcome has been dismissed for good.
    static QString shownKey();

private:
    Choice m_choice = None;
    QCheckBox* m_again = nullptr;
};

} // namespace ttc
