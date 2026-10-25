// SPDX-License-Identifier: GPL-3.0-or-later
#include "ttc/serverwindow.h"

#include "ttc/a11yaudit.h"
#include "ttc/localserver.h"

#include <QCheckBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHostAddress>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QShortcut>
#include <QSpinBox>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace ttc {

// --- AccountDialog -----------------------------------------------------------

AccountDialog::AccountDialog(const ServerAccount& account, const QStringList& takenNames, QWidget* parent)
    : QDialog(parent)
    , m_taken(takenNames)
{
    setWindowTitle(account.username.isEmpty() ? tr("Add Account") : tr("Edit Account"));
    auto* root = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    m_username = new QLineEdit(account.username, this);
    form->addRow(tr("&Username:"), m_username);
    m_password = new QLineEdit(account.password, this);
    m_password->setEchoMode(QLineEdit::Password);
    form->addRow(tr("&Password:"), m_password);
    m_admin = new QCheckBox(tr("&Administrator (can change the server, its channels and accounts)"), this);
    m_admin->setChecked(account.admin);
    form->addRow(m_admin);
    m_note = new QLineEdit(account.note, this);
    form->addRow(tr("&Note:"), m_note);
    root->addLayout(form);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        const QString err = validationError();
        if (!err.isEmpty())
        {
            QMessageBox::warning(this, windowTitle(), err);
            return;
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    nameFormFields(this);
}

ServerAccount AccountDialog::account() const
{
    return ServerAccount{m_username->text().trimmed(), m_password->text(), m_admin->isChecked(), m_note->text().trimmed()};
}

QString AccountDialog::validationError() const
{
    const QString u = m_username->text().trimmed();
    if (u.isEmpty())
        return tr("Enter a username.");
    for (const QString& t : m_taken)
        if (t.compare(u, Qt::CaseInsensitive) == 0)
            return tr("There is already an account named %1.").arg(u);
    return {};
}

// --- ServerWindow --------------------------------------------------------------

QString ServerWindow::autoStartKey()
{
    return QStringLiteral("ttcentral/server/auto-start");
}

ServerWindow::ServerWindow(LocalServer* server, QSettings* settings, ClientHooks hooks, QWidget* parent)
    : QDialog(parent)
    , m_server(server)
    , m_settings(settings)
    , m_hooks(std::move(hooks))
{
    setWindowTitle(tr("TT Central Local Server"));
    auto* outer = new QVBoxLayout(this);

    // Status and the main actions stay visible above the scrolling settings.
    auto* top = new QFormLayout;
    m_state = new QLineEdit(this);
    m_state->setReadOnly(true);
    top->addRow(tr("St&atus:"), m_state);
    outer->addLayout(top);
    auto* actions = new QHBoxLayout;
    m_run = new QPushButton(this);
    m_addToList = new QPushButton(tr("Add to &my server list"), this);
    m_addToList->setAccessibleDescription(tr("Adds this server to TT Central's server list, so you can connect to it."));
    auto* folder = new QPushButton(tr("Open server &folder"), this);
    actions->addWidget(m_run);
    actions->addWidget(m_addToList);
    actions->addWidget(folder);
    actions->addStretch();
    outer->addLayout(actions);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setFocusPolicy(Qt::NoFocus);
    scroll->setAccessibleName(tr("Server settings"));
    auto* content = new QWidget(scroll);
    auto* root = new QVBoxLayout(content);
    scroll->setWidget(content);
    outer->addWidget(scroll, 3);

    auto* intro = new QLabel(tr("Host a TeamTalk server on this computer with BearWare's TeamTalk server. "
                                "People on your network connect to this computer's address; for people on the "
                                "internet, your router must forward the TCP and UDP ports (or turn on UPnP below). "
                                "Windows may ask whether to allow the server through its firewall. This server "
                                "does not encrypt connections, and without a TeamTalk license it runs as a trial."),
                             content);
    intro->setWordWrap(true);
    intro->setFocusPolicy(Qt::StrongFocus);
    intro->setAccessibleName(intro->text());
    root->addWidget(intro);

    auto* general = new QGroupBox(tr("Server"), content);
    auto* gf = new QFormLayout(general);
    m_name = new QLineEdit(general);
    gf->addRow(tr("Server &name:"), m_name);
    m_motd = new QPlainTextEdit(general);
    m_motd->setTabChangesFocus(true);
    m_motd->setMaximumHeight(80);
    gf->addRow(tr("&Welcome message:"), m_motd);
    m_tcp = new QSpinBox(general);
    m_tcp->setRange(1, 65535);
    gf->addRow(tr("&TCP port:"), m_tcp);
    m_udp = new QSpinBox(general);
    m_udp->setRange(1, 65535);
    gf->addRow(tr("&UDP port:"), m_udp);
    m_bind = new QLineEdit(general);
    m_bind->setPlaceholderText(tr("empty: all of this computer's addresses"));
    gf->addRow(tr("Listen on address:"), m_bind);
    m_upnp = new QCheckBox(tr("Ask my router to open the ports (UPnP)"), general);
    gf->addRow(m_upnp);
    m_autoStart = new QCheckBox(tr("Start the server when TT Central starts"), general);
    gf->addRow(m_autoStart);
    root->addWidget(general);

    auto* limits = new QGroupBox(tr("Limits and security"), content);
    auto* lf = new QFormLayout(limits);
    m_maxUsers = new QSpinBox(limits);
    m_maxUsers->setRange(1, 100000);
    lf->addRow(tr("Most users online:"), m_maxUsers);
    m_maxPerIp = new QSpinBox(limits);
    m_maxPerIp->setRange(0, 1000);
    m_maxPerIp->setSpecialValueText(tr("No limit"));
    lf->addRow(tr("Logins per IP address:"), m_maxPerIp);
    m_loginAttempts = new QSpinBox(limits);
    m_loginAttempts->setRange(0, 1000);
    m_loginAttempts->setSpecialValueText(tr("Never ban"));
    lf->addRow(tr("Wrong passwords before a ban:"), m_loginAttempts);
    m_loginDelay = new QSpinBox(limits);
    m_loginDelay->setRange(0, 600000);
    m_loginDelay->setSingleStep(100);
    m_loginDelay->setSuffix(tr(" ms"));
    m_loginDelay->setSpecialValueText(tr("None"));
    lf->addRow(tr("Delay between logins from one address:"), m_loginDelay);
    root->addWidget(limits);

    auto* files = new QGroupBox(tr("File sharing"), content);
    auto* ff = new QFormLayout(files);
    auto* filesRow = new QWidget(files);
    auto* fh = new QHBoxLayout(filesRow);
    fh->setContentsMargins(0, 0, 0, 0);
    m_files = new QLineEdit(filesRow);
    m_files->setAccessibleName(tr("Folder for shared files (empty: no file sharing)"));
    m_files->setPlaceholderText(tr("empty: file sharing off"));
    auto* browse = new QPushButton(tr("Browse..."), filesRow);
    browse->setAccessibleName(tr("Browse for the shared files folder"));
    fh->addWidget(m_files, 1);
    fh->addWidget(browse);
    ff->addRow(tr("Folder for shared files:"), filesRow);
    m_diskMb = new QSpinBox(files);
    m_diskMb->setRange(0, 10000000);
    m_diskMb->setSuffix(tr(" MB"));
    m_diskMb->setSpecialValueText(tr("No limit"));
    ff->addRow(tr("Most disk space for files:"), m_diskMb);
    root->addWidget(files);

    auto* accounts = new QGroupBox(tr("Accounts"), content);
    auto* av = new QVBoxLayout(accounts);
    auto* accLabel = new QLabel(tr("A&ccounts (the server needs at least one administrator):"), accounts);
    m_accountList = new QTreeWidget(accounts);
    m_accountList->setHeaderLabels({tr("Username"), tr("Type"), tr("Note")});
    m_accountList->setRootIsDecorated(false);
    m_accountList->setAccessibleName(tr("Accounts"));
    m_accountList->setMinimumHeight(120);
    accLabel->setBuddy(m_accountList);
    av->addWidget(accLabel);
    av->addWidget(m_accountList);
    auto* accButtons = new QHBoxLayout;
    auto* add = new QPushButton(tr("A&dd account..."), accounts);
    auto* edit = new QPushButton(tr("&Edit account..."), accounts);
    auto* remove = new QPushButton(tr("&Remove account"), accounts);
    accButtons->addWidget(add);
    accButtons->addWidget(edit);
    accButtons->addWidget(remove);
    accButtons->addStretch();
    av->addLayout(accButtons);
    root->addWidget(accounts);

    auto* logLabel = new QLabel(tr("Server &log (newest first):"), this);
    m_log = new QListWidget(this);
    m_log->setAccessibleName(tr("Server log"));
    logLabel->setBuddy(m_log);
    outer->addWidget(logLabel);
    outer->addWidget(m_log, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto* saveButton = buttons->addButton(tr("Sa&ve settings"), QDialogButtonBox::ApplyRole);
    saveButton->setAccessibleDescription(tr("Saves the settings. A running server restarts to use them."));
    outer->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    connect(saveButton, &QPushButton::clicked, this, [this] { save(); });
    connect(m_run, &QPushButton::clicked, this, [this] {
        if (m_server->isRunning())
        {
            m_server->stop();
            return;
        }
        if (save())
            m_server->start();
    });
    connect(m_addToList, &QPushButton::clicked, this, &ServerWindow::addToServerList);
    connect(folder, &QPushButton::clicked, this, [this] {
        QDir().mkpath(m_server->workDir());
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_server->workDir()));
    });
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString d = QFileDialog::getExistingDirectory(this, tr("Folder for shared files"), m_files->text());
        if (!d.isEmpty())
            m_files->setText(QDir::toNativeSeparators(d));
    });
    connect(add, &QPushButton::clicked, this, &ServerWindow::addAccount);
    connect(edit, &QPushButton::clicked, this, &ServerWindow::editAccount);
    connect(remove, &QPushButton::clicked, this, &ServerWindow::removeAccount);
    connect(m_accountList, &QTreeWidget::itemActivated, this, &ServerWindow::editAccount);
    auto* del = new QShortcut(QKeySequence::Delete, m_accountList, nullptr, nullptr, Qt::WidgetShortcut);
    connect(del, &QShortcut::activated, this, &ServerWindow::removeAccount);
    connect(m_server, &LocalServer::stateChanged, this, &ServerWindow::refresh);
    connect(m_server, &LocalServer::logLine, this, [this](const QString& line) {
        m_log->insertItem(0, line);
        while (m_log->count() > 500)
            delete m_log->takeItem(m_log->count() - 1);
    });

    for (QPushButton* b : findChildren<QPushButton*>())
        b->setAutoDefault(false);
    nameFormFields(this);
    load();
    const QStringList tail = m_server->logTail();
    for (const QString& line : tail)
        m_log->insertItem(0, line);
    refresh();
    resize(780, 680);
}

