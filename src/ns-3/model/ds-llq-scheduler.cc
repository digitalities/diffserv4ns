/*
 * Copyright (C) 2001-2026 Sergio Andreozzi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Ported from DiffServ4NS dsscheduler.cc class dsLLQ (2001).
 * Low Latency Queueing (Cisco): strict priority for queue 0,
 * configurable fair-queueing scheduler for queues 1..N-1.
 */

#include "ds-llq-scheduler.h"

#include "ds-scfq-scheduler.h"
#include "ds-sfq-scheduler.h"
#include "ds-wf2qp-scheduler.h"
#include "ds-wfq-scheduler.h"

#include "ns3/assert.h"
#include "ns3/double.h"
#include "ns3/enum.h"
#include "ns3/log.h"
#include "ns3/object-factory.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DsLlqScheduler");

namespace diffserv
{

NS_OBJECT_ENSURE_REGISTERED(DsLlqScheduler);

TypeId
DsLlqScheduler::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::diffserv::DsLlqScheduler")
            .SetParent<DsScheduler>()
            .SetGroupName("DiffServ")
            .AddConstructor<DsLlqScheduler>()
            .AddAttribute("FqVariant",
                          "Inner fair-queueing variant served on queues "
                          "1..N-1. Queue 0 is always priority. "
                          "Construct-only because the inner-scheduler "
                          "object is materialised in DoInitialize; changing "
                          "this after Initialize has no effect on "
                          "already-constructed sub-schedulers.",
                          TypeId::ATTR_GET | TypeId::ATTR_CONSTRUCT,
                          EnumValue(FqVariant::WFQ),
                          MakeEnumAccessor<FqVariant>(&DsLlqScheduler::SetFqVariant,
                                                      &DsLlqScheduler::GetFqVariant),
                          MakeEnumChecker(FqVariant::WFQ,
                                          "WFQ",
                                          FqVariant::WF2Qp,
                                          "WF2Qp",
                                          FqVariant::SCFQ,
                                          "SCFQ",
                                          FqVariant::SFQ,
                                          "SFQ"));
    return tid;
}

DsLlqScheduler::DsLlqScheduler() = default;
DsLlqScheduler::~DsLlqScheduler() = default;

void
DsLlqScheduler::SetFqVariant(FqVariant v)
{
    m_fqVariant = v;
}

DsLlqScheduler::FqVariant
DsLlqScheduler::GetFqVariant() const
{
    return m_fqVariant;
}

void
DsLlqScheduler::NotifyConstructionCompleted()
{
    NS_ASSERT_MSG(m_numQueues >= 2, "LLQ requires at least 2 queues");

    const double linkBw = GetLinkBandwidth();
    // Forward the wire-byte basis to the inner FQ sub-scheduler so a
    // single L2OverheadBytes set on the LLQ propagates to the PFQ that
    // computes finish times. PQ sub-scheduler does not consume it.
    const uint32_t l2 = GetL2OverheadBytes();

    // PQ sub-scheduler for queue 0 (1 queue, winLen=1).
    m_pq = CreateObjectWithAttributes<DsPriorityScheduler>("NumQueues",
                                                           UintegerValue(1),
                                                           "WinLen",
                                                           DoubleValue(1.0));

    // FQ sub-scheduler for queues 1..N-1.
    const uint32_t fqQueues = m_numQueues - 1;
    switch (m_fqVariant)
    {
    case FqVariant::WFQ:
        m_pfq = CreateObjectWithAttributes<DsWfqScheduler>("NumQueues",
                                                           UintegerValue(fqQueues),
                                                           "LinkBandwidth",
                                                           DoubleValue(linkBw),
                                                           "L2OverheadBytes",
                                                           UintegerValue(l2));
        break;
    case FqVariant::WF2Qp:
        m_pfq = CreateObjectWithAttributes<DsWf2qPlusScheduler>("NumQueues",
                                                                UintegerValue(fqQueues),
                                                                "LinkBandwidth",
                                                                DoubleValue(linkBw),
                                                                "L2OverheadBytes",
                                                                UintegerValue(l2));
        break;
    case FqVariant::SCFQ:
        m_pfq = CreateObjectWithAttributes<DsScfqScheduler>("NumQueues",
                                                            UintegerValue(fqQueues),
                                                            "LinkBandwidth",
                                                            DoubleValue(linkBw),
                                                            "L2OverheadBytes",
                                                            UintegerValue(l2));
        break;
    case FqVariant::SFQ:
        m_pfq = CreateObjectWithAttributes<DsSfqScheduler>("NumQueues",
                                                           UintegerValue(fqQueues),
                                                           "LinkBandwidth",
                                                           DoubleValue(linkBw),
                                                           "L2OverheadBytes",
                                                           UintegerValue(l2));
        break;
    }

    DsScheduler::NotifyConstructionCompleted();
}

void
DsLlqScheduler::Reset()
{
    m_pq->Reset();
    m_pfq->Reset();
}

void
DsLlqScheduler::OnEnqueue(uint32_t queueIndex, uint32_t packetSizeBytes)
{
    // Delegate to OnEnqueueWithTime with time=0 for backward compat.
    // Callers should prefer OnEnqueueWithTime for FQ sub-schedulers.
    if (queueIndex == 0)
    {
        m_pq->OnEnqueue(0, packetSizeBytes);
    }
    else
    {
        m_pfq->OnEnqueue(queueIndex - 1, packetSizeBytes);
    }
}

void
DsLlqScheduler::OnEnqueueWithTime(uint32_t queueIndex, uint32_t packetSizeBytes, double nowSeconds)
{
    NS_LOG_FUNCTION(this << queueIndex << packetSizeBytes << nowSeconds);
    NS_ASSERT_MSG(queueIndex < m_numQueues, "Queue index out of range");

    if (queueIndex == 0)
    {
        m_pq->OnEnqueueWithTime(0, packetSizeBytes, nowSeconds);
    }
    else
    {
        m_pfq->OnEnqueueWithTime(queueIndex - 1, packetSizeBytes, nowSeconds);
    }
}

int
DsLlqScheduler::SelectNextQueue()
{
    NS_LOG_FUNCTION(this);

    // Priority queue is served strictly first
    int q = m_pq->SelectNextQueue();
    if (q >= 0)
    {
        return 0;
    }

    // Fall back to fair-queueing sub-scheduler
    q = m_pfq->SelectNextQueue();
    if (q >= 0)
    {
        return q + 1; // Offset back to original numbering
    }

    return -1;
}

void
DsLlqScheduler::SetParam(uint32_t queueIndex, double weight)
{
    NS_LOG_FUNCTION(this << queueIndex << weight);

    if (queueIndex == 0)
    {
        // PQ doesn't use weights; silently ignore (matches ns-2 AddParam).
        return;
    }
    m_pfq->SetParam(queueIndex - 1, weight);
}

void
DsLlqScheduler::SetPqRateCap(double rateBps)
{
    NS_LOG_FUNCTION(this << rateBps);
    m_pq->SetParam(0, rateBps);
}

} // namespace diffserv
} // namespace ns3
