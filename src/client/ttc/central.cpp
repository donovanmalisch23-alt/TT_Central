// SPDX-License-Identifier: GPL-3.0-or-later
#include "ttc/central.h"

#include "ttc/a11yaudit.h"
#include "ttc/localserver.h"
#include "ttc/serverwindow.h"
#include "ttc/mediabotcontroller.h"
#include "ttc/mediabotpage.h"
#include "ttc/mediabotwindow.h"
#include "ttc/monitoreditdialog.h"
#include "ttc/monitormanager.h"
#include "ttc/monitorswindow.h"
#include "ttc/notificationcenter.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QMenuBar>
#include <QKeySequence>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QTimer>
#include <QWidget>

#ifndef TTC_VERSION
#define TTC_VERSION "0.0.0"
#endif

namespace ttc {

QString version() { return QStringLiteral(TTC_VERSION); }

namespace {
const QKeySequence kMonitorsKeys(QStringLiteral("Ctrl+Shift+O"));
const QKeySequence kReadLastKeys(QStringLiteral("Ctrl+Shift+N"));
const QKeySequence kSettingsKeys(QStringLiteral("Ctrl+Shift+P"));
const QKeySequence kMediaKeys(QStringLiteral("Ctrl+Shift+J"));
const QKeySequence kServerKeys(QStringLiteral("Ctrl+Shift+K"));
} // namespace

Central::Central(ClientHooks hooks, QWidget* mainWindow)
    : QObject(mainWindow)
    , m_hooks(std::move(hooks))
    , m_window(mainWindow)
{
    m_monitors = new MonitorManager(m_hooks.settings, &createSdkClient, this);
    m_notify = new NotificationCenter(m_hooks.outputs, this);
    if (m_hooks.settings)
        m_notify->setPrefs(NotificationPrefs::load(*m_hooks.settings));
    connect(m_monitors, &MonitorManager::event, m_notify, &NotificationCenter::post);
    connect(m_monitors, &MonitorManager::runningChanged, this, &Central::updateRunAction);

    m_media = new MediaBotController(m_hooks.settings, {}, this);
    connect(m_media, &MediaBotController::stateChanged, this, &Central::updateMediaAction);
#ifdef TTC_DEV_SERVER_EXE
    const QString devServer = QStringLiteral(TTC_DEV_SERVER_EXE);
#else
    const QString devServer;
#endif
    m_server = new LocalServer(LocalServer::findServerExecutable(devServer), defaultServerFolder(m_hooks.settings), this);
    connect(m_server, &LocalServer::stateChanged, this, &Central::updateServerAction);

    buildMenu();

    if (m_hooks.settings && loadStartPolicy(*m_hooks.settings) == MonitorStartPolicy::AtStartup)
        QTimer::singleShot(1500, m_monitors, &MonitorManager::startAll);
    if (m_media->config().autoStart && m_media->isConfigured())
        QTimer::singleShot(2000, m_media, &MediaBotController::start);
    // The local server first, so a bot or monitor pointed at it finds it up.
    if (m_hooks.settings && m_hooks.settings->value(ServerWindow::autoStartKey(), false).toBool())
        QTimer::singleShot(500, m_server, &LocalServer::start);
}

Central::~Central() = default;

void Central::buildMenu()
{
    m_menu = new QMenu(tr("&TT Central"), m_window);

    QAction* monitors = m_menu->addAction(tr("&Monitors and notifications..."));
    monitors->setShortcut(kMonitorsKeys);
    connect(monitors, &QAction::triggered, this, &Central::showMonitors);

    QAction* readLast = m_menu->addAction(tr("&Read last notification"));
    readLast->setShortcut(kReadLastKeys);
    connect(readLast, &QAction::triggered, this, &Central::readLastNotification);

    m_runAction = m_menu->addAction(QString());
    connect(m_runAction, &QAction::triggered, this, [this] {
        if (m_monitors->isRunning())
            m_monitors->stopAll();
        else
            m_monitors->startAll();
    });
    updateRunAction();

    m_menu->addSeparator();
    QAction* media = m_menu->addAction(tr("Media &bot..."));
    media->setShortcut(kMediaKeys);
    connect(media, &QAction::triggered, this, &Central::showMediaBot);

    m_mediaRunAction = m_menu->addAction(QString());
    connect(m_mediaRunAction, &QAction::triggered, this, [this] {
        if (m_media->isRunning())
            m_media->stop();
        else if (m_media->isConfigured())
            m_media->start();
        else
            showSettings(PageMediaBot);
    });
    updateMediaAction();

    m_menu->addSeparator();
    QAction* server = m_menu->addAction(tr("&Local server..."));
    server->setShortcut(kServerKeys);
    connect(server, &QAction::triggered, this, &Central::showLocalServer);
    m_serverRunAction = m_menu->addAction(QString());
    connect(m_serverRunAction, &QAction::triggered, this, [this] {
        if (m_server->isRunning())
            m_server->stop();
        else if (m_server->hasConfig())
            m_server->start();
        else
            showLocalServer();
    });
    updateServerAction();
    buildAdminMenu();

    m_menu->addSeparator();
    QAction* settings = m_menu->addAction(tr("TT Central &settings..."));
    settings->setShortcut(kSettingsKeys);
    connect(settings, &QAction::triggered, this, [this] { showSettings(PageMonitors); });

    m_menu->addSeparator();
    QAction* about = m_menu->addAction(tr("&About TT Central"));
    connect(about, &QAction::triggered, this, &Central::showAbout);
}

QList<ShortcutInfo> Central::shortcuts() const
{
    return {
        {tr("Monitors and notifications"), kMonitorsKeys.toString(QKeySequence::NativeText)},
        {tr("Read last notification aloud"), kReadLastKeys.toString(QKeySequence::NativeText)},
        {tr("Media bot window"), kMediaKeys.toString(QKeySequence::NativeText)},
        {tr("Local server window"), kServerKeys.toString(QKeySequence::NativeText)},
        {tr("TT Central settings"), kSettingsKeys.toString(QKeySequence::NativeText)},
        {tr("TT Central menu"), tr("Alt+T")},
    };
}

void Central::updateRunAction()
{
    if (m_runAction)
        m_runAction->setText(m_monitors->isRunning() ? tr("S&top all monitors") : tr("S&tart all monitors"));
}

void Central::updateMediaAction()
{
    if (!m_mediaRunAction)
        return;
    if (m_media->isRunning())
        m_mediaRunAction->setText(tr("Stop the media b&ot"));
    else
        m_mediaRunAction->setText(m_media->isConfigured() ? tr("Start the media b&ot") : tr("Set up the media b&ot..."));
}

QString Central::defaultServerFolder(QSettings* settings)
{
    const QString base = settings ? QFileInfo(settings->fileName()).absolutePath() : QDir::homePath();
    return QDir(base).filePath(QStringLiteral("server"));
}

void Central::updateServerAction()
{
    if (!m_serverRunAction)
        return;
    if (m_server->isRunning())
        m_serverRunAction->setText(tr("Stop the local ser&ver"));
    else
        m_serverRunAction->setText(m_server->hasConfig() ? tr("Start the local ser&ver") : tr("Set up a local ser&ver..."));
}

void Central::buildAdminMenu()
{
    if (!m_hooks.adminActions)
        return;
    const QList<QAction*> originals = m_hooks.adminActions();
    if (originals.isEmpty())
        return;
    m_adminMenu = m_menu->addMenu(tr("Server &administration"));
    for (QAction* orig : originals)
    {
        // A twin without the shortcut (the original keeps it) that follows
        // the original's state: enabled only when you are an administrator.
        auto* twin = m_adminMenu->addAction(orig->text());
        twin->setEnabled(orig->isEnabled());
        twin->setToolTip(orig->toolTip());
        connect(twin, &QAction::triggered, orig, &QAction::trigger);
        connect(orig, &QAction::changed, twin, [twin, orig] {
            twin->setEnabled(orig->isEnabled());
            twin->setText(orig->text());
        });
    }
}

void Central::showLocalServer()
{
    if (!m_serverWindow)
    {
        m_serverWindow = new ServerWindow(m_server, m_hooks.settings, m_hooks, m_window);
        m_serverWindow->setAttribute(Qt::WA_DeleteOnClose);
    }
    m_serverWindow->show();
    m_serverWindow->raise();
    m_serverWindow->activateWindow();
}

void Central::showMediaBot()
{
    if (!m_mediaWindow)
    {
        m_mediaWindow = new MediaBotWindow(m_media, m_window);
        m_mediaWindow->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_mediaWindow, &MediaBotWindow::settingsRequested, this, [this] { showSettings(PageMediaBot); });
    }
    m_mediaWindow->show();
    m_mediaWindow->raise();
    m_mediaWindow->activateWindow();
}