void ServerWindow::load()
{
    const LocalServerConfig c = m_server->config();
    m_name->setText(c.serverName);
    m_motd->setPlainText(c.motd);
    m_tcp->setValue(c.tcpPort);
    m_udp->setValue(c.udpPort);
    m_bind->setText(c.bindIp);
    m_upnp->setChecked(c.upnp);
    m_maxUsers->setValue(c.maxUsers);
    m_maxPerIp->setValue(c.maxLoginsPerIp);
    m_loginAttempts->setValue(c.loginAttempts);
    m_loginDelay->setValue(c.loginDelayMsec);
    m_files->setText(QDir::toNativeSeparators(c.filesRoot));
    m_diskMb->setValue(int(c.maxDiskUsage / (1024 * 1024)));
    m_accounts = c.accounts;
    m_autoStart->setChecked(m_settings && m_settings->value(autoStartKey(), false).toBool());
    refreshAccounts();
}

LocalServerConfig ServerWindow::current() const
{
    LocalServerConfig c = m_server->config();
    c.serverName = m_name->text().trimmed();
    c.motd = m_motd->toPlainText().trimmed();
    c.tcpPort = m_tcp->value();
    c.udpPort = m_udp->value();
    c.bindIp = m_bind->text().trimmed();
    c.upnp = m_upnp->isChecked();
    c.maxUsers = m_maxUsers->value();
    c.maxLoginsPerIp = m_maxPerIp->value();
    c.loginAttempts = m_loginAttempts->value();
    c.loginDelayMsec = m_loginDelay->value();
    c.filesRoot = QDir::fromNativeSeparators(m_files->text().trimmed());
    c.maxDiskUsage = qint64(m_diskMb->value()) * 1024 * 1024;
    c.accounts = m_accounts;
    c.autoSave = true;
    return c;
}

