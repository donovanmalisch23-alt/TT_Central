// SPDX-License-Identifier: MIT
// Unit tests for TT Central's core helpers: SDK strings, text reassembly,
// monitor configuration and local server configuration.
#include "fakeclient.h"
#include "ttc/monitorconfig.h"
#include "ttc/monitorevent.h"
#include "ttc/sdk.h"
#include "ttc/serverconfig.h"
#include "ttc/textassembler.h"

#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

using namespace ttc;

class TestCore : public QObject
{
    Q_OBJECT
private slots:
    // --- SDK strings ----------------------------------------------------
    void roundTripsUnicode()
    {
        const QString s = QStringLiteral("Grüße, 世界! \U0001F3B5");
        TTCHAR buf[TT_STRLEN];
        copyToTT(s, buf);
        QCOMPARE(fromTT(buf), s);
    }

    void truncatesOnCharacterBoundary()
    {
        const QString s = QString(600, QChar('x')) + QStringLiteral("\U0001F3B5");
        TTCHAR buf[16];
        copyToTT(s, buf, 16);
        QCOMPARE(fromTT(buf), QString(15, QChar('x')));

        // An emoji (surrogate pair / 4 UTF-8 bytes) is never cut in half.
        const QString e = QStringLiteral("ab\U0001F3B5\U0001F3B5");
        TTCHAR small[6];
        copyToTT(e, small, 6);
        const QString got = fromTT(small);
        QVERIFY(e.startsWith(got));
        QVERIFY(got.isEmpty() || !got.back().isHighSurrogate());
    }

    void splitsLongTextIntoSdkSizedParts()
    {
        QString text;
        for (int i = 0; i < 300; ++i)
            text += QStringLiteral("word%1 ").arg(i);
        text += QStringLiteral("\U0001F3B5 end");
        const QStringList parts = splitForTextMessage(text, 100);
        QVERIFY(parts.size() > 1);
        for (const QString& p : parts)
            QVERIFY(encodedLength(p) <= 100);
        QCOMPARE(parts.join(QString()), text);
        QCOMPARE(splitForTextMessage(QStringLiteral("short"), 100), QStringList{QStringLiteral("short")});
    }

    void namesCommonErrors()
    {
        QCOMPARE(errorName(CMDERR_INVALID_ACCOUNT), QStringLiteral("invalid username or password"));
        QCOMPARE(errorName(123456), QStringLiteral("error 123456"));
    }

    // --- text reassembly ------------------------------------------------
    void assemblesInterleavedSenders()
    {
        TextAssembler a;
        auto part = [](int from, const char* body, bool more) {
            return test::text(MSGTYPE_USER, from, QStringLiteral("u%1").arg(from), 7, 0,
                              QString::fromUtf8(body), more).textmessage;
        };
        QVERIFY(!a.add(part(1, "Hel", true)));
        QVERIFY(!a.add(part(2, "Wor", true)));
        auto first = a.add(part(1, "lo", false));
        QVERIFY(first);
        QCOMPARE(first->text, QStringLiteral("Hello"));
        QCOMPARE(first->fromUsername, QStringLiteral("u1"));
        QCOMPARE(a.pendingCount(), 1);
        a.clearUser(2);
        QCOMPARE(a.pendingCount(), 0);
    }

    // --- monitor config -------------------------------------------------
    void savesAndLoadsMonitors()
    {
        QTemporaryDir dir;
        QSettings s(dir.filePath(QStringLiteral("t.ini")), QSettings::IniFormat);
        MonitorConfig a = MonitorConfig::create();
        a.name = QStringLiteral("Home");
        a.host = QStringLiteral("tt.example.org");
        a.encrypted = true;
        a.notifyBroadcasts = true;
        MonitorConfig b = MonitorConfig::create();
        b.host = QStringLiteral("10.0.0.5");
        b.channelPath = QStringLiteral("/Music/");
        saveMonitorConfigs(s, {a, b});
        s.sync();
        QSettings s2(dir.filePath(QStringLiteral("t.ini")), QSettings::IniFormat);
        const auto loaded = loadMonitorConfigs(s2);
        QCOMPARE(loaded.size(), 2);
        QVERIFY(loaded.at(0) == a);
        QVERIFY(loaded.at(1) == b);
        QCOMPARE(loaded.at(1).displayName(), QStringLiteral("10.0.0.5"));
        // Saving fewer removes the rest.
        saveMonitorConfigs(s2, {b});
        QCOMPARE(loadMonitorConfigs(s2).size(), 1);
    }

    void validatesMonitorConfig()
    {
        MonitorConfig c = MonitorConfig::create();
        QVERIFY(!c.validationError().isEmpty());
        c.host = QStringLiteral("example.org");
        QVERIFY(c.validationError().isEmpty());
        c.tcpPort = 70000;
        QVERIFY(!c.validationError().isEmpty());
        QVERIFY(oneLoginPerIpNote().contains(QStringLiteral("one login per IP address")));
    }

