/*
 * Copyright (C) 2026 Sergio Andreozzi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * L4S queue disc — composition over QueueDisc with a pluggable
 * inner classic AQM.
 */

#include "ds-l4s-queue-disc.h"

#include "ds-l4s-timestamp-tag.h"
#include "ds-red-sub-queue.h"
#include "ds-rr-scheduler.h"

#include "ns3/double.h"
#include "ns3/enum.h"
#include "ns3/fifo-queue-disc.h"
#include "ns3/fq-codel-queue-disc.h"
#include "ns3/ipv4-header.h"
#include "ns3/ipv4-queue-disc-item.h"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/queue-disc.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"

#include <algorithm>
#include <cmath>

namespace ns3
{
namespace diffserv
{

NS_LOG_COMPONENT_DEFINE("DsL4sQueueDisc");

NS_OBJECT_ENSURE_REGISTERED(DsL4sQueueDisc);

TypeId
DsL4sQueueDisc::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::diffserv::DsL4sQueueDisc")
            .SetParent<QueueDisc>()
            .SetGroupName("DiffServ")
            .AddConstructor<DsL4sQueueDisc>()
            .AddAttribute("L4sQueueIdx",
                          "Deprecated legacy slot index for the L4S lane. The L4S "
                          "queue is always composer child 0; this attribute is "
                          "round-tripped for backward compatibility only.",
                          UintegerValue(1),
                          MakeUintegerAccessor(&DsL4sQueueDisc::m_l4sQueueIdxLegacy),
                          MakeUintegerChecker<uint32_t>(0, kMaxQueues - 1))
            .AddAttribute("L4sTargetSojournMs",
                          "L4S target sojourn time in milliseconds. RFC 9332 default 1.0 "
                          "ms.",
                          DoubleValue(1.0),
                          MakeDoubleAccessor(&DsL4sQueueDisc::SetL4sTargetSojournMs,
                                             &DsL4sQueueDisc::GetL4sTargetSojournMs),
                          MakeDoubleChecker<double>(0.0))
            .AddAttribute("ClassicTargetSojournMs",
                          "Classic-queue target sojourn time in milliseconds; drives the "
                          "P.I.² controller's integrator. RFC 9332 §A.2 default 15.0 ms.",
                          DoubleValue(15.0),
                          MakeDoubleAccessor(&DsL4sQueueDisc::SetClassicTargetSojournMs,
                                             &DsL4sQueueDisc::GetClassicTargetSojournMs),
                          MakeDoubleChecker<double>(0.0))
            .AddAttribute("CouplingFactor",
                          "Coupling factor k. Classic drop probability is (k * p')^2; "
                          "RFC 9332 default k = 2 (squared coupling = 4).",
                          DoubleValue(2.0),
                          MakeDoubleAccessor(&DsL4sQueueDisc::SetCouplingFactor,
                                             &DsL4sQueueDisc::GetCouplingFactor),
                          MakeDoubleChecker<double>(0.0))
            .AddAttribute("ClassicAqm",
                          "Classic-queue AQM strategy. Wred = parent WRED + coupled drop "
                          "overlay. CoupledOnly = coupled drop is sole AQM. "
                          "FqCoDel = mainline FqCoDelQueueDisc as inner classic AQM. "
                          "Construct-only: consumed by CheckConfig to pick "
                          "the default inner AQM; Config::Set after Initialize has no "
                          "effect because the inner is already materialised.",
                          TypeId::ATTR_GET | TypeId::ATTR_CONSTRUCT,
                          EnumValue(ClassicAqm::Wred),
                          MakeEnumAccessor<ClassicAqm>(&DsL4sQueueDisc::SetClassicAqm,
                                                       &DsL4sQueueDisc::GetClassicAqm),
                          MakeEnumChecker(ClassicAqm::Wred,
                                          "Wred",
                                          ClassicAqm::CoupledOnly,
                                          "CoupledOnly",
                                          ClassicAqm::FqCoDel,
                                          "FqCoDel"))
            .AddAttribute("L4sBandwidthBps",
                          "Bandwidth used as a sojourn-time fallback proxy when no "
                          "per-packet enqueue-time tracking is available. Default 1 Gbps.",
                          DoubleValue(1e9),
                          MakeDoubleAccessor(&DsL4sQueueDisc::SetL4sBandwidthBps,
                                             &DsL4sQueueDisc::GetL4sBandwidthBps),
                          MakeDoubleChecker<double>(1.0))
            .AddAttribute("ControllerInterval",
                          "Periodic P.I controller tick interval. RFC 9332 §A.2 default 16 "
                          "ms.",
                          TimeValue(MilliSeconds(16)),
                          MakeTimeAccessor(&DsL4sQueueDisc::SetControllerInterval,
                                           &DsL4sQueueDisc::GetControllerInterval),
                          MakeTimeChecker(MilliSeconds(1)))
            .AddTraceSource("L4sClassified",
                            "Fires per enqueue with a routing flag: true for "
                            "L4S sub-queue, false for classic path.",
                            MakeTraceSourceAccessor(&DsL4sQueueDisc::m_l4sClassifiedTrace),
                            "ns3::diffserv::DsL4sQueueDisc::ClassifiedTracedCallback")
            .AddTraceSource("L4sMarked",
                            "Fires when a packet is CE-marked by the L4S immediate-mark "
                            "step.",
                            MakeTraceSourceAccessor(&DsL4sQueueDisc::m_l4sMarkTrace),
                            "ns3::diffserv::DsL4sQueueDisc::L4sMarkedTracedCallback")
            .AddTraceSource("ClassicCoupledDrop",
                            "Fires when a classic-path packet is dropped by the coupled p_C.",
                            MakeTraceSourceAccessor(&DsL4sQueueDisc::m_classicCoupledDropTrace),
                            "ns3::diffserv::DsL4sQueueDisc::"
                            "ClassicCoupledDropTracedCallback");
    return tid;
}

DsL4sQueueDisc::DsL4sQueueDisc()
    : QueueDisc(QueueDiscSizePolicy::NO_LIMITS, QueueSizeUnit::PACKETS),
      m_l4sQueueIdxLegacy(1),
      m_l4sTargetSojournMs(1.0),
      m_classicTargetSojournMs(15.0),
      m_couplingFactor(2.0),
      m_classicAqmMode(ClassicAqm::Wred),
      m_l4sBandwidthBps(1e9),
      m_baseProb(0.0),
      m_controllerInterval(MilliSeconds(16)),
      m_lastSojournMs(0.0),
      m_lastSojournInitialized(false),
      m_forceBaseProbForTest(false),
      m_lastCoupledProb(0.0),
      m_lastL4sMarkProb(0.0)
{
    NS_LOG_FUNCTION(this);
    m_rng = CreateObject<UniformRandomVariable>();
}

DsL4sQueueDisc::~DsL4sQueueDisc()
{
    NS_LOG_FUNCTION(this);
}

// --- Strategy injection ---

void
DsL4sQueueDisc::EnsureDefaultChildren()
{
    if (!m_classicAqm)
    {
        // The enum mode chooses the default inner disc when the caller
        // did not inject one via SetClassicAqmDisc. FqCoDel produces a
        // mainline FqCoDelQueueDisc; Wred and CoupledOnly
        // both keep the DsRedQueueDisc default (CoupledOnly differs at
        // DoInitialize time, not at construction time).
        if (m_classicAqmMode == ClassicAqm::FqCoDel)
        {
            auto fq = CreateObject<FqCoDelQueueDisc>();
            // FqCoDel's Quantum auto-sets from the NetDevice MTU only when
            // installed on a device. As a nested inner disc there is no
            // device, so CheckConfig would fail on Quantum=0. Force the
            // Ethernet-MTU default used by l4s-routing-test and
            // diffserv-l4s-fqcodel-comparison.
            fq->SetQuantum(1500);
            m_classicAqm = fq;
        }
        else
        {
            m_classicAqm = CreateObject<DsRedQueueDisc>();
        }
    }
    if (!m_l4sQueue)
    {
        m_l4sQueue = CreateObject<FifoQueueDisc>();
    }
    // Register as QueueDiscClass children if not yet present. The two
    // slots are fixed by the kL4sChildIdx / kClassicChildIdx constants.
    if (GetNQueueDiscClasses() == 0)
    {
        Ptr<QueueDiscClass> l4sCls = CreateObject<QueueDiscClass>();
        l4sCls->SetQueueDisc(m_l4sQueue);
        AddQueueDiscClass(l4sCls);

        Ptr<QueueDiscClass> classicCls = CreateObject<QueueDiscClass>();
        classicCls->SetQueueDisc(m_classicAqm);
        AddQueueDiscClass(classicCls);
    }
}

void
DsL4sQueueDisc::SetClassicAqmDisc(Ptr<QueueDisc> aqm)
{
    NS_LOG_FUNCTION(this << aqm);
    NS_ASSERT_MSG(GetNQueueDiscClasses() == 0,
                  "SetClassicAqmDisc must be called before Initialize");
    m_classicAqm = aqm;
}

Ptr<QueueDisc>
DsL4sQueueDisc::GetClassicAqmDisc() const
{
    return m_classicAqm;
}

void
DsL4sQueueDisc::SetL4sQueueDisc(Ptr<QueueDisc> l4s)
{
    NS_LOG_FUNCTION(this << l4s);
    NS_ASSERT_MSG(GetNQueueDiscClasses() == 0, "SetL4sQueueDisc must be called before Initialize");
    m_l4sQueue = l4s;
}

Ptr<QueueDisc>
DsL4sQueueDisc::GetL4sQueueDisc() const
{
    return m_l4sQueue;
}

void
DsL4sQueueDisc::SetScheduler(Ptr<DsScheduler> scheduler)
{
    NS_LOG_FUNCTION(this << scheduler);
    m_scheduler = scheduler;
}

Ptr<DsScheduler>
DsL4sQueueDisc::GetScheduler() const
{
    return m_scheduler;
}

Ptr<DsRedQueueDisc>
DsL4sQueueDisc::GetClassicAsRed() const
{
    Ptr<DsRedQueueDisc> red = DynamicCast<DsRedQueueDisc>(m_classicAqm);
    NS_ASSERT_MSG(red,
                  "Classic AQM is not a DsRedQueueDisc; this accessor "
                  "is only valid when the inner classic AQM is Red");
    return red;
}

// --- PHB forwarders ---

void
DsL4sQueueDisc::AddPhbEntry(uint8_t codePt, uint8_t queue, uint8_t prec)
{
    NS_LOG_FUNCTION(this << static_cast<uint32_t>(codePt));
    if (!m_classicAqm)
    {
        EnsureDefaultChildren();
    }
    m_classicUserConfigured = true;
    GetClassicAsRed()->AddPhbEntry(codePt, queue, prec);
}

bool
DsL4sQueueDisc::LookupPhb(uint8_t codePt, uint8_t& queue, uint8_t& prec) const
{
    return GetClassicAsRed()->LookupPhb(codePt, queue, prec);
}

// --- Red-specific forwarders ---

void
DsL4sQueueDisc::SetNumQueues(uint32_t numQueues)
{
    if (!m_classicAqm)
    {
        EnsureDefaultChildren();
    }
    GetClassicAsRed()->SetNumQueues(numQueues);
}

uint32_t
DsL4sQueueDisc::GetNumQueues() const
{
    return GetClassicAsRed()->GetNumQueues();
}

void
DsL4sQueueDisc::PrintStats() const
{
    // QueueStatsProvider override. Simple summary: forward to the
    // classic-AQM side's stats if Red; L4S-side occupancy is already
    // observable via QueueLen sampling.
    Ptr<DsRedQueueDisc> red = DynamicCast<DsRedQueueDisc>(m_classicAqm);
    if (red)
    {
        red->PrintStats();
    }
}

void
DsL4sQueueDisc::ConfigQueue(uint32_t q, uint32_t prec, double thMin, double thMax, double maxP)
{
    m_classicUserConfigured = true;
    GetClassicAsRed()->ConfigQueue(q, prec, thMin, thMax, maxP);
}

void
DsL4sQueueDisc::SetMredMode(MredMode mode, uint32_t q)
{
    m_classicUserConfigured = true;
    GetClassicAsRed()->SetMredMode(mode, q);
}

void
DsL4sQueueDisc::SetNumPrec(uint32_t q, uint32_t n)
{
    GetClassicAsRed()->SetNumPrec(q, n);
}

void
DsL4sQueueDisc::SetQueueLimit(uint32_t q, uint32_t n)
{
    if (q == m_l4sQueueIdxLegacy)
    {
        // L4S slot: set the FIFO child's MaxSize. FifoQueueDisc's default
        // is 1000 packets, which is too small for the coupling-invariant
        // tests that enqueue 4000+ ECT(1) packets.
        if (!m_l4sQueue)
        {
            EnsureDefaultChildren();
        }
        m_l4sQueue->SetAttribute("MaxSize", QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, n)));
        return;
    }
    m_classicUserConfigured = true;
    GetClassicAsRed()->SetQueueLimit(q, n);
}

