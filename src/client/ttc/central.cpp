// SPDX-License-Identifier: GPL-3.0-or-later
#include "ttc/central.h"

#include "ttc/a11yaudit.h"
#include "ttc/monitoreditdialog.h"
#include "ttc/monitormanager.h"
#include "ttc/monitorswindow.h"
#include "ttc/notificationcenter.h"

#include <QAction>
#include <QApplication>
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

    buildMenu();

    if (m_hooks.settings && loadStartPolicy(*m_hooks.settings) == MonitorStartPolicy::AtStartup)
        QTimer::singleShot(1500, m_monitors, &MonitorManager::startAll);
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
        {tr("TT Central settings"), kSettingsKeys.toString(QKeySequence::NativeText)},
        {tr("TT Central menu"), tr("Alt+T")},
    };
}

void Central::updateRunAction()
{
    if (m_runAction)
        m_runAction->setText(m_monitors->isRunning() ? tr("S&top all monitors") : tr("S&tart all monitors"));
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
    pages << new AccessibilityPage(shortcuts(), m_hooks.openClientPreferences);
    return new SettingsDialog(pages, page, parent);
}

void Central::showSettings(int page)
{
    QWidget* parent = m_monitorsWindow && m_monitorsWindow->isVisible() ? static_cast<QWidget*>(m_monitorsWindow) : m_window;
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
