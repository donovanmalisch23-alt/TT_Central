// TT Central — Settings > Media bot: the bot's account, behaviour, audio,
// sources and helper tools.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ttc/media/botconfig.h"
#include "ttc/settingsdialog.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;

namespace ttc {

class MediaBotController;

class MediaBotPage : public SettingsPage
{
    Q_OBJECT
public:
    MediaBotPage(MediaBotController* bot, std::function<QList<SavedServer>()> savedServers, QWidget* parent = nullptr);
    QString title() const override { return tr("Media bot"); }
    void load() override;
    bool apply() override;

    /// The settings as currently entered (for tests).
    media::MediaBotConfig current() const;
    /// Shows why the settings can't be saved (a message box by default).
    std::function<void(const QString&)> warn;

private:
    void importSaved(int index);
    void updateTools();
    QLineEdit* pathField(QWidget* parent, const QString& name, QWidget** row);

    MediaBotController* m_bot;
    QList<SavedServer> m_saved;
    media::MediaBotConfig m_base;

    QComboBox* m_import = nullptr;
    QLineEdit* m_name = nullptr;
    QLineEdit* m_host = nullptr;
    QSpinBox* m_tcp = nullptr;
    QSpinBox* m_udp = nullptr;
    QCheckBox* m_encrypted = nullptr;
    QLineEdit* m_username = nullptr;
    QLineEdit* m_password = nullptr;
    QLineEdit* m_nickname = nullptr;
    QLineEdit* m_status = nullptr;
    QLineEdit* m_channel = nullptr;
    QLineEdit* m_channelPassword = nullptr;
    QCheckBox* m_autoStart = nullptr;
    QLineEdit* m_admins = nullptr;
    QLineEdit* m_prefix = nullptr;
    QCheckBox* m_announce = nullptr;
    QSpinBox* m_maxMinutes = nullptr;
    QSpinBox* m_defaultVolume = nullptr;
    QSpinBox* m_maxVolume = nullptr;
    QCheckBox* m_youtube = nullptr;
    QCheckBox* m_soundcloud = nullptr;
    QCheckBox* m_spotify = nullptr;
    QLineEdit* m_spotifyId = nullptr;
    QLineEdit* m_spotifySecret = nullptr;
    QCheckBox* m_apple = nullptr;
    QCheckBox* m_direct = nullptr;
    QLineEdit* m_ffmpeg = nullptr;
    QLineEdit* m_ytdlp = nullptr;
    QLineEdit* m_deno = nullptr;
    QLabel* m_toolsFound = nullptr;
};

} // namespace ttc