void
DsL4sQueueDisc::SetMeanPacketSize(int mps)
{
    GetClassicAsRed()->SetMeanPacketSize(mps);
}

void
DsL4sQueueDisc::SetQueueBandwidth(uint32_t q, double bps)
{
    GetClassicAsRed()->SetQueueBandwidth(q, bps);
}

int
DsL4sQueueDisc::GetVirtualQueueLen(uint32_t q, uint32_t prec) const
{
    if (q == m_l4sQueueIdxLegacy)
    {
        // L4S lane — FifoQueueDisc has no precedence, report current size.
        return m_l4sQueue ? static_cast<int>(m_l4sQueue->GetCurrentSize().GetValue()) : 0;
    }
    return GetClassicAsRed()->GetVirtualQueueLen(q, prec);
}

// --- Attribute setters/getters ---

void
DsL4sQueueDisc::SetL4sQueueIdx(uint32_t idx)
{
    NS_LOG_FUNCTION(this << idx << " [deprecated: composer uses fixed child indices]");
    m_l4sQueueIdxLegacy = idx;
}

uint32_t
DsL4sQueueDisc::GetL4sQueueIdx() const
{
    return m_l4sQueueIdxLegacy;
}

void
DsL4sQueueDisc::SetL4sTargetSojournMs(double ms)
{
    m_l4sTargetSojournMs = ms;
}

