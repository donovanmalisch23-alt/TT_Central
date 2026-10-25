// SPDX-License-Identifier: GPL-3.0-or-later
#include "ttc/notificationcenter.h"

#include <QCoreApplication>
#include <QSettings>

namespace ttc {

namespace {
const char* const kGroup = "ttcentral/notifications/";
QString key(const char* k) { return QLatin1String(kGroup) + QLatin1String(k); }
} // namespace

NotificationPrefs NotificationPrefs::load(QSettings& s)
{
    NotificationPrefs p;
    p.speak = s.value(key("speak"), p.speak).toBool();
    p.sounds = s.value(key("sounds"), p.sounds).toBool();
    p.toasts = s.value(key("toasts"), p.toasts).toBool();
    p.doNotDisturb = s.value(key("do-not-disturb"), p.doNotDisturb).toBool();
    p.historyLimit = qBound(50, s.value(key("history-limit"), p.historyLimit).toInt(), 5000);
    return p;
}

void NotificationPrefs::save(QSettings& s) const
{
    s.setValue(key("speak"), speak);
    s.setValue(key("sounds"), sounds);
    s.setValue(key("toasts"), toasts);
    s.setValue(key("do-not-disturb"), doNotDisturb);
    s.setValue(key("history-limit"), historyLimit);
}

NotificationCenter::NotificationCenter(NotificationOutputs outputs, QObject* parent)
    : QObject(parent)
    , m_out(std::move(outputs))
{
}

void NotificationCenter::setPrefs(const NotificationPrefs& prefs)
{
    m_prefs = prefs;
    while (m_history.size() > m_prefs.historyLimit)
        m_history.removeFirst();
}

void NotificationCenter::post(const MonitorEvent& e)
{
    m_history.append(e);
    while (m_history.size() > m_prefs.historyLimit)
        m_history.removeFirst();
    emit posted(e);

    if (m_prefs.doNotDisturb)
        return;
    const QString sentence = e.describe();
    if (m_prefs.sounds && m_out.playSound)
        m_out.playSound(e.kind);
    if (m_prefs.speak && m_out.speak)
        m_out.speak(sentence);
    if (m_prefs.toasts && m_out.toast)
    {
        // Title carries the server; the body is the sentence without it.
        QString body = sentence;
        const QString prefix = e.serverName + QLatin1String(": ");
        if (body.startsWith(prefix))
            body = body.mid(prefix.size());
        m_out.toast(e.serverName, body);
    }
}

void NotificationCenter::readLast()
{
    if (!m_out.speak)
        return;
    if (m_history.isEmpty())
        m_out.speak(QCoreApplication::translate("ttc::NotificationCenter", "No notifications yet"));
    else
        m_out.speak(m_history.last().describe());
}

void NotificationCenter::clearHistory()
{
    m_history.clear();
    emit historyCleared();
}

} // namespace ttc
