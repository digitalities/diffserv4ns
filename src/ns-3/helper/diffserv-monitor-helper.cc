/*
 * Copyright (C) 2026 Sergio Andreozzi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 */

#include "diffserv-monitor-helper.h"

#include "diffserv-helper.h"

#include "ns3/abort.h"
#include "ns3/diffserv-core-queue-disc.h"
#include "ns3/diffserv-edge-queue-disc.h"
#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/queue-item.h"
#include "ns3/simulator.h"
#include "ns3/tcp-retransmit-tag.h"

#include <iostream>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DiffServMonitorHelper");

namespace diffserv
{

DiffServMonitorHelper::DiffServMonitorHelper()
    : m_disc(nullptr),
      m_stats(nullptr),
      m_outputDir("."),
      m_departureRateInterval(Seconds(1.0)),
      m_queueLengthInterval(Seconds(0.5)),
      m_samplingStartTime(Seconds(0.0)),
      m_installed(false)
{
    NS_LOG_FUNCTION(this);
}

DiffServMonitorHelper::~DiffServMonitorHelper()
{
    // Cancel any pending sampling events before file handles are torn
    // down. The callbacks hold a raw `this` pointer (the helper is not
    // an ns-3 Object), so a helper that goes out of scope before
    // `Simulator::Destroy()` would otherwise leave a use-after-free
    // waiting in the event queue.
    Simulator::Cancel(m_depRateEvent);
    Simulator::Cancel(m_qLenEvent);
    Close();
}

// -- Configuration -----------------------------------------------------------

void
DiffServMonitorHelper::SetOutputDirectory(const std::string& dir)
{
    NS_LOG_FUNCTION(this << dir);
    m_outputDir = dir;
}

void
DiffServMonitorHelper::SetDepartureRateInterval(Time interval)
{
    NS_LOG_FUNCTION(this << interval);
    m_departureRateInterval = interval;
}

void
DiffServMonitorHelper::SetQueueLengthInterval(Time interval)
{
    NS_LOG_FUNCTION(this << interval);
    m_queueLengthInterval = interval;
}

void
DiffServMonitorHelper::SetSamplingStartTime(Time startTime)
{
    NS_LOG_FUNCTION(this << startTime);
    m_samplingStartTime = startTime;
}

// -- Installation ------------------------------------------------------------

void
DiffServMonitorHelper::Install(Ptr<QueueDisc> disc)
{
    NS_LOG_FUNCTION(this << disc);
    NS_ASSERT_MSG(!m_installed, "DiffServMonitorHelper::Install() called twice");
    NS_ASSERT_MSG(disc, "DiffServMonitorHelper::Install() called with null disc");

    // Resolve to the inner queue disc. Accept:
    // - a bare QueueDisc directly (Red / L4S / other),
    // - a DiffServEdgeQueueDisc composer (unwrap via GetInnerDisc),
    // - a DiffServCoreQueueDisc composer (unwrap via GetInnerDisc).
    //
    // The helper tolerates any inner type for periodic queue-length
    // sampling (via ns-3's generic `GetNQueueDiscClasses()` + per-class
    // `GetNPackets()`). Red-specific operations — scheduler sampling
    // and DsEnqueue / DsDequeue / DsDrop trace wiring — still require
    // a `DsRedQueueDisc` and degrade to warnings when absent.
    Ptr<QueueDisc> target = disc;
    if (auto e = DynamicCast<DiffServEdgeQueueDisc>(disc))
    {
        target = e->GetInnerDisc();
        NS_ASSERT_MSG(target,
                      "DiffServEdgeQueueDisc inner not yet materialised — "
                      "install the inner (e.g. via helper.InstallRedInner) "
                      "before DiffServMonitorHelper::Install");
    }
    else if (auto c = DynamicCast<DiffServCoreQueueDisc>(disc))
    {
        target = c->GetInnerDisc();
        NS_ASSERT_MSG(target,
                      "DiffServCoreQueueDisc inner not yet materialised — "
                      "install the inner before DiffServMonitorHelper::Install");
    }

    m_disc = target;
    m_redDisc = DynamicCast<DsRedQueueDisc>(target);

    // Create statistics collector
    m_stats = CreateObject<DiffServStatistics>();

    // Create output directory if needed
    EnsureDir(m_outputDir);

    // Trace-source wiring is Red-specific: DsEnqueue / DsDequeue / DsDrop
    // emit DSCP-aware payloads that map onto DiffServStatistics buckets.
    // Foreign inners (DsL4sQueueDisc etc.) have different native signals
    // (L4sClassified / L4sMarked / ClassicCoupledDrop) with different
    // semantics; a scenario-specific monitor is needed for those. Log a
    // warning and continue — periodic queue-length sampling still works.
    if (m_redDisc)
    {
        m_redDisc->TraceConnectWithoutContext(
            "DsEnqueue",
            MakeCallback(&DiffServMonitorHelper::OnEnqueue, this));
        m_redDisc->TraceConnectWithoutContext(
            "DsDequeue",
            MakeCallback(&DiffServMonitorHelper::OnDequeue, this));
        m_redDisc->TraceConnectWithoutContext("DsDrop",
                                              MakeCallback(&DiffServMonitorHelper::OnDrop, this));
    }
    else
    {
        NS_LOG_WARN("DiffServMonitorHelper: inner is not DsRedQueueDisc "
                    "(foreign inners like DsL4sQueueDisc have different "
                    "native trace signals); DsEnqueue/DsDequeue/DsDrop "
                    "counters will stay at zero. Periodic queue-length "
                    "sampling still writes.");
    }

    // Open service-rate trace file (only if we have a Red-based scheduler;
    // see SampleDepartureRate for the sampling-side no-op on non-Red).
    std::string serviceRatePath = m_outputDir + "/ServiceRate.tr";
    m_serviceRateFile.open(serviceRatePath);
    NS_ASSERT_MSG(m_serviceRateFile.is_open(), "Failed to open " << serviceRatePath);

    // Open per-queue length trace files. Number of classes comes from
    // the generic `QueueDisc::GetNQueueDiscClasses()` API — works for
    // any inner regardless of type.
    uint32_t numClasses = m_disc->GetNQueueDiscClasses();
    m_queueLenFiles.resize(numClasses);
    for (uint32_t i = 0; i < numClasses; ++i)
    {
        std::string path = m_outputDir + "/QueueLen-Q" + std::to_string(i) + ".tr";
        m_queueLenFiles[i].open(path);
        NS_ASSERT_MSG(m_queueLenFiles[i].is_open(), "Failed to open " << path);
    }

    // Schedule periodic sampling. The returned EventIds are tracked so
    // the destructor can cancel any still-pending callback (see dtor
    // comment).
    m_depRateEvent =
        Simulator::Schedule(m_samplingStartTime, &DiffServMonitorHelper::SampleDepartureRate, this);
    m_qLenEvent =
        Simulator::Schedule(m_samplingStartTime, &DiffServMonitorHelper::SampleQueueLength, this);

    m_installed = true;
    NS_LOG_INFO(
        "DiffServMonitorHelper installed on "
        << m_disc << " with " << numClasses << " queues, output dir: " << m_outputDir
        << (m_redDisc ? " (Red trace wiring active)" : " (foreign inner: length sampling only)"));
}

// -- Post-simulation ---------------------------------------------------------

void
DiffServMonitorHelper::PrintStats() const
{
    NS_LOG_FUNCTION(this);
    if (m_stats)
    {
        m_stats->PrintStats(std::cout);
    }
}

void
DiffServMonitorHelper::Close()
{
    NS_LOG_FUNCTION(this);
    if (m_serviceRateFile.is_open())
    {
        m_serviceRateFile.close();
    }
    for (auto& f : m_queueLenFiles)
    {
        if (f.is_open())
        {
            f.close();
        }
    }
}

Ptr<DiffServStatistics>
DiffServMonitorHelper::GetStats() const
{
    return m_stats;
}

// -- Trace sink callbacks ----------------------------------------------------

void
DiffServMonitorHelper::OnEnqueue(Ptr<const QueueDiscItem> item, uint8_t dscp)
{
    NS_LOG_FUNCTION(this << item << static_cast<uint32_t>(dscp));
    uint32_t bytes = item->GetSize();
    m_stats->RecordEnqueue(dscp, bytes);

    // Bin the byte count by retransmission status. The TcpRetransmitTag is
    // stamped by the patched ns-3 mainline TcpSocketBase::SendDataPacket on
    // every retransmitted segment (patches/ns3/0002) and stripped by
    // mainline at TcpSocketBase::DoForwardUp at the receiver, so we can
    // read it non-destructively here without disrupting any downstream
    // observer. Untagged packets are either fresh TCP transmissions or
    // non-TCP traffic; both count as "original" for thesis-style goodput.
    TcpRetransmitTag retxTag;
    if (item->GetPacket()->PeekPacketTag(retxTag))
    {
        m_stats->RecordRetxBytes(dscp, bytes);
    }
    else
    {
        m_stats->RecordOrigBytes(dscp, bytes);
    }
}

void
DiffServMonitorHelper::OnDequeue(Ptr<const QueueDiscItem> item, uint8_t dscp, uint32_t queueIndex)
{
    NS_LOG_FUNCTION(this << item << static_cast<uint32_t>(dscp) << queueIndex);
    m_stats->RecordDequeue(dscp, item->GetSize());
}

void
DiffServMonitorHelper::OnDrop(Ptr<const QueueDiscItem> item, uint8_t dscp, uint32_t dropReason)
{
    NS_LOG_FUNCTION(this << item << static_cast<uint32_t>(dscp) << dropReason);
    if (dropReason == 0)
    {
        m_stats->RecordRedDrop(dscp, item->GetSize());
    }
    else
    {
        m_stats->RecordTailDrop(dscp, item->GetSize());
    }
}

// -- Periodic sampling callbacks ---------------------------------------------

void
DiffServMonitorHelper::SampleDepartureRate()
{
    NS_LOG_FUNCTION(this);

    double nowSec = Simulator::Now().GetSeconds();
    // DsScheduler is RED-specific — L4S has its own DualPI2 controller,
    // not a DsScheduler. Skip departure-rate sampling on non-Red inners.
    if (!m_redDisc)
    {
        m_depRateEvent = Simulator::Schedule(m_departureRateInterval,
                                             &DiffServMonitorHelper::SampleDepartureRate,
                                             this);
        return;
    }
    Ptr<DsScheduler> sched = m_redDisc->GetScheduler();
    uint32_t numQueues = m_redDisc->GetNumQueues();

    m_serviceRateFile << nowSec;
    for (uint32_t i = 0; i < numQueues; ++i)
    {
        // GetDepartureRate returns bits/s; convert to kbps
        double rateBps = sched->GetDepartureRate(i, -1);
        m_serviceRateFile << " " << (rateBps / 1000.0);
    }
    m_serviceRateFile << "\n";

    // Reschedule (capture EventId so dtor can cancel).
    m_depRateEvent = Simulator::Schedule(m_departureRateInterval,
                                         &DiffServMonitorHelper::SampleDepartureRate,
                                         this);
}

void
DiffServMonitorHelper::SampleQueueLength()
{
    NS_LOG_FUNCTION(this);

    double nowSec = Simulator::Now().GetSeconds();
    // Generic path: works for any QueueDisc inner via ns-3's base-class
    // GetNQueueDiscClasses / GetQueueDiscClass APIs. The monitor helper
    // writes per-class length samples regardless of whether the inner
    // is Red or L4S.
    uint32_t numClasses = m_disc->GetNQueueDiscClasses();

    for (uint32_t i = 0; i < numClasses; ++i)
    {
        uint32_t len = m_disc->GetQueueDiscClass(i)->GetQueueDisc()->GetNPackets();
        m_queueLenFiles[i] << nowSec << " " << len << "\n";
    }

    // Reschedule (capture EventId so dtor can cancel).
    m_qLenEvent =
        Simulator::Schedule(m_queueLengthInterval, &DiffServMonitorHelper::SampleQueueLength, this);
}

} // namespace diffserv
} // namespace ns3
