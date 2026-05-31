/*
 * Copyright (C) 2001-2026 Sergio Andreozzi
 * Copyright (C) 2000 Nortel Networks
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Ported from DiffServ4NS dsred.cc class dsREDQueue (2001).
 */

#include "ds-red-queue-disc.h"

#include "diffserv-dscp-tag.h"
#include "ds-rr-scheduler.h"

#include "ns3/ipv4-queue-disc-item.h"
#include "ns3/log.h"
#include "ns3/object-factory.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"

#include <cstdio>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DsRedQueueDisc");

namespace diffserv
{

NS_OBJECT_ENSURE_REGISTERED(DsRedQueueDisc);

// --- Stats ---

DsRedQueueDisc::Stats::Stats()
    : drops(0),
      edrops(0),
      pkts(0)
{
    for (int i = 0; i < kMaxCodePoints; ++i)
    {
        drops_cp[i] = 0;
        edrops_cp[i] = 0;
        pkts_cp[i] = 0;
    }
}

// --- DsRedQueueDisc ---

TypeId
DsRedQueueDisc::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::diffserv::DsRedQueueDisc")
            .SetParent<QueueDisc>()
            .SetGroupName("DiffServ")
            .AddConstructor<DsRedQueueDisc>()
            .AddAttribute(
                "NumQueues",
                "Number of physical queues [1, kMaxQueues]. "
                "Reachable via Config::Set path (ns-3 App Store "
                "configurability).",
                UintegerValue(1),
                MakeUintegerAccessor(&DsRedQueueDisc::SetNumQueues, &DsRedQueueDisc::GetNumQueues),
                MakeUintegerChecker<uint32_t>(1, kMaxQueues))
            .AddTraceSource("DsEnqueue",
                            "Fires after a packet is successfully enqueued, "
                            "with the DSCP code point used for PHB lookup.",
                            MakeTraceSourceAccessor(&DsRedQueueDisc::m_dsEnqueueTrace),
                            "ns3::diffserv::DsRedQueueDisc::EnqueueTracedCallback")
            .AddTraceSource("DsDequeue",
                            "Fires after a packet is dequeued, with the DSCP "
                            "code point and physical queue index.",
                            MakeTraceSourceAccessor(&DsRedQueueDisc::m_dsDequeueTrace),
                            "ns3::diffserv::DsRedQueueDisc::DequeueTracedCallback")
            .AddTraceSource("DsDrop",
                            "Fires when a packet is dropped (RED early drop "
                            "or tail drop), with DSCP and drop reason.",
                            MakeTraceSourceAccessor(&DsRedQueueDisc::m_dsDropTrace),
                            "ns3::diffserv::DsRedQueueDisc::DropTracedCallback");
    return tid;
}

DsRedQueueDisc::DsRedQueueDisc()
    : QueueDisc(QueueDiscSizePolicy::NO_LIMITS),
      m_scheduler(nullptr),
      m_numQueues(1),
      m_ecn(false)
{
    NS_LOG_FUNCTION(this);
    for (uint32_t i = 0; i < kMaxQueues; ++i)
    {
        m_perQueueLimit[i] = 0;
    }
}

DsRedQueueDisc::~DsRedQueueDisc()
{
    NS_LOG_FUNCTION(this);
}

void
DsRedQueueDisc::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_scheduler = nullptr;
    QueueDisc::DoDispose();
}

// --- PHB Table ---

void
DsRedQueueDisc::AddPhbEntry(uint8_t codePt, uint8_t queue, uint8_t prec)
{
    m_phb.AddEntry(codePt, queue, prec);
}

bool
DsRedQueueDisc::LookupPhb(uint8_t codePt, uint8_t& queue, uint8_t& prec) const
{
    return m_phb.Lookup(codePt, queue, prec);
}

// --- Scheduler ---

void
DsRedQueueDisc::SetScheduler(Ptr<DsScheduler> scheduler)
{
    NS_LOG_FUNCTION(this << scheduler);
    m_scheduler = scheduler;
}

