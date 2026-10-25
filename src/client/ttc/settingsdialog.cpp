// SPDX-License-Identifier: GPL-3.0-or-later
#include "ttc/settingsdialog.h"

#include "ttc/a11yaudit.h"
#include "ttc/monitoreditdialog.h"
#include "ttc/monitormanager.h"
#include "ttc/notificationcenter.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace ttc {

// --- SettingsDialog --------------------------------------------------------

SettingsDialog::SettingsDialog(const QList<SettingsPage*>& pages, int initialPage, QWidget* parent)
    : QDialog(parent)
    , m_pages(pages)
{
    setWindowTitle(tr("TT Central Settings"));
    auto* root = new QVBoxLayout(this);
    auto* body = new QHBoxLayout;

    auto* catLabel = new QLabel(tr("&Category:"), this);
    m_categories = new QListWidget(this);
    m_categories->setAccessibleName(tr("Settings categories"));
    m_categories->setMaximumWidth(220);
    catLabel->setBuddy(m_categories);
    auto* left = new QVBoxLayout;
    left->addWidget(catLabel);
    left->addWidget(m_categories);
    body->addLayout(left);

    m_stack = new QStackedWidget(this);
    for (SettingsPage* p : std::as_const(m_pages))
    {
        p->setParent(m_stack);
        p->load();
        m_stack->addWidget(p);
        m_categories->addItem(p->title());
    }
    body->addWidget(m_stack, 1);
    root->addLayout(body, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel |
                                             QDialogButtonBox::Apply, this);
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        if (applyAll())
            accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, [this] { applyAll(); });
    connect(m_categories, &QListWidget::currentRowChanged, m_stack, &QStackedWidget::setCurrentIndex);

    nameFormFields(this);
    showPage(initialPage);
    resize(820, 560);
}

void SettingsDialog::showPage(int index)
{
    if (index < 0 || index >= m_pages.size())
        index = 0;
    m_categories->setCurrentRow(index);
    m_categories->setFocus();
}

bool SettingsDialog::applyAll()
{
    for (int i = 0; i < m_pages.size(); ++i)
        if (!m_pages.at(i)->apply())
        {
            showPage(i);
            return false;
        }
    return true;
}

// --- MonitorsPage ----------------------------------------------------------

namespace {
enum Col { CName, CServer, CChannel, CEnabled };
}

MonitorsPage::MonitorsPage(MonitorManager* monitors, QSettings* settings,
                           std::function<QList<SavedServer>()> savedServers, QWidget* parent)
    : SettingsPage(parent)
    , m_monitors(monitors)
    , m_settings(settings)
    , m_savedServers(std::move(savedServers))
{
    auto* root = new QVBoxLayout(this);
    auto* intro = new QLabel(tr("Monitors are background logins that sit in a server channel and notify "
                                "you about logins, logouts, private messages sent to them, and messages "
                                "in their channel. Every notification says which server it came from."), this);
    intro->setWordWrap(true);
    root->addWidget(intro);

    auto* form = new QFormLayout;
    m_policy = new QComboBox(this);
    m_policy->addItem(tr("After I log in to a server"), int(MonitorStartPolicy::AfterLogin));
    m_policy->addItem(tr("When TT Central starts"), int(MonitorStartPolicy::AtStartup));
    m_policy->addItem(tr("Only when I start them"), int(MonitorStartPolicy::Manual));
    form->addRow(tr("&Start monitors:"), m_policy);
    root->addLayout(form);

    auto* listLabel = new QLabel(tr("&Monitors:"), this);
    m_list = new QTreeWidget(this);
    m_list->setHeaderLabels({tr("Name"), tr("Server"), tr("Channel"), tr("Enabled")});
    m_list->setRootIsDecorated(false);
    m_list->setAccessibleName(tr("Monitors"));
    m_list->header()->setStretchLastSection(false);
    m_list->header()->setSectionResizeMode(CName, QHeaderView::Stretch);
    listLabel->setBuddy(m_list);
    root->addWidget(listLabel);
    root->addWidget(m_list, 1);

    auto* row = new QHBoxLayout;
    auto* addB = new QPushButton(tr("&Add..."), this);
    auto* editB = new QPushButton(tr("&Edit..."), this);
    auto* removeB = new QPushButton(tr("&Remove"), this);
    auto* upB = new QPushButton(tr("Move &up"), this);
    auto* downB = new QPushButton(tr("Move &down"), this);
    for (QPushButton* b : {addB, editB, removeB, upB, downB})
        row->addWidget(b);
    row->addStretch();
    root->addLayout(row);

    auto* note = new QLabel(oneLoginPerIpNote(), this);
    note->setWordWrap(true);
    note->setTextInteractionFlags(Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse);
    note->setFocusPolicy(Qt::StrongFocus);
    note->setAccessibleName(tr("Note about using your own account"));
    root->addWidget(note);

    connect(addB, &QPushButton::clicked, this, &MonitorsPage::add);
    connect(editB, &QPushButton::clicked, this, &MonitorsPage::edit);
    connect(removeB, &QPushButton::clicked, this, &MonitorsPage::remove);
    connect(upB, &QPushButton::clicked, this, [this] { move(-1); });
    connect(downB, &QPushButton::clicked, this, [this] { move(1); });
    connect(m_list, &QTreeWidget::itemActivated, this, [this] { edit(); });
}