void Central::onClientLoggedIn()
{
    if (m_autoStarted || m_monitors->isRunning() || !m_hooks.settings)
        return;
    if (loadStartPolicy(*m_hooks.settings) != MonitorStartPolicy::AfterLogin)
        return;
    m_autoStarted = true; // only the first login; after that you are in control
    m_monitors->startAll();
}

void Central::showMonitors()
{
    if (!m_monitorsWindow)
    {
        m_monitorsWindow = new MonitorsWindow(m_monitors, m_notify, m_window);
        m_monitorsWindow->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_monitorsWindow, &MonitorsWindow::editMonitorsRequested, this, [this] { showSettings(PageMonitors); });
    }
    m_monitorsWindow->show();
    m_monitorsWindow->raise();
    m_monitorsWindow->activateWindow();
}

SettingsDialog* Central::createSettingsDialog(int page, QWidget* parent)
{
    QList<SettingsPage*> pages;
    pages << new MonitorsPage(m_monitors, m_hooks.settings, m_hooks.savedServers);
    pages << new NotificationsPage(m_notify, m_hooks.settings);
    pages << new SoundsPage(m_notify, m_hooks.openClientPreferences);
    pages << new MediaBotPage(m_media, m_hooks.savedServers);
    pages << new AccessibilityPage(shortcuts(), m_hooks.openClientPreferences);
    return new SettingsDialog(pages, page, parent);
}

