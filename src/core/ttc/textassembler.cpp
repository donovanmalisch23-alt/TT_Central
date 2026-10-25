// SPDX-License-Identifier: GPL-3.0-or-later
#include "ttc/textassembler.h"

namespace ttc {

namespace {
QString keyFor(const TextMessage& m)
{
    return QStringLiteral("%1:%2:%3:%4")
        .arg(int(m.nMsgType)).arg(m.nFromUserID).arg(m.nToUserID).arg(m.nChannelID);
}
} // namespace

std::optional<AssembledMessage> TextAssembler::add(const TextMessage& part)
{
    const QString key = keyFor(part);
    auto it = m_pending.find(key);
    if (it == m_pending.end())
    {
        AssembledMessage msg;
        msg.type = part.nMsgType;
        msg.fromUserId = part.nFromUserID;
        msg.fromUsername = fromTT(part.szFromUsername);
        msg.toUserId = part.nToUserID;
        msg.channelId = part.nChannelID;
        it = m_pending.insert(key, msg);
    }
    it->text += fromTT(part.szMessage);

    if (part.bMore)
        return std::nullopt;

    AssembledMessage done = *it;
    m_pending.erase(it);
    return done;
}

void TextAssembler::clearUser(int userId)
{
    for (auto it = m_pending.begin(); it != m_pending.end();)
    {
        if (it->fromUserId == userId)
            it = m_pending.erase(it);
        else
            ++it;
    }
}

} // namespace ttc
