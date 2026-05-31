/*
 * Copyright (C) 2026 Sergio Andreozzi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ds-l4s-coupled-scheduler.h"

#include "ns3/log.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DsL4sCoupledScheduler");

namespace diffserv
{

NS_OBJECT_ENSURE_REGISTERED(DsL4sCoupledScheduler);

TypeId
DsL4sCoupledScheduler::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::diffserv::DsL4sCoupledScheduler")
            .SetParent<DsScheduler>()
            .SetGroupName("DiffServ")
            .AddConstructor<DsL4sCoupledScheduler>()
            .AddAttribute("L4sQueueIdx",
                          "Queue index reserved for L4S traffic. "
                          "Construct-only because SelectNextQueue indexes "
                          "into it throughout the instance lifetime; "
                          "changing post-Initialize would misroute pending "
                          "packets.",
                          TypeId::ATTR_GET | TypeId::ATTR_CONSTRUCT,
                          UintegerValue(0),
                          MakeUintegerAccessor(&DsL4sCoupledScheduler::m_l4sQueueIdx),
                          MakeUintegerChecker<uint32_t>(0, kMaxQueues - 1))
            .AddAttribute("BurstCap",
                          "Maximum consecutive L4S dequeues before a "
                          "non-empty classic queue is forced (RFC 9332 §A.4 "
                          "starvation safeguard). Settable at any time; "
                          "counter state recovers naturally.",
                          UintegerValue(8),
                          MakeUintegerAccessor(&DsL4sCoupledScheduler::SetL4sBurstCap,
                                               &DsL4sCoupledScheduler::GetL4sBurstCap),
                          MakeUintegerChecker<uint32_t>(1));
    return tid;
}

DsL4sCoupledScheduler::DsL4sCoupledScheduler()
{
    for (uint32_t i = 0; i < kMaxQueues; ++i)
    {
        m_queueLen[i] = 0;
    }
}

DsL4sCoupledScheduler::~DsL4sCoupledScheduler() = default;

void
DsL4sCoupledScheduler::Reset()
{
    NS_LOG_FUNCTION(this);
    m_l4sBurstCount = 0;
    m_classicRrCursor = 0;
    for (uint32_t i = 0; i < kMaxQueues; ++i)
    {
        m_queueArrTime[i] = 0.0;
        m_queueLen[i] = 0;
    }
}

void
DsL4sCoupledScheduler::OnEnqueue(uint32_t queueIndex, uint32_t /*packetSizeBytes*/)
{
    m_queueLen[queueIndex]++;
}

uint32_t
DsL4sCoupledScheduler::GetL4sBurstCap() const
{
    return m_burstCap;
}

void
DsL4sCoupledScheduler::SetL4sBurstCap(uint32_t cap)
{
    NS_LOG_FUNCTION(this << cap);
    m_burstCap = cap;
}

uint32_t
DsL4sCoupledScheduler::GetL4sBurstCount() const
{
    return m_l4sBurstCount;
}

uint64_t
DsL4sCoupledScheduler::GetForcedClassicCount() const
{
    return m_forcedClassicCount;
}

int
DsL4sCoupledScheduler::SelectNextQueue()
{
    // 1) If the L4S burst cap has been reached AND any classic queue
    //    has packets, force a classic dequeue (round-robin among the
    //    non-L4S queues). Resets the L4S burst counter and increments
    //    the diagnostic forced-classic counter.
    if (m_l4sBurstCount >= m_burstCap)
    {
        for (uint32_t step = 0; step < m_numQueues; ++step)
        {
            uint32_t cand = (m_classicRrCursor + step) % m_numQueues;
            if (cand == m_l4sQueueIdx)
            {
                continue;
            }
            if (m_queueLen[cand] > 0)
            {
                m_queueLen[cand]--;
                m_l4sBurstCount = 0;
                m_classicRrCursor = (cand + 1) % m_numQueues;
                ++m_forcedClassicCount;
                return static_cast<int>(cand);
            }
        }
        // No classic packet available; fall through to L4S.
    }

    // 2) L4S priority: serve the L4S queue if non-empty, increment burst.
    if (m_queueLen[m_l4sQueueIdx] > 0)
    {
        m_queueLen[m_l4sQueueIdx]--;
        ++m_l4sBurstCount;
        return static_cast<int>(m_l4sQueueIdx);
    }

    // 3) L4S empty: serve any classic queue (round-robin). Reset the
    //    L4S burst counter so it doesn't carry stale state across an
    //    idle gap.
    m_l4sBurstCount = 0;
    for (uint32_t step = 0; step < m_numQueues; ++step)
    {
        uint32_t cand = (m_classicRrCursor + step) % m_numQueues;
        if (cand == m_l4sQueueIdx)
        {
            continue;
        }
        if (m_queueLen[cand] > 0)
        {
            m_queueLen[cand]--;
            m_classicRrCursor = (cand + 1) % m_numQueues;
            return static_cast<int>(cand);
        }
    }

    // 4) Nothing to dequeue.
    return -1;
}

} // namespace diffserv
} // namespace ns3