    void describesEventsWithServerFirst()
    {
        MonitorEvent e;
        e.serverName = QStringLiteral("Club");
        e.kind = MonitorEvent::Kind::ConnectFailed;
        QCOMPARE(e.describe(), QStringLiteral("Club: unable to connect, retrying"));
        e.text = QStringLiteral("timed out");
        QCOMPARE(e.describe(), QStringLiteral("Club: unable to connect (timed out), retrying"));
        QVERIFY(e.isConnectionEvent());
        e.kind = MonitorEvent::Kind::UserLoggedIn;
        e.username = QStringLiteral("bob");
        QCOMPARE(e.describe(), QStringLiteral("Club: bob logged in"));
        QVERIFY(!e.isConnectionEvent());
    }

    // --- local server config ---------------------------------------------
    void writesAndReadsServerConfig()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("tt5srv.xml"));
        LocalServerConfig c;
        c.serverName = QStringLiteral("Test Server");
        c.motd = QStringLiteral("Welcome <friends> & family");
        c.tcpPort = 11000;
        c.udpPort = 11001;
        c.maxLoginsPerIp = 1;
        c.accounts = {{QStringLiteral("admin"), QStringLiteral("pw"), true, {}},
                      {QStringLiteral("guest"), QStringLiteral("guest"), false, QStringLiteral("visitors")}};
        QString err;
        QVERIFY2(writeServerXml(path, c, &err), qPrintable(err));
        LocalServerConfig back;
        QVERIFY2(readServerXml(path, back, &err), qPrintable(err));
        QVERIFY(back == c);

        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QString xml = QString::fromUtf8(f.readAll());
        QVERIFY(xml.contains(QStringLiteral("<user-type>2</user-type>")));
        QVERIFY(xml.contains(QStringLiteral("<user-rights>%1</user-rights>").arg(kDefaultUserRights)));
        QVERIFY(xml.contains(QStringLiteral("&lt;friends")) && xml.contains(QStringLiteral("&amp; family")));
    }

    void preservesWhatTheServerSaved()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("tt5srv.xml"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(R"(<?xml version="1.0" encoding="UTF-8"?>
<teamtalk version="5.3">
    <general><server-name>Old</server-name><tcpport>10333</tcpport><udpport>10333</udpport></general>
    <users>
        <user><username>keep</username><password>a</password><user-type>1</user-type>
              <user-rights>99</user-rights><last-login-time>2026/01/02 03:04</last-login-time></user>
        <user><username>drop</username><password>b</password><user-type>1</user-type></user>
    </users>
    <channels><channel><name>Music</name><topic>Tunes</topic></channel></channels>
    <serverbans><serverban><ip-address>10.0.0.9</ip-address></serverban></serverbans>
</teamtalk>)");
        f.close();

        LocalServerConfig c;
        c.serverName = QStringLiteral("New");
        c.accounts = {{QStringLiteral("keep"), QStringLiteral("a"), false, {}},
                      {QStringLiteral("added"), QStringLiteral("c"), false, {}}};
        QString err;
        QVERIFY2(writeServerXml(path, c, &err), qPrintable(err));

        QVERIFY(f.open(QIODevice::ReadOnly));
        const QString xml = QString::fromUtf8(f.readAll());
        QVERIFY(xml.contains(QStringLiteral("<name>Music</name>")));       // channels kept
        QVERIFY(xml.contains(QStringLiteral("10.0.0.9")));                 // bans kept
        QVERIFY(xml.contains(QStringLiteral("2026/01/02 03:04")));         // history kept
        QVERIFY(xml.contains(QStringLiteral("<user-rights>99</user-rights>"))); // unchanged rights kept
        QVERIFY(!xml.contains(QStringLiteral("<username>drop</username>")));
        QVERIFY(xml.contains(QStringLiteral("<username>added</username>")));
        QVERIFY(xml.contains(QStringLiteral("<server-name>New</server-name>")));
    }

    void validatesServerConfig()
    {
        LocalServerConfig c;
        QVERIFY(!c.validationError().isEmpty()); // no accounts
        c.accounts = {{QStringLiteral("a"), QStringLiteral("1"), true, {}}};
        QVERIFY(c.validationError().isEmpty());
        c.accounts << ServerAccount{QStringLiteral("A"), QStringLiteral("2"), false, {}};
        QVERIFY(!c.validationError().isEmpty()); // duplicate (case-insensitive)
        c.accounts.removeLast();
        c.bindIp = QStringLiteral("not-an-ip");
        QVERIFY(!c.validationError().isEmpty());
    }

    void rejectsForeignXml()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("x.xml"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("<notteamtalk/>");
        f.close();
        LocalServerConfig c;
        c.accounts = {{QStringLiteral("a"), QStringLiteral("1"), true, {}}};
        QString err;
        QVERIFY(!writeServerXml(path, c, &err));
        QVERIFY(!err.isEmpty());
        LocalServerConfig out;
        QVERIFY(!readServerXml(path, out, &err));
    }
};

QTEST_MAIN(TestCore)
#include "tst_core.moc"
