// TT Central — the Local Server window: host a TeamTalk server on this
// computer (settings, accounts, start/stop, and its log).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ttc/clienthooks.h"
#include "ttc/serverconfig.h"

#include <QDialog>

class QCheckBox;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QSettings;
class QSpinBox;
class QTreeWidget;

namespace ttc {

class LocalServer;

/// Add or edit one server account.
class AccountDialog : public QDialog
{
    Q_OBJECT
public:
    AccountDialog(const ServerAccount& account, const QStringList& takenNames, QWidget* parent = nullptr);
    ServerAccount account() const;
    /// Empty when acceptable.
    QString validationError() const;

private:
    QStringList m_taken;
    QLineEdit* m_username = nullptr;
    QLineEdit* m_password = nullptr;
    QCheckBox* m_admin = nullptr;
    QLineEdit* m_note = nullptr;
};

class ServerWindow : public QDialog
{
    Q_OBJECT
public:
    ServerWindow(LocalServer* server, QSettings* settings, ClientHooks hooks, QWidget* parent = nullptr);

    /// The settings as entered (for tests).
    LocalServerConfig current() const;
    /// Save the settings (restarting a running server). False when invalid.
    bool save();
    /// Shows why settings can't be saved (a message box by default).
    std::function<void(const QString&)> warn;

    /// Settings key: start the local server when TT Central starts.
    static QString autoStartKey();

private:
    void load();
    void refresh();
    void refreshAccounts();
    void addAccount();
    void editAccount();
    void removeAccount();
    void addToServerList();
    void showWarning(const QString& text);

    LocalServer* m_server;
    QSettings* m_settings;
    ClientHooks m_hooks;
    QList<ServerAccount> m_accounts;

    QLineEdit* m_state = nullptr;
    QPushButton* m_run = nullptr;
    QPushButton* m_addToList = nullptr;
    QLineEdit* m_name = nullptr;
    QPlainTextEdit* m_motd = nullptr;
    QSpinBox* m_tcp = nullptr;
    QSpinBox* m_udp = nullptr;
    QLineEdit* m_bind = nullptr;
    QSpinBox* m_maxUsers = nullptr;
    QSpinBox* m_maxPerIp = nullptr;
    QSpinBox* m_loginAttempts = nullptr;
    QSpinBox* m_loginDelay = nullptr;
    QCheckBox* m_upnp = nullptr;
    QLineEdit* m_files = nullptr;
    QSpinBox* m_diskMb = nullptr;
    QCheckBox* m_autoStart = nullptr;
    QTreeWidget* m_accountList = nullptr;
    QListWidget* m_log = nullptr;
};

} // namespace ttc
