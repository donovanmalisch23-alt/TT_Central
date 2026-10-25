// TT Central — live monitor status and the notification history.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ttc/monitorevent.h"

#include <QDialog>

class QListWidget;
class QPushButton;
class QTreeWidget;

namespace ttc {

class MonitorManager;
class NotificationCenter;

class MonitorsWindow : public QDialog
{
    Q_OBJECT
public:
    MonitorsWindow(MonitorManager* monitors, NotificationCenter* notifications, QWidget* parent = nullptr);

signals:
    /// "Edit monitors..." pressed: open Settings → Monitors.
    void editMonitorsRequested();

private:
    void rebuildMonitors();
    void updateStatus(const QString& id);
    void updateRunButton();
    void addEvent(const ttc::MonitorEvent& e);
    void rebuildEvents();

    MonitorManager* m_monitors;
    NotificationCenter* m_notify;
    QTreeWidget* m_list = nullptr;
    QListWidget* m_events = nullptr;
    QPushButton* m_run = nullptr;
};

} // namespace ttc
