// SPDX-License-Identifier: GPL-3.0-or-later
#include "ttc/monitoreditdialog.h"

#include "ttc/a11yaudit.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

namespace ttc {

MonitorEditDialog::MonitorEditDialog(const MonitorConfig& config, const QList<SavedServer>& savedServers,
                                     QWidget* parent)
    : QDialog(parent)
    , m_base(config)
    , m_saved(savedServers)
{
    setWindowTitle(config.host.isEmpty() ? tr("Add Monitor") : tr("Edit Monitor"));
    // Content scrolls on small screens / large fonts; buttons stay visible.
    auto* outer = new QVBoxLayout(this);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setFocusPolicy(Qt::NoFocus); // the fields inside are the tab stops
    scroll->setAccessibleName(tr("Monitor settings"));
    auto* content = new QWidget(scroll);
    auto* root = new QVBoxLayout(content);
    scroll->setWidget(content);
    outer->addWidget(scroll);

    // --- Import ----------------------------------------------------------
    if (!m_saved.isEmpty())
    {
        auto* importForm = new QFormLayout;
        m_import = new QComboBox(this);
        m_import->addItem(tr("(choose a saved server to copy its details)"));
        for (const SavedServer& s : std::as_const(m_saved))
            m_import->addItem(s.name.isEmpty() ? s.host : tr("%1 (%2)").arg(s.name, s.host));
        m_import->setAccessibleDescription(tr("Fills in the server, account and channel from a server saved in TT Central's server list."));
        importForm->addRow(tr("&Import from saved server:"), m_import);
        root->addLayout(importForm);
        connect(m_import, qOverload<int>(&QComboBox::activated), this, &MonitorEditDialog::importSaved);
    }

    // --- Monitor ---------------------------------------------------------
    auto* general = new QGroupBox(tr("Monitor"), this);
    auto* gf = new QFormLayout(general);
    m_name = new QLineEdit(general);
    m_name->setPlaceholderText(tr("e.g. Home Server"));
    m_name->setAccessibleDescription(tr("Every notification from this monitor starts with this name, so you know which server it came from."));
    gf->addRow(tr("&Name shown in notifications:"), m_name);
    m_enabled = new QCheckBox(tr("&Enabled"), general);
    gf->addRow(m_enabled);
    root->addWidget(general);

    // --- Server ----------------------------------------------------------
    auto* server = new QGroupBox(tr("Server"), this);
    auto* sf = new QFormLayout(server);
    m_host = new QLineEdit(server);
    sf->addRow(tr("Server &address:"), m_host);
    m_tcp = new QSpinBox(server);
    m_tcp->setRange(1, 65535);
    sf->addRow(tr("&TCP port:"), m_tcp);
    m_udp = new QSpinBox(server);
    m_udp->setRange(1, 65535);
    sf->addRow(tr("&UDP port:"), m_udp);
    m_encrypted = new QCheckBox(tr("En&crypted server"), server);
    sf->addRow(m_encrypted);
    root->addWidget(server);

    // --- Account ---------------------------------------------------------
    auto* account = new QGroupBox(tr("Account"), this);
    auto* af = new QFormLayout(account);
    const QString note = oneLoginPerIpNote();
    m_username = new QLineEdit(account);
    m_username->setAccessibleDescription(note);
    af->addRow(tr("U&sername:"), m_username);
    m_password = new QLineEdit(account);
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setAccessibleDescription(note);
    af->addRow(tr("&Password:"), m_password);
    m_showPassword = new QCheckBox(tr("Sho&w password"), account);
    af->addRow(m_showPassword);
    connect(m_showPassword, &QCheckBox::toggled, this, [this](bool on) {
        m_password->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
    });
    auto* noteLabel = new QLabel(note, account);
    noteLabel->setWordWrap(true);
    noteLabel->setTextInteractionFlags(Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse);
    noteLabel->setFocusPolicy(Qt::StrongFocus); // reachable with Tab for screen readers
    noteLabel->setAccessibleName(tr("Note about using your own account"));
    af->addRow(noteLabel);
    m_nickname = new QLineEdit(account);
    af->addRow(tr("Nic&kname:"), m_nickname);
    m_status = new QLineEdit(account);
    af->addRow(tr("Status &message:"), m_status);
    root->addWidget(account);

    // --- Channel ---------------------------------------------------------
    auto* channel = new QGroupBox(tr("Channel"), this);
    auto* cf = new QFormLayout(channel);
    m_channel = new QLineEdit(channel);
    m_channel->setPlaceholderText(tr("/ for the root channel, or e.g. /Lobby/"));
    m_channel->setAccessibleDescription(tr("The channel the monitor sits in. Its channel messages are reported. If it cannot join, it uses the root channel."));
    cf->addRow(tr("C&hannel to station in:"), m_channel);
    m_chanPassword = new QLineEdit(channel);
    m_chanPassword->setEchoMode(QLineEdit::Password);
    cf->addRow(tr("Channel pass&word:"), m_chanPassword);
    root->addWidget(channel);

    // --- Notifications ---------------------------------------------------
    auto* notify = new QGroupBox(tr("Notify me about"), this);
    auto* nl = new QVBoxLayout(notify);
    m_logins = new QCheckBox(tr("Users &logging in"), notify);
    m_logouts = new QCheckBox(tr("Users logging &out"), notify);
    m_private = new QCheckBox(tr("&Private messages to the monitor"), notify);
    m_channelMsgs = new QCheckBox(tr("Messages in the monitor's c&hannel"), notify);
    m_broadcasts = new QCheckBox(tr("Server &broadcasts"), notify);
    m_connection = new QCheckBox(tr("The monitor's own connection (online, lost, login failed)"), notify);
    for (QCheckBox* b : {m_logins, m_logouts, m_private, m_channelMsgs, m_broadcasts, m_connection})
        nl->addWidget(b);
    root->addWidget(notify);

    // --- Advanced --------------------------------------------------------
    auto* adv = new QGroupBox(tr("Advanced"), this);
    auto* al = new QVBoxLayout(adv);
    m_ignoreOwn = new QCheckBox(tr("Ignore logins and logouts of the monitor's own account"), adv);
    m_ignoreOwn->setAccessibleDescription(tr("When the monitor uses your account, those are your own sessions."));
    m_audio = new QCheckBox(tr("Receive channel audio (uses more bandwidth)"), adv);
    al->addWidget(m_ignoreOwn);
    al->addWidget(m_audio);
    root->addWidget(adv);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &MonitorEditDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);
    resize(560, 640);