Ptr<DsScheduler>
DsRedQueueDisc::GetScheduler() const
{
    return m_scheduler;
}

// --- Configuration ---

void
DsRedQueueDisc::SetNumQueues(uint32_t numQueues)
{
    NS_LOG_FUNCTION(this << numQueues);
    m_numQueues = numQueues;
}

uint32_t
DsRedQueueDisc::GetNumQueues() const
{
    return m_numQueues;
}

void
DsRedQueueDisc::ConfigQueue(uint32_t queue, uint32_t prec, double thMin, double thMax, double maxP)
{
    NS_LOG_FUNCTION(this << queue << prec << thMin << thMax << maxP);

    if (GetNQueueDiscClasses() > queue)
    {
        GetSubQueue(queue)->ConfigureVirtualQueue(prec, thMin, thMax, maxP);
    }
    else
    {
        NS_LOG_WARN("ConfigQueue called before children created; "
                    "queue "
                    << queue << " does not exist yet.");
    }
}

void
DsRedQueueDisc::SetMredMode(MredMode mode, uint32_t queue)
{
    NS_LOG_FUNCTION(this << static_cast<uint32_t>(mode) << queue);

    if (GetNQueueDiscClasses() == 0)
    {
        NS_LOG_WARN("SetMredMode called before children created.");
        return;
    }

    if (queue >= kMaxQueues)
    {
        // Apply to all queues
        for (uint32_t i = 0; i < GetNQueueDiscClasses(); ++i)
        {
            GetSubQueue(i)->SetMredMode(mode);
        }
    }
    else
    {
        GetSubQueue(queue)->SetMredMode(mode);
    }
}

void
DsRedQueueDisc::SetNumPrec(uint32_t queue, uint32_t numPrec)
{
    NS_LOG_FUNCTION(this << queue << numPrec);

    if (GetNQueueDiscClasses() > queue)
    {
        GetSubQueue(queue)->SetNumPrec(numPrec);
    }
    else
    {
        NS_LOG_WARN("SetNumPrec called before children created.");
    }
}

void
DsRedQueueDisc::SetQueueLimit(uint32_t queue, uint32_t limit)
{
    NS_LOG_FUNCTION(this << queue << limit);

    m_perQueueLimit[queue] = limit;
    if (GetNQueueDiscClasses() > queue)
    {
        GetSubQueue(queue)->SetQueueLimit(limit);
    }
}

void
DsRedQueueDisc::SetMeanPacketSize(int mps)
{
    NS_LOG_FUNCTION(this << mps);

    for (uint32_t i = 0; i < GetNQueueDiscClasses(); ++i)
    {
        GetSubQueue(i)->SetMeanPacketSize(mps);
    }
}

void
DsRedQueueDisc::SetQueueBandwidth(uint32_t queue, double bandwidthBps)
{
    NS_LOG_FUNCTION(this << queue << bandwidthBps);

    if (GetNQueueDiscClasses() > queue)
    {
        GetSubQueue(queue)->SetPtc(bandwidthBps);
    }
}

int
DsRedQueueDisc::GetVirtualQueueLen(uint32_t queue, uint32_t prec) const
{
    if (GetNQueueDiscClasses() > queue)
    {
        return GetSubQueue(queue)->GetVirtualQueueLen(prec);
    }
    return 0;
}

// --- Statistics ---

