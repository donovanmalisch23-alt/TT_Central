// TT Central — owns all background monitors.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ttc/monitorconfig.h"
#include "ttc/monitorsession.h"

#include <QHash>
#include <QObject>

#include <functional>
#include <memory>

class QSettings;

namespace ttc {

/// When monitors start on their own.
enum class MonitorStartPolicy
{
    AfterLogin = 0,   // the first time you log in to a server (default)
    AtStartup = 1,    // as soon as TT Central opens
    Manual = 2,       // only from the menu / Monitors window
};

MonitorStartPolicy loadStartPolicy(QSettings& s);
void saveStartPolicy(QSettings& s, MonitorStartPolicy p);

class MonitorManager : public QObject
{
    Q_OBJECT
public:
    using ClientFactory = std::function<std::unique_ptr<TeamTalkClient>()>;

    explicit MonitorManager(QSettings* settings, ClientFactory factory = &createSdkClient,
                            QObject* parent = nullptr);
    ~MonitorManager() override;

    QList<MonitorConfig> configs() const { return m_configs; }
    /// Save and apply a new monitor list. While monitoring is active, only
    /// added, removed or changed monitors are (re)started or stopped.
    void setConfigs(const QList<MonitorConfig>& configs);

    bool isRunning() const { return m_running; }
    /// The live session for a monitor, or nullptr if it is not running.
    MonitorSession* session(const QString& id) const;
    /// "Online", "Waiting to reconnect: ...", "Disabled", "Stopped", ...
    QString statusText(const QString& id) const;
    int onlineCount() const;

public slots:
    void startAll();
    void stopAll();

signals:
    void event(const ttc::MonitorEvent& e);
    void statusChanged(const QString& monitorId);
    void configsChanged();
    void runningChanged(bool running);

private:
    void startOne(const MonitorConfig& c);
    void stopOne(const QString& id);

    QSettings* m_settings = nullptr;
    ClientFactory m_factory;
    QList<MonitorConfig> m_configs;
    QHash<QString, MonitorSession*> m_sessions;
    bool m_running = false;
};

} // namespace ttc
