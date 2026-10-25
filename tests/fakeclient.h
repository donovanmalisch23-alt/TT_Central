// TT Central tests — a scripted TeamTalkClient for unit tests.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ttc/teamtalkclient.h"

#include <QHash>
#include <QList>
#include <QQueue>
#include <QSet>
#include <QStringList>

#include <atomic>
#include <cstring>

namespace ttc::test {

struct SentText
{
    TextMsgType type;
    int toUserId;
    int channelId;
    QString text;
};

class FakeClient final : public TeamTalkClient
{
public:
    // --- what the test controls ---------------------------------------
    QQueue<TTMessage> queue;
    bool connectResult = true;
    int myId = 0;
    int myChannel = 0;
    int root = 1;
    QHash<int, QString> channelNames;  // id -> name
    QHash<QString, int> channelPaths;  // path -> id
    QHash<int, User> users;
    QHash<int, QVector<int>> channelMembers;
    QSet<int> channelsWithoutVoice;

    // --- what the test inspects ---------------------------------------
    int connects = 0;
    int disconnects = 0;
    QStringList logins;                // "nick|user|pass|client"
    QList<QPair<int, QString>> joins;  // (channel, password)
    QList<QPair<int, int>> unsubscribes;
    QList<SentText> sent;
    int nextCmd = 100;
    std::atomic<int> audioBlocks{0};   // TT_InsertAudioBlock calls (any thread)
    std::atomic<int> audioEnds{0};     // ... with NULL (end of input)

    bool connect(const QString&, int, int, bool) override { ++connects; return connectResult; }
    void disconnect() override { ++disconnects; myChannel = 0; }
    int login(const QString& n, const QString& u, const QString& p, const QString& c) override
    {
        logins << QStringLiteral("%1|%2|%3|%4").arg(n, u, p, c);
        return ++nextCmd;
    }
    int joinChannel(int channelId, const QString& password) override
    {
        joins.append({channelId, password});
        return ++nextCmd;
    }
    int changeStatus(int, const QString&) override { return ++nextCmd; }
    int unsubscribe(int userId, Subscriptions s) override
    {
        unsubscribes.append({userId, int(s)});
        return ++nextCmd;
    }
    int sendTextMessage(TextMsgType type, int to, int chan, const QString& text) override
    {
        sent.append({type, to, chan, text});
        return ++nextCmd;
    }
    int myUserId() override { return myId; }
    int myChannelId() override { return myChannel; }
    int rootChannelId() override { return root; }
    int channelIdFromPath(const QString& path) override { return channelPaths.value(path, 0); }
    QString channelPath(int id) override
    {
        for (auto it = channelPaths.cbegin(); it != channelPaths.cend(); ++it)
            if (it.value() == id) return it.key();
        return {};
    }
    QString channelName(int id) override { return channelNames.value(id); }
    QVector<int> channelUsers(int id) override { return channelMembers.value(id); }
    bool user(int id, User& out) override
    {
        if (!users.contains(id)) return false;
        out = users.value(id);
        return true;
    }
    bool channelHasAudioCodec(int id) override { return !channelsWithoutVoice.contains(id); }
    bool insertAudioBlock(const AudioBlock* b) override
    {
        if (b)
            ++audioBlocks;
        else
            ++audioEnds;
        return true;
    }
    bool nextMessage(TTMessage& msg, int) override
    {
        if (queue.isEmpty()) return false;
        msg = queue.dequeue();
        return true;
    }
};

// --- event builders ----------------------------------------------------

inline TTMessage blank(ClientEvent ev, int source = 0)
{
    TTMessage m;
    std::memset(&m, 0, sizeof(m));
    m.nClientEvent = ev;
    m.nSource = source;
    return m;
}

inline TTMessage processing(int cmdId, bool active)
{
    TTMessage m = blank(CLIENTEVENT_CMD_PROCESSING, cmdId);
    m.ttType = __TTBOOL;
    m.bActive = active ? TRUE : FALSE;
    return m;
}

inline TTMessage cmdError(int cmdId, int errorNo)
{
    TTMessage m = blank(CLIENTEVENT_CMD_ERROR, cmdId);
    m.ttType = __CLIENTERRORMSG;
    m.clienterrormsg.nErrorNo = errorNo;
    return m;
}

inline User makeUser(int id, const QString& nick, const QString& username, int channel = 0)
{
    User u;
    std::memset(&u, 0, sizeof(u));
    u.nUserID = id;
    u.nChannelID = channel;
    copyToTT(nick, u.szNickname);
    copyToTT(username, u.szUsername);
    return u;
}

inline TTMessage userEvent(ClientEvent ev, const User& u)
{
    TTMessage m = blank(ev, u.nUserID);
    m.ttType = __USER;
    m.user = u;
    return m;
}

inline TTMessage text(TextMsgType type, int from, const QString& fromUser, int to, int chan,
                      const QString& body, bool more = false)
{
    TTMessage m = blank(CLIENTEVENT_CMD_USER_TEXTMSG, from);
    m.ttType = __TEXTMESSAGE;
    m.textmessage.nMsgType = type;
    m.textmessage.nFromUserID = from;
    copyToTT(fromUser, m.textmessage.szFromUsername);
    m.textmessage.nToUserID = to;
    m.textmessage.nChannelID = chan;
    copyToTT(body, m.textmessage.szMessage);
    m.textmessage.bMore = more ? TRUE : FALSE;
    return m;
}

} // namespace ttc::test
