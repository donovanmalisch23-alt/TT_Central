// TT Central media bot — the control connection used by TT Central's media
// window: newline-delimited JSON over a local socket that only the current
// user can open.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// From TT Central:
//   {"cmd":"command","id":7,"text":"p daft punk"}   run a chat command
//   {"cmd":"status"}                                  ask for the status now
//   {"cmd":"shutdown"}                                stop the bot
// From the bot:
//   {"type":"hello","version":"..."}                  once, on connect
//   {"type":"reply","id":7,"text":"..."}              a command's reply
//   {"type":"status", ...MediaBot::status()}          on every change, and
//                                                     each second while playing
//   {"type":"log","text":"..."}                       announcements, problems

#pragma once

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QTimer>

class QLocalServer;
class QLocalSocket;

namespace ttc::media {

class MediaBot;

class ControlServer final : public QObject
{
    Q_OBJECT
public:
    ControlServer(MediaBot* bot, QObject* parent = nullptr);
    ~ControlServer() override;

    bool listen(const QString& name, QString* error);
    int clientCount() const { return int(m_clients.size()); }

    static QByteArray encode(const QJsonObject& o);

private:
    void onNewConnection();
    void onReadyRead(QLocalSocket* s);
    void handle(QLocalSocket* s, const QJsonObject& msg);
    void send(QLocalSocket* s, const QJsonObject& o);
    void broadcast(const QJsonObject& o);
    void pushStatus();

    MediaBot* m_bot;
    QLocalServer* m_server;
    QList<QLocalSocket*> m_clients;
    QTimer m_statusThrottle;
    QTimer m_tick;
};

} // namespace ttc::media