void Central::showSettings(int page)
{
    QWidget* parent = m_window;
    if (m_mediaWindow && m_mediaWindow->isVisible())
        parent = m_mediaWindow;
    else if (m_monitorsWindow && m_monitorsWindow->isVisible())
        parent = m_monitorsWindow;
    std::unique_ptr<SettingsDialog> dlg(createSettingsDialog(page, parent));
    dlg->exec();
    if (m_hooks.settings)
        m_hooks.settings->sync();
}

Central::SelfTestResult Central::runSelfTest(QWidget* mainWindow, QMenuBar* menuBar)
{
    SelfTestResult r;
    auto pass = [&](const QString& s) { r.lines << QStringLiteral("PASS  ") + s; };
    auto fail = [&](const QString& s) { r.ok = false; r.lines << QStringLiteral("FAIL  ") + s; };
    auto audit = [&](const QString& what, QWidget* w) {
        QApplication::processEvents();
        const QStringList problems = auditAccessibility(w);
        if (problems.isEmpty())
            pass(QStringLiteral("accessibility: %1 - every focusable control is named").arg(what));
        for (const QString& p : problems)
            fail(QStringLiteral("accessibility: %1 - %2").arg(what, p));
    };

    // 1. Menu placement.
    const QList<QAction*> bar = menuBar ? menuBar->actions() : QList<QAction*>{};
    const int ours = int(bar.indexOf(m_menu->menuAction()));
    auto* help = mainWindow->findChild<QMenu*>(QStringLiteral("menuHelp"));
    const int helpIdx = help ? int(bar.indexOf(help->menuAction())) : -1;
    if (ours < 0)
        fail(QStringLiteral("TT Central menu is not in the menu bar"));
    else if (helpIdx >= 0 && ours > helpIdx)
        fail(QStringLiteral("TT Central menu is after Help"));
    else
        pass(QStringLiteral("TT Central menu is in the menu bar, before Help"));

    // 2. Shortcut conflicts with the client's own actions.
    QHash<QString, QStringList> byKeys;
    const auto actions = mainWindow->findChildren<QAction*>();
    for (QAction* a : actions)
        for (const QKeySequence& k : a->shortcuts())
            if (!k.isEmpty())
                byKeys[k.toString(QKeySequence::PortableText)] << a->text().remove(QLatin1Char('&'));
    bool clash = false;
    for (QAction* a : m_menu->actions())
        for (const QKeySequence& k : a->shortcuts())
        {
            const QStringList users = byKeys.value(k.toString(QKeySequence::PortableText));
            if (users.size() > 1)
            {
                clash = true;
                fail(QStringLiteral("shortcut %1 is used by: %2").arg(k.toString(), users.join(QStringLiteral(", "))));
            }
        }
    if (!clash)
        pass(QStringLiteral("TT Central shortcuts do not clash with the client's (%1 actions checked)").arg(actions.size()));

    // 3. Windows open, and pass the accessibility audit.
    showMonitors();
    if (m_monitorsWindow && m_monitorsWindow->isVisible())
    {
        pass(QStringLiteral("Monitors window opens"));
        audit(QStringLiteral("Monitors window"), m_monitorsWindow);
        m_monitorsWindow->close();
    }
    else
        fail(QStringLiteral("Monitors window did not open"));

    showMediaBot();
    if (m_mediaWindow && m_mediaWindow->isVisible())
    {
        pass(QStringLiteral("Media bot window opens"));
        audit(QStringLiteral("Media bot window"), m_mediaWindow);
        m_mediaWindow->close();
    }
    else
        fail(QStringLiteral("Media bot window did not open"));
    if (QFileInfo::exists(m_media->botExecutable()))
        pass(QStringLiteral("media bot program found: %1").arg(QDir::toNativeSeparators(m_media->botExecutable())));
    else
        fail(QStringLiteral("media bot program missing: %1").arg(QDir::toNativeSeparators(m_media->botExecutable())));

    showLocalServer();
    if (m_serverWindow && m_serverWindow->isVisible())
    {
        pass(QStringLiteral("Local server window opens"));
        audit(QStringLiteral("Local server window"), m_serverWindow);
        AccountDialog account(ServerAccount{}, {}, m_serverWindow);
        account.show();
        audit(QStringLiteral("Add Account dialog"), &account);
        account.close();
        m_serverWindow->close();
    }
    else
        fail(QStringLiteral("Local server window did not open"));
    if (QFileInfo::exists(m_server->serverExecutable()))
        pass(QStringLiteral("TeamTalk server program found: %1").arg(QDir::toNativeSeparators(m_server->serverExecutable())));
    else
        fail(QStringLiteral("TeamTalk server program missing: %1").arg(QDir::toNativeSeparators(m_server->serverExecutable())));
    if (m_adminMenu && !m_adminMenu->actions().isEmpty())
        pass(QStringLiteral("server administration menu lists %1 of the client's actions").arg(m_adminMenu->actions().size()));
    else
        fail(QStringLiteral("server administration menu is missing"));

    std::unique_ptr<SettingsDialog> settings(createSettingsDialog(PageMonitors, mainWindow));
    settings->show();
    for (int i = 0; i <= PageAccessibility; ++i)
    {
        settings->showPage(i);
        audit(QStringLiteral("Settings page %1").arg(i + 1), settings.get());
    }
    pass(QStringLiteral("Settings dialog opens with %1 pages").arg(PageAccessibility + 1));
    settings->close();

    SavedServer sample;
    sample.name = QStringLiteral("Sample");
    sample.host = QStringLiteral("tt.example.org");
    MonitorEditDialog edit(MonitorConfig::create(), {sample}, mainWindow);
    edit.show();
    audit(QStringLiteral("Add Monitor dialog"), &edit);
    edit.close();

    // 4. Notification history records events.
    const int before = int(m_notify->history().size());
    MonitorEvent e;
    e.serverName = QStringLiteral("Self-test");
    e.kind = MonitorEvent::Kind::UserLoggedIn;
    e.nickname = QStringLiteral("Tester");
    NotificationPrefs quiet = m_notify->prefs();
    const NotificationPrefs saved = quiet;
    quiet.doNotDisturb = true; // no speech/sound during the test
    m_notify->setPrefs(quiet);
    m_notify->post(e);
    m_notify->setPrefs(saved);
    if (m_notify->history().size() == before + 1 && m_notify->history().last().describe() == QLatin1String("Self-test: Tester logged in"))
        pass(QStringLiteral("notification history records events"));
    else
        fail(QStringLiteral("notification history did not record the test event"));

    r.lines.prepend(r.ok ? QStringLiteral("TT Central self-test: PASSED") : QStringLiteral("TT Central self-test: FAILED"));
    return r;
}

void Central::showAbout()
{
    QMessageBox::about(m_window, tr("About TT Central"),
                       tr("<h3>TT Central %1</h3>"
                          "<p>A unified, accessible TeamTalk 5 client for Windows with background "
                          "server monitors, a media-player bot and a local server manager.</p>"
                          "<p>Built on BearWare.dk's TeamTalk 5 client (GPLv3) and the TeamTalk 5 SDK "
                          "(proprietary; runs in trial mode without a license key).</p>"
                          "<p>TT Central is free software under the GNU GPL, version 3 or later. "
                          "<a href=\"https://github.com/donovanmalisch23-alt/TT_Central\">github.com/donovanmalisch23-alt/TT_Central</a></p>")
                           .arg(version()));
}

void Central::readLastNotification()
{
    m_notify->readLast();
}

} // namespace ttc
