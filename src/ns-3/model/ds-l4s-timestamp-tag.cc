/*
 * Copyright (C) 2026 Sergio Andreozzi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ds-l4s-timestamp-tag.h"

namespace ns3
{
namespace diffserv
{

NS_OBJECT_ENSURE_REGISTERED(DsL4sTimestampTag);

TypeId
DsL4sTimestampTag::GetTypeId()
{
    static TypeId tid = TypeId("ns3::diffserv::DsL4sTimestampTag")
                            .SetParent<Tag>()
                            .SetGroupName("DiffServ")
                            .AddConstructor<DsL4sTimestampTag>();
    return tid;
}

TypeId
DsL4sTimestampTag::GetInstanceTypeId() const
{
    return GetTypeId();
}

DsL4sTimestampTag::DsL4sTimestampTag() = default;

DsL4sTimestampTag::DsL4sTimestampTag(Time t)
    : m_timestamp(t)
{
}

void
DsL4sTimestampTag::Serialize(TagBuffer i) const
{
    i.WriteU64(m_timestamp.GetTimeStep());
}

void
DsL4sTimestampTag::Deserialize(TagBuffer i)
{
    m_timestamp = TimeStep(i.ReadU64());
}

uint32_t
DsL4sTimestampTag::GetSerializedSize() const
{
    return sizeof(uint64_t);
}

void
DsL4sTimestampTag::Print(std::ostream& os) const
{
    os << "ts=" << m_timestamp.As(Time::S);
}

Time
DsL4sTimestampTag::GetTimestamp() const
{
    return m_timestamp;
}

void
DsL4sTimestampTag::SetTimestamp(Time t)
{
    m_timestamp = t;
}

} // namespace diffserv
} // namespace ns3