double
DsL4sQueueDisc::GetL4sTargetSojournMs() const
{
    return m_l4sTargetSojournMs;
}

void
DsL4sQueueDisc::SetClassicTargetSojournMs(double ms)
{
    m_classicTargetSojournMs = ms;
}

double
DsL4sQueueDisc::GetClassicTargetSojournMs() const
{
    return m_classicTargetSojournMs;
}

void
DsL4sQueueDisc::SetCouplingFactor(double k)
{
    m_couplingFactor = k;
}

double
DsL4sQueueDisc::GetCouplingFactor() const
{
    return m_couplingFactor;
}

void
DsL4sQueueDisc::SetClassicAqm(ClassicAqm m)
{
    m_classicAqmMode = m;
}

DsL4sQueueDisc::ClassicAqm
DsL4sQueueDisc::GetClassicAqm() const
{
    return m_classicAqmMode;
}

void
DsL4sQueueDisc::SetL4sBandwidthBps(double bps)
{
    m_l4sBandwidthBps = bps;
}

double
DsL4sQueueDisc::GetL4sBandwidthBps() const
{
    return m_l4sBandwidthBps;
}

void
DsL4sQueueDisc::SetControllerInterval(Time interval)
{
    m_controllerInterval = interval;
}

Time
DsL4sQueueDisc::GetControllerInterval() const
{
    return m_controllerInterval;
}

