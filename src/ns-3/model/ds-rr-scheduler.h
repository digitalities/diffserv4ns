/*
 * Copyright (C) 2001-2026 Sergio Andreozzi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Ported from DiffServ4NS dsscheduler.h class dsRR (2001).
 */

#ifndef NS3_DIFFSERV_DS_RR_SCHEDULER_H
#define NS3_DIFFSERV_DS_RR_SCHEDULER_H

#include "ds-scheduler.h"

namespace ns3
{
namespace diffserv
{

/**
 * @ingroup diffserv
 *
 * @brief Round-Robin scheduler.
 *
 * Cycles through the physical queues in fixed index order, dequeueing
 * one packet from each non-empty queue per visit. Empty queues are
 * skipped. Work-conserving: returns -1 only when all queues are empty.
 *
 * Ported from DiffServ4NS `dsRR` (2001). The 2001 thesis §3.3.3
 * describes this as the lowest-overhead scheduler in the suite;
 * fairness is per-packet, not per-byte, so it mis-serves flows whose
 * packet-size distributions differ.
 *
 */
class DsRoundRobinScheduler : public DsScheduler
{
  public:
    static TypeId GetTypeId();
    DsRoundRobinScheduler();
    ~DsRoundRobinScheduler() override;

    void Reset() override;
    void OnEnqueue(uint32_t queueIndex, uint32_t packetSizeBytes) override;
    int SelectNextQueue() override;

  private:
    int m_queueLen[kMaxQueues];
    int m_qToDq{-1};
};

} // namespace diffserv
} // namespace ns3

#endif // NS3_DIFFSERV_DS_RR_SCHEDULER_H
