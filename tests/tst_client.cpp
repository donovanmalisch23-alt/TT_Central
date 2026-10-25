// SPDX-License-Identifier: GPL-3.0-or-later
// Unit tests for TT Central's client layer (ttc_client): notification routing,
// the monitor manager, the TT Central menu, and accessibility of every dialog.
#include "fakeclient.h"
#include "ttc/a11yaudit.h"
#include "ttc/central.h"
#include "ttc/monitoreditdialog.h"
#include "ttc/monitormanager.h"
#include "ttc/monitorswindow.h"
#include "ttc/notificationcenter.h"
#include "ttc/settingsdialog.h"

#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>
#include <QTemporaryDir>
#include <QVBoxLayout>
#include <QtTest>

using namespace ttc;

class TestClient : public QObject
{
    Q_OBJECT

    struct Captured
    {
        QStringList spoken;
        QList<MonitorEvent::Kind> sounds;
        QStringList toasts;
    };

    static NotificationOutputs outputsFor(Captured& c)
    {
        NotificationOutputs o;
        o.speak = [&c](const QString& t) { c.spoken << t; };
        o.playSound = [&c](MonitorEvent::Kind k) { c.sounds << k; };
        o.toast = [&c](const QString& title, const QString& text) { c.toasts << title + QLatin1String(" | ") + text; };
        return o;
    }

    static MonitorEvent loginEvent(const QString& server, const QString& who)
    {
        MonitorEvent e;
        e.serverName = server;
        e.kind = MonitorEvent::Kind::UserLoggedIn;
        e.nickname = who;
        return e;
    }

    static MonitorConfig monitor(const QString& name, bool enabled = true)
    {
        MonitorConfig c = MonitorConfig::create();
        c.name = name;
        c.host = QStringLiteral("tt.example.org");
        c.enabled = enabled;
        return c;
    }

private slots:
    void initTestCase()
    {
        // Never write to the developer's real settings.
        QStandardPaths::setTestModeEnabled(true);
    }

    // --- NotificationCenter ---------------------------------------------
    void routesToSpeechSoundAndToast()
    {
        Captured c;
        NotificationCenter center(outputsFor(c));
        NotificationPrefs p;
        p.toasts = true;
        center.setPrefs(p);
        center.post(loginEvent(QStringLiteral("Home"), QStringLiteral("Alice")));
        QCOMPARE(c.spoken, QStringList{QStringLiteral("Home: Alice logged in")});
        QCOMPARE(c.sounds.size(), 1);
        QCOMPARE(c.sounds.first(), MonitorEvent::Kind::UserLoggedIn);
        QCOMPARE(c.toasts, QStringList{QStringLiteral("Home | Alice logged in")});
        QCOMPARE(center.history().size(), 1);
    }

    void respectsPrefsAndDoNotDisturb()
    {
        Captured c;
        NotificationCenter center(outputsFor(c));
        NotificationPrefs p;
        p.speak = false;
        center.setPrefs(p);
        center.post(loginEvent(QStringLiteral("Home"), QStringLiteral("Alice")));
        QVERIFY(c.spoken.isEmpty());
        QCOMPARE(c.sounds.size(), 1);
        QVERIFY(c.toasts.isEmpty()); // toasts are off by default

        p.speak = true;
        p.doNotDisturb = true;
        center.setPrefs(p);
        center.post(loginEvent(QStringLiteral("Home"), QStringLiteral("Bob")));
        QCOMPARE(c.sounds.size(), 1);   // nothing new
        QVERIFY(c.spoken.isEmpty());
        QCOMPARE(center.history().size(), 2); // but still logged

        // An explicit "read last" is honoured even in do-not-disturb.
        center.readLast();
        QCOMPARE(c.spoken, QStringList{QStringLiteral("Home: Bob logged in")});
    }

    void readLastWithEmptyHistory()
    {
        Captured c;
        NotificationCenter center(outputsFor(c));
        center.readLast();
        QCOMPARE(c.spoken, QStringList{QStringLiteral("No notifications yet")});
    }

    void capsHistory()
    {
        Captured c;
        NotificationCenter center(outputsFor(c));
        NotificationPrefs p;
        p.historyLimit = 50;
        p.doNotDisturb = true;
        center.setPrefs(p);
        for (int i = 0; i < 80; ++i)
            center.post(loginEvent(QStringLiteral("S"), QString::number(i)));
        QCOMPARE(center.history().size(), 50);
        QCOMPARE(center.history().first().nickname, QStringLiteral("30"));
        center.clearHistory();
        QVERIFY(!center.hasHistory());
    }

