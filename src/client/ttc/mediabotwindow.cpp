// SPDX-License-Identifier: GPL-3.0-or-later
#include "ttc/mediabotwindow.h"

#include "ttc/mediabotcontroller.h"
#include "ttc/media/track.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QShortcut>
#include <QSpinBox>
#include <QTime>
#include <QVBoxLayout>

namespace ttc {

namespace {
constexpr int kLogLimit = 300;
}

MediaBotWindow::MediaBotWindow(MediaBotController* bot, QWidget* parent)
    : QDialog(parent)
    , m_bot(bot)
{
    setWindowTitle(tr("TT Central Media Bot"));
    auto* root = new QVBoxLayout(this);

    // --- Status ------------------------------------------------------------
    auto* form = new QFormLayout;
    auto readOnly = [this](const QString& name) {
        auto* e = new QLineEdit(this);
        e->setReadOnly(true);
        e->setAccessibleName(name);
        return e;
    };
    m_state = readOnly(tr("Bot status"));
    m_now = readOnly(tr("Now playing"));
    m_position = readOnly(tr("Position"));
    form->addRow(tr("Bot &status:"), m_state);
    form->addRow(tr("&Now playing:"), m_now);
    form->addRow(tr("Pos&ition:"), m_position);
    root->addLayout(form);

    // --- Controls ------------------------------------------------------------
    auto* controls = new QHBoxLayout;
    m_run = new QPushButton(this);
    m_playPause = new QPushButton(tr("P&ause"), this);
    m_prev = new QPushButton(tr("&Previous"), this);
    m_next = new QPushButton(tr("Ne&xt"), this);
    m_stopPlayback = new QPushButton(tr("Stop pla&yback"), this);
    for (QPushButton* b : {m_run, m_playPause, m_prev, m_next, m_stopPlayback})
        controls->addWidget(b);
    auto* volLabel = new QLabel(tr("&Volume:"), this);
    m_volume = new QSpinBox(this);
    m_volume->setRange(0, 200);
    m_volume->setSuffix(QStringLiteral("%"));
    m_volume->setAccessibleName(tr("Volume"));
    volLabel->setBuddy(m_volume);
    controls->addWidget(volLabel);
    controls->addWidget(m_volume);
    controls->addStretch();
    root->addLayout(controls);

    // --- Queue -----------------------------------------------------------------
    auto* addRow = new QHBoxLayout;
    auto* addLabel = new QLabel(tr("Search or &link:"), this);
    m_add = new QLineEdit(this);
    m_add->setAccessibleName(tr("Search words or a link to play"));
    m_add->setPlaceholderText(tr("Words to search for, or a YouTube, SoundCloud, Spotify, Apple Music or radio link"));
    addLabel->setBuddy(m_add);
    auto* addButton = new QPushButton(tr("A&dd to queue"), this);
    addRow->addWidget(addLabel);
    addRow->addWidget(m_add, 1);
    addRow->addWidget(addButton);
    root->addLayout(addRow);

    auto* queueLabel = new QLabel(tr("&Queue (Enter plays the selected track, Delete removes it):"), this);
    m_queue = new QListWidget(this);
    m_queue->setAccessibleName(tr("Queue"));
    m_queue->setAccessibleDescription(tr("Tracks waiting to play. Enter plays the selected track now; Delete removes it."));
    queueLabel->setBuddy(m_queue);
    root->addWidget(queueLabel);
    root->addWidget(m_queue, 2);

    // --- Commands and log ------------------------------------------------------
    auto* cmdRow = new QHBoxLayout;
    auto* cmdLabel = new QLabel(tr("&Command:"), this);
    m_command = new QLineEdit(this);
    m_command->setAccessibleName(tr("Bot command"));
    m_command->setAccessibleDescription(tr("Any command the bot understands in a private message, for example: "
                                           "repeat queue, shuffle, seek 1:30, h for help."));
    m_command->setPlaceholderText(tr("For example: h, repeat queue, shuffle, seek 1:30"));
    cmdLabel->setBuddy(m_command);
    auto* sendButton = new QPushButton(tr("S&end"), this);
    cmdRow->addWidget(cmdLabel);
    cmdRow->addWidget(m_command, 1);
    cmdRow->addWidget(sendButton);
    root->addLayout(cmdRow);

    auto* logLabel = new QLabel(tr("&Replies and log (newest first):"), this);
    m_log = new QListWidget(this);
    m_log->setAccessibleName(tr("Replies and log"));
    m_log->setWordWrap(true);
    logLabel->setBuddy(m_log);
    root->addWidget(logLabel);
    root->addWidget(m_log, 2);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto* settings = buttons->addButton(tr("Media bot se&ttings..."), QDialogButtonBox::ActionRole);
    root->addWidget(buttons);

    // --- Wiring ------------------------------------------------------------------
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    connect(settings, &QPushButton::clicked, this, &MediaBotWindow::settingsRequested);
    connect(m_run, &QPushButton::clicked, this, [this] {
        if (m_bot->isRunning())
            m_bot->stop();
        else if (!m_bot->isConfigured())
            emit settingsRequested();
        else
            m_bot->start();
    });
    connect(m_playPause, &QPushButton::clicked, this, [this] { send(QStringLiteral("pp")); });
    connect(m_prev, &QPushButton::clicked, this, [this] { send(QStringLiteral("prev")); });
    connect(m_next, &QPushButton::clicked, this, [this] { send(QStringLiteral("next")); });
    connect(m_stopPlayback, &QPushButton::clicked, this, [this] { send(QStringLiteral("stop")); });
    connect(m_volume, &QSpinBox::editingFinished, this, [this] {
        if (!m_updatingVolume)
            send(QStringLiteral("v %1").arg(m_volume->value()));
    });
    auto addNow = [this] {
        const QString text = m_add->text().trimmed();
        if (text.isEmpty())
            return;
        send(QStringLiteral("p ") + text);
        m_add->clear();
    };
    connect(m_add, &QLineEdit::returnPressed, this, addNow);
    connect(addButton, &QPushButton::clicked, this, addNow);
    auto sendNow = [this] {
        const QString text = m_command->text().trimmed();
        if (text.isEmpty())
            return;
        send(text);
        m_command->clear();
    };
    connect(m_command, &QLineEdit::returnPressed, this, sendNow);
    connect(sendButton, &QPushButton::clicked, this, sendNow);
    connect(m_queue, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
        send(QStringLiteral("go %1").arg(m_queue->row(item) + 1));
    });
    auto* del = new QShortcut(QKeySequence::Delete, m_queue, nullptr, nullptr, Qt::WidgetShortcut);
    connect(del, &QShortcut::activated, this, [this] {
        if (m_queue->currentRow() >= 0)
            send(QStringLiteral("rm %1").arg(m_queue->currentRow() + 1));
    });

