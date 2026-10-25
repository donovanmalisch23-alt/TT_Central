// TT Central — add/edit one background monitor.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ttc/clienthooks.h"
#include "ttc/monitorconfig.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;

namespace ttc {

class MonitorEditDialog : public QDialog
{
    Q_OBJECT
public:
    MonitorEditDialog(const MonitorConfig& config, const QList<SavedServer>& savedServers,
                      QWidget* parent = nullptr);

    MonitorConfig config() const;

public slots:
    void accept() override;

private:
    void importSaved(int index);
    void load(const MonitorConfig& c);

    MonitorConfig m_base;
    QList<SavedServer> m_saved;

    QComboBox* m_import = nullptr;
    QLineEdit* m_name = nullptr;
    QCheckBox* m_enabled = nullptr;
    QLineEdit* m_host = nullptr;
    QSpinBox* m_tcp = nullptr;
    QSpinBox* m_udp = nullptr;
    QCheckBox* m_encrypted = nullptr;
    QLineEdit* m_username = nullptr;
    QLineEdit* m_password = nullptr;
    QCheckBox* m_showPassword = nullptr;
    QLineEdit* m_nickname = nullptr;
    QLineEdit* m_status = nullptr;
    QLineEdit* m_channel = nullptr;
    QLineEdit* m_chanPassword = nullptr;
    QCheckBox* m_logins = nullptr;
    QCheckBox* m_logouts = nullptr;
    QCheckBox* m_private = nullptr;
    QCheckBox* m_channelMsgs = nullptr;
    QCheckBox* m_broadcasts = nullptr;
    QCheckBox* m_connection = nullptr;
    QCheckBox* m_ignoreOwn = nullptr;
    QCheckBox* m_audio = nullptr;
};

} // namespace ttc
