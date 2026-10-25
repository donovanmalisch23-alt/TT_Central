// TT Central — the features TT Central adds to the client, and their menu.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ttc/clienthooks.h"
#include "ttc/settingsdialog.h"

#include <QObject>
#include <QPointer>

class QAction;
class QMenu;
class QMenuBar;
class QWidget;

namespace ttc {

class MediaBotController;
class MediaBotWindow;
class MonitorManager;
class MonitorsWindow;
class NotificationCenter;

/// TT Central's version (CMake project version).
QString version();

class Central : public QObject
{
    Q_OBJECT
public:
    Central(ClientHooks hooks, QWidget* mainWindow);
    ~Central() override;

    /// The "TT Central" menu for the client's menu bar.
    QMenu* menu() const { return m_menu; }

    MonitorManager* monitors() const { return m_monitors; }
    MediaBotController* mediaBot() const { return m_media; }
    NotificationCenter* notifications() const { return m_notify; }
    QList<ShortcutInfo> shortcuts() const;

    /// The client logged in to a server (CLIENTEVENT_CMD_MYSELF_LOGGEDIN).
    void onClientLoggedIn();

    enum Page { PageMonitors = 0, PageNotifications, PageSounds, PageMediaBot, PageAccessibility };

    /// Build the Settings dialog (not shown yet).
    SettingsDialog* createSettingsDialog(int page, QWidget* parent);

    /// In-app self-test used by CI (environment variable TTC_SELFTEST):
    /// checks the menu's placement, that TT Central's shortcuts do not clash
    /// with the client's, that every window opens, and audits accessibility.
    struct SelfTestResult
    {
        bool ok = true;
        QStringList lines;
    };
    SelfTestResult runSelfTest(QWidget* mainWindow, QMenuBar* menuBar);

public slots:
    void showMonitors();
    void showMediaBot();
    void showSettings(int page = PageMonitors);
    void showAbout();
    void readLastNotification();

private:
    void buildMenu();
    void updateRunAction();
    void updateMediaAction();

    ClientHooks m_hooks;
    QWidget* m_window = nullptr;
    MonitorManager* m_monitors = nullptr;
    NotificationCenter* m_notify = nullptr;
    QMenu* m_menu = nullptr;
    QAction* m_runAction = nullptr;
    QAction* m_mediaRunAction = nullptr;
    MediaBotController* m_media = nullptr;
    QPointer<MonitorsWindow> m_monitorsWindow;
    QPointer<MediaBotWindow> m_mediaWindow;
    bool m_autoStarted = false;
};

} // namespace ttc
