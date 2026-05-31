/*
 * Copyright (C) 2001-2026 Sergio Andreozzi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Ported from DiffServ4NS dsscheduler.cc class dsRR (2001).
 */

#include "ds-rr-scheduler.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DsRoundRobinScheduler");

namespace diffserv
{

NS_OBJECT_ENSURE_REGISTERED(DsRoundRobinScheduler);

TypeId
DsRoundRobinScheduler::GetTypeId()
{
    static TypeId tid = TypeId("ns3::diffserv::DsRoundRobinScheduler")
                            .SetParent<DsScheduler>()
                            .SetGroupName("DiffServ")
                            .AddConstructor<DsRoundRobinScheduler>();
    return tid;
}

DsRoundRobinScheduler::DsRoundRobinScheduler()
{
    for (uint32_t i = 0; i < kMaxQueues; ++i)
    {
        m_queueLen[i] = 0;
    }
}

DsRoundRobinScheduler::~DsRoundRobinScheduler() = default;

void
DsRoundRobinScheduler::Reset()
{
    m_qToDq = -1;
    for (uint32_t i = 0; i < kMaxQueues; ++i)
    {
        m_queueLen[i] = 0;
    }
}

void
DsRoundRobinScheduler::OnEnqueue(uint32_t queueIndex, uint32_t /*packetSizeBytes*/)
{
    m_queueLen[queueIndex]++;
}

int
DsRoundRobinScheduler::SelectNextQueue()
{
    uint32_t i = 0;
    m_qToDq = (m_qToDq + 1) % static_cast<int>(m_numQueues);
    while (i < m_numQueues && m_queueLen[m_qToDq] == 0)
    {
        m_qToDq = (m_qToDq + 1) % static_cast<int>(m_numQueues);
        ++i;
    }
    if (i == m_numQueues)
    {
        return -1;
    }
    m_queueLen[m_qToDq]--;
    return m_qToDq;
}

} // namespace diffserv
} // namespace ns3