void MonitorsPage::load()
{
    m_edit = m_monitors->configs();
    if (m_settings)
    {
        const int v = int(loadStartPolicy(*m_settings));
        m_policy->setCurrentIndex(qMax(0, m_policy->findData(v)));
    }
    refresh();
}

bool MonitorsPage::apply()
{
    if (m_settings)
        saveStartPolicy(*m_settings, MonitorStartPolicy(m_policy->currentData().toInt()));
    if (m_edit != m_monitors->configs())
        m_monitors->setConfigs(m_edit);
    return true;
}

void MonitorsPage::refresh(const QString& selectId)
{
    m_list->clear();
    for (const MonitorConfig& c : std::as_const(m_edit))
    {
        auto* it = new QTreeWidgetItem(m_list);
        it->setData(0, Qt::UserRole, c.id);
        it->setText(CName, c.displayName());
        it->setText(CServer, QStringLiteral("%1:%2").arg(c.host).arg(c.tcpPort));
        it->setText(CChannel, c.channelPath);
        it->setText(CEnabled, c.enabled ? tr("Yes") : tr("No"));
        if (c.id == selectId)
            m_list->setCurrentItem(it);
    }
    if (!m_list->currentItem() && m_list->topLevelItemCount() > 0)
        m_list->setCurrentItem(m_list->topLevelItem(0));
}

int MonitorsPage::currentRow() const
{
    return m_list->currentItem() ? m_list->indexOfTopLevelItem(m_list->currentItem()) : -1;
}