    connect(m_bot, &MediaBotController::stateChanged, this, &MediaBotWindow::refresh);
    connect(m_bot, &MediaBotController::statusChanged, this, &MediaBotWindow::applyStatus);
    connect(m_bot, &MediaBotController::replyReceived, this, &MediaBotWindow::addLog);
    connect(m_bot, &MediaBotController::logLine, this, &MediaBotWindow::addLog);

    // Keep Enter in the text fields from closing the window.
    for (QPushButton* b : findChildren<QPushButton*>())
        b->setAutoDefault(false);

    applyStatus(m_bot->status());
    refresh();
    resize(760, 620);
}

QStringList MediaBotWindow::logLines() const
{
    QStringList l;
    for (int i = 0; i < m_log->count(); ++i)
        l << m_log->item(i)->text();
    return l;
}

void MediaBotWindow::send(const QString& command)
{
    m_bot->command(command);
}

void MediaBotWindow::addLog(const QString& text)
{
    m_log->insertItem(0, QStringLiteral("%1  %2").arg(QTime::currentTime().toString(QStringLiteral("HH:mm")), text));
    while (m_log->count() > kLogLimit)
        delete m_log->takeItem(m_log->count() - 1);
}

void MediaBotWindow::refresh()
{
    const bool running = m_bot->isRunning();
    m_run->setText(running ? tr("Stop &bot") : (m_bot->isConfigured() ? tr("Start &bot") : tr("Set up the &bot...")));
    m_state->setText(m_bot->isConfigured() || running ? m_bot->stateText()
                                                      : tr("Not set up yet. Choose Set up the bot to enter its account."));
    const bool online = m_bot->state() == MediaBotController::State::Running;
    for (QWidget* w : std::initializer_list<QWidget*>{m_playPause, m_prev, m_next, m_stopPlayback, m_volume})
        w->setEnabled(online);
}

void MediaBotWindow::applyStatus(const QJsonObject& st)
{
    const QJsonObject cur = st.value(QStringLiteral("current")).toObject();
    if (cur.isEmpty())
    {
        m_now->setText(st.isEmpty() ? QString() : tr("Nothing"));
        m_position->clear();
    }
    else
    {
        QString now = cur.value(QStringLiteral("label")).toString();
        if (cur.value(QStringLiteral("loading")).toBool())
            now = tr("Loading: %1").arg(now);
        const QString by = cur.value(QStringLiteral("requestedBy")).toString();
        if (!by.isEmpty())
            now += tr(" (requested by %1)").arg(by);
        if (m_now->text() != now)
            m_now->setText(now);
        const double dur = cur.value(QStringLiteral("duration")).toDouble();
        QString pos = media::fmtDuration(cur.value(QStringLiteral("elapsed")).toDouble());
        pos += QStringLiteral(" / ") + (cur.value(QStringLiteral("live")).toBool() ? tr("live") : media::fmtDuration(dur));
        if (st.value(QStringLiteral("paused")).toBool())
            pos += tr(", paused");
        m_position->setText(pos);
    }
    m_playPause->setText(st.value(QStringLiteral("paused")).toBool() ? tr("Resu&me") : tr("P&ause"));

    if (!m_volume->hasFocus() && st.contains(QStringLiteral("volume")))
    {
        m_updatingVolume = true;
        m_volume->setMaximum(st.value(QStringLiteral("maxVolume")).toInt(200));
        m_volume->setValue(st.value(QStringLiteral("volume")).toInt());
        m_updatingVolume = false;
    }

    // Update the queue in place so a screen reader keeps its position.
    const QJsonArray q = st.value(QStringLiteral("queue")).toArray();
    const int keep = m_queue->currentRow();
    for (int i = 0; i < q.size(); ++i)
    {
        const QJsonObject t = q.at(i).toObject();
        QString text = QStringLiteral("%1. %2").arg(i + 1).arg(t.value(QStringLiteral("label")).toString());
        const QString by = t.value(QStringLiteral("requestedBy")).toString();
        if (!by.isEmpty())
            text += QStringLiteral(" (%1)").arg(by);
        if (i < m_queue->count())
        {
            if (m_queue->item(i)->text() != text)
                m_queue->item(i)->setText(text);
        }
        else
        {
            m_queue->addItem(text);
        }
    }
    while (m_queue->count() > q.size())
        delete m_queue->takeItem(m_queue->count() - 1);
    const int total = st.value(QStringLiteral("queueLength")).toInt();
    if (total > q.size())
        m_queue->addItem(tr("...and %1 more").arg(total - q.size()));
    if (keep >= 0 && keep < m_queue->count())
        m_queue->setCurrentRow(keep);
    refresh();
}

} // namespace ttc
