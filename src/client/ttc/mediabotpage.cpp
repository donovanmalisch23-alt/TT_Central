// SPDX-License-Identifier: GPL-3.0-or-later
#include "ttc/mediabotpage.h"

#include "ttc/mediabotcontroller.h"
#include "ttc/monitorconfig.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

namespace ttc {

MediaBotPage::MediaBotPage(MediaBotController* bot, std::function<QList<SavedServer>()> savedServers, QWidget* parent)
    : SettingsPage(parent)
    , m_bot(bot)
    , m_saved(savedServers ? savedServers() : QList<SavedServer>{})
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setFocusPolicy(Qt::NoFocus);
    scroll->setAccessibleName(tr("Media bot settings"));
    auto* content = new QWidget(scroll);
    auto* root = new QVBoxLayout(content);
    scroll->setWidget(content);
    outer->addWidget(scroll);

    auto* intro = new QLabel(tr("The media bot is a separate TeamTalk login that plays music into its channel. "
                                "People send it commands in a private message (send it 'h' for help), and you "
                                "can control it from TT Central > Media bot. Give it its own account: it is online "
                                "at the same time as you, and some servers allow only one login per IP address."),
                             content);
    intro->setWordWrap(true);
    intro->setFocusPolicy(Qt::StrongFocus); // so screen-reader users can Tab to the explanation
    intro->setAccessibleName(intro->text());
    root->addWidget(intro);

    if (!m_saved.isEmpty())
    {
        auto* f = new QFormLayout;
        m_import = new QComboBox(content);
        m_import->addItem(tr("(choose a saved server to copy its address)"));
        for (const SavedServer& s : std::as_const(m_saved))
            m_import->addItem(s.name.isEmpty() ? s.host : tr("%1 (%2)").arg(s.name, s.host));
        f->addRow(tr("&Import from saved server:"), m_import);
        root->addLayout(f);
        connect(m_import, qOverload<int>(&QComboBox::activated), this, &MediaBotPage::importSaved);
    }

    // --- Server and account ---------------------------------------------------
    auto* server = new QGroupBox(tr("Server and account"), content);
    auto* sf = new QFormLayout(server);
    m_name = new QLineEdit(server);
    m_name->setPlaceholderText(tr("e.g. Music on Home Server"));
    sf->addRow(tr("&Name:"), m_name);
    m_host = new QLineEdit(server);
    sf->addRow(tr("Server &address:"), m_host);
    m_tcp = new QSpinBox(server);
    m_tcp->setRange(1, 65535);
    sf->addRow(tr("TCP port:"), m_tcp);
    m_udp = new QSpinBox(server);
    m_udp->setRange(1, 65535);
    sf->addRow(tr("UDP port:"), m_udp);
    m_encrypted = new QCheckBox(tr("Encrypted server"), server);
    sf->addRow(m_encrypted);
    m_username = new QLineEdit(server);
    m_username->setAccessibleDescription(tr("The bot's own TeamTalk account."));
    sf->addRow(tr("Bot &username:"), m_username);
    m_password = new QLineEdit(server);
    m_password->setEchoMode(QLineEdit::Password);
    sf->addRow(tr("Bot pass&word:"), m_password);
    m_nickname = new QLineEdit(server);
    sf->addRow(tr("Nickname:"), m_nickname);
    m_status = new QLineEdit(server);
    sf->addRow(tr("Status message:"), m_status);
    m_channel = new QLineEdit(server);
    m_channel->setPlaceholderText(tr("/ for the root channel, or a path like /Music/"));
    sf->addRow(tr("C&hannel:"), m_channel);
    m_channelPassword = new QLineEdit(server);
    m_channelPassword->setEchoMode(QLineEdit::Password);
    sf->addRow(tr("Channel password:"), m_channelPassword);
    root->addWidget(server);

    // --- Behaviour ------------------------------------------------------------
    auto* behaviour = new QGroupBox(tr("Behaviour"), content);
    auto* bf = new QFormLayout(behaviour);
    m_autoStart = new QCheckBox(tr("Start the bot when TT Central &starts"), behaviour);
    bf->addRow(m_autoStart);
    m_admins = new QLineEdit(behaviour);
    m_admins->setPlaceholderText(tr("usernames, separated by commas"));
    m_admins->setAccessibleDescription(tr("These TeamTalk usernames may use the bot's administrator commands "
                                          "(say, update, quit). Server administrators always can."));
    bf->addRow(tr("Bot ad&ministrators:"), m_admins);
    m_prefix = new QLineEdit(behaviour);
    m_prefix->setPlaceholderText(tr("e.g. !  (empty: commands only by private message)"));
    m_prefix->setAccessibleDescription(tr("With a prefix such as !, people can also type commands in the channel, "
                                          "like !p song name. Leave empty to accept commands only by private message."));
    bf->addRow(tr("Channel command prefix:"), m_prefix);
    m_announce = new QCheckBox(tr("Announce each new track in the channel"), behaviour);
    bf->addRow(m_announce);
    m_maxMinutes = new QSpinBox(behaviour);
    m_maxMinutes->setRange(0, 600);
    m_maxMinutes->setSpecialValueText(tr("No limit"));
    m_maxMinutes->setSuffix(tr(" minutes"));
    bf->addRow(tr("Longest track allowed:"), m_maxMinutes);
    root->addWidget(behaviour);

