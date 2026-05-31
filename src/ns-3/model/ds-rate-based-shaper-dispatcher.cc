/*
 * Copyright (C) 2026 Sergio Andreozzi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ds-rate-based-shaper-dispatcher.h"

#include "ds-cake-linux-autorate-hook.h"

#include "ns3/drop-tail-queue.h"
#include "ns3/ipv4-header.h"
#include "ns3/ipv4-queue-disc-item.h"
#include "ns3/log.h"
#include "ns3/object-factory.h"
#include "ns3/simulator.h"

#include <algorithm>

namespace ns3
{
namespace diffserv
{

NS_LOG_COMPONENT_DEFINE("DsRateBasedShaperDispatcher");
NS_OBJECT_ENSURE_REGISTERED(DsRateBasedShaperDispatcher);

TypeId
DsRateBasedShaperDispatcher::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::diffserv::DsRateBasedShaperDispatcher")
            .SetParent<QueueDisc>()
            .SetGroupName("Diffserv")
            .AddConstructor<DsRateBasedShaperDispatcher>()
            .AddAttribute("MaxSize",
                          "The maximum number of packets accepted by this queue disc.",
                          QueueSizeValue(QueueSize("1000p")),
                          MakeQueueSizeAccessor(&QueueDisc::SetMaxSize, &QueueDisc::GetMaxSize),
                          MakeQueueSizeChecker());
    return tid;
}

DsRateBasedShaperDispatcher::DsRateBasedShaperDispatcher()
    : QueueDisc(QueueDiscSizePolicy::MULTIPLE_QUEUES, QueueSizeUnit::PACKETS)
{
    NS_LOG_FUNCTION(this);
}

DsRateBasedShaperDispatcher::~DsRateBasedShaperDispatcher()
{
    NS_LOG_FUNCTION(this);
}

void
DsRateBasedShaperDispatcher::ConfigureTin(uint32_t slot,
                                          uint64_t rateBps,
                                          int32_t overhead,
                                          uint32_t mpu,
                                          RateBasedTinClock::FramingMode framing)
{
    NS_LOG_FUNCTION(this << slot << rateBps);
    if (m_tinClocks.size() <= slot)
    {
        m_tinClocks.resize(slot + 1);
    }
    m_tinClocks[slot].rateBps = rateBps;
    m_tinClocks[slot].overhead = overhead;
    m_tinClocks[slot].mpu = mpu;
    m_tinClocks[slot].framing = framing;
}

void
DsRateBasedShaperDispatcher::ConfigureGlobal(uint64_t rateBps)
{
    NS_LOG_FUNCTION(this << rateBps);
    m_globalClock.rateBps = rateBps;
}

void
DsRateBasedShaperDispatcher::SetEnableLlq(bool enabled)
{
    NS_LOG_FUNCTION(this << enabled);
    m_enableLlq = enabled;
}

void
DsRateBasedShaperDispatcher::SetDscpToSlot(uint8_t dscp, uint32_t slot)
{
    NS_LOG_FUNCTION(this << static_cast<uint32_t>(dscp) << slot);
    NS_ASSERT_MSG(dscp < 64, "DSCP codepoint out of 6-bit range");
    m_dscpToSlot[dscp] = static_cast<uint8_t>(slot);
}

void
DsRateBasedShaperDispatcher::SetIngressMode(bool enabled)
{
    NS_LOG_FUNCTION(this << enabled);
    m_ingressMode = enabled;
}

bool
DsRateBasedShaperDispatcher::GetIngressMode() const
{
    return m_ingressMode;
}

uint64_t
DsRateBasedShaperDispatcher::GetTinBytesCharged(uint32_t slot) const
{
    if (slot >= m_bytesCharged.size())
    {
        return 0;
    }
    return m_bytesCharged[slot];
}

void
DsRateBasedShaperDispatcher::SetAutorateHook(std::shared_ptr<DsCakeLinuxAutorateHook> hook)
{
    NS_LOG_FUNCTION(this);
    m_autorateHook = std::move(hook);
}

int32_t
DsRateBasedShaperDispatcher::ClassifyByDscp(Ptr<QueueDiscItem> item) const
{
    Ptr<Ipv4QueueDiscItem> ipv4Item = DynamicCast<Ipv4QueueDiscItem>(item);
    if (!ipv4Item)
    {
        return 0; // non-IPv4 item: dispatch to tin 0
    }
    const uint8_t dscp = static_cast<uint8_t>(ipv4Item->GetHeader().GetDscp());
    if (dscp >= 64)
    {
        return 0;
    }
    return static_cast<int32_t>(m_dscpToSlot[dscp]);
}

bool
DsRateBasedShaperDispatcher::DoEnqueue(Ptr<QueueDiscItem> item)
{
    NS_LOG_FUNCTION(this << item);

    // Classify the item to a tin slot. When this dispatcher is installed
    // standalone (DsCakeHelper::BuildAndInstall), classification reads
    // the IPv4 DSCP and indexes a 64-entry slot table populated by the
    // helper for the configured tc-cake profile.
    int32_t slot = ClassifyByDscp(item);
    if (slot < 0 || static_cast<uint32_t>(slot) >= GetNInternalQueues())
    {
        DropBeforeEnqueue(item, "no-classifier-match");
        return false;
    }

    bool tinWasEmpty = (GetInternalQueue(slot)->GetNPackets() == 0);
    bool globalWasEmpty = (GetNPackets() == 0);
    Time now = Simulator::Now();

    bool ok = GetInternalQueue(slot)->Enqueue(item);
    if (!ok)
    {
        // Inner queue overflow. In ingress mode, advance the per-tin and
        // global clocks for the dropped packet, matching Linux
        // sch_cake.c::cake_enqueue calling cake_advance_shaper(..., true)
        // when CAKE_FLAG_INGRESS is set.
        if (m_ingressMode && static_cast<uint32_t>(slot) < m_tinClocks.size())
        {
            const uint32_t adjLen = RateBasedTinClock::ComputeAdjLen(
                item->GetSize(),
                m_tinClocks[slot].overhead,
                m_tinClocks[slot].framing,
                m_tinClocks[slot].mpu);
            m_tinClocks[slot].Charge(adjLen, now);
            m_globalClock.Charge(adjLen, now);
            if (static_cast<uint32_t>(slot) >= m_bytesCharged.size())
            {
                m_bytesCharged.resize(static_cast<uint32_t>(slot) + 1, 0);
            }
            m_bytesCharged[slot] += adjLen;
        }
        return false;
    }

    if (tinWasEmpty && static_cast<uint32_t>(slot) < m_tinClocks.size())
    {
        m_tinClocks[slot].OnEnqueueIdleReset(now);
    }
    if (globalWasEmpty)
    {
        m_globalClock.OnEnqueueIdleReset(now);
    }

    // Notify the Linux-faithful autorate hook so it can maintain its
    // EWMA state across the arrival stream. The DynamicCast is only
    // needed when the caller passes a base-class pointer; SetAutorateHook
    // already stores a typed pointer, so this is a direct call.
    if (m_autorateHook != nullptr)
    {
        const uint32_t adjLen = RateBasedTinClock::ComputeAdjLen(
            item->GetSize(),
            static_cast<uint32_t>(slot) < m_tinClocks.size()
                ? m_tinClocks[slot].overhead
                : 0,
            static_cast<uint32_t>(slot) < m_tinClocks.size()
                ? m_tinClocks[slot].framing
                : RateBasedTinClock::FramingMode::NoAtm,
            static_cast<uint32_t>(slot) < m_tinClocks.size()
                ? m_tinClocks[slot].mpu
                : 0U);
        m_autorateHook->OnEnqueue(adjLen, now);
    }

    MaybeArmSelfWake();
    return true;
}

Ptr<QueueDiscItem>
DsRateBasedShaperDispatcher::DoDequeue()
{
    NS_LOG_FUNCTION(this);
    Time now = Simulator::Now();
    Time minWake = Time::Max();
    bool anyBacklog = false;

    auto isEligible = [&](uint32_t slot) -> bool {
        if (slot >= m_tinClocks.size())
        {
            return true; // unshaped tin: only global gates
        }
        Time gate = std::max(m_tinClocks[slot].tNext, m_globalClock.tNext);
        if (now >= gate)
        {
            return true;
        }
        if (gate < minWake)
        {
            minWake = gate;
        }
        return false;
    };

    auto tryDequeue = [&](uint32_t slot) -> Ptr<QueueDiscItem> {
        if (slot >= GetNInternalQueues())
        {
            return nullptr;
        }
        if (GetInternalQueue(slot)->GetNPackets() == 0)
        {
            return nullptr;
        }
        anyBacklog = true;
        if (!isEligible(slot))
        {
            return nullptr;
        }
        Ptr<QueueDiscItem> item = GetInternalQueue(slot)->Dequeue();
        if (item == nullptr)
        {
            return nullptr;
        }
        uint32_t adjLen = RateBasedTinClock::ComputeAdjLen(
            item->GetSize(),
            slot < m_tinClocks.size() ? m_tinClocks[slot].overhead : 0,
            slot < m_tinClocks.size() ? m_tinClocks[slot].framing
                                      : RateBasedTinClock::FramingMode::NoAtm,
            slot < m_tinClocks.size() ? m_tinClocks[slot].mpu : 0);
        if (slot < m_tinClocks.size())
        {
            m_tinClocks[slot].Charge(adjLen, now);
            if (slot >= m_bytesCharged.size())
            {
                m_bytesCharged.resize(slot + 1, 0);
            }
            m_bytesCharged[slot] += adjLen;
        }
        m_globalClock.Charge(adjLen, now);
        return item;
    };

    if (m_enableLlq)
    {
        Ptr<QueueDiscItem> item = tryDequeue(0);
        if (item != nullptr)
        {
            return item;
        }
    }

    // DRR rotation across remaining tins.
    uint32_t nQ = GetNInternalQueues();
    uint32_t startSlot = m_enableLlq ? 1 : 0;
    if (nQ > startSlot)
    {
        for (uint32_t i = 0; i < nQ - startSlot; ++i)
        {
            uint32_t slot = startSlot + ((m_drrCursor + i) % (nQ - startSlot));
            Ptr<QueueDiscItem> item = tryDequeue(slot);
            if (item != nullptr)
            {
                m_drrCursor = (slot + 1) % nQ;
                return item;
            }
        }
    }

    // No eligible tin. If anything is backlogged, schedule SelfWake.
    if (anyBacklog && minWake != Time::Max())
    {
        m_selfWakeEvent.Cancel();
        Time delay = minWake > now ? minWake - now : Seconds(0);
        m_selfWakeEvent =
            Simulator::Schedule(delay, &DsRateBasedShaperDispatcher::OnSelfWake, this);
    }
    return nullptr;
}

Ptr<const QueueDiscItem>
DsRateBasedShaperDispatcher::DoPeek()
{
    if (GetNInternalQueues() == 0)
    {
        return nullptr;
    }
    return GetInternalQueue(0)->Peek();
}

bool
DsRateBasedShaperDispatcher::CheckConfig()
{
    NS_LOG_FUNCTION(this);
    if (GetNInternalQueues() == 0)
    {
        // Mirror pfifo-fast-queue-disc.cc: install one DropTailQueue per
        // configured tin (or one queue when no tin clocks are defined,
        // for the besteffort layout).
        const uint32_t numTins =
            std::max<uint32_t>(static_cast<uint32_t>(m_tinClocks.size()), 1u);
        ObjectFactory factory;
        factory.SetTypeId("ns3::DropTailQueue<QueueDiscItem>");
        factory.Set("MaxSize", QueueSizeValue(GetMaxSize()));
        for (uint32_t i = 0; i < numTins; ++i)
        {
            AddInternalQueue(factory.Create<InternalQueue>());
        }
    }
    return true;
}

void
DsRateBasedShaperDispatcher::InitializeParams()
{
}

void
DsRateBasedShaperDispatcher::OnSelfWake()
{
    NS_LOG_FUNCTION(this);
    Run();
}

void
DsRateBasedShaperDispatcher::MaybeArmSelfWake()
{
    if (m_selfWakeEvent.IsPending())
    {
        return;
    }
    Time now = Simulator::Now();
    Time minWake = Time::Max();
    for (uint32_t slot = 0; slot < GetNInternalQueues(); ++slot)
    {
        if (GetInternalQueue(slot)->GetNPackets() == 0)
        {
            continue;
        }
        Time gate = (slot < m_tinClocks.size())
                        ? std::max(m_tinClocks[slot].tNext, m_globalClock.tNext)
                        : m_globalClock.tNext;
        if (gate < minWake)
        {
            minWake = gate;
        }
    }
    if (minWake != Time::Max() && minWake > now)
    {
        m_selfWakeEvent =
            Simulator::Schedule(minWake - now,
                                &DsRateBasedShaperDispatcher::OnSelfWake,
                                this);
    }
}

} // namespace diffserv
} // namespace ns3
