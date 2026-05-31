/*
 * Copyright (C) 2026 Sergio Andreozzi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef NS3_DIFFSERV_ONOFF_HELPER_H
#define NS3_DIFFSERV_ONOFF_HELPER_H

#include "ns3/address.h"
#include "ns3/application-container.h"
#include "ns3/attribute.h"
#include "ns3/node-container.h"
#include "ns3/object-factory.h"
#include "ns3/ptr.h"

#include <string>

namespace ns3
{
class Node;

namespace diffserv
{

/**
 * @ingroup diffserv
 *
 * @brief Convenience helper for installing DiffServOnOffApplication instances.
 *
 * Mirrors the ns-3 OnOffHelper API: construct with the remote sink address,
 * call SetAttribute() to configure, then Install() on a Node or NodeContainer
 * to create the applications and return an ApplicationContainer.
 *
 * @code
 * DiffServOnOffHelper voip(InetSocketAddress(sinkAddr, 5060));
 * voip.SetAttribute("PacketSize", UintegerValue(48));
 * voip.SetAttribute("DataRate",   UintegerValue(6400));   // bits/s
 * voip.SetAttribute("OnMean",     DoubleValue(0.340));
 * voip.SetAttribute("OffMean",    DoubleValue(0.427));
 * ApplicationContainer apps = voip.Install(senderNodes);
 * @endcode
 *
 * @see DiffServOnOffApplication
 */
class DiffServOnOffHelper
{
  public:
    /**
     * @param remote the remote (sink) address — typically InetSocketAddress(ip,
     * port).
     */
    explicit DiffServOnOffHelper(const Address& remote);

    /**
     * Set an attribute on each DiffServOnOffApplication created by this helper.
     */
    void SetAttribute(const std::string& name, const AttributeValue& value);

    /** Install one application on a single node. */
    ApplicationContainer Install(Ptr<Node> node) const;

    /** Install one application on each node in the container. */
    ApplicationContainer Install(NodeContainer nodes) const;

  private:
    Ptr<Application> InstallOn(Ptr<Node> node) const;

    ObjectFactory m_factory;
};

} // namespace diffserv
} // namespace ns3

#endif // NS3_DIFFSERV_ONOFF_HELPER_H