    void persistsPrefs()
    {
        QTemporaryDir dir;
        QSettings s(dir.filePath(QStringLiteral("p.ini")), QSettings::IniFormat);
        NotificationPrefs p;
        p.speak = false;
        p.toasts = true;
        p.historyLimit = 200;
        p.save(s);
        QVERIFY(NotificationPrefs::load(s) == p);
    }

    // --- MonitorManager ---------------------------------------------------
    void startsOnlyEnabledMonitorsAndRestartsOnlyChanges()
    {
        QTemporaryDir dir;
        QSettings s(dir.filePath(QStringLiteral("m.ini")), QSettings::IniFormat);
        int created = 0;
        MonitorManager mgr(&s, [&created] { ++created; return std::make_unique<test::FakeClient>(); });

        const MonitorConfig a = monitor(QStringLiteral("A"));
        const MonitorConfig b = monitor(QStringLiteral("B"), false);
        const MonitorConfig c = monitor(QStringLiteral("C"));
        mgr.setConfigs({a, b, c});
        QCOMPARE(created, 0); // not running yet
        QCOMPARE(mgr.statusText(b.id), QStringLiteral("Disabled"));

        QSignalSpy running(&mgr, &MonitorManager::runningChanged);
        mgr.startAll();
        QCOMPARE(running.size(), 1);
        QCOMPARE(created, 2); // A and C
        QVERIFY(mgr.session(a.id));
        QVERIFY(!mgr.session(b.id));
        MonitorSession* cSession = mgr.session(c.id);

        // Change only A: A restarts, C keeps its session.
        MonitorConfig a2 = a;
        a2.channelPath = QStringLiteral("/Music/");
        mgr.setConfigs({a2, b, c});
        QCOMPARE(created, 3);
        QCOMPARE(mgr.session(c.id), cSession);

        // Enable B, remove C.
        MonitorConfig b2 = b;
        b2.enabled = true;
        mgr.setConfigs({a2, b2});
        QCOMPARE(created, 4);
        QVERIFY(mgr.session(b2.id));
        QVERIFY(!mgr.session(c.id));

        mgr.stopAll();
        QVERIFY(!mgr.isRunning());
        QVERIFY(!mgr.session(a.id));

        // Persisted.
        MonitorManager reloaded(&s, [] { return std::make_unique<test::FakeClient>(); });
        QCOMPARE(reloaded.configs().size(), 2);
        QCOMPARE(reloaded.configs().first().channelPath, QStringLiteral("/Music/"));
    }

    void forwardsSessionEvents()
    {
        test::FakeClient* fake = nullptr;
        MonitorManager mgr(nullptr, [&fake] {
            auto f = std::make_unique<test::FakeClient>();
            fake = f.get();
            return f;
        });
        mgr.setConfigs({monitor(QStringLiteral("Home"))});
        QSignalSpy events(&mgr, &MonitorManager::event);
        mgr.startAll();
        QVERIFY(fake);
        fake->queue.enqueue(test::blank(CLIENTEVENT_CON_LOST));
        QTRY_COMPARE(events.size(), 1);
        QCOMPARE(events.first().first().value<MonitorEvent>().describe(),
                 QStringLiteral("Home: connection lost, reconnecting"));
    }

    void startPolicyRoundTrips()
    {
        QTemporaryDir dir;
        QSettings s(dir.filePath(QStringLiteral("p.ini")), QSettings::IniFormat);
        QCOMPARE(loadStartPolicy(s), MonitorStartPolicy::AfterLogin); // default
        saveStartPolicy(s, MonitorStartPolicy::Manual);
        QCOMPARE(loadStartPolicy(s), MonitorStartPolicy::Manual);
        s.setValue(QStringLiteral("ttcentral/monitors-start"), 99);
        QCOMPARE(loadStartPolicy(s), MonitorStartPolicy::AfterLogin);
    }

