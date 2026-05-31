/*
 * Copyright (C) 2026 Sergio Andreozzi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 */

#include "ds-cake-helper.h"

#include "ds-cake-stats-formatter.h"

#include "../model/ds-cake-linux-autorate-hook.h"
#include "../model/ds-cake-live-bulk-counter.h"

#include "../model/ds-rate-based-shaper-dispatcher.h"

#include "ns3/boolean.h"
#include "ns3/data-rate.h"
#include "ns3/enum.h"
#include "ns3/string.h"
#include "ns3/ds-hybrid-llq-dispatcher.h"
#include "ns3/ds-l4s-coupled-scheduler.h"
#include "ns3/ds-l4s-queue-disc.h"
#include "ns3/ds-tin-shaper-dispatcher.h"
#include "ns3/fq-cobalt-queue-disc.h"
#include "ns3/log.h"
#include "ns3/net-device.h"
#include "ns3/simulator.h"
#include "ns3/node.h"
#include "ns3/queue-disc.h"
#include "ns3/tbf-queue-disc.h"
#include "ns3/traffic-control-layer.h"
#include "ns3/uinteger.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>

namespace ns3
{
namespace diffserv
{

NS_LOG_COMPONENT_DEFINE("DsCakeHelper");

void
DsCakeHelper::SetShaperMode(DsCakeHelper::ShaperMode mode)
{
    m_shaperMode = mode;
}

DsCakeHelper::ShaperMode
DsCakeHelper::GetShaperMode() const
{
    return m_shaperMode;
}

void
DsCakeHelper::SetUseInnerTbfShaping(bool enable)
{
    if (enable)
    {
        m_shaperMode = ShaperMode::TbfInner;
    }
    else if (m_shaperMode == ShaperMode::TbfInner)
    {
        m_shaperMode = ShaperMode::TokenBucket;
    }
    // RateBased is preserved unchanged when enable=false.
}

void
DsCakeHelper::SetGlobalRateBps(uint64_t rateBps)
{
    m_globalRateBps = rateBps;
}

void
DsCakeHelper::SetTinRateBpsAll(uint64_t rateBps)
{
    m_uniformTinRateBps = rateBps;
}

void
DsCakeHelper::SetTinCount(uint32_t n)
{
    m_tinCount = n;
}

void
DsCakeHelper::SetEnableAutorateIngress(bool enable)
{
    m_enableAutorateIngress = enable;
    if (enable)
    {
        if (m_autorateImpl == AutorateImpl::Linux)
        {
            m_autorateHook = std::make_shared<DsCakeLinuxAutorateHook>();
        }
        else
        {
            m_autorateHook = std::make_shared<DsCakeNoOpAutorateHook>();
        }
    }
    else
    {
        m_autorateHook.reset();
    }
}

bool
DsCakeHelper::GetEnableAutorateIngress() const
{
    return m_enableAutorateIngress;
}

const DsCakeAutorateIngressHook*
DsCakeHelper::GetAutorateIngressHook() const
{
    return m_autorateHook.get();
}

void
DsCakeHelper::SetAutorateImpl(AutorateImpl impl)
{
    m_autorateImpl = impl;
    // If autorate is already enabled, recreate the hook with the new
    // implementation so the change takes effect before BuildDispatcher.
    if (m_enableAutorateIngress)
    {
        if (impl == AutorateImpl::Linux)
        {
            m_autorateHook = std::make_shared<DsCakeLinuxAutorateHook>();
        }
        else
        {
            m_autorateHook = std::make_shared<DsCakeNoOpAutorateHook>();
        }
    }
}

DsCakeHelper::AutorateImpl
DsCakeHelper::GetAutorateImpl() const
{
    return m_autorateImpl;
}

void
DsCakeHelper::SetEnableIngressMode(bool enabled)
{
    m_enableIngressMode = enabled;
}

bool
DsCakeHelper::GetEnableIngressMode() const
{
    return m_enableIngressMode;
}

Ptr<QueueDisc>
DsCakeHelper::BuildDispatcher()
{
    switch (m_shaperMode)
    {
    case ShaperMode::TokenBucket:
    case ShaperMode::TbfInner: {
        Ptr<DiffServEdgeQueueDisc> edge = CreateObject<DiffServEdgeQueueDisc>();
        const bool useInnerTbf = (m_shaperMode == ShaperMode::TbfInner);
        DsCakeHelper::SetAsCakeDiffserv4(edge,
                                         DataRate(m_globalRateBps),
                                         /*enableAckFilter=*/false,
                                         /*enableLlq=*/false,
                                         /*enableTinShaping=*/useInnerTbf,
                                         /*enableHostIsolation=*/false,
                                         /*useInnerTbfShaping=*/useInnerTbf);
        return edge;
    }
    case ShaperMode::RateBased: {
        Ptr<DsRateBasedShaperDispatcher> rb = CreateObject<DsRateBasedShaperDispatcher>();
        rb->ConfigureGlobal(m_globalRateBps);
        for (uint32_t slot = 0; slot < m_tinCount; ++slot)
        {
            rb->ConfigureTin(slot,
                             m_uniformTinRateBps,
                             /*overhead=*/0,
                             /*mpu=*/0,
                             RateBasedTinClock::FramingMode::NoAtm);
        }
        // Default DSCP -> slot map. For m_tinCount == 4 we replicate the
        // diffserv4 layout used by SetAsCakeDiffserv4 (Bulk / BE / Video /
        // Voice). For other tin counts we leave the table at its zero-
        // initialised default (all DSCPs -> tin 0), which suits the v1
        // bulk-TCP scenarios at DSCP=0.
        if (m_tinCount == 4)
        {
            // Tin 0 (Bulk): CS1, LE, AF11
            rb->SetDscpToSlot(8, 0);  // CS1
            rb->SetDscpToSlot(1, 0);  // LE
            rb->SetDscpToSlot(10, 0); // AF11

            // Tin 2 (Video): AF12, AF13, CS2, AF21..AF43
            for (uint8_t d : {uint8_t{12},
                              uint8_t{14},
                              uint8_t{16},
                              uint8_t{18},
                              uint8_t{20},
                              uint8_t{22},
                              uint8_t{24},
                              uint8_t{26},
                              uint8_t{28},
                              uint8_t{30},
                              uint8_t{34},
                              uint8_t{36},
                              uint8_t{38}})
            {
                rb->SetDscpToSlot(d, 2);
            }
            // Tin 3 (Voice): CS4, CS5, VA, EF, CS6, CS7
            for (uint8_t d :
                 {uint8_t{32}, uint8_t{40}, uint8_t{44}, uint8_t{46}, uint8_t{48}, uint8_t{56}})
            {
                rb->SetDscpToSlot(d, 3);
            }
            // All remaining DSCPs -> Tin 1 (Best-Effort default).
            for (uint8_t d = 0; d < 64; ++d)
            {
                const bool isBulk = (d == 8 || d == 1 || d == 10);
                const bool isVideo = (d == 12 || d == 14 || d == 16 || d == 18 || d == 20 ||
                                      d == 22 || d == 24 || d == 26 || d == 28 || d == 30 ||
                                      d == 34 || d == 36 || d == 38);
                const bool isVoice =
                    (d == 32 || d == 40 || d == 44 || d == 46 || d == 48 || d == 56);
                if (!isBulk && !isVideo && !isVoice)
                {
                    rb->SetDscpToSlot(d, 1);
                }
            }
        }
        if (m_enableIngressMode)
        {
            rb->SetIngressMode(true);
        }
        // If the Linux autorate hook is installed, wire it into the dispatcher
        // so that DoEnqueue calls OnEnqueue on every accepted packet.
        // The dispatcher receives a shared_ptr so it co-owns the hook and
        // the hook outlives a stack-local helper.
        if (m_enableAutorateIngress && m_autorateImpl == AutorateImpl::Linux &&
            m_autorateHook != nullptr)
        {
            std::shared_ptr<DsCakeLinuxAutorateHook> linuxHook =
                std::dynamic_pointer_cast<DsCakeLinuxAutorateHook>(m_autorateHook);
            if (linuxHook != nullptr)
            {
                rb->SetAutorateHook(linuxHook);
            }
        }
        return rb;
    }
    }
    // Unreachable; satisfies non-void return on compilers that do not
    // analyse the exhaustive switch above.
    return CreateObject<DiffServEdgeQueueDisc>();
}

Ptr<QueueDisc>
DsCakeHelper::BuildAndInstall(Ptr<NetDevice> device)
{
    Ptr<QueueDisc> qdisc = BuildDispatcher();
    Ptr<TrafficControlLayer> tc = device->GetNode()->GetObject<TrafficControlLayer>();
    NS_ASSERT_MSG(tc, "TrafficControlLayer must be installed on the node");
    // InternetStackHelper::Install attaches a default root qdisc per
    // device; remove it before installing the CAKE dispatcher so the
    // install does not collide with the existing root qdisc.  The Get
    // probe is required because DeleteRootQueueDiscOnDevice asserts
    // when no root qdisc is present on the device.
    if (tc->GetRootQueueDiscOnDevice(device))
    {
        tc->DeleteRootQueueDiscOnDevice(device);
    }
    tc->SetRootQueueDiscOnDevice(device, qdisc);
    return qdisc;
}

namespace
{

constexpr uint32_t kCakeMtu = 1514;   // Ethernet frame, matches Linux tc-cake.
constexpr uint32_t kQuantumScale = 4; // DRR quantum = MTU * share * scale.

/**
 * Build one tin as a mainline `FqCobaltQueueDisc` configured per the
 * CAKE §III-B (8-way set-associative hashing, SetWays=8,
 * Quantum=MTU). Per-tin rate-capping is enforced by the across-tin DRR
 * quantum, not by an outer rate-shaper — the bottleneck link itself caps
 * aggregate rate; DRR distributes share among busy tins; idle-tin
 * capacity is redistributed to busy tins (Linux tc-cake's default).
 *
 * When @p enableHostIsolation is true, `EnableHostIsolation=true` and
 * `HostIsolationMode=Triple` are set on the disc (attributes from
 * `patches/ns3/0016-fq-cobalt-host-isolation.patch`), matching Linux
 * `sch_cake.c` triple-isolate semantics (67dc6c56b871).
 *
 * `EnableAckFilter`, `EnableAckFilterAggressive`, and `MemLimit` are
 * exposed by `patches/ns3/0006-fq-cobalt-ack-filter-memlimit.patch`
 * (filed upstream; patch retires once the pin advances past the merge).
 */
Ptr<QueueDisc>
MakeTin(bool enableAckFilter,
        bool enableAckFilterAggressive,
        bool enableHostIsolation,
        bool useDualPi2Inner)
{
    NS_ASSERT_MSG(!(useDualPi2Inner && enableHostIsolation),
                  "DualPI2 inner is mutually exclusive with host-isolation: "
                  "DualPI2 has its own classic+L4S queueing layer that does not "
                  "compose with the host-isolation attributes on FqCobaltQueueDisc. "
                  "To compose CAKE host-fairness with L4S signaling, the substrate "
                  "would need a host-isolated DualPI2 variant (future work).");
    if (useDualPi2Inner)
    {
        // L4S DualPI2 as the per-tin inner. The tin's outer rate cap is
        // enforced by the across-tin DRR + optional TBF wrapper; DualPI2
        // does marking (ECT(1)) for scalable congestion controls and
        // drop-based AQM (RED-flavoured) for classic flows, with coupled
        // marking probability between the two queues per RFC 9332. The
        // ACK-filter knobs do not apply (DualPI2 has no ACK filter); they
        // are silently ignored when this branch is taken.
        //
        // Configure the inner the same way a standalone DualPI2 bottleneck
        // is configured. Left at raw construction defaults the inner runs a
        // shallow WRED classic queue (25-packet limit, early-mark from an
        // average of 5 packets) with no starvation-safe scheduler — far
        // below a typical bandwidth-delay product, so the two responsive
        // flows cannot keep the link full and aggregate throughput
        // collapses to a fraction of the standalone case. CoupledOnly turns
        // the classic queue into a deep pass-through FIFO whose only
        // congestion signal is the coupled probability, BDP-scale buffering
        // lets the flows fill the pipe, and the coupled scheduler keeps the
        // classic queue from starving under sustained L4S load.
        constexpr uint32_t kInnerClassicLimitPkts = 500;
        constexpr uint32_t kInnerL4sLimitPkts = 500;
        Ptr<DsL4sQueueDisc> dualPi2 = CreateObject<DsL4sQueueDisc>();
        dualPi2->SetClassicAqm(DsL4sQueueDisc::ClassicAqm::CoupledOnly);
        dualPi2->SetNumQueues(2);
        dualPi2->SetL4sQueueIdx(1);
        dualPi2->SetQueueLimit(0, kInnerClassicLimitPkts);
        dualPi2->SetQueueLimit(1, kInnerL4sLimitPkts);
        dualPi2->AddPhbEntry(0, 0, 0);
        Ptr<DsL4sCoupledScheduler> sched =
            CreateObjectWithAttributes<DsL4sCoupledScheduler>("NumQueues",
                                                              UintegerValue(2),
                                                              "L4sQueueIdx",
                                                              UintegerValue(1),
                                                              "BurstCap",
                                                              UintegerValue(8));
        dualPi2->SetScheduler(sched);
        return dualPi2;
    }
    // Host-iso path and standard path both produce a mainline
    // FqCobaltQueueDisc; the host-isolation knob is threaded in via the
    // EnableHostIsolation / HostIsolationMode attributes supplied by
    // patches/ns3/0016-fq-cobalt-host-isolation.patch. Per-side-max
    // triple-isolate keying matches Linux sch_cake.c @ 67dc6c56b871.
    Ptr<FqCobaltQueueDisc> fq =
        CreateObjectWithAttributes<FqCobaltQueueDisc>("EnableSetAssociativeHash",
                                                      BooleanValue(true),
                                                      "SetWays",
                                                      UintegerValue(8),
                                                      "EnableAckFilter",
                                                      BooleanValue(enableAckFilter),
                                                      "EnableAckFilterAggressive",
                                                      BooleanValue(enableAckFilterAggressive),
                                                      "EnableHostIsolation",
                                                      BooleanValue(enableHostIsolation),
                                                      "HostIsolationMode",
                                                      EnumValue<FqCobaltQueueDisc::HostIsolationMode>(
                                                          FqCobaltQueueDisc::HostIsolationMode::Triple));
    fq->SetQuantum(kCakeMtu);
    return fq;
}

/// Sentinel meaning "no LLQ slot configured; use pure-DRR dispatcher".
constexpr uint32_t kNoLlqSlot = std::numeric_limits<uint32_t>::max();

/**
 * Install @p numTins tins on @p edge with @p shares, build the across-
 * tin dispatcher with share-proportional quanta, and stamp the DSCP map.
 *
 * @param edge fresh DiffServEdgeQueueDisc
 * @param totalRate aggregate rate
 * @param enableAckFilter ACK-filter opt-in
 * @param shares per-tin shares (size == numTins)
 * @param numTins tin count
 * @param dscpMap 64-entry DSCP -> tin index lookup
 * @param llqSlot if < numTins, install `DsHybridLlqDispatcher` and mark
 *        this slot strict-priority; otherwise install
 *        `DsTinShaperDispatcher` (pure DRR)
 * @param enableTinShaping when true, set per-tin hard rate caps on the
 *        dispatcher at `share × totalRate` with a 100 ms burst floored
 *        at `4 × MTU` (Linux tc-cake "bandwidth N" semantics). When
 *        false the dispatcher runs work-conserving and tin shares are
 *        enforced only by the DRR quanta.
 */
void
InstallTins(Ptr<DiffServEdgeQueueDisc> edge,
            DataRate totalRate,
            bool enableAckFilter,
            const double* shares,
            uint32_t numTins,
            const uint8_t* dscpMap,
            uint32_t llqSlot,
            bool enableTinShaping,
            bool enableHostIsolation,
            bool useInnerTbfShaping,
            bool enableAckFilterAggressive,
            bool useDualPi2Inner)
{
    NS_ASSERT_MSG(!useInnerTbfShaping || enableTinShaping,
                  "useInnerTbfShaping requires enableTinShaping; the toggle picks the "
                  "implementation of tin shaping, not whether shaping happens");
    NS_ASSERT_MSG(!(useDualPi2Inner && enableHostIsolation),
                  "useDualPi2Inner and enableHostIsolation are mutually exclusive "
                  "(see MakeTin contract).");
    NS_ASSERT_MSG(numTins >= 1 && numTins <= DiffServEdgeQueueDisc::kMaxInnerSlots,
                  "tin count " << numTins << " out of range");
    NS_ASSERT_MSG(llqSlot == kNoLlqSlot || llqSlot < numTins,
                  "LLQ slot " << llqSlot << " out of range for tin count " << numTins);

    const bool useLlq = (llqSlot != kNoLlqSlot);

    // Compose the across-tin dispatcher. The two paths share the per-
    // slot quantum derivation (MTU * share * scale, floored at one MTU);
    // the LLQ path skips quantum installation on the SP slot since SP
    // and DRR are mutually exclusive.
    Ptr<DsTinShaperDispatcher> shaper;
    Ptr<DsHybridLlqDispatcher> hybrid;
    if (useLlq)
    {
        hybrid = CreateObject<DsHybridLlqDispatcher>();
        hybrid->SetSlotStrictPriority(llqSlot);
    }
    else
    {
        shaper = CreateObject<DsTinShaperDispatcher>();
    }

    for (uint32_t s = 0; s < numTins; ++s)
    {
        Ptr<QueueDisc> tinCore =
            MakeTin(enableAckFilter, enableAckFilterAggressive, enableHostIsolation, useDualPi2Inner);

        // Compute the per-tin shaping parameters once (used by both path
        // alpha and path gamma below). Linux tc-cake "bandwidth N <profile>":
        // each tin's hard rate is `share × totalRate`; the burst is 100 ms
        // of that rate floored at 4 × MTU.
        const auto tinRateBps = static_cast<uint64_t>(totalRate.GetBitRate() * shares[s]);
        const uint64_t burstBytes =
            std::max<uint64_t>(static_cast<uint64_t>(4 * kCakeMtu), tinRateBps / 8 / 10);

        Ptr<QueueDisc> tinAsInner = tinCore;
        if (useInnerTbfShaping && enableTinShaping)
        {
            // Path gamma: wrap the tin core (FqCobalt-flavoured) with a
            // mainline TbfQueueDisc that enforces the per-tin cap. The
            // dispatcher's SetRateCap is intentionally skipped below; the
            // gate lives in the TBF. This composition relies on the
            // patches/ns3/0004 inner-mode guard so the TBF's watchdog
            // doesn't trip the m_send assertion when nested.
            Ptr<TbfQueueDisc> tbf = CreateObject<TbfQueueDisc>();
            tbf->SetAttribute("Rate", DataRateValue(DataRate(tinRateBps)));
            tbf->SetAttribute("Burst", UintegerValue(burstBytes));
            tbf->SetAttribute("Mtu", UintegerValue(kCakeMtu));
            // The TBF's auto-default FifoQueueDisc child is replaced by
            // tinCore via AddQueueDiscClass before Initialize. Setting
            // MaxSize is a no-op here because the FqCobalt-flavoured
            // inner uses MULTIPLE_QUEUES sizing; the FQ owns its own
            // packet-count limit per-flow.
            Ptr<QueueDiscClass> cls = CreateObject<QueueDiscClass>();
            cls->SetQueueDisc(tinCore);
            tbf->AddQueueDiscClass(cls);
            tinAsInner = tbf;
        }

        edge->SetInnerDiscAt(s, tinAsInner);

        const bool isSpSlot = (useLlq && s == llqSlot);

        if (!isSpSlot)
        {
            const uint32_t quantum =
                std::max<uint32_t>(static_cast<uint32_t>(kCakeMtu * shares[s] * kQuantumScale),
                                   kCakeMtu);
            if (useLlq)
            {
                hybrid->SetQuantum(s, quantum);
            }
            else
            {
                shaper->SetQuantum(s, quantum);
            }
        }

        if (enableTinShaping && !useInnerTbfShaping)
        {
            // Path alpha: in-dispatcher TinTokenBucket gate. Applies to SP
            // and DRR slots equally — (enableLlq && enableTinShaping) is
            // the Cisco MQC LLQ pattern (priority class with hard cap).
            if (useLlq)
            {
                hybrid->SetRateCap(s, tinRateBps, burstBytes);
            }
            else
            {
                shaper->SetRateCap(s, tinRateBps, burstBytes);
            }
        }
    }

    edge->SetSlotDispatcher(useLlq ? Ptr<DsSlotDispatcher>(hybrid) : Ptr<DsSlotDispatcher>(shaper));

    for (uint32_t dscp = 0; dscp < kMaxCodePoints; ++dscp)
    {
        edge->SetDscpToSlot(static_cast<uint8_t>(dscp), dscpMap[dscp]);
    }
}

} // namespace

void
DsCakeHelper::SetAsCakeDiffserv4(Ptr<DiffServEdgeQueueDisc> edge,
                                 DataRate totalRate,
                                 bool enableAckFilter,
                                 bool enableLlq,
                                 bool enableTinShaping,
                                 bool enableHostIsolation,
                                 bool useInnerTbfShaping,
                                 bool enableAckFilterAggressive,
                                 bool useDualPi2Inner)
{
    NS_LOG_FUNCTION(edge << totalRate << enableAckFilter << enableLlq << enableTinShaping
                         << enableHostIsolation << useInnerTbfShaping
                         << enableAckFilterAggressive << useDualPi2Inner);

    // Linux tc-cake(8) diffserv4 shares: Bulk 6.25%, BE 100%, Video 50%, Voice 25%.
    static constexpr std::array<double, 4> kShares = {0.0625, 1.0, 0.5, 0.25};

    // Linux tc-cake(8) diffserv4 DSCP -> tin map.
    // 0 (Bulk) | 1 (BE - default) | 2 (Video) | 3 (Voice)
    std::array<uint8_t, kMaxCodePoints> dscpMap{};
    dscpMap.fill(1); // default: Best-Effort (Tin 1)

    // Tin 0 (Bulk): CS1, LE, AF11
    dscpMap[8] = 0;  // CS1
    dscpMap[1] = 0;  // LE
    dscpMap[10] = 0; // AF11

    // Tin 2 (Video): AF12, AF13, CS2, AF21, AF22, AF23, CS3, AF31, AF32, AF33,
    //                AF41, AF42, AF43
    for (uint8_t d : {uint8_t{12},
                      uint8_t{14},
                      uint8_t{16},
                      uint8_t{18},
                      uint8_t{20},
                      uint8_t{22},
                      uint8_t{24},
                      uint8_t{26},
                      uint8_t{28},
                      uint8_t{30},
                      uint8_t{34},
                      uint8_t{36},
                      uint8_t{38}})
    {
        dscpMap[d] = 2;
    }

    // Tin 3 (Voice): CS4, CS5, VA, EF, CS6, CS7
    for (uint8_t d : {uint8_t{32}, uint8_t{40}, uint8_t{44}, uint8_t{46}, uint8_t{48}, uint8_t{56}})
    {
        dscpMap[d] = 3;
    }

    // diffserv4 LLQ slot is Voice (slot 3, EF/CS5/CS4/CS6/CS7/VA).
    InstallTins(edge,
                totalRate,
                enableAckFilter,
                kShares.data(),
                4,
                dscpMap.data(),
                enableLlq ? 3u : kNoLlqSlot,
                enableTinShaping,
                enableHostIsolation,
                useInnerTbfShaping,
                enableAckFilterAggressive,
                useDualPi2Inner);
}

void
DsCakeHelper::SetAsCakeDiffserv3(Ptr<DiffServEdgeQueueDisc> edge,
                                 DataRate totalRate,
                                 bool enableAckFilter,
                                 bool enableLlq,
                                 bool enableTinShaping,
                                 bool enableHostIsolation,
                                 bool useInnerTbfShaping,
                                 bool enableAckFilterAggressive)
{
    NS_LOG_FUNCTION(edge << totalRate << enableAckFilter << enableLlq << enableTinShaping
                         << enableHostIsolation << useInnerTbfShaping
                         << enableAckFilterAggressive);

    // Linux tc-cake(8) diffserv3 shares: Bulk 6.25%, Latency-Sensitive 50%, BE 100%.
    static constexpr std::array<double, 3> kShares = {0.0625, 0.5, 1.0};

    // Linux tc-cake(8) diffserv3 DSCP -> tin map.
    // 0 (Bulk) | 1 (Latency-Sensitive) | 2 (BE - default)
    std::array<uint8_t, kMaxCodePoints> dscpMap{};
    dscpMap.fill(2); // default: Best-Effort (Tin 2)

    // Tin 0 (Bulk): CS1, LE, AF11
    dscpMap[8] = 0;
    dscpMap[1] = 0;
    dscpMap[10] = 0;

    // Tin 1 (Latency-Sensitive): EF, VA, CS7, CS6, CS5, CS4, CS3, CS2,
    // AF12, AF13, AF21, AF22, AF23, AF31, AF32, AF33, AF41, AF42, AF43
    for (uint8_t d : {uint8_t{46},
                      uint8_t{44},
                      uint8_t{56},
                      uint8_t{48},
                      uint8_t{40},
                      uint8_t{32},
                      uint8_t{24},
                      uint8_t{16},
                      uint8_t{12},
                      uint8_t{14},
                      uint8_t{18},
                      uint8_t{20},
                      uint8_t{22},
                      uint8_t{26},
                      uint8_t{28},
                      uint8_t{30},
                      uint8_t{34},
                      uint8_t{36},
                      uint8_t{38}})
    {
        dscpMap[d] = 1;
    }

    // diffserv3 LLQ slot is Latency-Sensitive (slot 1).
    InstallTins(edge,
                totalRate,
                enableAckFilter,
                kShares.data(),
                3,
                dscpMap.data(),
                enableLlq ? 1u : kNoLlqSlot,
                enableTinShaping,
                enableHostIsolation,
                useInnerTbfShaping,
                enableAckFilterAggressive,
                /*useDualPi2Inner=*/false);
}

void
DsCakeHelper::SetAsCakeDiffserv8(Ptr<DiffServEdgeQueueDisc> edge,
                                 DataRate totalRate,
                                 bool enableAckFilter,
                                 bool enableLlq,
                                 bool enableTinShaping,
                                 bool enableHostIsolation,
                                 bool useInnerTbfShaping,
                                 bool enableAckFilterAggressive)
{
    NS_LOG_FUNCTION(edge << totalRate << enableAckFilter << enableLlq << enableTinShaping
                         << enableHostIsolation << useInnerTbfShaping
                         << enableAckFilterAggressive);

    // diffserv8 shares: Tin 0 (CS0/Default) gets full rate, Tin 1 (CS1)
    // is bulk-tier, Tin 2..5 (data classes) at 50%, Tin 6..7 (network/
    // signalling) at 25%.
    static constexpr std::array<double, 8> kShares = {1.0, 0.0625, 0.5, 0.5, 0.5, 0.5, 0.25, 0.25};

    std::array<uint8_t, kMaxCodePoints> dscpMap{};
    dscpMap.fill(0); // default: CS0 / Default tin

    // Tin 1: CS1, LE
    dscpMap[8] = 1;
    dscpMap[1] = 1;

    // Tin 2: CS2, AF11, AF12, AF13
    for (uint8_t d : {uint8_t{16}, uint8_t{10}, uint8_t{12}, uint8_t{14}})
    {
        dscpMap[d] = 2;
    }

    // Tin 3: CS3, AF21, AF22, AF23
    for (uint8_t d : {uint8_t{24}, uint8_t{18}, uint8_t{20}, uint8_t{22}})
    {
        dscpMap[d] = 3;
    }

    // Tin 4: CS4, AF31, AF32, AF33
    for (uint8_t d : {uint8_t{32}, uint8_t{26}, uint8_t{28}, uint8_t{30}})
    {
        dscpMap[d] = 4;
    }

    // Tin 5: CS5, AF41, AF42, AF43
    for (uint8_t d : {uint8_t{40}, uint8_t{34}, uint8_t{36}, uint8_t{38}})
    {
        dscpMap[d] = 5;
    }

    // Tin 6: CS6, EF, VA
    for (uint8_t d : {uint8_t{48}, uint8_t{46}, uint8_t{44}})
    {
        dscpMap[d] = 6;
    }

    // Tin 7: CS7
    dscpMap[56] = 7;

    // diffserv8 LLQ slot is Tin 6 (CS6/EF/VA).
    InstallTins(edge,
                totalRate,
                enableAckFilter,
                kShares.data(),
                8,
                dscpMap.data(),
                enableLlq ? 6u : kNoLlqSlot,
                enableTinShaping,
                enableHostIsolation,
                useInnerTbfShaping,
                enableAckFilterAggressive,
                /*useDualPi2Inner=*/false);
}

void
DsCakeHelper::SetAsCakeBestEffort(Ptr<DiffServEdgeQueueDisc> edge,
                                  DataRate totalRate,
                                  bool enableAckFilter,
                                  bool enableLlq,
                                  bool enableTinShaping,
                                  bool enableHostIsolation,
                                  bool useInnerTbfShaping,
                                  bool enableAckFilterAggressive)
{
    NS_LOG_FUNCTION(edge << totalRate << enableAckFilter << enableLlq << enableTinShaping
                         << enableHostIsolation << useInnerTbfShaping
                         << enableAckFilterAggressive);

    // besteffort: a single tin, full rate. enableLlq is a no-op (one tin
    // — there is no cross-tin priority ordering).
    static constexpr std::array<double, 1> kShares = {1.0};

    std::array<uint8_t, kMaxCodePoints> dscpMap{};
    dscpMap.fill(0); // every DSCP -> tin 0

    InstallTins(edge,
                totalRate,
                enableAckFilter,
                kShares.data(),
                1,
                dscpMap.data(),
                kNoLlqSlot,
                enableTinShaping,
                enableHostIsolation,
                useInnerTbfShaping,
                enableAckFilterAggressive,
                /*useDualPi2Inner=*/false);
}

void
DsCakeHelper::SetAsCakePrecedence(Ptr<DiffServEdgeQueueDisc> edge,
                                  DataRate totalRate,
                                  bool enableAckFilter,
                                  bool enableLlq,
                                  bool enableTinShaping,
                                  bool enableHostIsolation,
                                  bool useInnerTbfShaping,
                                  bool enableAckFilterAggressive)
{
    NS_LOG_FUNCTION(edge << totalRate << enableAckFilter << enableLlq << enableTinShaping
                         << enableHostIsolation << useInnerTbfShaping
                         << enableAckFilterAggressive);

    // precedence: 8 tins, one per IP precedence value (top 3 bits of
    // DSCP). Shares are progressive — tin 0 = 0.125 (lowest precedence,
    // bulk-equivalent), tin 7 = 1.0 (network control). The relative
    // ordering mirrors Linux's `cake_class_quanta_precedence` table.
    static constexpr std::array<double, 8> kShares = {
        0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875, 1.0};

    std::array<uint8_t, kMaxCodePoints> dscpMap{};
    for (uint32_t dscp = 0; dscp < kMaxCodePoints; ++dscp)
    {
        // Top 3 bits of DSCP yield IP precedence (0..7). All eight DSCPs
        // sharing the same precedence land in the same tin.
        dscpMap[dscp] = static_cast<uint8_t>(dscp >> 3);
    }

    InstallTins(edge,
                totalRate,
                enableAckFilter,
                kShares.data(),
                8,
                dscpMap.data(),
                enableLlq ? 7u : kNoLlqSlot,
                enableTinShaping,
                enableHostIsolation,
                useInnerTbfShaping,
                enableAckFilterAggressive,
                /*useDualPi2Inner=*/false);
}

namespace
{
// CAKE statistical-overhead bimodal Internet mix:
// 50% small ACK-class packets (64B) + 50% MTU-class packets (1500B).
// Linux `tc-cake` measurements use ~equivalent traffic; deviation
// from this mix produces deterministic relative error bounded by
// the spread between min and max wire(s) values.
constexpr uint32_t kCakeBimodalSmallBytes = 64;
constexpr uint32_t kCakeBimodalLargeBytes = 1500;
constexpr double kCakeBimodalSmallProb = 0.5;
constexpr double kCakeBimodalLargeProb = 0.5;

// Mirror of Linux `cake_overhead()` for a single packet: apply the
// per-packet adjustment, then optional ATM/PTM cell rounding, then
// floor at MPU. Not stateful — pure function of the five inputs.
uint32_t
CakeWireBytesFor(uint32_t ipBytes, uint32_t overhead, bool atm, bool ptm, uint32_t mpu)
{
    const uint32_t base = ipBytes + overhead;
    uint32_t framed;
    if (atm)
    {
        framed = ((base + 47) / 48) * 53;
    }
    else if (ptm)
    {
        framed = base + (base + 63) / 64;
    }
    else
    {
        framed = base;
    }
    return std::max(framed, mpu);
}

// E[wire(s)] / E[s] over the bimodal mix. Returns 1.0 in degenerate
// configs (no overhead, no ATM, no PTM, no MPU above smallest packet)
// so the caller can no-op the rate-adjustment pass without branching.
double
CakeGammaForBimodalMix(uint32_t overhead, bool atm, bool ptm, uint32_t mpu)
{
    const uint32_t wireSmall =
        CakeWireBytesFor(kCakeBimodalSmallBytes, overhead, atm, ptm, mpu);
    const uint32_t wireLarge =
        CakeWireBytesFor(kCakeBimodalLargeBytes, overhead, atm, ptm, mpu);
    const double numerator =
        kCakeBimodalSmallProb * wireSmall + kCakeBimodalLargeProb * wireLarge;
    const double denominator = kCakeBimodalSmallProb * kCakeBimodalSmallBytes +
                               kCakeBimodalLargeProb * kCakeBimodalLargeBytes;
    return numerator / denominator;
}
struct LinkPresetTuple
{
    uint32_t overhead;
    bool atm;
    bool ptm;
    uint32_t mpu;
    bool raw{false};
};

LinkPresetTuple
ResolveLinkPreset(DsCakeHelper::LinkPreset preset)
{
    switch (preset)
    {
    case DsCakeHelper::LinkPreset::Raw:            return {0, false, false, 0, /*raw=*/true};
    case DsCakeHelper::LinkPreset::Conservative:   return {48, false, false, 64};
    case DsCakeHelper::LinkPreset::Ethernet:       return {38, false, false, 84};
    case DsCakeHelper::LinkPreset::EtherVlan:      return {42, false, false, 84};
    case DsCakeHelper::LinkPreset::Docsis:         return {18, false, false, 64};
    case DsCakeHelper::LinkPreset::PppoePtm:       return {30, false, true, 0};
    case DsCakeHelper::LinkPreset::PppoeVcmux:     return {32, true, false, 0};
    case DsCakeHelper::LinkPreset::PppoeLlcsnap:   return {40, true, false, 0};
    case DsCakeHelper::LinkPreset::PppoaVcmux:     return {10, true, false, 0};
    case DsCakeHelper::LinkPreset::PppoaLlc:       return {14, true, false, 0};
    case DsCakeHelper::LinkPreset::BridgedPtm:     return {22, false, true, 0};
    case DsCakeHelper::LinkPreset::BridgedVcmux:   return {24, true, false, 0};
    case DsCakeHelper::LinkPreset::BridgedLlcsnap: return {32, true, false, 0};
    case DsCakeHelper::LinkPreset::IpoaVcmux:      return {8, true, false, 0};
    case DsCakeHelper::LinkPreset::IpoaLlcsnap:    return {16, true, false, 0};
    }
    NS_FATAL_ERROR("Unknown LinkPreset enum value");
    return {0, false, false, 0};
}

// FqCobaltQueueDisc stores Target and Interval as string attributes
// (e.g. "5ms", "100ms"); they are parsed by StringValue and forwarded
// to each inner CobaltQueueDisc when InitializeParams fires.
struct RttPresetTuple
{
    const char* target;
    const char* interval;
};

RttPresetTuple
ResolveRttPreset(DsCakeHelper::RttPreset preset)
{
    switch (preset)
    {
    case DsCakeHelper::RttPreset::Datacentre:    return {"5us",    "100us"};
    case DsCakeHelper::RttPreset::Lan:           return {"50us",   "1ms"};
    case DsCakeHelper::RttPreset::Metro:         return {"500us",  "10ms"};
    case DsCakeHelper::RttPreset::Regional:      return {"1500us", "30ms"};
    case DsCakeHelper::RttPreset::Internet:      return {"5ms",    "100ms"};
    case DsCakeHelper::RttPreset::Oceanic:       return {"15ms",   "300ms"};
    case DsCakeHelper::RttPreset::Satellite:     return {"50ms",   "1000ms"};
    case DsCakeHelper::RttPreset::Interplanetary:return {"50s",    "1000s"};
    }
    NS_FATAL_ERROR("Unknown RttPreset enum value");
    return {nullptr, nullptr};
}
} // namespace

void
DsCakeHelper::SetAsCakeAlphaTinShaped(Ptr<DiffServEdgeQueueDisc> edge,
                                      DataRate totalRate,
                                      bool enableAckFilter,
                                      bool enableLlq,
                                      bool enableHostIsolation,
                                      bool enableAckFilterAggressive)
{
    NS_LOG_FUNCTION(edge << totalRate << enableAckFilter << enableLlq << enableHostIsolation
                         << enableAckFilterAggressive);
    NS_ASSERT_MSG(edge, "SetAsCakeAlphaTinShaped requires non-null edge");
    // Path-α with per-tin caps: TokenBucket-via-dispatcher across tins
    // (useInnerTbfShaping=false) plus the in-dispatcher TinTokenBucket
    // gate inside each tin (enableTinShaping=true). Closes the gap
    // surfaced by the path α/β/γ comparison panel where default α
    // composition lets traffic through at link rate.
    DsCakeHelper::SetAsCakeDiffserv4(edge,
                                     totalRate,
                                     enableAckFilter,
                                     enableLlq,
                                     /*enableTinShaping=*/true,
                                     enableHostIsolation,
                                     /*useInnerTbfShaping=*/false,
                                     enableAckFilterAggressive);
}

void
DsCakeHelper::SetAsCakeConservative(Ptr<DiffServEdgeQueueDisc> edge)
{
    NS_LOG_FUNCTION(edge);
    NS_ASSERT_MSG(edge, "SetAsCakeConservative requires non-null edge");
    SetLinkLayer(edge, LinkPreset::Conservative);
}

void
DsCakeHelper::SetLinkLayer(Ptr<DiffServEdgeQueueDisc> edge, LinkPreset preset)
{
    NS_LOG_FUNCTION(edge << static_cast<int>(preset));
    NS_ASSERT_MSG(edge, "SetLinkLayer requires non-null edge");

    const LinkPresetTuple t = ResolveLinkPreset(preset);
    ConfigureLinkLayerOverhead(edge, t.overhead, t.atm, t.ptm, t.mpu, t.raw);
}

void
DsCakeHelper::SetRttPreset(Ptr<DiffServEdgeQueueDisc> edge, RttPreset preset)
{
    NS_LOG_FUNCTION(edge << static_cast<int>(preset));
    NS_ASSERT_MSG(edge, "SetRttPreset requires non-null edge");

    const RttPresetTuple t = ResolveRttPreset(preset);

    for (uint32_t slot = 0; slot < edge->GetNumInnerSlots(); ++slot)
    {
        Ptr<QueueDisc> inner = edge->GetInnerDiscAt(slot);
        if (!inner)
        {
            continue;
        }
        // Walk to the nearest FqCobaltQueueDisc: direct, or wrapped by TBF.
        // L4S (DsL4sQueueDisc) tins are skipped — they carry no
        // FqCobaltQueueDisc and the GetObject<> / DynamicCast<> below will
        // fall through to the NS_LOG_WARN path.
        Ptr<FqCobaltQueueDisc> fq = inner->GetObject<FqCobaltQueueDisc>();
        if (!fq)
        {
            Ptr<TbfQueueDisc> tbf = inner->GetObject<TbfQueueDisc>();
            if (tbf && tbf->GetNQueueDiscClasses() > 0)
            {
                fq = DynamicCast<FqCobaltQueueDisc>(
                    tbf->GetQueueDiscClass(0)->GetQueueDisc());
            }
        }
        if (!fq)
        {
            NS_LOG_WARN("SetRttPreset: slot " << slot
                        << " has no FqCobaltQueueDisc to configure; skipped");
            continue;
        }
        fq->SetAttribute("Target", StringValue(t.target));
        fq->SetAttribute("Interval", StringValue(t.interval));
    }
}

void
DsCakeHelper::ConfigureLinkLayerOverhead(Ptr<DiffServEdgeQueueDisc> edge,
                                         uint32_t overhead,
                                         bool atm,
                                         bool ptm,
                                         uint32_t mpu,
                                         bool raw)
{
    NS_LOG_FUNCTION(edge << overhead << atm << ptm << mpu << raw);
    NS_ASSERT_MSG(edge, "ConfigureLinkLayerOverhead requires non-null edge");
    NS_ASSERT_MSG(!(atm && ptm),
                  "ATM and PTM framing are mutually exclusive");

    if (raw)
    {
        // Linux `raw` flag — interpret configured `bandwidth` at the IP
        // layer. No per-tin TBF rate adjustment.
        return;
    }

    const double gamma = CakeGammaForBimodalMix(overhead, atm, ptm, mpu);

    // Walk every populated inner slot; if it wraps a TBF (i.e. the user
    // composed with `enableTinShaping=true` or `useInnerTbfShaping=true`),
    // downscale that TBF's `Rate` attribute by gamma. Untouched slots
    // (no TBF wrapper) are silently skipped — matches Linux semantics
    // where overhead correction is meaningful only when bandwidth-shaping
    // is active.
    for (uint32_t slot = 0; slot < edge->GetNumInnerSlots(); ++slot)
    {
        Ptr<QueueDisc> inner = edge->GetInnerDiscAt(slot);
        if (!inner)
        {
            continue;
        }
        Ptr<TbfQueueDisc> tbf = inner->GetObject<TbfQueueDisc>();
        if (!tbf)
        {
            continue;
        }
        // TbfQueueDisc's `Rate` attribute is bound to SetRate only — the
        // mainline accessor is asymmetric. Read via GetRate(), write via
        // SetRate() on the concrete type.
        const uint64_t scaledBps =
            static_cast<uint64_t>(tbf->GetRate().GetBitRate() / gamma);
        tbf->SetRate(DataRate(scaledBps));
    }
}

void
DsCakeHelper::PrintTcStats(std::ostream& os, Ptr<const QueueDisc> edge) const
{
    NS_LOG_FUNCTION(this << edge);
    DsCakeStatsFormatter::Print(os, edge);
}

void
DsCakeHelper::AttachLiveBulkCounter(Ptr<DiffServEdgeQueueDisc> edge, Time idleWindow)
{
    NS_LOG_FUNCTION(edge << idleWindow);
    if (!edge)
    {
        return;
    }
    for (uint32_t slot = 0; slot < edge->GetNumInnerSlots(); ++slot)
    {
        Ptr<QueueDisc> inner = edge->GetInnerDiscAt(slot);
        if (!inner)
        {
            continue;
        }
        // Try direct cast first; if wrapped by TBF, walk one level deeper.
        Ptr<FqCobaltQueueDisc> fq = inner->GetObject<FqCobaltQueueDisc>();
        if (!fq)
        {
            Ptr<TbfQueueDisc> tbf = inner->GetObject<TbfQueueDisc>();
            if (tbf && tbf->GetNQueueDiscClasses() > 0)
            {
                fq = DynamicCast<FqCobaltQueueDisc>(
                    tbf->GetQueueDiscClass(0)->GetQueueDisc());
            }
        }
        if (!fq)
        {
            NS_LOG_WARN("AttachLiveBulkCounter: slot "
                        << slot << " has no FqCobaltQueueDisc to attach to; skipped");
            continue;
        }
        Ptr<DsCakeLiveBulkCounter> counter = CreateObject<DsCakeLiveBulkCounter>();
        counter->Attach(fq, idleWindow);
        fq->AggregateObject(counter);
    }
}

uint32_t
DsCakeHelper::GetLiveBulkCount(Ptr<DiffServEdgeQueueDisc> edge, uint32_t slot)
{
    NS_LOG_FUNCTION(edge << slot);
    if (!edge)
    {
        return 0;
    }
    Ptr<QueueDisc> inner = edge->GetInnerDiscAt(slot);
    if (!inner)
    {
        return 0;
    }
    Ptr<FqCobaltQueueDisc> fq = inner->GetObject<FqCobaltQueueDisc>();
    if (!fq)
    {
        return 0;
    }
    Ptr<DsCakeLiveBulkCounter> counter = fq->GetObject<DsCakeLiveBulkCounter>();
    if (!counter)
    {
        return 0;
    }
    return counter->GetLiveCount(Simulator::Now());
}

} // namespace diffserv
} // namespace ns3
