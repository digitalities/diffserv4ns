/*
 * Copyright (C) 2026 Sergio Andreozzi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "diffserv-app-type-tag.h"

#include "ns3/log.h"
#include "ns3/type-id.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DiffServAppTypeTag");

namespace diffserv
{

NS_OBJECT_ENSURE_REGISTERED(DiffServAppTypeTag);

TypeId
DiffServAppTypeTag::GetTypeId()
{
    static TypeId tid = TypeId("ns3::diffserv::DiffServAppTypeTag")
                            .SetParent<Tag>()
                            .SetGroupName("DiffServ")
                            .AddConstructor<DiffServAppTypeTag>();
    return tid;
}

TypeId
DiffServAppTypeTag::GetInstanceTypeId() const
{
    return GetTypeId();
}

DiffServAppTypeTag::DiffServAppTypeTag()
    : Tag(),
      m_appType(kAnyAppType)
{
    NS_LOG_FUNCTION(this);
}

DiffServAppTypeTag::DiffServAppTypeTag(uint32_t appType)
    : Tag(),
      m_appType(appType)
{
    NS_LOG_FUNCTION(this << appType);
}

void
DiffServAppTypeTag::SetAppType(uint32_t appType)
{
    NS_LOG_FUNCTION(this << appType);
    m_appType = appType;
}

uint32_t
DiffServAppTypeTag::GetAppType() const
{
    NS_LOG_FUNCTION(this);
    return m_appType;
}

uint32_t
DiffServAppTypeTag::GetSerializedSize() const
{
    NS_LOG_FUNCTION(this);
    return 4;
}

void
DiffServAppTypeTag::Serialize(TagBuffer i) const
{
    NS_LOG_FUNCTION(this << &i);
    i.WriteU32(m_appType);
}

void
DiffServAppTypeTag::Deserialize(TagBuffer i)
{
    NS_LOG_FUNCTION(this << &i);
    m_appType = i.ReadU32();
}

void
DiffServAppTypeTag::Print(std::ostream& os) const
{
    NS_LOG_FUNCTION(this << &os);
    os << "DiffServAppType=" << m_appType;
}

} // namespace diffserv
} // namespace ns3