double
DsL4sQueueDisc::GetBaseProb() const
{
    return m_baseProb;
}

double
DsL4sQueueDisc::GetLastClassicCoupledProb() const
{
    return m_lastCoupledProb;
}

double
DsL4sQueueDisc::GetLastL4sMarkProb() const
{
    return m_lastL4sMarkProb;
}

void
DsL4sQueueDisc::ForceBaseProbForTest(double p)
{
    m_baseProb = std::clamp(p, 0.0, 1.0);
    m_forceBaseProbForTest = true;
}

void
DsL4sQueueDisc::ClearForcedBaseProbForTest()
{
    m_forceBaseProbForTest = false;
}

int64_t
DsL4sQueueDisc::AssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(this << stream);
    // Walk children in fixed order so RNG-stream assignment is stable.
    // Order matters for byte-for-byte S1/S2 equivalence; do not
    // re-order without a matching re-baseline.
    m_rng->SetStream(stream);
    int64_t next = stream + 1;

    // m_l4sQueue (FifoQueueDisc) has no RNG: 0 streams consumed.

    // m_classicAqm as DsRedQueueDisc: walk inner DsRedSubQueue children.
    Ptr<DsRedQueueDisc> red = DynamicCast<DsRedQueueDisc>(m_classicAqm);
    if (red)
    {
        for (uint32_t i = 0; i < red->GetNQueueDiscClasses(); ++i)
        {
            Ptr<DsRedSubQueue> sub =
                DynamicCast<DsRedSubQueue>(red->GetQueueDiscClass(i)->GetQueueDisc());
            if (sub)
            {
                sub->AssignStreams(next++);
            }
        }
    }
    return next - stream;
}

