/*
 * Copyright (C) 2026 Sergio Andreozzi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "diffserv-onoff-helper.h"

#include "ns3/diffserv-onoff-application.h"
#include "ns3/node.h"

namespace ns3
{
namespace diffserv
{

DiffServOnOffHelper::DiffServOnOffHelper(const Address& remote)
{
    m_factory.SetTypeId(DiffServOnOffApplication::GetTypeId());
    m_factory.Set("Remote", AddressValue(remote));
}

void
DiffServOnOffHelper::SetAttribute(const std::string& name, const AttributeValue& value)
{
    m_factory.Set(name, value);
}

Ptr<Application>
DiffServOnOffHelper::InstallOn(Ptr<Node> node) const
{
    Ptr<Application> app = m_factory.Create<Application>();
    node->AddApplication(app);
    return app;
}

ApplicationContainer
DiffServOnOffHelper::Install(Ptr<Node> node) const
{
    ApplicationContainer apps;
    apps.Add(InstallOn(node));
    return apps;
}

ApplicationContainer
DiffServOnOffHelper::Install(NodeContainer nodes) const
{
    ApplicationContainer apps;
    for (auto it = nodes.Begin(); it != nodes.End(); ++it)
    {
        apps.Add(InstallOn(*it));
    }
    return apps;
}

} // namespace diffserv
} // namespace ns3
