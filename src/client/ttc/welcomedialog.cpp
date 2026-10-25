// SPDX-License-Identifier: GPL-3.0-or-later
#include "ttc/welcomedialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace ttc {

QString WelcomeDialog::shownKey()
{
    return QStringLiteral("ttcentral/welcome-shown");
}

WelcomeDialog::WelcomeDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Welcome to TT Central"));
    auto* root = new QVBoxLayout(this);
    auto* text = new QLabel(
        tr("TT Central is the TeamTalk 5 client with three additions, all in the TT Central menu (Alt+T):\n\n"
           "Monitors: background logins that tell you who logs in and out, and about messages, on any "
           "number of servers.\n\n"
           "Media bot: a music bot account that plays YouTube, SoundCloud, Spotify, Apple Music and radio "
           "into a channel.\n\n"
           "Local server: host your own TeamTalk server on this computer.\n\n"
           "To connect to a server as usual, use Client, Connect (F2). What would you like to set up now?"),
        this);
    text->setWordWrap(true);
    text->setFocusPolicy(Qt::StrongFocus);
    text->setAccessibleName(text->text());
    root->addWidget(text);

    auto* buttons = new QVBoxLayout;
    auto add = [&](const QString& label, Choice c) {
        auto* b = new QPushButton(label, this);
        b->setAutoDefault(false);
        connect(b, &QPushButton::clicked, this, [this, c] {
            m_choice = c;
            accept();
        });
        buttons->addWidget(b);
        return b;
    };
    add(tr("Add a &monitor..."), SetUpMonitor);
    add(tr("Set up the media &bot..."), SetUpMediaBot);
    add(tr("Host a local &server..."), SetUpServer);
    add(tr("Read the user &guide"), OpenGuide);
    root->addLayout(buttons);

    m_again = new QCheckBox(tr("Show this &welcome next time"), this);
    root->addWidget(m_again);
    auto* box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    box->button(QDialogButtonBox::Close)->setText(tr("&Not now"));
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(box);
    text->setFocus();
}

bool WelcomeDialog::showAgain() const
{
    return m_again->isChecked();
}

} // namespace ttc