    // --- Central (menu + login hook) --------------------------------------
    void buildsMenuAndAutoStartsAfterFirstLogin()
    {
        QTemporaryDir dir;
        QSettings s(dir.filePath(QStringLiteral("c.ini")), QSettings::IniFormat);
        QMainWindow mw;
        ClientHooks hooks;
        hooks.settings = &s;
        Central central(hooks, &mw);
        mw.menuBar()->addMenu(central.menu());

        QStringList titles;
        for (QAction* a : central.menu()->actions())
            if (!a->isSeparator())
                titles << a->text();
        QCOMPARE(titles.size(), 5);
        QCOMPARE(central.menu()->title(), QStringLiteral("&TT Central"));
        QCOMPARE(central.menu()->actions().first()->shortcut(), QKeySequence(QStringLiteral("Ctrl+Shift+O")));
        QCOMPARE(central.shortcuts().size(), 4);

        QVERIFY(!central.monitors()->isRunning());
        central.onClientLoggedIn();
        QVERIFY(central.monitors()->isRunning());
        central.monitors()->stopAll();
        central.onClientLoggedIn(); // only the first login auto-starts
        QVERIFY(!central.monitors()->isRunning());
    }

    void manualPolicyNeverAutoStarts()
    {
        QTemporaryDir dir;
        QSettings s(dir.filePath(QStringLiteral("c.ini")), QSettings::IniFormat);
        saveStartPolicy(s, MonitorStartPolicy::Manual);
        QMainWindow mw;
        ClientHooks hooks;
        hooks.settings = &s;
        Central central(hooks, &mw);
        central.onClientLoggedIn();
        QVERIFY(!central.monitors()->isRunning());
    }

    // --- Accessibility of every window --------------------------------------
    void editDialogIsAccessible()
    {
        SavedServer saved;
        saved.name = QStringLiteral("Club");
        saved.host = QStringLiteral("club.example.org");
        saved.username = QStringLiteral("me");
        MonitorEditDialog dlg(MonitorConfig::create(), {saved});
        dlg.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dlg));
        const QStringList problems = auditAccessibility(&dlg);
        QVERIFY2(problems.isEmpty(), qPrintable(problems.join(QLatin1Char('\n'))));
    }

    void editDialogRoundTripsConfig()
    {
        MonitorConfig c = monitor(QStringLiteral("Home"));
        c.channelPath = QStringLiteral("Music");   // normalized on save
        c.notifyBroadcasts = true;
        c.username = QStringLiteral("me");
        MonitorEditDialog dlg(c, {});
        const MonitorConfig out = dlg.config();
        QCOMPARE(out.id, c.id);
        QCOMPARE(out.channelPath, QStringLiteral("/Music/"));
        QCOMPARE(out.notifyBroadcasts, true);
        QCOMPARE(out.username, QStringLiteral("me"));
    }

    void settingsAndMonitorsWindowsAreAccessible()
    {
        QTemporaryDir dir;
        QSettings s(dir.filePath(QStringLiteral("a.ini")), QSettings::IniFormat);
        QMainWindow mw;
        ClientHooks hooks;
        hooks.settings = &s;
        Central central(hooks, &mw);
        central.monitors()->setConfigs({monitor(QStringLiteral("Home"))});

        std::unique_ptr<SettingsDialog> settings(central.createSettingsDialog(0, &mw));
        settings->show();
        QVERIFY(QTest::qWaitForWindowExposed(settings.get()));
        for (int page = 0; page < 4; ++page)
        {
            settings->showPage(page);
            const QStringList problems = auditAccessibility(settings.get());
            QVERIFY2(problems.isEmpty(), qPrintable(QStringLiteral("page %1: ").arg(page) + problems.join(QLatin1Char('\n'))));
        }

        MonitorsWindow win(central.monitors(), central.notifications());
        win.show();
        QVERIFY(QTest::qWaitForWindowExposed(&win));
        const QStringList problems = auditAccessibility(&win);
        QVERIFY2(problems.isEmpty(), qPrintable(problems.join(QLatin1Char('\n'))));
    }

    void auditCatchesUnnamedControls()
    {
        QWidget w;
        auto* layout = new QVBoxLayout(&w);
        auto* unnamed = new QLineEdit(&w); // no label, no accessible name
        layout->addWidget(unnamed);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QCOMPARE(auditAccessibility(&w).size(), 1);
        unnamed->setAccessibleName(QStringLiteral("Search"));
        QVERIFY(auditAccessibility(&w).isEmpty());
    }
};

QTEST_MAIN(TestClient)
#include "tst_client.moc"
