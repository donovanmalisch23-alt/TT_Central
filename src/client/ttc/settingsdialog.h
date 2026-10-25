// TT Central — the Settings dialog and its category pages.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ttc/clienthooks.h"
#include "ttc/monitorconfig.h"

#include <QDialog>
#include <QWidget>

#include <functional>

class QCheckBox;
class QComboBox;
class QListWidget;
class QSpinBox;
class QStackedWidget;
class QTreeWidget;

namespace ttc {

class MonitorManager;
class NotificationCenter;

/// One category in the Settings dialog.
class SettingsPage : public QWidget
{
    Q_OBJECT
public:
    using QWidget::QWidget;
    virtual QString title() const = 0;
    virtual void load() = 0;
    /// Return false (after telling the user why) to keep the dialog open.
    virtual bool apply() = 0;
};

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    SettingsDialog(const QList<SettingsPage*>& pages, int initialPage, QWidget* parent = nullptr);
    void showPage(int index);

private:
    bool applyAll();

    QList<SettingsPage*> m_pages;
    QListWidget* m_categories = nullptr;
    QStackedWidget* m_stack = nullptr;
};

// --- Pages ---------------------------------------------------------------

class MonitorsPage : public SettingsPage
{
    Q_OBJECT
public:
    MonitorsPage(MonitorManager* monitors, QSettings* settings,
                 std::function<QList<SavedServer>()> savedServers, QWidget* parent = nullptr);
    QString title() const override { return tr("Monitors"); }
    void load() override;
    bool apply() override;

private:
    void refresh(const QString& selectId = {});
    void add();
    void edit();
    void remove();
    void move(int delta);
    int currentRow() const;

    MonitorManager* m_monitors;
    QSettings* m_settings;
    std::function<QList<SavedServer>()> m_savedServers;
    QList<MonitorConfig> m_edit;
    QTreeWidget* m_list = nullptr;
    QComboBox* m_policy = nullptr;
};

class NotificationsPage : public SettingsPage
{
    Q_OBJECT
public:
    NotificationsPage(NotificationCenter* center, QSettings* settings, QWidget* parent = nullptr);
    QString title() const override { return tr("Notifications"); }
    void load() override;
    bool apply() override;

private:
    NotificationCenter* m_center;
    QSettings* m_settings;
    QCheckBox* m_speak = nullptr;
    QCheckBox* m_sounds = nullptr;
    QCheckBox* m_toasts = nullptr;
    QCheckBox* m_dnd = nullptr;
    QSpinBox* m_history = nullptr;
};

class SoundsPage : public SettingsPage
{
    Q_OBJECT
public:
    SoundsPage(NotificationCenter* center, std::function<void()> openClientPreferences, QWidget* parent = nullptr);
    QString title() const override { return tr("Sounds"); }
    void load() override {}
    bool apply() override { return true; }
};

/// An action with a shortcut, listed on the Accessibility page.
struct ShortcutInfo
{
    QString action;
    QString keys;
};

class AccessibilityPage : public SettingsPage
{
    Q_OBJECT
public:
    AccessibilityPage(const QList<ShortcutInfo>& shortcuts, std::function<void()> openClientPreferences,
                      QWidget* parent = nullptr);
    QString title() const override { return tr("Accessibility and shortcuts"); }
    void load() override {}
    bool apply() override { return true; }
};

} // namespace ttc