// --- Classification + enqueue ---

bool
DsL4sQueueDisc::IsL4sPacket(Ptr<const QueueDiscItem> item) const
{
    Ptr<const Ipv4QueueDiscItem> ipItem = DynamicCast<const Ipv4QueueDiscItem>(item);
    if (!ipItem)
    {
        return false;
    }
    Ipv4Header::EcnType ecn = ipItem->GetHeader().GetEcn();
    return ecn == Ipv4Header::ECN_ECT1 || ecn == Ipv4Header::ECN_CE;
}

double
DsL4sQueueDisc::ComputeL4sSojournMs() const
{
    if (!m_l4sQueue)
    {
        return 0.0;
    }

    // Head packet's enqueue timestamp — true sojourn.
    Ptr<const QueueDiscItem> head = m_l4sQueue->Peek();
    if (head)
    {
        DsL4sTimestampTag tag;
        if (head->GetPacket()->PeekPacketTag(tag))
        {
            Time sojourn = Simulator::Now() - tag.GetTimestamp();
            return sojourn.GetSeconds() * 1e3;
        }
    }

    // Bandwidth proxy fallback.
    uint32_t qlen = m_l4sQueue->GetCurrentSize().GetValue();
    if (qlen == 0 || m_l4sBandwidthBps <= 0.0)
    {
        return 0.0;
    }
    constexpr double kMtuBytes = 1500.0;
    double sojournSec = static_cast<double>(qlen) * 8.0 * kMtuBytes / m_l4sBandwidthBps;
    return sojournSec * 1e3;
}

double
DsL4sQueueDisc::ComputeClassicSojournMs() const
{
    if (!m_classicAqm)
    {
        return 0.0;
    }

    // Head packet's enqueue timestamp — true sojourn. Classic packets are
    // tagged with the same DsL4sTimestampTag in DoEnqueue so the same
    // measurement path applies on both sub-queues.
    Ptr<const QueueDiscItem> head = m_classicAqm->Peek();
    if (head)
    {
        DsL4sTimestampTag tag;
        if (head->GetPacket()->PeekPacketTag(tag))
        {
            Time sojourn = Simulator::Now() - tag.GetTimestamp();
            return sojourn.GetSeconds() * 1e3;
        }
    }

    // Bandwidth proxy fallback (same bottleneck rate as the L4S lane).
    // GetNPackets() (not GetCurrentSize) so we don't require the inner AQM
    // to advertise a MaxSize policy — under ClassicAqm::CoupledOnly the
    // classic AQM may be NO_LIMITS by design.
    uint32_t qlen = m_classicAqm->GetNPackets();
    if (qlen == 0 || m_l4sBandwidthBps <= 0.0)
    {
        return 0.0;
    }
    constexpr double kMtuBytes = 1500.0;
    double sojournSec = static_cast<double>(qlen) * 8.0 * kMtuBytes / m_l4sBandwidthBps;
    return sojournSec * 1e3;
}

