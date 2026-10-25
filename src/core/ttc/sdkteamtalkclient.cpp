// SPDX-License-Identifier: MIT
#include "ttc/teamtalkclient.h"

#include <cstring>

namespace ttc {

namespace {

class SdkTeamTalkClient final : public TeamTalkClient
{
public:
    SdkTeamTalkClient() : m_tt(TT_InitTeamTalkPoll()) {}
    ~SdkTeamTalkClient() override
    {
        if (m_tt)
        {
            TT_Disconnect(m_tt);
            TT_CloseTeamTalk(m_tt);
        }
    }

    bool connect(const QString& host, int tcpPort, int udpPort, bool encrypted) override
    {
        return m_tt && TT_Connect(m_tt, TTStr(host).c_str(), tcpPort, udpPort, 0, 0, encrypted ? TRUE : FALSE);
    }

    void disconnect() override
    {
        if (m_tt)
            TT_Disconnect(m_tt);
    }

    int login(const QString& nickname, const QString& username,
              const QString& password, const QString& clientName) override
    {
        return TT_DoLoginEx(m_tt, TTStr(nickname).c_str(), TTStr(username).c_str(),
                            TTStr(password).c_str(), TTStr(clientName).c_str());
    }

    int joinChannel(int channelId, const QString& password) override
    {
        return TT_DoJoinChannelByID(m_tt, channelId, TTStr(password).c_str());
    }

    int changeStatus(int statusMode, const QString& statusMessage) override
    {
        return TT_DoChangeStatus(m_tt, statusMode, TTStr(statusMessage).c_str());
    }

    int unsubscribe(int userId, Subscriptions subscriptions) override
    {
        return TT_DoUnsubscribe(m_tt, userId, subscriptions);
    }

    int sendTextMessage(TextMsgType type, int toUserId, int channelId, const QString& text) override
    {
        const QStringList parts = splitForTextMessage(text);
        int cmdId = 0;
        for (int i = 0; i < parts.size(); ++i)
        {
            TextMessage msg;
            std::memset(&msg, 0, sizeof(msg));
            msg.nMsgType = type;
            msg.nFromUserID = TT_GetMyUserID(m_tt);
            msg.nToUserID = toUserId;
            msg.nChannelID = channelId;
            msg.bMore = (i + 1 < parts.size()) ? TRUE : FALSE;
            copyToTT(parts.at(i), msg.szMessage);
            cmdId = TT_DoTextMessage(m_tt, &msg);
            if (cmdId <= 0)
                return cmdId;
        }
        return cmdId;
    }

    int myUserId() override { return TT_GetMyUserID(m_tt); }
    int myChannelId() override { return TT_GetMyChannelID(m_tt); }
    int rootChannelId() override { return TT_GetRootChannelID(m_tt); }

    int channelIdFromPath(const QString& path) override
    {
        return TT_GetChannelIDFromPath(m_tt, TTStr(path).c_str());
    }

    QString channelPath(int channelId) override
    {
        TTCHAR buf[TT_STRLEN] = {};
        return TT_GetChannelPath(m_tt, channelId, buf) ? fromTT(buf) : QString();
    }

    QString channelName(int channelId) override
    {
        Channel chan;
        std::memset(&chan, 0, sizeof(chan));
        if (!TT_GetChannel(m_tt, channelId, &chan))
            return {};
        return fromTT(chan.szName);
    }

    QVector<int> channelUsers(int channelId) override
    {
        INT32 count = 0;
        TT_GetChannelUsers(m_tt, channelId, nullptr, &count);
        QVector<int> ids;
        if (count <= 0)
            return ids;
        QVector<User> users(count);
        if (!TT_GetChannelUsers(m_tt, channelId, users.data(), &count))
            return ids;
        for (int i = 0; i < count; ++i)
            ids << users.at(i).nUserID;
        return ids;
    }

    bool user(int userId, User& out) override
    {
        std::memset(&out, 0, sizeof(out));
        return TT_GetUser(m_tt, userId, &out);
    }

    bool nextMessage(TTMessage& msg, int waitMs) override
    {
        INT32 wait = waitMs;
        return m_tt && TT_GetMessage(m_tt, &msg, &wait);
    }

    TTInstance* instance() override { return m_tt; }

private:
    TTInstance* m_tt = nullptr;
};

} // namespace

std::unique_ptr<TeamTalkClient> createSdkClient()
{
    return std::make_unique<SdkTeamTalkClient>();
}

} // namespace ttc
