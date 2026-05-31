/*
 * Copyright (C) 2026 Sergio Andreozzi
 * Copyright (C) 2026 Sergio Andreozzi (ns-3 port)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ds-rate-based-global-clock.h"

#include "ns3/log.h"

namespace ns3
{
namespace diffserv
{

NS_LOG_COMPONENT_DEFINE("DsRateBasedGlobalClock");

bool
RateBasedGlobalClock::MaybeAllow(Time now) const
{
    return now >= tNext;
}

void
RateBasedGlobalClock::Charge(uint32_t adjLen, Time /*now*/)
{
    if (rateBps == 0)
    {
        return;
    }

    // Global clock advances unconditionally per cake_advance_shaper:
    //   q->time_next_packet = ktime_add_ns(q->time_next_packet, global_dur);
    __int128 numer = static_cast<__int128>(adjLen) * 8 * 1'000'000'000ULL;
    int64_t globalDurNs = static_cast<int64_t>(numer / rateBps);
    tNext = tNext + NanoSeconds(globalDurNs);
}

void
RateBasedGlobalClock::OnEnqueueIdleReset(Time now)
{
    // Linux cake_enqueue (when !sch->q.qlen): hard snap-to-now if the
    // aggregate clock fell behind real time during the all-tins-empty
    // idle period. Mirrors the per-tin site at the global scope.
    if (tNext < now)
    {
        tNext = now;
    }
}

} // namespace diffserv
} // namespace ns3