void ServerWindow::showWarning(const QString& text)
{
    if (warn)
        warn(text);
    else
        QMessageBox::warning(this, windowTitle(), text);
}

bool ServerWindow::save()
{
    const LocalServerConfig c = current();
    if (!c.filesRoot.isEmpty() && !QDir(c.filesRoot).exists() && !QDir().mkpath(c.filesRoot))
    {
        showWarning(tr("The folder for shared files can't be created."));
        return false;
    }
    QString err;
    if (!m_server->setConfig(c, &err))
    {
        showWarning(err);
        return false;
    }
    if (m_settings)
    {
        m_settings->setValue(autoStartKey(), m_autoStart->isChecked());
        m_settings->sync();
    }
    refresh();
    return true;
}

void ServerWindow::refresh()
{
    m_run->setText(m_server->isRunning() ? tr("Stop &server") : tr("Start &server"));
    m_state->setText(m_server->stateText());
    m_addToList->setEnabled(bool(m_hooks.addSavedServer));
}

void ServerWindow::refreshAccounts()
{
    const int keep = m_accountList->indexOfTopLevelItem(m_accountList->currentItem());
    m_accountList->clear();
    for (const ServerAccount& a : std::as_const(m_accounts))
        m_accountList->addTopLevelItem(new QTreeWidgetItem(
            QStringList{a.username, a.admin ? tr("Administrator") : tr("User"), a.note}));
    if (m_accountList->topLevelItemCount() > 0)
        m_accountList->setCurrentItem(m_accountList->topLevelItem(qBound(0, keep, m_accountList->topLevelItemCount() - 1)));
}