    // --- Audio --------------------------------------------------------------
    auto* audio = new QGroupBox(tr("Volume"), content);
    auto* af = new QFormLayout(audio);
    m_defaultVolume = new QSpinBox(audio);
    m_defaultVolume->setRange(0, 400);
    m_defaultVolume->setSuffix(QStringLiteral("%"));
    af->addRow(tr("Starting volume:"), m_defaultVolume);
    m_maxVolume = new QSpinBox(audio);
    m_maxVolume->setRange(1, 400);
    m_maxVolume->setSuffix(QStringLiteral("%"));
    af->addRow(tr("Highest volume people may set:"), m_maxVolume);
    root->addWidget(audio);

    // --- Sources --------------------------------------------------------------
    auto* sources = new QGroupBox(tr("Sources"), content);
    auto* srcf = new QFormLayout(sources);
    m_youtube = new QCheckBox(tr("YouTube (search and links)"), sources);
    m_soundcloud = new QCheckBox(tr("SoundCloud (search and links)"), sources);
    m_apple = new QCheckBox(tr("Apple Music (search, song and album links; played from YouTube)"), sources);
    m_spotify = new QCheckBox(tr("Spotify links (played from YouTube)"), sources);
    m_direct = new QCheckBox(tr("Direct links and internet radio"), sources);
    for (QCheckBox* c : {m_youtube, m_soundcloud, m_apple, m_spotify, m_direct})
        srcf->addRow(c);
    m_spotifyId = new QLineEdit(sources);
    m_spotifyId->setAccessibleDescription(tr("Optional. Spotify albums and playlists need a free app from "
                                             "developer.spotify.com; single tracks work without it."));
    srcf->addRow(tr("Spotify client ID (optional):"), m_spotifyId);
    m_spotifySecret = new QLineEdit(sources);
    m_spotifySecret->setEchoMode(QLineEdit::Password);
    srcf->addRow(tr("Spotify client secret:"), m_spotifySecret);
    root->addWidget(sources);

    // --- Tools ----------------------------------------------------------------
    auto* tools = new QGroupBox(tr("Helper programs"), content);
    auto* tf = new QFormLayout(tools);
    QWidget* row = nullptr;
    m_ffmpeg = pathField(tools, tr("ffmpeg"), &row);
    tf->addRow(tr("ffmpeg (leave empty to find it):"), row);
    m_ytdlp = pathField(tools, tr("yt-dlp"), &row);
    tf->addRow(tr("yt-dlp (leave empty to find it):"), row);
    m_deno = pathField(tools, tr("Deno"), &row);
    tf->addRow(tr("Deno (leave empty to find it):"), row);
    m_toolsFound = new QLabel(tools);
    m_toolsFound->setWordWrap(true);
    m_toolsFound->setFocusPolicy(Qt::StrongFocus);
    m_toolsFound->setTextInteractionFlags(Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse);
    tf->addRow(m_toolsFound);
    root->addWidget(tools);
    root->addStretch();

    for (QLineEdit* e : {m_ffmpeg, m_ytdlp, m_deno})
        connect(e, &QLineEdit::editingFinished, this, &MediaBotPage::updateTools);
}

QLineEdit* MediaBotPage::pathField(QWidget* parent, const QString& name, QWidget** row)
{
    auto* w = new QWidget(parent);
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    auto* edit = new QLineEdit(w);
    edit->setAccessibleName(tr("%1 program (leave empty to find it)").arg(name));
    auto* browse = new QPushButton(tr("Browse..."), w);
    browse->setAccessibleName(tr("Browse for %1").arg(name));
    h->addWidget(edit, 1);
    h->addWidget(browse);
    connect(browse, &QPushButton::clicked, this, [this, edit, name] {
        const QString f = QFileDialog::getOpenFileName(this, tr("Choose %1").arg(name), edit->text());
        if (!f.isEmpty())
        {
            edit->setText(QDir::toNativeSeparators(f));
            updateTools();
        }
    });
    *row = w;
    return edit;
}

void MediaBotPage::importSaved(int index)
{
    if (index <= 0 || index > m_saved.size())
        return;
    const SavedServer& s = m_saved.at(index - 1);
    m_name->setText(s.name);
    m_host->setText(s.host);
    m_tcp->setValue(s.tcpPort);
    m_udp->setValue(s.udpPort);
    m_encrypted->setChecked(s.encrypted);
    if (!s.channel.isEmpty())
        m_channel->setText(s.channel);
    // The account is deliberately not copied: the bot should have its own.
    m_username->setFocus();
}

