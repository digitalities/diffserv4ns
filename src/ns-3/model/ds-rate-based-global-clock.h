/*
 * Copyright (C) 2026 Sergio Andreozzi
 * Copyright (C) 2026 Sergio Andreozzi (ns-3 port)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef NS3_DIFFSERV_DS_RATE_BASED_GLOBAL_CLOCK_H
#define NS3_DIFFSERV_DS_RATE_BASED_GLOBAL_CLOCK_H

#include "ns3/nstime.h"

#include <cstdint>

namespace ns3
{
namespace diffserv
{

/**
 * @brief Aggregate-rate virtual clock for the CAKE rate-based shaper.
 *
 * Mirrors `q->time_next_packet` on `struct cake_sched_data`. The global
 * clock advances on every dequeue (regardless of which tin) and binds
 * the aggregate egress rate independently of the per-tin clocks.
 *
 * Per Linux `cake_configure_rates`, the global rate is set to the
 * fastest tin's rate; this implementation accepts any rate set by
 * the dispatcher at construction.
 */
struct RateBasedGlobalClock
{
    /// Next eligible egress time for the aggregate.
    Time tNext{Time(0)};

    /// Aggregate rate (bits per second). Zero means unshaped.
    uint64_t rateBps{0};

    /**
     * @brief Test whether aggregate is eligible at @p now.
     */
    bool MaybeAllow(Time now) const;

    /**
     * @brief Advance the global clock unconditionally by adj_len/rate.
     *        Single branch: tNext += adjLen / rateBps. No three-branch logic.
     */
    void Charge(uint32_t adjLen, Time now);

    /**
     * @brief Hard snap-to-now when all tins were empty (cake_enqueue site).
     */
    void OnEnqueueIdleReset(Time now);
};

} // namespace diffserv
} // namespace ns3

#endif /* NS3_DIFFSERV_DS_RATE_BASED_GLOBAL_CLOCK_H */
