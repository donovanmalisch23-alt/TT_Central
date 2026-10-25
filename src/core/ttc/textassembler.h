// TT Central — reassembles multi-part TeamTalk text messages.
// SPDX-License-Identifier: MIT
//
// Long text messages arrive as several TextMessage parts; every part except
// the last has bMore set. Parts from different senders (or different
// conversations) can interleave, so they are collected per conversation.

#pragma once

#include "ttc/sdk.h"

#include <QHash>
#include <QString>

#include <optional>

namespace ttc {

struct AssembledMessage
{
    TextMsgType type = MSGTYPE_NONE;
    int fromUserId = 0;
    QString fromUsername;
    int toUserId = 0;
    int channelId = 0;
    QString text;
};

class TextAssembler
{
public:
    /// Add one SDK text message part. Returns the complete message once its
    /// final part (bMore == FALSE) arrives, otherwise std::nullopt.
    std::optional<AssembledMessage> add(const TextMessage& part);

    /// Drop pending parts from a user (e.g. when they log out).
    void clearUser(int userId);
    void clear() { m_pending.clear(); }
    int pendingCount() const { return int(m_pending.size()); }

private:
    QHash<QString, AssembledMessage> m_pending;
};

} // namespace ttc