    load(config);
    nameFormFields(this);
    (m_import ? static_cast<QWidget*>(m_import) : m_name)->setFocus();
}

void MonitorEditDialog::load(const MonitorConfig& c)
{
    m_name->setText(c.name);
    m_enabled->setChecked(c.enabled);
    m_host->setText(c.host);
    m_tcp->setValue(c.tcpPort);
    m_udp->setValue(c.udpPort);
    m_encrypted->setChecked(c.encrypted);
    m_username->setText(c.username);
    m_password->setText(c.password);
    m_nickname->setText(c.nickname);
    m_status->setText(c.statusMessage);
    m_channel->setText(c.channelPath);
    m_chanPassword->setText(c.channelPassword);
    m_logins->setChecked(c.notifyLogins);
    m_logouts->setChecked(c.notifyLogouts);
    m_private->setChecked(c.notifyPrivateMessages);
    m_channelMsgs->setChecked(c.notifyChannelMessages);
    m_broadcasts->setChecked(c.notifyBroadcasts);
    m_connection->setChecked(c.notifyConnection);
    m_ignoreOwn->setChecked(c.ignoreOwnAccount);
    m_audio->setChecked(c.receiveAudio);
}

void MonitorEditDialog::importSaved(int index)
{
    if (index <= 0 || index > m_saved.size())
        return;
    const SavedServer& s = m_saved.at(index - 1);
    if (m_name->text().trimmed().isEmpty())
        m_name->setText(s.name);
    m_host->setText(s.host);
    m_tcp->setValue(s.tcpPort);
    m_udp->setValue(s.udpPort);
    m_encrypted->setChecked(s.encrypted);
    m_username->setText(s.username);
    m_password->setText(s.password);
    if (!s.channel.isEmpty())
    {
        m_channel->setText(s.channel);
        m_chanPassword->setText(s.channelPassword);
    }
    m_host->setFocus();
}

MonitorConfig MonitorEditDialog::config() const
{
    MonitorConfig c = m_base;
    c.name = m_name->text().trimmed();
    c.enabled = m_enabled->isChecked();
    c.host = m_host->text().trimmed();
    c.tcpPort = m_tcp->value();
    c.udpPort = m_udp->value();
    c.encrypted = m_encrypted->isChecked();
    c.username = m_username->text().trimmed();
    c.password = m_password->text();
    c.nickname = m_nickname->text().trimmed();
    c.statusMessage = m_status->text();
    c.channelPath = normalizeChannelPath(m_channel->text());
    c.channelPassword = m_chanPassword->text();
    c.notifyLogins = m_logins->isChecked();
    c.notifyLogouts = m_logouts->isChecked();
    c.notifyPrivateMessages = m_private->isChecked();
    c.notifyChannelMessages = m_channelMsgs->isChecked();
    c.notifyBroadcasts = m_broadcasts->isChecked();
    c.notifyConnection = m_connection->isChecked();
    c.ignoreOwnAccount = m_ignoreOwn->isChecked();
    c.receiveAudio = m_audio->isChecked();
    return c;
}

void MonitorEditDialog::accept()
{
    const QString err = config().validationError();
    if (!err.isEmpty())
    {
        QMessageBox::warning(this, windowTitle(), err);
        if (m_host->text().trimmed().isEmpty())
            m_host->setFocus();
        else if (m_nickname->text().trimmed().isEmpty())
            m_nickname->setFocus();
        return;
    }
    QDialog::accept();
}

} // namespace ttc
