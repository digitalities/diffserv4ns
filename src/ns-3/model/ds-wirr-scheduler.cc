/*
 * Copyright (C) 2001-2026 Sergio Andreozzi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Ported from DiffServ4NS dsscheduler.cc class dsWIRR (2001).
 */

#include "ds-wirr-scheduler.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DsWeightedInterleavedRoundRobinScheduler");

namespace diffserv
{

NS_OBJECT_ENSURE_REGISTERED(DsWeightedInterleavedRoundRobinScheduler);

TypeId
DsWeightedInterleavedRoundRobinScheduler::GetTypeId()
{
    static TypeId tid = TypeId("ns3::diffserv::DsWeightedInterleavedRoundRobinScheduler")
                            .SetParent<DsScheduler>()
                            .SetGroupName("DiffServ")
                            .AddConstructor<DsWeightedInterleavedRoundRobinScheduler>();
    return tid;
}

DsWeightedInterleavedRoundRobinScheduler::DsWeightedInterleavedRoundRobinScheduler()
{
    for (uint32_t i = 0; i < kMaxQueues; ++i)
    {
        m_queueLen[i] = 0;
        m_queueWeight[i] = 1;
        m_wirrTemp[i] = 0;
        m_slicecount[i] = 0;
        m_wirrqDone[i] = false;
    }
    Reset();
}

DsWeightedInterleavedRoundRobinScheduler::~DsWeightedInterleavedRoundRobinScheduler() = default;

void
DsWeightedInterleavedRoundRobinScheduler::Reset()
{
    for (uint32_t i = 0; i < kMaxQueues; ++i)
    {
        m_slicecount[i] = 0;
        m_wirrTemp[i] = 0;
        m_wirrqDone[i] = false;
    }
}

void
DsWeightedInterleavedRoundRobinScheduler::OnEnqueue(uint32_t queueIndex,
                                                    uint32_t /*packetSizeBytes*/)
{
    m_queueLen[queueIndex]++;
}

void
DsWeightedInterleavedRoundRobinScheduler::SetParam(uint32_t queueIndex, double weight)
{
    m_queueWeight[queueIndex] = static_cast<int>(std::ceil(weight));
}

int
DsWeightedInterleavedRoundRobinScheduler::SelectNextQueue()
{
    uint32_t i = 0;
    m_qToDq = (m_qToDq + 1) % static_cast<int>(m_numQueues);
    while (i < m_numQueues && (m_queueLen[m_qToDq] == 0 || m_wirrqDone[m_qToDq]))
    {
        if (!m_wirrqDone[m_qToDq] && m_queueLen[m_qToDq] == 0)
        {
            m_queuesDone++;
            m_wirrqDone[m_qToDq] = true;
        }
        m_qToDq = (m_qToDq + 1) % static_cast<int>(m_numQueues);
        ++i;
    }
    if (m_wirrTemp[m_qToDq] == 1)
    {
        m_queuesDone++;
        m_wirrqDone[m_qToDq] = true;
    }
    m_wirrTemp[m_qToDq]--;
    if (m_queuesDone >= m_numQueues)
    {
        m_queuesDone = 0;
        for (uint32_t j = 0; j < m_numQueues; ++j)
        {
            m_wirrTemp[j] = m_queueWeight[j];
            m_wirrqDone[j] = false;
        }
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