void MonitorsPage::add()
{
    MonitorEditDialog dlg(MonitorConfig::create(), m_savedServers ? m_savedServers() : QList<SavedServer>{}, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const MonitorConfig c = dlg.config();
    m_edit << c;
    refresh(c.id);
    m_list->setFocus();
}

void MonitorsPage::edit()
{
    const int row = currentRow();
    if (row < 0 || row >= m_edit.size())
        return;
    MonitorEditDialog dlg(m_edit.at(row), m_savedServers ? m_savedServers() : QList<SavedServer>{}, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    m_edit[row] = dlg.config();
    refresh(m_edit.at(row).id);
    m_list->setFocus();
}

void MonitorsPage::remove()
{
    const int row = currentRow();
    if (row < 0 || row >= m_edit.size())
        return;
    const auto answer = QMessageBox::question(this, tr("Remove Monitor"),
                                              tr("Remove the monitor \"%1\"?").arg(m_edit.at(row).displayName()));
    if (answer != QMessageBox::Yes)
        return;
    m_edit.removeAt(row);
    refresh(row < m_edit.size() ? m_edit.at(row).id : (m_edit.isEmpty() ? QString() : m_edit.last().id));
    m_list->setFocus();
}

void MonitorsPage::move(int delta)
{
    const int row = currentRow();
    const int to = row + delta;
    if (row < 0 || to < 0 || to >= m_edit.size())
        return;
    m_edit.swapItemsAt(row, to);
    refresh(m_edit.at(to).id);
    m_list->setFocus();
}

// --- NotificationsPage -----------------------------------------------------

NotificationsPage::NotificationsPage(NotificationCenter* center, QSettings* settings, QWidget* parent)
    : SettingsPage(parent)
    , m_center(center)
    , m_settings(settings)
{
    auto* root = new QVBoxLayout(this);
    auto* intro = new QLabel(tr("Choose how monitor notifications reach you. They always appear in the "
                                "Monitors window history."), this);
    intro->setWordWrap(true);
    root->addWidget(intro);
    m_speak = new QCheckBox(tr("&Speak notifications (screen reader, text-to-speech or braille, as chosen in the client's text-to-speech preferences)"), this);
    m_sounds = new QCheckBox(tr("&Play notification sounds from the TeamTalk sound pack"), this);
    m_toasts = new QCheckBox(tr("Show &Windows notifications"), this);
    m_dnd = new QCheckBox(tr("&Do not disturb (keep notifications in the history only)"), this);
    for (QCheckBox* b : {m_speak, m_sounds, m_toasts, m_dnd})
        root->addWidget(b);
    auto* form = new QFormLayout;
    m_history = new QSpinBox(this);
    m_history->setRange(50, 5000);
    m_history->setSingleStep(50);
    form->addRow(tr("Notifications to &keep in history:"), m_history);
    root->addLayout(form);
    root->addStretch();
}

void NotificationsPage::load()
{
    const NotificationPrefs p = m_center->prefs();
    m_speak->setChecked(p.speak);
    m_sounds->setChecked(p.sounds);
    m_toasts->setChecked(p.toasts);
    m_dnd->setChecked(p.doNotDisturb);
    m_history->setValue(p.historyLimit);
}

bool NotificationsPage::apply()
{
    NotificationPrefs p;
    p.speak = m_speak->isChecked();
    p.sounds = m_sounds->isChecked();
    p.toasts = m_toasts->isChecked();
    p.doNotDisturb = m_dnd->isChecked();
    p.historyLimit = m_history->value();
    if (m_settings)
        p.save(*m_settings);
    m_center->setPrefs(p);
    return true;
}

// --- SoundsPage ------------------------------------------------------------

SoundsPage::SoundsPage(NotificationCenter* center, std::function<void()> openClientPreferences, QWidget* parent)
    : SettingsPage(parent)
{
    auto* root = new QVBoxLayout(this);
    auto* text = new QLabel(tr("TT Central plays the original TeamTalk sound packs (Default, Majorly-G and Old). "
                               "Monitor notifications use the client's sound events: user logged in, user "
                               "logged out, private message, channel message, broadcast and server lost.\n\n"
                               "Choose the sound pack and individual sounds in the client's Preferences, "
                               "under Sound Events."), this);
    text->setWordWrap(true);
    text->setTextInteractionFlags(Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse);
    root->addWidget(text);
    auto* open = new QPushButton(tr("&Open sound preferences..."), this);
    open->setEnabled(bool(openClientPreferences));
    connect(open, &QPushButton::clicked, this, [openClientPreferences] {
        if (openClientPreferences)
            openClientPreferences();
    });
    auto* test = new QPushButton(tr("Play a &test notification"), this);
    connect(test, &QPushButton::clicked, center, [center] {
        MonitorEvent e;
        e.serverName = tr("TT Central");
        e.kind = MonitorEvent::Kind::UserLoggedIn;
        e.nickname = tr("Test user");
        center->post(e);
    });
    auto* row = new QHBoxLayout;
    row->addWidget(open);
    row->addWidget(test);
    row->addStretch();
    root->addLayout(row);
    root->addStretch();
}

// --- AccessibilityPage -----------------------------------------------------

AccessibilityPage::AccessibilityPage(const QList<ShortcutInfo>& shortcuts, std::function<void()> openClientPreferences,
                                     QWidget* parent)
    : SettingsPage(parent)
{
    auto* root = new QVBoxLayout(this);
    auto* text = new QLabel(tr("TT Central is built for keyboard and screen-reader use. Every control has a "
                               "label and a keyboard shortcut (underlined letter with Alt). Spoken "
                               "notifications go through the client's text-to-speech engine, which can "
                               "speak through your screen reader (NVDA, JAWS and others), Windows speech, "
                               "or braille.\n\nTT Central keyboard shortcuts:"), this);
    text->setWordWrap(true);
    root->addWidget(text);
    auto* list = new QTreeWidget(this);
    list->setHeaderLabels({tr("Action"), tr("Shortcut")});
    list->setRootIsDecorated(false);
    list->setAccessibleName(tr("TT Central keyboard shortcuts"));
    for (const ShortcutInfo& s : shortcuts)
        new QTreeWidgetItem(list, {s.action, s.keys});
    list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    text->setBuddy(list);
    root->addWidget(list, 1);
    auto* open = new QPushButton(tr("Open &text-to-speech preferences..."), this);
    open->setEnabled(bool(openClientPreferences));
    connect(open, &QPushButton::clicked, this, [openClientPreferences] {
        if (openClientPreferences)
            openClientPreferences();
    });
    auto* row = new QHBoxLayout;
    row->addWidget(open);
    row->addStretch();
    root->addLayout(row);
}

} // namespace ttc