void
DsRedQueueDisc::PrintStats() const
{
    std::printf("\nPackets Statistics\n");
    std::printf("=======================================\n");
    std::printf(" CP  TotPkts   TxPkts   ldrops   edrops\n");
    std::printf(" --  -------   ------   ------   ------\n");

    for (int i = 0; i < kMaxCodePoints; ++i)
    {
        if (m_stats.pkts_cp[i] != 0)
        {
            double pktsCp = m_stats.pkts_cp[i];
            double txPct = (pktsCp - m_stats.drops_cp[i] - m_stats.edrops_cp[i]) * 100.0 / pktsCp;
            double dropPct = m_stats.drops_cp[i] * 100.0 / pktsCp;
            double edropPct = m_stats.edrops_cp[i] * 100.0 / pktsCp;
            std::printf("%3d %8d  %6.2f%%  %6.2f%%   %6.2f%%\n",
                        i,
                        m_stats.pkts_cp[i],
                        txPct,
                        dropPct,
                        edropPct);
        }
    }

    std::printf("----------------------------------------\n");
    if (m_stats.pkts != 0)
    {
        double pkts = m_stats.pkts;
        double txPct = (pkts - m_stats.drops - m_stats.edrops) * 100.0 / pkts;
        double dropPct = m_stats.drops * 100.0 / pkts;
        double edropPct = m_stats.edrops * 100.0 / pkts;
        std::printf("All %8d  %6.2f%%  %6.2f%%   %6.2f%%\n",
                    m_stats.pkts,
                    txPct,
                    dropPct,
                    edropPct);
    }
}

void
DsRedQueueDisc::PrintPhbTable() const
{
    m_phb.Print();
}

// --- Helpers ---

uint8_t
DsRedQueueDisc::GetCodePoint(Ptr<const QueueDiscItem> item) const
{
    Ptr<const Ipv4QueueDiscItem> ipItem = DynamicCast<const Ipv4QueueDiscItem>(item);
    if (!ipItem)
    {
        NS_LOG_WARN("Non-IPv4 packet; returning code point 0.");
        return 0;
    }
    return static_cast<uint8_t>(ipItem->GetHeader().GetDscp());
}

Ptr<DsRedSubQueue>
DsRedQueueDisc::GetSubQueue(uint32_t index) const
{
    return DynamicCast<DsRedSubQueue>(GetQueueDiscClass(index)->GetQueueDisc());
}

// --- QueueDisc overrides ---

bool
DsRedQueueDisc::CheckConfig()
{
    NS_LOG_FUNCTION(this);

    if (GetNInternalQueues() > 0)
    {
        NS_LOG_ERROR("DsRedQueueDisc cannot have internal queues");
        return false;
    }

    // Create child sub-queues if none configured externally
    if (GetNQueueDiscClasses() == 0)
    {
        for (uint32_t i = 0; i < m_numQueues; ++i)
        {
            Ptr<DsRedSubQueue> subQ = CreateObject<DsRedSubQueue>();
            uint32_t limit = (m_perQueueLimit[i] == 0) ? 50 : m_perQueueLimit[i];
            subQ->SetQueueLimit(limit);
            subQ->Initialize();

            Ptr<QueueDiscClass> cls = CreateObject<QueueDiscClass>();
            cls->SetQueueDisc(subQ);
            AddQueueDiscClass(cls);
        }
    }

    // Create default scheduler if none set
    if (!m_scheduler)
    {
        m_scheduler = CreateObjectWithAttributes<DsRoundRobinScheduler>("NumQueues",
                                                                        UintegerValue(m_numQueues));
    }

    if (GetNQueueDiscClasses() < m_numQueues)
    {
        NS_LOG_ERROR("DsRedQueueDisc needs at least " << m_numQueues << " queue disc classes");
        return false;
    }

    return true;
}

void
DsRedQueueDisc::InitializeParams()
{
    NS_LOG_FUNCTION(this);
    // InitRedStateVars is called by each sub-queue's own InitializeParams
}

