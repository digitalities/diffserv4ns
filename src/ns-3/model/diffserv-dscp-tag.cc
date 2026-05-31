/*
 * Copyright (C) 2026 Sergio Andreozzi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "diffserv-dscp-tag.h"

#include "ns3/log.h"
#include "ns3/type-id.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DiffServDscpTag");

namespace diffserv
{

NS_OBJECT_ENSURE_REGISTERED(DiffServDscpTag);

TypeId
DiffServDscpTag::GetTypeId()
{
    static TypeId tid = TypeId("ns3::diffserv::DiffServDscpTag")
                            .SetParent<Tag>()
                            .SetGroupName("DiffServ")
                            .AddConstructor<DiffServDscpTag>();
    return tid;
}

TypeId
DiffServDscpTag::GetInstanceTypeId() const
{
    return GetTypeId();
}

DiffServDscpTag::DiffServDscpTag()
    : Tag(),
      m_dscp(0)
{
    NS_LOG_FUNCTION(this);
}

DiffServDscpTag::DiffServDscpTag(uint8_t dscp)
    : Tag(),
      m_dscp(dscp)
{
    NS_LOG_FUNCTION(this << static_cast<uint32_t>(dscp));
}

void
DiffServDscpTag::SetDscp(uint8_t dscp)
{
    NS_LOG_FUNCTION(this << static_cast<uint32_t>(dscp));
    m_dscp = dscp;
}

uint8_t
DiffServDscpTag::GetDscp() const
{
    NS_LOG_FUNCTION(this);
    return m_dscp;
}

uint32_t
DiffServDscpTag::GetSerializedSize() const
{
    NS_LOG_FUNCTION(this);
    return 1;
}

void
DiffServDscpTag::Serialize(TagBuffer i) const
{
    NS_LOG_FUNCTION(this << &i);
    i.WriteU8(m_dscp);
}

void
DiffServDscpTag::Deserialize(TagBuffer i)
{
    NS_LOG_FUNCTION(this << &i);
    m_dscp = i.ReadU8();
}

void
DiffServDscpTag::Print(std::ostream& os) const
{
    NS_LOG_FUNCTION(this << &os);
    os << "DiffServDscp=" << static_cast<uint32_t>(m_dscp);
}

} // namespace diffserv
} // namespace ns3
