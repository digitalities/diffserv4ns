/*
 * Copyright (C) 2026 Sergio Andreozzi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "diffserv-send-time-tag.h"

#include "ns3/log.h"
#include "ns3/type-id.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DiffServSendTimeTag");

namespace diffserv
{

NS_OBJECT_ENSURE_REGISTERED(DiffServSendTimeTag);

TypeId
DiffServSendTimeTag::GetTypeId()
{
    static TypeId tid = TypeId("ns3::diffserv::DiffServSendTimeTag")
                            .SetParent<Tag>()
                            .SetGroupName("DiffServ")
                            .AddConstructor<DiffServSendTimeTag>();
    return tid;
}

TypeId
DiffServSendTimeTag::GetInstanceTypeId() const
{
    return GetTypeId();
}

DiffServSendTimeTag::DiffServSendTimeTag()
    : Tag(),
      m_sendTime(0.0)
{
}

DiffServSendTimeTag::DiffServSendTimeTag(double sendTimeSeconds)
    : Tag(),
      m_sendTime(sendTimeSeconds)
{
}

void
DiffServSendTimeTag::SetSendTime(double sendTimeSeconds)
{
    m_sendTime = sendTimeSeconds;
}

double
DiffServSendTimeTag::GetSendTime() const
{
    return m_sendTime;
}

uint32_t
DiffServSendTimeTag::GetSerializedSize() const
{
    return 8; // sizeof(double)
}

void
DiffServSendTimeTag::Serialize(TagBuffer i) const
{
    i.WriteDouble(m_sendTime);
}

void
DiffServSendTimeTag::Deserialize(TagBuffer i)
{
    m_sendTime = i.ReadDouble();
}

void
DiffServSendTimeTag::Print(std::ostream& os) const
{
    os << "DiffServSendTime=" << m_sendTime << "s";
}

} // namespace diffserv
} // namespace ns3