void
DsL4sQueueDisc::UpdateBaseProb()
{
    if (m_forceBaseProbForTest)
    {
        return;
    }

    constexpr double kAlphaHz = 0.3125;
    constexpr double kBetaHz = 3.125;

    // RFC 9332 §A.2: the P.I.² controller integrates the *classic* queue's
    // sojourn against the classic target. The L4S queue's sojourn drives a
    // separate step-AQM mechanism (immediate marking at L4sTargetSojournMs),
    // not the PI integrator.
    double sojournMs = ComputeClassicSojournMs();
    double sojournSec = sojournMs * 1e-3;
    double targetSec = m_classicTargetSojournMs * 1e-3;
    double prevSojournSec = m_lastSojournMs * 1e-3;

    double error = sojournSec - targetSec;
    double derivative = m_lastSojournInitialized ? (sojournSec - prevSojournSec) : 0.0;

    m_baseProb += kAlphaHz * error + kBetaHz * derivative;
    m_baseProb = std::clamp(m_baseProb, 0.0, 1.0);

    m_lastSojournMs = sojournMs;
    m_lastSojournInitialized = true;
}

void
DsL4sQueueDisc::ControllerTick()
{
    NS_LOG_FUNCTION(this);
    UpdateBaseProb();
    m_controllerEvent =
        Simulator::Schedule(m_controllerInterval, &DsL4sQueueDisc::ControllerTick, this);
}

double
DsL4sQueueDisc::ComputeCoupledDropProb() const
{
    double v = m_couplingFactor * m_baseProb;
    return std::clamp(v * v, 0.0, 1.0);
}

bool
DsL4sQueueDisc::ApplyL4sCoupledMark(Ptr<QueueDiscItem> item)
{
    double sojournMs = ComputeL4sSojournMs();
    double pL = (sojournMs >= m_l4sTargetSojournMs) ? 1.0 : std::clamp(2.0 * m_baseProb, 0.0, 1.0);
    m_lastL4sMarkProb = pL;

    bool drewMark = (pL > 0.0) && (m_rng->GetValue(0.0, 1.0) < pL);
    if (!drewMark)
    {
        return false;
    }

    // RFC 9331 §5 CE idempotence guard.
    Ptr<const Ipv4QueueDiscItem> ipItem = DynamicCast<const Ipv4QueueDiscItem>(item);
    if (ipItem && ipItem->GetHeader().GetEcn() == Ipv4Header::ECN_CE)
    {
        return true;
    }

    bool marked = Mark(item, "L4S_IMMEDIATE_MARK");
    if (marked)
    {
        m_l4sMarkTrace(item);
    }
    return true;
}

bool
DsL4sQueueDisc::MaybeCoupledDrop(Ptr<QueueDiscItem> item)
{
    double pC = ComputeCoupledDropProb();
    m_lastCoupledProb = pC;
    if (pC <= 0.0)
    {
        return false;
    }
    if (m_rng->GetValue(0.0, 1.0) < pC)
    {
        m_classicCoupledDropTrace(item, pC);
        // Composer-originated drop (before delegation to either child):
        // the manual DropBeforeEnqueue is correct here because no child
        // disc has been touched yet, so no ChildQueueDiscDropFunctor is
        // in play.
        DropBeforeEnqueue(item, "L4S_COUPLED_DROP");
        return true;
    }
    return false;
}

bool
DsL4sQueueDisc::DoEnqueue(Ptr<QueueDiscItem> item)
{
    NS_LOG_FUNCTION(this << item);

    if (IsL4sPacket(item))
    {
        m_l4sClassifiedTrace(item, true);
        return EnqueueL4s(item);
    }

    m_l4sClassifiedTrace(item, false);

    if (MaybeCoupledDrop(item))
    {
        return false;
    }

    // Timestamp tag for true classic-queue sojourn measurement at the head.
    // Mirrors the L4S-lane tagging in EnqueueL4s(); ComputeClassicSojournMs()
    // reads this tag from the classic sub-queue's head packet.
    DsL4sTimestampTag tag(Simulator::Now());
    item->GetPacket()->ReplacePacketTag(tag);

    bool ok = m_classicAqm->Enqueue(item);
    if (ok && m_scheduler)
    {
        const uint32_t classicSlot = (m_l4sQueueIdxLegacy == 0) ? 1 : 0;
        m_scheduler->OnEnqueueWithTime(classicSlot, item->GetSize(), Simulator::Now().GetSeconds());
    }
    return ok;
}

