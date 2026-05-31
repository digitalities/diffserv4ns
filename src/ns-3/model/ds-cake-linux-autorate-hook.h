/*
 * Copyright (C) 2026 Sergio Andreozzi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef NS3_DIFFSERV_DS_CAKE_LINUX_AUTORATE_HOOK_H
#define NS3_DIFFSERV_DS_CAKE_LINUX_AUTORATE_HOOK_H

#include "ns3/ds-cake-helper.h"
#include "ns3/nstime.h"

#include <cstdint>

namespace ns3
{
namespace diffserv
{

/**
 * @brief Linux-faithful peak-rate-EWMA autorate hook.
 *
 * Implements the autorate-ingress algorithm from
 * `sch_cake.c::cake_enqueue`: per-packet EWMA of inter-arrival times
 * (alpha=1/2 if new interval > old for fast slowdown response;
 * alpha=1/8 otherwise for sticky-on-bursts), a window closed when an
 * inter-arrival exceeds the running average, an EWMA over window-bps
 * with the same alpha rule for `avg_peak_bandwidth`, a 250 ms reconfig
 * deadband, and target rate = 0.9375 x measured peak
 * (15/16 in Linux fixed-point arithmetic).
 *
 * \see provenance/linux-sch-cake-67dc6c56b871/sch_cake.c::cake_enqueue
 */
class DsCakeLinuxAutorateHook : public DsCakeAutorateIngressHook
{
  public:
    DsCakeLinuxAutorateHook();

    /**
     * @brief Observe a packet arrival and update EWMA state.
     *
     * @param adjLenBytes packet wire-length after overhead/MPU/framing
     * @param now         current simulation time
     */
    void OnEnqueue(uint32_t adjLenBytes, Time now);

    int64_t ComputeRateDelta(uint64_t currentRateBps) const override;

  private:
    /**
     * @brief One-step EWMA: old + (new - old) >> shift, handling
     *        unsigned underflow safely.
     *
     * @param oldVal current EWMA
     * @param newVal new sample
     * @param shift  log2 of the filter weight (1 => alpha=1/2,
     *               3 => alpha=1/8)
     * @return updated EWMA
     */
    static uint64_t EwmaUpdate(uint64_t oldVal, uint64_t newVal, uint32_t shift);

    Time m_lastEnqueue{Time(0)};        //!< Simulation time of the previous arrival
    uint64_t m_avgPacketInterval{0};    //!< EWMA of inter-arrival time in nanoseconds
    bool m_windowOpen{false};           //!< True once the first window has started
    Time m_windowStart{Time(0)};        //!< Start of the current measurement window
    uint64_t m_windowBytes{0};          //!< Bytes accumulated in current window (clamped to UINT32_MAX)
    uint64_t m_avgPeakBandwidth{0};     //!< EWMA of per-window peak bandwidth in bps
    mutable bool m_haveReconfigured{false}; //!< True once ComputeRateDelta has run at least once
    mutable Time m_lastReconfig{Time(0)};   //!< Time of the last ComputeRateDelta update
};

} // namespace diffserv
} // namespace ns3

#endif /* NS3_DIFFSERV_DS_CAKE_LINUX_AUTORATE_HOOK_H */
