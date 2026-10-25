// TT Central — routes monitor events to speech, sounds, toasts and the log.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The outputs are injected so this class stays independent of qtTeamTalk:
// the client wires speech to its TTS/Prism engine, sounds to the selected
// TeamTalk sound pack, and toasts to its notification helper.

#pragma once

#include "ttc/monitorevent.h"

#include <QList>
#include <QObject>

#include <functional>

class QSettings;

namespace ttc {

struct NotificationOutputs
{
    std::function<void(const QString& text)> speak;                 // screen reader / TTS / braille
    std::function<void(MonitorEvent::Kind kind)> playSound;        // TeamTalk sound pack
    std::function<void(const QString& title, const QString& text)> toast;
};

struct NotificationPrefs
{
    bool speak = true;
    bool sounds = true;
    bool toasts = false;
    bool doNotDisturb = false;   // keep logging, but no speech/sound/toast
    int historyLimit = 500;

    static NotificationPrefs load(QSettings& s);
    void save(QSettings& s) const;
    bool operator==(const NotificationPrefs& o) const
    {
        return speak == o.speak && sounds == o.sounds && toasts == o.toasts &&
               doNotDisturb == o.doNotDisturb && historyLimit == o.historyLimit;
    }
};

class NotificationCenter : public QObject
{
    Q_OBJECT
public:
    explicit NotificationCenter(NotificationOutputs outputs, QObject* parent = nullptr);

    void setPrefs(const NotificationPrefs& prefs);
    const NotificationPrefs& prefs() const { return m_prefs; }

    /// Newest last.
    const QList<MonitorEvent>& history() const { return m_history; }
    bool hasHistory() const { return !m_history.isEmpty(); }

public slots:
    void post(const ttc::MonitorEvent& e);
    /// Speak the most recent notification again (an explicit request, so it
    /// is spoken even in do-not-disturb mode).
    void readLast();
    void clearHistory();

signals:
    void posted(const ttc::MonitorEvent& e);
    void historyCleared();

private:
    NotificationOutputs m_out;
    NotificationPrefs m_prefs;
    QList<MonitorEvent> m_history;
};

} // namespace ttc