bool
DsL4sQueueDisc::EnqueueL4s(Ptr<QueueDiscItem> item)
{
    NS_LOG_FUNCTION(this << item);

    ApplyL4sCoupledMark(item);

    // Timestamp tag for true sojourn measurement at the head.
    DsL4sTimestampTag tag(Simulator::Now());
    item->GetPacket()->ReplacePacketTag(tag);

    bool ok = m_l4sQueue->Enqueue(item);
    if (ok && m_scheduler)
    {
        m_scheduler->OnEnqueueWithTime(m_l4sQueueIdxLegacy,
                                       item->GetSize(),
                                       Simulator::Now().GetSeconds());
    }
    return ok;
}

Ptr<QueueDiscItem>
DsL4sQueueDisc::DoDequeue()
{
    if (!m_scheduler)
    {
        // Fallback dispatcher: L4S-first, classic-fallback. Preserves
        // the priority intent when no scheduler is configured.
        if (m_l4sQueue)
        {
            Ptr<QueueDiscItem> item = m_l4sQueue->Dequeue();
            if (item)
            {
                return item;
            }
        }
        if (m_classicAqm)
        {
            return m_classicAqm->Dequeue();
        }
        return nullptr;
    }

    int32_t idx = m_scheduler->SelectNextQueue();
    if (idx < 0)
    {
        return nullptr;
    }
    if (static_cast<uint32_t>(idx) == m_l4sQueueIdxLegacy)
    {
        return m_l4sQueue->Dequeue();
    }
    return m_classicAqm->Dequeue();
}

Ptr<const QueueDiscItem>
DsL4sQueueDisc::DoPeek()
{
    // Match DoDequeue priority order when no scheduler is configured.
    if (m_l4sQueue)
    {
        Ptr<const QueueDiscItem> item = m_l4sQueue->Peek();
        if (item)
        {
            return item;
        }
    }
    if (m_classicAqm)
    {
        return m_classicAqm->Peek();
    }
    return nullptr;
}

bool
DsL4sQueueDisc::CheckConfig()
{
    NS_LOG_FUNCTION(this);

    if (GetNInternalQueues() > 0)
    {
        NS_LOG_ERROR("DsL4sQueueDisc must not have internal queues");
        return false;
    }

    EnsureDefaultChildren();

    // Briscoe draft-briscoe-tsvwg-l4s-diffserv-02 §6 prescribes the DualQ
    // as an "indivisible atomic component". The fixed 2-child shape (L4S
    // + classic) is the structural expression of that prescription; any
    // deviation breaks the atomicity guarantee documented in the class
    // header.
    NS_ASSERT_MSG(GetNQueueDiscClasses() == 2,
                  "DualQ atomicity violated: DsL4sQueueDisc requires exactly "
                  "2 QueueDiscClass children (L4S at idx 0, classic at idx 1) "
                  "per Briscoe draft-briscoe-tsvwg-l4s-diffserv-02 §6. "
                  "Got: "
                      << GetNQueueDiscClasses());
    if (GetNQueueDiscClasses() != 2)
    {
        NS_LOG_ERROR("DsL4sQueueDisc expects exactly 2 children "
                     "(0=L4S, 1=classic); got "
                     << GetNQueueDiscClasses());
        return false;
    }

    // Any Ptr<QueueDisc> is accepted as the inner classic AQM.
    // Type-specific forwarders (PHB, WRED config, etc.) assert on
    // their own when called against a foreign inner.
    return true;
}

void
DsL4sQueueDisc::InitializeParams()
{
    NS_LOG_FUNCTION(this);
    // Composer-level param init runs before child initialization (see
    // QueueDisc::DoInitialize order). The CoupledOnly munging and the
    // controller arming both need children to already exist, so they
    // live in DoInitialize instead.
}