void MediaBotPage::updateTools()
{
    const QStringList dirs = media::defaultToolDirs();
    QStringList lines;
    auto line = [&](const QString& label, const QString& configured, const char* name, bool required) {
        const QString found = media::findTool(configured, QLatin1String(name), dirs);
        if (found.isEmpty())
            lines << (required ? tr("%1: not found. The bot can't play anything without it.").arg(label)
                               : tr("%1: not found.").arg(label));
        else
            lines << tr("%1: %2").arg(label, QDir::toNativeSeparators(found));
    };
    line(tr("ffmpeg"), m_ffmpeg->text(), "ffmpeg", true);
    line(tr("yt-dlp"), m_ytdlp->text(), "yt-dlp", false);
    line(tr("Deno"), m_deno->text(), "deno", false);
    m_toolsFound->setText(lines.join(QChar('\n')));
    m_toolsFound->setAccessibleName(tr("Helper programs found: %1").arg(lines.join(QStringLiteral("; "))));
}

void MediaBotPage::load()
{
    m_base = m_bot->config();
    const media::MediaBotConfig& c = m_base;
    m_name->setText(c.name);
    m_host->setText(c.host);
    m_tcp->setValue(c.tcpPort);
    m_udp->setValue(c.udpPort);
    m_encrypted->setChecked(c.encrypted);
    m_username->setText(c.username);
    m_password->setText(c.password);
    m_nickname->setText(c.nickname);
    m_status->setText(c.statusMessage);
    m_channel->setText(c.channelPath);
    m_channelPassword->setText(c.channelPassword);
    m_autoStart->setChecked(c.autoStart);
    m_admins->setText(c.admins.join(QStringLiteral(", ")));
    m_prefix->setText(c.channelPrefix);
    m_announce->setChecked(c.announceNowPlaying);
    m_maxMinutes->setValue(c.maxTrackSeconds / 60);
    m_defaultVolume->setValue(c.audio.defaultVolume);
    m_maxVolume->setValue(c.audio.maxVolume);
    m_youtube->setChecked(c.sources.youtube);
    m_soundcloud->setChecked(c.sources.soundcloud);
    m_spotify->setChecked(c.sources.spotify);
    m_spotifyId->setText(c.sources.spotifyClientId);
    m_spotifySecret->setText(c.sources.spotifyClientSecret);
    m_apple->setChecked(c.sources.appleMusic);
    m_direct->setChecked(c.sources.directUrl);
    m_ffmpeg->setText(c.ffmpegPath);
    m_ytdlp->setText(c.sources.ytdlpPath);
    m_deno->setText(c.sources.denoPath);
    updateTools();
}

media::MediaBotConfig MediaBotPage::current() const
{
    media::MediaBotConfig c = m_base;
    c.name = m_name->text().trimmed();
    c.host = m_host->text().trimmed();
    c.tcpPort = m_tcp->value();
    c.udpPort = m_udp->value();
    c.encrypted = m_encrypted->isChecked();
    c.username = m_username->text().trimmed();
    c.password = m_password->text();
    c.nickname = m_nickname->text().trimmed();
    c.statusMessage = m_status->text();
    c.channelPath = normalizeChannelPath(m_channel->text());
    c.channelPassword = m_channelPassword->text();
    c.autoStart = m_autoStart->isChecked();
    c.admins.clear();
    for (const QString& a : m_admins->text().split(QChar(','), Qt::SkipEmptyParts))
        if (!a.trimmed().isEmpty())
            c.admins << a.trimmed();
    c.channelPrefix = m_prefix->text().trimmed();
    c.announceNowPlaying = m_announce->isChecked();
    c.maxTrackSeconds = m_maxMinutes->value() * 60;
    c.audio.defaultVolume = m_defaultVolume->value();
    c.audio.maxVolume = m_maxVolume->value();
    c.sources.youtube = m_youtube->isChecked();
    c.sources.soundcloud = m_soundcloud->isChecked();
    c.sources.spotify = m_spotify->isChecked();
    c.sources.spotifyClientId = m_spotifyId->text().trimmed();
    c.sources.spotifyClientSecret = m_spotifySecret->text().trimmed();
    c.sources.appleMusic = m_apple->isChecked();
    c.sources.directUrl = m_direct->isChecked();
    c.ffmpegPath = m_ffmpeg->text().trimmed();
    c.sources.ytdlpPath = m_ytdlp->text().trimmed();
    c.sources.denoPath = m_deno->text().trimmed();
    return c;
}

bool MediaBotPage::apply()
{
    const media::MediaBotConfig c = current();
    // An empty server means "not set up yet"; that's fine unless the bot
    // should start by itself.
    const bool blank = c.host.isEmpty() && c.username.isEmpty();
    if (!blank || c.autoStart)
    {
        const QString err = c.validationError();
        if (!err.isEmpty())
        {
            if (warn)
                warn(err);
            else
                QMessageBox::warning(this, tr("Media bot"), err);
            return false;
        }
    }
    if (c != m_bot->config())
        m_bot->setConfig(c);
    m_base = c;
    return true;
}

} // namespace ttc
