// TT Central — client abstraction over one TeamTalk SDK instance.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Monitors and the media bot each own one TeamTalkClient. The real
// implementation (createSdkClient) wraps a polled TTInstance; unit tests use
// a scripted fake so the session logic can be tested without a server.

#pragma once

#include "ttc/sdk.h"

#include <QString>
#include <QVector>

#include <memory>

namespace ttc {

class TeamTalkClient
{
public:
    virtual ~TeamTalkClient() = default;

    virtual bool connect(const QString& host, int tcpPort, int udpPort, bool encrypted) = 0;
    virtual void disconnect() = 0;

    /// Returns the command id, or <= 0 on failure.
    virtual int login(const QString& nickname, const QString& username,
                      const QString& password, const QString& clientName) = 0;
    virtual int joinChannel(int channelId, const QString& password) = 0;
    virtual int changeStatus(int statusMode, const QString& statusMessage) = 0;
    virtual int unsubscribe(int userId, Subscriptions subscriptions) = 0;

    /// Sends a text message, splitting it across several SDK messages when
    /// needed. Returns the id of the last command, or <= 0 on failure.
    virtual int sendTextMessage(TextMsgType type, int toUserId, int channelId,
                                const QString& text) = 0;

    virtual int myUserId() = 0;
    virtual int myChannelId() = 0;
    virtual int channelIdFromPath(const QString& path) = 0;
    virtual int rootChannelId() = 0;
    virtual QString channelPath(int channelId) = 0;
    virtual QString channelName(int channelId) = 0;
    virtual QVector<int> channelUsers(int channelId) = 0;
    virtual bool user(int userId, User& out) = 0;

    /// Next queued SDK event, waiting up to waitMs. False when none.
    virtual bool nextMessage(TTMessage& msg, int waitMs) = 0;

    /// Whether the channel carries voice (has an audio codec).
    virtual bool channelHasAudioCodec(int channelId) { (void)channelId; return true; }

    /// Transmit application audio as voice (TT_InsertAudioBlock); nullptr
    /// ends the current audio input. May be called from any thread.
    virtual bool insertAudioBlock(const AudioBlock* block) { (void)block; return false; }

    /// The underlying SDK instance (nullptr for fakes). Used by the media
    /// bot for audio injection.
    virtual TTInstance* instance() { return nullptr; }
};

/// A client backed by a real TeamTalk SDK instance (TT_InitTeamTalkPoll).
std::unique_ptr<TeamTalkClient> createSdkClient();

} // namespace ttc
