// SPDX-License-Identifier: GPL-3.0-or-later
// Unit tests for TT Central's client layer (ttc_client): notification routing,
// the monitor manager, the TT Central menu, and accessibility of every dialog.
#include "fakeclient.h"
#include "ttc/a11yaudit.h"
#include "ttc/central.h"
#include "ttc/mediabotcontroller.h"
#include "ttc/mediabotpage.h"
#include "ttc/mediabotwindow.h"
#include "ttc/monitoreditdialog.h"
#include "ttc/monitormanager.h"
#include "ttc/monitorswindow.h"
#include "ttc/notificationcenter.h"
#include "ttc/settingsdialog.h"

#include <QApplication>
#include <QComboBox>
#include <QJsonArray>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QSpinBox>
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
        QCOMPARE(titles.size(), 7);
        QVERIFY(titles.contains(QStringLiteral("Media &bot...")));
        QVERIFY(titles.contains(QStringLiteral("Set up the media b&ot...")));
        QCOMPARE(central.menu()->title(), QStringLiteral("&TT Central"));
        QCOMPARE(central.menu()->actions().first()->shortcut(), QKeySequence(QStringLiteral("Ctrl+Shift+O")));
        QCOMPARE(central.shortcuts().size(), 5);

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
        for (int page = 0; page <= Central::PageAccessibility; ++page)
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

    // --- Media bot (client side) -------------------------------------------
    void mediaBotWindowShowsStatusAccessibly()
    {
        QTemporaryDir dir;
        QSettings s(dir.filePath(QStringLiteral("m.ini")), QSettings::IniFormat);
        MediaBotController bot(&s, dir.filePath(QStringLiteral("no-bot")));
        MediaBotWindow win(&bot);
        win.show();
        QVERIFY(QTest::qWaitForWindowExposed(&win));
        QStringList problems = auditAccessibility(&win);
        QVERIFY2(problems.isEmpty(), qPrintable(problems.join(QLatin1Char('\n'))));

        auto* state = win.findChild<QLineEdit*>();
        QVERIFY(state && state->text().contains(QStringLiteral("Not set up")));

        QJsonObject cur{{QStringLiteral("label"), QStringLiteral("Band - Song [3:00]")},
                        {QStringLiteral("duration"), 180.0}, {QStringLiteral("elapsed"), 65.0},
                        {QStringLiteral("requestedBy"), QStringLiteral("Alice")}};
        QJsonArray queue{QJsonObject{{QStringLiteral("label"), QStringLiteral("Next one")}},
                         QJsonObject{{QStringLiteral("label"), QStringLiteral("Last one")},
                                     {QStringLiteral("requestedBy"), QStringLiteral("Bob")}}};
        win.applyStatus({{QStringLiteral("current"), cur}, {QStringLiteral("queue"), queue},
                         {QStringLiteral("queueLength"), 3}, {QStringLiteral("paused"), true},
                         {QStringLiteral("volume"), 70}, {QStringLiteral("maxVolume"), 150}});
        const auto edits = win.findChildren<QLineEdit*>();
        QStringList texts;
        for (QLineEdit* e : edits)
            texts << e->text();
        QVERIFY2(texts.contains(QStringLiteral("Band - Song [3:00] (requested by Alice)")), qPrintable(texts.join(QStringLiteral(" | "))));
        QVERIFY(texts.contains(QStringLiteral("1:05 / 3:00, paused")));
        QListWidget* q = nullptr;
        for (QListWidget* l : win.findChildren<QListWidget*>())
            if (l->accessibleName() == QLatin1String("Queue"))
                q = l;
        QVERIFY(q);
        QCOMPARE(q->count(), 3);
        QCOMPARE(q->item(1)->text(), QStringLiteral("2. Last one (Bob)"));
        QCOMPARE(q->item(2)->text(), QStringLiteral("...and 1 more"));
        auto* vol = win.findChild<QSpinBox*>();
        QCOMPARE(vol->value(), 70);
        QCOMPARE(vol->maximum(), 150);
        problems = auditAccessibility(&win);
        QVERIFY2(problems.isEmpty(), qPrintable(problems.join(QLatin1Char('\n'))));
    }

    void mediaBotPageSavesAndValidates()
    {
        QTemporaryDir dir;
        QSettings s(dir.filePath(QStringLiteral("m.ini")), QSettings::IniFormat);
        MediaBotController bot(&s, dir.filePath(QStringLiteral("no-bot")));
        SavedServer saved;
        saved.name = QStringLiteral("Club");
        saved.host = QStringLiteral("club.example.org");
        saved.username = QStringLiteral("me");
        MediaBotPage page(&bot, [saved] { return QList<SavedServer>{saved}; });
        nameFormFields(&page);   // as the Settings dialog does
        page.load();
        QVERIFY(page.apply());                      // blank = not set up: fine
        auto* import = page.findChild<QComboBox*>();
        QVERIFY(import);
        import->setCurrentIndex(1);
        emit import->activated(1);
        media::MediaBotConfig c = page.current();
        QCOMPARE(c.host, QStringLiteral("club.example.org"));
        QVERIFY(c.username.isEmpty());              // the bot gets its own account
        // Partly filled in: apply refuses and says why.
        QString why;
        page.warn = [&](const QString& w) { why = w; };
        QVERIFY(!page.apply());
        QVERIFY(why.contains(QStringLiteral("username")));
        for (QLineEdit* e : page.findChildren<QLineEdit*>())
            if (e->accessibleName().contains(QStringLiteral("username"), Qt::CaseInsensitive))
                e->setText(QStringLiteral("jukebox"));
        QVERIFY(page.apply());
        QCOMPARE(media::loadMediaBotConfig(s, false).username, QStringLiteral("jukebox"));
        QCOMPARE(bot.config().host, QStringLiteral("club.example.org"));
    }

    void mediaBotControllerExplainsWhyItCannotStart()
    {
        QTemporaryDir dir;
        QSettings s(dir.filePath(QStringLiteral("m.ini")), QSettings::IniFormat);
        MediaBotController bot(&s, dir.filePath(QStringLiteral("no-bot")));
        QVERIFY(!bot.isConfigured());
        bot.start();
        QCOMPARE(bot.state(), MediaBotController::State::Failed);
        QVERIFY(bot.stateText().contains(QStringLiteral("server address")));
        media::MediaBotConfig c;
        c.host = QStringLiteral("example.org");
        c.username = QStringLiteral("bot");
        bot.setConfig(c);
        bot.start();
        QCOMPARE(bot.state(), MediaBotController::State::Failed);
        QVERIFY(bot.stateText().contains(QStringLiteral("not found")));
        QString reply;
        bot.command(QStringLiteral("v"), [&](const QString& r) { reply = r; });
        QVERIFY(reply.contains(QStringLiteral("not running")));
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
