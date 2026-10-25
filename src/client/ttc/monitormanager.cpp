// SPDX-License-Identifier: GPL-3.0-or-later
#include "ttc/monitormanager.h"

#include <QCoreApplication>
#include <QSet>
#include <QSettings>

namespace ttc {

namespace {
const char* const kPolicyKey = "ttcentral/monitors-start";
} // namespace

MonitorStartPolicy loadStartPolicy(QSettings& s)
{
    const int v = s.value(QLatin1String(kPolicyKey), int(MonitorStartPolicy::AfterLogin)).toInt();
    if (v < 0 || v > 2)
        return MonitorStartPolicy::AfterLogin;
    return MonitorStartPolicy(v);
}

void saveStartPolicy(QSettings& s, MonitorStartPolicy p)
{
    s.setValue(QLatin1String(kPolicyKey), int(p));
}

MonitorManager::MonitorManager(QSettings* settings, ClientFactory factory, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_factory(std::move(factory))
{
    if (m_settings)
        m_configs = loadMonitorConfigs(*m_settings);
}

MonitorManager::~MonitorManager()
{
    const auto ids = m_sessions.keys();
    for (const QString& id : ids)
        stopOne(id);
}

void MonitorManager::setConfigs(const QList<MonitorConfig>& configs)
{
    const QList<MonitorConfig> old = m_configs;
    m_configs = configs;
    if (m_settings)
    {
        saveMonitorConfigs(*m_settings, m_configs);
        m_settings->sync();
    }

    if (m_running)
    {
        QHash<QString, MonitorConfig> before;
        for (const MonitorConfig& c : old)
            before.insert(c.id, c);
        QSet<QString> keep;
        for (const MonitorConfig& c : m_configs)
        {
            keep.insert(c.id);
            const bool existed = before.contains(c.id);
            const bool changed = existed && before.value(c.id) != c;
            if (!c.enabled)
                stopOne(c.id);
            else if (!existed || changed || !m_sessions.contains(c.id))
            {
                stopOne(c.id);
                startOne(c);
            }
        }
        const auto running = m_sessions.keys();
        for (const QString& id : running)
            if (!keep.contains(id))
                stopOne(id);
    }
    emit configsChanged();
}

void MonitorManager::startAll()
{
    if (m_running)
        return;
    m_running = true;
    for (const MonitorConfig& c : std::as_const(m_configs))
        if (c.enabled)
            startOne(c);
    emit runningChanged(true);
}

void MonitorManager::stopAll()
{
    if (!m_running)
        return;
    const auto ids = m_sessions.keys();
    for (const QString& id : ids)
        stopOne(id);
    m_running = false;
    emit runningChanged(false);
}

void MonitorManager::startOne(const MonitorConfig& c)
{
    auto* s = new MonitorSession(c, m_factory(), this);
    m_sessions.insert(c.id, s);
    const QString id = c.id;
    connect(s, &MonitorSession::event, this, &MonitorManager::event);
    connect(s, &MonitorSession::stateChanged, this, [this, id] { emit statusChanged(id); });
    s->start();
    emit statusChanged(id);
}

void MonitorManager::stopOne(const QString& id)
{
    MonitorSession* s = m_sessions.take(id);
    if (!s)
        return;
    s->disconnect(this);
    s->stop();
    s->deleteLater();
    emit statusChanged(id);
}

MonitorSession* MonitorManager::session(const QString& id) const
{
    return m_sessions.value(id, nullptr);
}

QString MonitorManager::statusText(const QString& id) const
{
    for (const MonitorConfig& c : m_configs)
        if (c.id == id && !c.enabled)
            return tr("Disabled");
    if (const MonitorSession* s = session(id))
        return s->stateText();
    return m_running ? tr("Not running") : tr("Stopped");
}

int MonitorManager::onlineCount() const
{
    int n = 0;
    for (const MonitorSession* s : m_sessions)
        if (s->state() == MonitorSession::State::Online)
            ++n;
    return n;
}

} // namespace ttc
