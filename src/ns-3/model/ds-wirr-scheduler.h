/*
 * Copyright (C) 2001-2026 Sergio Andreozzi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Ported from DiffServ4NS dsscheduler.h class dsWIRR (2001).
 */

#ifndef NS3_DIFFSERV_DS_WIRR_SCHEDULER_H
#define NS3_DIFFSERV_DS_WIRR_SCHEDULER_H

#include "ds-scheduler.h"

#include <cmath>

namespace ns3
{
namespace diffserv
{

/**
 * @ingroup diffserv
 *
 * @brief Weighted Interleaved Round-Robin (WIRR) scheduler.
 *
 * Like WRR each queue carries an integer weight, but the slots are
 * interleaved across the cycle rather than consecutive: one pass
 * visits every queue with remaining slice-count > 0, decrements that
 * count, and advances. Weights `{3, 1}` therefore produce
 * `A B A A | A B A A | ...` — smoother than WRR's
 * `A A A B | A A A B | ...` for the same rate ratio.
 *
 * Ported from DiffServ4NS `dsWIRR` (2001). Thesis §3.3.3 motivates
 * this variant as the default when short-term burstiness matters.
 *
 */
class DsWeightedInterleavedRoundRobinScheduler : public DsScheduler
{
  public:
    static TypeId GetTypeId();
    DsWeightedInterleavedRoundRobinScheduler();
    ~DsWeightedInterleavedRoundRobinScheduler() override;

    void Reset() override;
    void OnEnqueue(uint32_t queueIndex, uint32_t packetSizeBytes) override;
    int SelectNextQueue() override;
    void SetParam(uint32_t queueIndex, double weight) override;

  private:
    int m_queueLen[kMaxQueues];
    int m_queueWeight[kMaxQueues];
    int m_wirrTemp[kMaxQueues];
    int m_slicecount[kMaxQueues];
    bool m_wirrqDone[kMaxQueues];
    int m_qToDq{0};
    uint32_t m_queuesDone{kMaxQueues};
};

} // namespace diffserv
} // namespace ns3

#endif // NS3_DIFFSERV_DS_WIRR_SCHEDULER_H
