/*
 * Copyright (C) 2001-2026 Sergio Andreozzi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Ported from DiffServ4NS dsscheduler.cc class dsWRR (2001).
 */

#include "ds-wrr-scheduler.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DsWeightedRoundRobinScheduler");

namespace diffserv
{

NS_OBJECT_ENSURE_REGISTERED(DsWeightedRoundRobinScheduler);

TypeId
DsWeightedRoundRobinScheduler::GetTypeId()
{
    static TypeId tid = TypeId("ns3::diffserv::DsWeightedRoundRobinScheduler")
                            .SetParent<DsScheduler>()
                            .SetGroupName("DiffServ")
                            .AddConstructor<DsWeightedRoundRobinScheduler>();
    return tid;
}

DsWeightedRoundRobinScheduler::DsWeightedRoundRobinScheduler()
{
    for (uint32_t i = 0; i < kMaxQueues; ++i)
    {
        m_queueLen[i] = 0;
        m_queueWeight[i] = 1;
        m_wirrTemp[i] = 0;
    }
}

DsWeightedRoundRobinScheduler::~DsWeightedRoundRobinScheduler() = default;

void
DsWeightedRoundRobinScheduler::Reset()
{
    for (uint32_t i = 0; i < kMaxQueues; ++i)
    {
        m_wirrTemp[i] = 0;
    }
}

void
DsWeightedRoundRobinScheduler::OnEnqueue(uint32_t queueIndex, uint32_t /*packetSizeBytes*/)
{
    m_queueLen[queueIndex]++;
}

void
DsWeightedRoundRobinScheduler::SetParam(uint32_t queueIndex, double weight)
{
    m_queueWeight[queueIndex] = static_cast<int>(std::ceil(weight));
}

int
DsWeightedRoundRobinScheduler::SelectNextQueue()
{
    uint32_t i = 0;
    if (m_wirrTemp[m_qToDq] <= 0)
    {
        m_qToDq = (m_qToDq + 1) % static_cast<int>(m_numQueues);
        m_wirrTemp[m_qToDq] = m_queueWeight[m_qToDq] - 1;
    }
    else
    {
        m_wirrTemp[m_qToDq]--;
    }

    while (i < m_numQueues && m_queueLen[m_qToDq] == 0)
    {
        m_wirrTemp[m_qToDq] = 0;
        m_qToDq = (m_qToDq + 1) % static_cast<int>(m_numQueues);
        m_wirrTemp[m_qToDq] = m_queueWeight[m_qToDq] - 1;
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
