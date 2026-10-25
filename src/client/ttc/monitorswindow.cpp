// SPDX-License-Identifier: GPL-3.0-or-later
#include "ttc/monitorswindow.h"

#include "ttc/monitormanager.h"
#include "ttc/notificationcenter.h"

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace ttc {

namespace {
enum Column { ColName, ColServer, ColStatus };
}

MonitorsWindow::MonitorsWindow(MonitorManager* monitors, NotificationCenter* notifications, QWidget* parent)
    : QDialog(parent)
    , m_monitors(monitors)
    , m_notify(notifications)
{
    setWindowTitle(tr("TT Central Monitors"));
    setAttribute(Qt::WA_DeleteOnClose, false);
    auto* root = new QVBoxLayout(this);

    // --- Monitors --------------------------------------------------------
    auto* listLabel = new QLabel(tr("&Monitors:"), this);
    m_list = new QTreeWidget(this);
    m_list->setHeaderLabels({tr("Monitor"), tr("Server"), tr("Status")});
    m_list->setRootIsDecorated(false);
    m_list->setAlternatingRowColors(true);
    m_list->setAccessibleName(tr("Monitors"));
    m_list->setAccessibleDescription(tr("Each background monitor and its current status."));
    m_list->header()->setStretchLastSection(true);
    listLabel->setBuddy(m_list);
    root->addWidget(listLabel);
    root->addWidget(m_list, 1);

    auto* row = new QHBoxLayout;
    m_run = new QPushButton(this);
    auto* edit = new QPushButton(tr("&Edit monitors..."), this);
    row->addWidget(m_run);
    row->addWidget(edit);
    row->addStretch();
    root->addLayout(row);

    // --- Notifications ---------------------------------------------------
    auto* eventsLabel = new QLabel(tr("&Notifications (newest first):"), this);
    m_events = new QListWidget(this);
    m_events->setAccessibleName(tr("Notifications"));
    m_events->setAccessibleDescription(tr("Notifications from all monitors, newest first. Each starts with the server it came from."));
    m_events->setWordWrap(true);
    eventsLabel->setBuddy(m_events);
    root->addWidget(eventsLabel);
    root->addWidget(m_events, 2);

    auto* row2 = new QHBoxLayout;
    auto* readLast = new QPushButton(tr("&Read last aloud"), this);
    auto* copy = new QPushButton(tr("C&opy selected"), this);
    auto* clear = new QPushButton(tr("C&lear"), this);
    row2->addWidget(readLast);
    row2->addWidget(copy);
    row2->addWidget(clear);
    row2->addStretch();
    root->addLayout(row2);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    root->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    connect(m_run, &QPushButton::clicked, this, [this] {
        if (m_monitors->isRunning())
            m_monitors->stopAll();
        else
            m_monitors->startAll();
    });
    connect(edit, &QPushButton::clicked, this, &MonitorsWindow::editMonitorsRequested);
    connect(readLast, &QPushButton::clicked, m_notify, &NotificationCenter::readLast);
    connect(copy, &QPushButton::clicked, this, [this] {
        if (QListWidgetItem* it = m_events->currentItem())
            QApplication::clipboard()->setText(it->text());
    });
    connect(clear, &QPushButton::clicked, m_notify, &NotificationCenter::clearHistory);

    connect(m_monitors, &MonitorManager::configsChanged, this, &MonitorsWindow::rebuildMonitors);
    connect(m_monitors, &MonitorManager::statusChanged, this, &MonitorsWindow::updateStatus);
    connect(m_monitors, &MonitorManager::runningChanged, this, [this] {
        updateRunButton();
        rebuildMonitors();
    });
    connect(m_notify, &NotificationCenter::posted, this, &MonitorsWindow::addEvent);
    connect(m_notify, &NotificationCenter::historyCleared, this, &MonitorsWindow::rebuildEvents);

    rebuildMonitors();
    rebuildEvents();
    updateRunButton();
    resize(640, 520);
    m_list->setFocus();
}

void MonitorsWindow::rebuildMonitors()
{
    const QString current = m_list->currentItem() ? m_list->currentItem()->data(0, Qt::UserRole).toString() : QString();
    m_list->clear();
    const auto configs = m_monitors->configs();
    if (configs.isEmpty())
    {
        auto* empty = new QTreeWidgetItem(m_list, {tr("No monitors yet. Press Edit monitors to add one."), QString(), QString()});
        empty->setFlags(Qt::ItemIsEnabled);
        return;
    }
    for (const MonitorConfig& c : configs)
    {
        auto* it = new QTreeWidgetItem(m_list);
        it->setData(0, Qt::UserRole, c.id);
        it->setText(ColName, c.displayName());
        it->setText(ColServer, QStringLiteral("%1:%2").arg(c.host).arg(c.tcpPort));
        it->setText(ColStatus, m_monitors->statusText(c.id));
        if (c.id == current)
            m_list->setCurrentItem(it);
    }
    if (!m_list->currentItem() && m_list->topLevelItemCount() > 0)
        m_list->setCurrentItem(m_list->topLevelItem(0));
}

void MonitorsWindow::updateStatus(const QString& id)
{
    for (int i = 0; i < m_list->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* it = m_list->topLevelItem(i);
        if (it->data(0, Qt::UserRole).toString() == id)
            it->setText(ColStatus, m_monitors->statusText(id));
    }
}

void MonitorsWindow::updateRunButton()
{
    m_run->setText(m_monitors->isRunning() ? tr("&Stop all monitors") : tr("&Start all monitors"));
}

void MonitorsWindow::addEvent(const MonitorEvent& e)
{
    auto* it = new QListWidgetItem(QStringLiteral("%1  %2").arg(e.time.toString(QStringLiteral("HH:mm")), e.describe()));
    m_events->insertItem(0, it);
    while (m_events->count() > m_notify->prefs().historyLimit)
        delete m_events->takeItem(m_events->count() - 1);
}

void MonitorsWindow::rebuildEvents()
{
    m_events->clear();
    for (const MonitorEvent& e : m_notify->history())
        addEvent(e);
}

} // namespace ttc