bool
DsRedQueueDisc::EnqueueWithCodePoint(Ptr<QueueDiscItem> item, uint8_t codePt)
{
    NS_LOG_FUNCTION(this << item << static_cast<uint32_t>(codePt));

    uint8_t queue;
    uint8_t prec;

    if (!LookupPhb(codePt, queue, prec))
    {
        NS_LOG_DEBUG("No PHB match for code point " << static_cast<uint32_t>(codePt));
        DropBeforeEnqueue(item, "NO_PHB_MATCH");
        return false;
    }

    Ptr<DsRedSubQueue> subQ = GetSubQueue(queue);
    PktResult result = subQ->EnqueueWithPrec(item, prec, m_ecn);

    switch (result)
    {
    case PktResult::PKT_ENQUEUED:
    case PktResult::PKT_MARKED: {
        uint32_t schedSize = item->GetSize();
        m_scheduler->OnEnqueueWithTime(queue, schedSize, Simulator::Now().GetSeconds());
        m_stats.pkts_cp[codePt]++;
        m_stats.pkts++;
        m_dsEnqueueTrace(item, codePt);
        return true;
    }
    case PktResult::PKT_DROPPED:
        m_stats.drops_cp[codePt]++;
        m_stats.drops++;
        m_stats.pkts_cp[codePt]++;
        m_stats.pkts++;
        m_dsDropTrace(item, codePt, 1); // 1 = tail drop
        return false;
    case PktResult::PKT_EDROPPED:
        m_stats.edrops_cp[codePt]++;
        m_stats.edrops++;
        m_stats.pkts_cp[codePt]++;
        m_stats.pkts++;
        m_dsDropTrace(item, codePt, 0); // 0 = RED early drop
        return false;
    default:
        return false;
    }
}

bool
DsRedQueueDisc::DoEnqueue(Ptr<QueueDiscItem> item)
{
    NS_LOG_FUNCTION(this << item);
    // Prefer a classifier-attached DiffServDscpTag over the IPv4
    // header DSCP when present — symmetric with DoDequeue's
    // tag-first lookup. The edge/core composer stamps the final
    // policed DSCP as a tag and delegates to this inner via the
    // public Enqueue path.
    uint8_t codePt;
    DiffServDscpTag dscpTag;
    if (item->GetPacket()->PeekPacketTag(dscpTag))
    {
        codePt = dscpTag.GetDscp();
    }
    else
    {
        codePt = GetCodePoint(item);
    }
    return EnqueueWithCodePoint(item, codePt);
}

Ptr<QueueDiscItem>
DsRedQueueDisc::DoDequeue()
{
    NS_LOG_FUNCTION(this);

    int q = m_scheduler->SelectNextQueue();
    if (q < 0)
    {
        NS_LOG_LOGIC("All queues empty");
        return nullptr;
    }

    Ptr<QueueDiscItem> item = GetSubQueue(static_cast<uint32_t>(q))->Dequeue();
    if (!item)
    {
        NS_LOG_LOGIC("Selected queue " << q << " was empty");
        return nullptr;
    }

    // Read DSCP from tag if present, else from IPv4 header
    uint8_t codePt;
    DiffServDscpTag dscpTag;
    if (item->GetPacket()->PeekPacketTag(dscpTag))
    {
        codePt = dscpTag.GetDscp();
    }
    else
    {
        codePt = GetCodePoint(item);
    }

    // Look up PHB for the dequeued packet to find its precedence
    uint8_t phbQueue;
    uint8_t prec;
    LookupPhb(codePt, phbQueue, prec);

    double now = Simulator::Now().GetSeconds();
    GetSubQueue(static_cast<uint32_t>(q))->UpdateRedStateVar(prec, now);
    m_scheduler->UpdateDepartureRate(static_cast<uint32_t>(q), prec, item->GetSize(), now);

    m_dsDequeueTrace(item, codePt, static_cast<uint32_t>(q));

    return item;
}

Ptr<const QueueDiscItem>
DsRedQueueDisc::DoPeek()
{
    NS_LOG_FUNCTION(this);

    for (uint32_t i = 0; i < GetNQueueDiscClasses(); ++i)
    {
        Ptr<const QueueDiscItem> item = GetQueueDiscClass(i)->GetQueueDisc()->Peek();
        if (item)
        {
            return item;
        }
    }
    NS_LOG_LOGIC("Queue empty");
    return nullptr;
}

} // namespace diffserv
} // namespace ns3