void ServerWindow::addAccount()
{
    QStringList taken;
    for (const ServerAccount& a : std::as_const(m_accounts))
        taken << a.username;
    ServerAccount fresh;
    fresh.admin = std::none_of(m_accounts.cbegin(), m_accounts.cend(), [](const ServerAccount& a) { return a.admin; });
    AccountDialog dlg(fresh, taken, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    m_accounts << dlg.account();
    refreshAccounts();
    m_accountList->setCurrentItem(m_accountList->topLevelItem(m_accountList->topLevelItemCount() - 1));
}

void ServerWindow::editAccount()
{
    const int row = m_accountList->indexOfTopLevelItem(m_accountList->currentItem());
    if (row < 0 || row >= m_accounts.size())
        return;
    QStringList taken;
    for (int i = 0; i < m_accounts.size(); ++i)
        if (i != row)
            taken << m_accounts.at(i).username;
    AccountDialog dlg(m_accounts.at(row), taken, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    m_accounts[row] = dlg.account();
    refreshAccounts();
}

void ServerWindow::removeAccount()
{
    const int row = m_accountList->indexOfTopLevelItem(m_accountList->currentItem());
    if (row < 0 || row >= m_accounts.size())
        return;
    m_accounts.removeAt(row);
    refreshAccounts();
}

void ServerWindow::addToServerList()
{
    if (!m_hooks.addSavedServer)
        return;
    const LocalServerConfig c = current();
    SavedServer s;
    s.name = c.serverName.isEmpty() ? tr("My local server") : c.serverName;
    s.host = c.bindIp.isEmpty() || c.bindIp == QLatin1String("0.0.0.0") ? QStringLiteral("127.0.0.1") : c.bindIp;
    s.tcpPort = c.tcpPort;
    s.udpPort = c.udpPort;
    for (const ServerAccount& a : c.accounts)
        if (a.admin)
        {
            s.username = a.username;
            s.password = a.password;
            break;
        }
    m_hooks.addSavedServer(s);
    m_state->setText(tr("Added %1 to your server list.").arg(s.name));
}

} // namespace ttc
