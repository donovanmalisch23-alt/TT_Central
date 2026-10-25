// TT Central — the Media Bot window: what the bot is playing, its queue, and
// controls. Every control works from the keyboard and is announced by name.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>
#include <QJsonObject>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;

namespace ttc {

class MediaBotController;

class MediaBotWindow : public QDialog
{
    Q_OBJECT
public:
    explicit MediaBotWindow(MediaBotController* bot, QWidget* parent = nullptr);

    /// Lines in the "Replies and log" list (newest first). For tests.
    QStringList logLines() const;
    /// Show a status message from the bot (normally from the controller).
    void applyStatus(const QJsonObject& status);

signals:
    void settingsRequested();

private:
    void refresh();
    void addLog(const QString& text);
    void send(const QString& command);

    MediaBotController* m_bot;
    QLineEdit* m_state = nullptr;
    QLineEdit* m_now = nullptr;
    QLineEdit* m_position = nullptr;
    QPushButton* m_run = nullptr;
    QPushButton* m_playPause = nullptr;
    QPushButton* m_prev = nullptr;
    QPushButton* m_next = nullptr;
    QPushButton* m_stopPlayback = nullptr;
    QSpinBox* m_volume = nullptr;
    QLineEdit* m_add = nullptr;
    QListWidget* m_queue = nullptr;
    QLineEdit* m_command = nullptr;
    QListWidget* m_log = nullptr;
    bool m_updatingVolume = false;
};

} // namespace ttc
