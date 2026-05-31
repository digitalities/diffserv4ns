/*
 * Copyright (C) 2001-2026 Sergio Andreozzi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Ported from DiffServ4NS dsscheduler.cc class dsScheduler (2001).
 */

#include "ds-scheduler.h"

#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DsScheduler");

namespace diffserv
{

NS_OBJECT_ENSURE_REGISTERED(DsScheduler);

TypeId
DsScheduler::GetTypeId()
{
    static TypeId tid = TypeId("ns3::diffserv::DsScheduler")
                            .SetParent<Object>()
                            .SetGroupName("DiffServ")
                            .AddAttribute("NumQueues",
                                          "Number of physical queues this scheduler services. "
                                          "Construct-only because per-queue state arrays "
                                          "(weights, queue depths, finish tags) are indexed "
                                          "by this value throughout the instance lifetime.",
                                          TypeId::ATTR_GET | TypeId::ATTR_CONSTRUCT,
                                          UintegerValue(1),
                                          MakeUintegerAccessor(&DsScheduler::m_numQueues),
                                          MakeUintegerChecker<uint32_t>(1, kMaxQueues))
                            .AddAttribute("LinkBandwidth",
                                          "Link bandwidth in bits per second; used by "
                                          "fair-queueing subclasses for finish-time "
                                          "computation. Settable at any time (Config::Set + "
                                          "runtime sweeps supported) but MUST be non-zero "
                                          "before first enqueue on SCFQ / SFQ / WFQ / WF2Q+ / "
                                          "LLQ; those subclasses assert bwBytes > 0.0 at "
                                          "enqueue.",
                                          DoubleValue(0.0),
                                          MakeDoubleAccessor(&DsScheduler::SetLinkBandwidth,
                                                             &DsScheduler::GetLinkBandwidth),
                                          MakeDoubleChecker<double>(0.0))
                            .AddAttribute("L2OverheadBytes",
                                          "L2 framing overhead bytes per packet, added to "
                                          "the IP-layer packet size by FQ subclasses "
                                          "(SCFQ/SFQ/WFQ/WF2Q+/LLQ-inner) before computing "
                                          "virtual-time / finish-time increments. PPP=2, "
                                          "Ethernet=14, SimpleLink=0. Default 0 preserves "
                                          "ns-2.35 SimpleLink parity. Strict-priority is "
                                          "unaffected. The companion meter/policer attribute "
                                          "MUST be set to the same value so meter and "
                                          "scheduler agree on byte basis.",
                                          UintegerValue(0),
                                          MakeUintegerAccessor(&DsScheduler::SetL2OverheadBytes,
                                                               &DsScheduler::GetL2OverheadBytes),
                                          MakeUintegerChecker<uint32_t>(0, 64));
    return tid;
}

DsScheduler::DsScheduler()
{
    for (uint32_t i = 0; i < kMaxQueues; ++i)
    {
        m_queueAvgRate[i] = 0.0;
        m_queueArrTime[i] = 0.0;
        for (uint32_t j = 0; j < kMaxPrec; ++j)
        {
            m_qpAvgRate[i][j] = 0.0;
            m_qpArrTime[i][j] = 0.0;
        }
    }
}

DsScheduler::~DsScheduler()
{
}

void
DsScheduler::Reset()
{
}

void
DsScheduler::SetParam(uint32_t /*queueIndex*/, double /*value*/)
{
}

void
DsScheduler::OnEnqueueWithTime(uint32_t queueIndex, uint32_t packetSizeBytes, double /*nowSeconds*/)
{
    // Default: delegate to time-free version for backward compatibility
    OnEnqueue(queueIndex, packetSizeBytes);
}

void
DsScheduler::SetLinkBandwidth(double bandwidthBps)
{
    m_linkBandwidth = bandwidthBps;
}

double
DsScheduler::GetLinkBandwidth() const
{
    return m_linkBandwidth;
}

void
DsScheduler::SetWinLen(double winLenSeconds)
{
    m_winLen = winLenSeconds;
}

double
DsScheduler::GetWinLen() const
{
    return m_winLen;
}

void
DsScheduler::SetL2OverheadBytes(uint32_t bytes)
{
    m_l2OverheadBytes = bytes;
}

uint32_t
DsScheduler::GetL2OverheadBytes() const
{
    return m_l2OverheadBytes;
}

void
DsScheduler::ApplyTswMeter(uint32_t packetSizeBytes,
                           double* avgRate,
                           double* arrTime,
                           double nowSeconds) const
{
    double bytesInTsw = (*avgRate) * m_winLen;
    double newBytes = bytesInTsw + packetSizeBytes;
    *avgRate = newBytes / (nowSeconds - *arrTime + m_winLen);
    *arrTime = nowSeconds;
}

void
DsScheduler::UpdateDepartureRate(uint32_t queueIndex,
                                 uint32_t prec,
                                 uint32_t packetSizeBytes,
                                 double nowSeconds)
{
    for (uint32_t i = 0; i < m_numQueues; ++i)
    {
        uint32_t sz = (i == queueIndex) ? packetSizeBytes : 0;
        ApplyTswMeter(sz, &m_queueAvgRate[i], &m_queueArrTime[i], nowSeconds);
        for (uint32_t j = 0; j < kMaxPrec; ++j)
        {
            uint32_t szp = (i == queueIndex && j == prec) ? packetSizeBytes : 0;
            ApplyTswMeter(szp, &m_qpAvgRate[i][j], &m_qpArrTime[i][j], nowSeconds);
        }
    }
}

double
DsScheduler::GetDepartureRate(uint32_t queueIndex, int prec) const
{
    if (prec < 0)
    {
        return m_queueAvgRate[queueIndex] * 8.0;
    }
    return m_qpAvgRate[queueIndex][prec] * 8.0;
}

} // namespace diffserv
} // namespace ns3