void
DsL4sQueueDisc::DoInitialize()
{
    NS_LOG_FUNCTION(this);

    // Let the base orchestrate CheckConfig -> InitializeParams -> child
    // Initialize() cascades. After this returns, m_classicAqm's inner
    // DsRedSubQueue children have been auto-created and initialized.
    QueueDisc::DoInitialize();

    if (m_classicAqmMode == ClassicAqm::CoupledOnly)
    {
        // CoupledOnly only makes sense against the Red pipeline it was
        // designed for. With a foreign inner AQM (FqCoDel, PIE, ...)
        // there is no WRED early-drop to suppress, so the mode is a
        // silent no-op — the coupled p_C drop in MaybeCoupledDrop still
        // fires inner-agnostically.
        Ptr<DsRedQueueDisc> red = DynamicCast<DsRedQueueDisc>(m_classicAqm);
        if (red)
        {
            for (uint32_t q = 0; q < red->GetNQueueDiscClasses(); ++q)
            {
                Ptr<DsRedSubQueue> subQ =
                    DynamicCast<DsRedSubQueue>(red->GetQueueDiscClass(q)->GetQueueDisc());
                if (!subQ)
                {
                    continue;
                }
                subQ->SetMredMode(MredMode::DROP_TAIL);
                uint32_t qlim = subQ->GetQueueLimit();
                auto passThrough = static_cast<double>(qlim + 1);
                for (uint32_t prec = 0; prec < subQ->GetNumPrec(); ++prec)
                {
                    subQ->ConfigureVirtualQueue(prec, passThrough, passThrough, 0.0);
                }
            }
            NS_LOG_INFO("CoupledOnly: inner classic sub-queues configured as "
                        "pass-through FIFO");
        }
        else
        {
            NS_LOG_INFO("CoupledOnly + non-Red inner AQM: no pass-through munging applies");
        }
    }
    else if (m_classicAqmMode == ClassicAqm::Wred && !m_classicUserConfigured)
    {
        // Wred mode + no user config: inject sane defaults so a fresh
        // `Wred` enum picker gets a functional classic queue rather than
        // the trap-chain default (RIO_C with thMin=thMax=0, empty PHB,
        // qlim=0 → near-100% drop). When the caller has already touched
        // the classic config (SetQueueLimit / ConfigQueue / SetMredMode /
        // AddPhbEntry), this block is skipped — the user's config wins
        // and we don't stomp on it during Initialize.
        Ptr<DsRedQueueDisc> red = DynamicCast<DsRedQueueDisc>(m_classicAqm);
        if (red)
        {
            for (uint32_t q = 0; q < red->GetNQueueDiscClasses(); ++q)
            {
                Ptr<DsRedSubQueue> subQ =
                    DynamicCast<DsRedSubQueue>(red->GetQueueDiscClass(q)->GetQueueDisc());
                if (!subQ)
                {
                    continue;
                }
                subQ->SetMredMode(MredMode::WRED);
                subQ->SetQueueLimit(25);
                for (uint32_t prec = 0; prec < subQ->GetNumPrec(); ++prec)
                {
                    subQ->ConfigureVirtualQueue(prec, 5.0, 15.0, 0.1);
                }
            }
            // PHB table: BE (DSCP 0) and EF (DSCP 46) -> sub-queue 0,
            // precedence 0. Without these AddPhbEntry calls the
            // LookupPhb path drops every classified packet with
            // NO_PHB_MATCH.
            red->AddPhbEntry(0, 0, 0);
            red->AddPhbEntry(46, 0, 0);
            NS_LOG_INFO("Wred: inner classic sub-queues configured as WRED "
                        "(thMin=5, thMax=15, maxP=0.1, qlim=25)");
        }
        else
        {
            NS_LOG_INFO("Wred + non-Red inner AQM: low-band mitigation skipped");
        }
    }

    // Arm the periodic controller tick (children are ready).
    m_controllerEvent =
        Simulator::Schedule(m_controllerInterval, &DsL4sQueueDisc::ControllerTick, this);
}

void
DsL4sQueueDisc::DoDispose()
{
    NS_LOG_FUNCTION(this);
    if (m_controllerEvent.IsPending())
    {
        Simulator::Cancel(m_controllerEvent);
    }
    m_l4sQueue = nullptr;
    m_classicAqm = nullptr;
    m_scheduler = nullptr;
    QueueDisc::DoDispose();
}

} // namespace diffserv
} // namespace ns3
