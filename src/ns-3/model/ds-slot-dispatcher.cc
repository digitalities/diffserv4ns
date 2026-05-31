/*
 * Copyright (C) 2026 Sergio Andreozzi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 */

#include "ds-slot-dispatcher.h"

#include "diffserv-edge-queue-disc.h"

#include "ns3/log.h"
#include "ns3/queue-disc.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DsSlotDispatcher");

namespace diffserv
{

NS_OBJECT_ENSURE_REGISTERED(DsSlotDispatcher);

TypeId
DsSlotDispatcher::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::diffserv::DsSlotDispatcher").SetParent<Object>().SetGroupName("DiffServ");
    return tid;
}

NS_OBJECT_ENSURE_REGISTERED(DsStrictPriorityDispatcher);

TypeId
DsStrictPriorityDispatcher::GetTypeId()
{
    static TypeId tid = TypeId("ns3::diffserv::DsStrictPriorityDispatcher")
                            .SetParent<DsSlotDispatcher>()
                            .SetGroupName("DiffServ")
                            .AddConstructor<DsStrictPriorityDispatcher>();
    return tid;
}

int32_t
DsStrictPriorityDispatcher::SelectDequeueSlot(DiffServEdgeQueueDisc* edge)
{
    return FirstNonEmpty(edge);
}

int32_t
DsStrictPriorityDispatcher::PeekSlot(DiffServEdgeQueueDisc* edge)
{
    return FirstNonEmpty(edge);
}

int32_t
DsStrictPriorityDispatcher::FirstNonEmpty(DiffServEdgeQueueDisc* edge) const
{
    NS_ASSERT_MSG(edge, "DsStrictPriorityDispatcher requires a non-null edge");
    const uint32_t kMax = DiffServEdgeQueueDisc::kMaxInnerSlots;
    for (uint32_t s = 0; s < kMax; ++s)
    {
        Ptr<QueueDisc> inner = edge->GetInnerDiscAt(s);
        if (!inner)
        {
            break; // slots fill monotonically — stop at first empty
        }
        if (inner->GetNPackets() > 0)
        {
            return static_cast<int32_t>(s);
        }
    }
    return -1;
}

} // namespace diffserv
} // namespace ns3
