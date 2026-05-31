/*
 * Copyright (C) 2026 Sergio Andreozzi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ds-cake-linux-autorate-hook.h"

#include "ns3/log.h"
#include "ns3/simulator.h"

namespace ns3
{
namespace diffserv
{

NS_LOG_COMPONENT_DEFINE("DsCakeLinuxAutorateHook");

DsCakeLinuxAutorateHook::DsCakeLinuxAutorateHook() = default;

uint64_t
DsCakeLinuxAutorateHook::EwmaUpdate(uint64_t oldVal, uint64_t newVal, uint32_t shift)
{
    if (newVal >= oldVal)
    {
        return oldVal + ((newVal - oldVal) >> shift);
    }
    return oldVal - ((oldVal - newVal) >> shift);
}

void
DsCakeLinuxAutorateHook::OnEnqueue(uint32_t adjLenBytes, Time now)
{
    NS_LOG_FUNCTION(this << adjLenBytes << now);

    if (!m_windowOpen)
    {
        m_lastEnqueue = now;
        m_windowStart = now;
        m_windowBytes = adjLenBytes;
        m_windowOpen = true;
        return;
    }

    const uint64_t interval = (now - m_lastEnqueue).GetNanoSeconds();
    m_lastEnqueue = now;

    if (interval == 0)
    {
        // Simultaneous arrivals: accumulate bytes, no interval update.
        m_windowBytes += adjLenBytes;
        return;
    }

    // EWMA on inter-arrival: alpha=1/2 if new > old (fast on slowdown),
    // alpha=1/8 otherwise (sticky on bursts).
    if (m_avgPacketInterval == 0)
    {
        m_avgPacketInterval = interval;
    }
    else
    {
        const uint32_t shift = (interval > m_avgPacketInterval) ? 1 : 3;
        m_avgPacketInterval = EwmaUpdate(m_avgPacketInterval, interval, shift);
    }

    // Window close: inter-arrival exceeded the running average.
    if (interval > m_avgPacketInterval && m_windowOpen)
    {
        const uint64_t windowNs = (now - m_windowStart).GetNanoSeconds();
        if (windowNs > 0)
        {
            const uint64_t windowBps =
                (m_windowBytes * 8ULL * 1'000'000'000ULL) / windowNs;
            if (m_avgPeakBandwidth == 0)
            {
                m_avgPeakBandwidth = windowBps;
            }
            else
            {
                const uint32_t bwShift = (windowBps > m_avgPeakBandwidth) ? 1 : 3;
                m_avgPeakBandwidth = EwmaUpdate(m_avgPeakBandwidth, windowBps, bwShift);
            }
        }
        m_windowStart = now;
        m_windowBytes = 0;
    }

    // Clamp accumulator to UINT32_MAX (4 GiB) to prevent the
    // m_windowBytes * 8 * 1e9 multiplication from overflowing uint64_t
    // under sustained high-rate traffic with a very long window.
    constexpr uint64_t kWindowBytesCap = static_cast<uint64_t>(UINT32_MAX);
    if (m_windowBytes <= kWindowBytesCap - adjLenBytes)
    {
        m_windowBytes += adjLenBytes;
    }
    else
    {
        m_windowBytes = kWindowBytesCap;
    }
}

int64_t
DsCakeLinuxAutorateHook::ComputeRateDelta(uint64_t currentRateBps) const
{
    const Time now = Simulator::Now();
    if (m_haveReconfigured && (now - m_lastReconfig) < MilliSeconds(250))
    {
        return 0;
    }
    m_haveReconfigured = true;
    m_lastReconfig = now;

    if (m_avgPeakBandwidth == 0)
    {
        return 0;
    }

    // Target rate = avg_peak_bandwidth × 15/16 (0.9375 x measured peak).
    const uint64_t targetBps = (m_avgPeakBandwidth * 15ULL) >> 4;
    return static_cast<int64_t>(targetBps) - static_cast<int64_t>(currentRateBps);
}

} // namespace diffserv
} // namespace ns3
