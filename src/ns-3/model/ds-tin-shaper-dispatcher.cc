/*
 * Copyright (C) 2026 Sergio Andreozzi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 */

#include "ds-tin-shaper-dispatcher.h"

#include "diffserv-edge-queue-disc.h"

#include "ns3/log.h"
#include "ns3/queue-disc.h"
#include "ns3/simulator.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DsTinShaperDispatcher");

namespace diffserv
{

NS_OBJECT_ENSURE_REGISTERED(DsTinShaperDispatcher);

TypeId
DsTinShaperDispatcher::GetTypeId()
{
    static TypeId tid = TypeId("ns3::diffserv::DsTinShaperDispatcher")
                            .SetParent<DsSlotDispatcher>()
                            .SetGroupName("DiffServ")
                            .AddConstructor<DsTinShaperDispatcher>();
    return tid;
}

DsTinShaperDispatcher::DsTinShaperDispatcher()
{
    NS_LOG_FUNCTION(this);
}

void
DsTinShaperDispatcher::SetQuantum(uint32_t slot, uint32_t bytes)
{
    NS_ASSERT_MSG(slot < DiffServEdgeQueueDisc::kMaxInnerSlots,
                  "DsTinShaperDispatcher::SetQuantum: slot " << slot << " out of range");
    NS_ASSERT_MSG(bytes > 0, "DsTinShaperDispatcher::SetQuantum: quantum must be > 0");
    m_quantum[slot] = bytes;
}

uint32_t
DsTinShaperDispatcher::GetQuantum(uint32_t slot) const
{
    NS_ASSERT_MSG(slot < DiffServEdgeQueueDisc::kMaxInnerSlots,
                  "DsTinShaperDispatcher::GetQuantum: slot " << slot << " out of range");
    return m_quantum[slot];
}

void
DsTinShaperDispatcher::SetRateCap(uint32_t slot, uint64_t rateBps, uint64_t burstBytes)
{
    NS_ASSERT_MSG(slot < DiffServEdgeQueueDisc::kMaxInnerSlots,
                  "DsTinShaperDispatcher::SetRateCap: slot " << slot << " out of range");
    if (rateBps > 0)
    {
        NS_ASSERT_MSG(burstBytes > 0,
                      "DsTinShaperDispatcher::SetRateCap: burstBytes must be > 0 when rateBps > 0 "
                      "(slot "
                          << slot << ")");
    }
    m_tokenBuckets[slot].Configure(rateBps, burstBytes, Simulator::Now());
}

uint64_t
DsTinShaperDispatcher::GetRateCapBps(uint32_t slot) const
{
    NS_ASSERT_MSG(slot < DiffServEdgeQueueDisc::kMaxInnerSlots,
                  "DsTinShaperDispatcher::GetRateCapBps: slot " << slot << " out of range");
    return m_tokenBuckets[slot].rateBps;
}

int32_t
DsTinShaperDispatcher::SelectDequeueSlot(DiffServEdgeQueueDisc* edge)
{
    NS_ASSERT_MSG(edge, "DsTinShaperDispatcher::SelectDequeueSlot: edge must be non-null");
    const uint32_t kMax = DiffServEdgeQueueDisc::kMaxInnerSlots;
    for (uint32_t tries = 0; tries < kMax; ++tries)
    {
        const uint32_t i = (m_activeSlot + tries) % kMax;
        Ptr<QueueDisc> inner = edge->GetInnerDiscAt(i);
        if (!inner || inner->GetNPackets() == 0)
        {
            // Idle slot. Skip without crediting so it does not accrue
            // save-up that would burst unfairly on refill.
            continue;
        }
        // Token gate: if the slot has a rate cap configured and the
        // head packet's wire bytes would overdraw the bucket, skip
        // the slot without crediting deficit. The bucket refills with
        // elapsed time so the slot becomes serviceable again
        // automatically. When `rateBps == 0` (the default) the bucket
        // is disabled and `HasTokensFor` returns true unconditionally,
        // making the gate a no-op.
        Ptr<const QueueDiscItem> head = inner->Peek();
        if (!head)
        {
            // Inner refused to peek even though GetNPackets > 0. The
            // canonical case is path-gamma per-tin shaping: the inner is
            // a TbfQueueDisc whose token bucket is exhausted; it returns
            // null from Peek/Dequeue until tokens accumulate. Skip the
            // slot so DRR moves on to a serviceable neighbour rather
            // than wasting the round on a slot we cannot drain.
            continue;
        }
        if (!m_tokenBuckets[i].HasTokensFor(head->GetSize(), Simulator::Now()))
        {
            continue;
        }
        // Round-start credit: top up whenever deficit has exhausted
        // (<= 0). Allowing the deficit to go negative after a dequeue is
        // the canonical FqCoDel DRR pattern (see Linux fq_codel_dequeue);
        // the next round adds quantum, restoring eligibility regardless
        // of where the previous round happened to land.
        if (m_deficit[i] <= 0)
        {
            m_deficit[i] += static_cast<int32_t>(m_quantum[i]);
        }
        // Slot now has positive deficit and at least one packet — serve.
        m_activeSlot = i;
        return static_cast<int32_t>(i);
    }
    return -1;
}

int32_t
DsTinShaperDispatcher::PeekSlot(DiffServEdgeQueueDisc* edge)
{
    NS_ASSERT_MSG(edge, "DsTinShaperDispatcher::PeekSlot: edge must be non-null");
    const uint32_t kMax = DiffServEdgeQueueDisc::kMaxInnerSlots;
    for (uint32_t tries = 0; tries < kMax; ++tries)
    {
        const uint32_t i = (m_activeSlot + tries) % kMax;
        Ptr<QueueDisc> inner = edge->GetInnerDiscAt(i);
        if (!inner || inner->GetNPackets() == 0)
        {
            continue;
        }
        // Mirror the SelectDequeueSlot token gate so peek and select
        // agree on which slot would actually serve next. HasTokensFor
        // is `const` (no bucket mutation) — the side-effect-free peek
        // contract enforced by the SlotDispatcherByteIdentityTest peek-side-effect-free assertion
        // is preserved.
        Ptr<const QueueDiscItem> head = inner->Peek();
        if (!head)
        {
            // Inner refused to peek even though GetNPackets > 0. The
            // canonical case is path-gamma per-tin shaping: the inner is
            // a TbfQueueDisc whose token bucket is exhausted; it returns
            // null from Peek/Dequeue until tokens accumulate. Skip the
            // slot so DRR moves on to a serviceable neighbour rather
            // than wasting the round on a slot we cannot drain.
            continue;
        }
        if (!m_tokenBuckets[i].HasTokensFor(head->GetSize(), Simulator::Now()))
        {
            continue;
        }
        // The first non-empty + token-eligible slot from the cursor is
        // what `SelectDequeueSlot` would serve next (after crediting if
        // the deficit has exhausted). The class member m_deficit[i] is
        // NOT mutated; this is the side-effect-free contract enforced by
        // the SlotDispatcherByteIdentityTest peek-side-effect-free assertion.
        return static_cast<int32_t>(i);
    }
    return -1;
}

void
DsTinShaperDispatcher::OnEnqueue(uint32_t slot,
                                 Ptr<QueueDiscItem> item,
                                 DiffServEdgeQueueDisc* /*edge*/)
{
    if (item && slot < DiffServEdgeQueueDisc::kMaxInnerSlots)
    {
        m_bytesEnqueued[slot] += item->GetSize();
    }
}

void
DsTinShaperDispatcher::OnDequeue(uint32_t slot,
                                 Ptr<QueueDiscItem> item,
                                 DiffServEdgeQueueDisc* edge)
{
    NS_ASSERT_MSG(item, "DsTinShaperDispatcher::OnDequeue: item must be non-null");
    NS_ASSERT_MSG(edge, "DsTinShaperDispatcher::OnDequeue: edge must be non-null");
    NS_ASSERT_MSG(slot < DiffServEdgeQueueDisc::kMaxInnerSlots,
                  "DsTinShaperDispatcher::OnDequeue: slot " << slot << " out of range");
    // S-17.32 — track wire-bytes drained alongside the existing
    // token-bucket / deficit bookkeeping.
    m_bytesDequeued[slot] += item->GetSize();
    // Charge the token bucket FIRST. When `rateBps == 0` the bucket
    // is disabled and Charge is a no-op. The deficit decrement is
    // independent.
    m_tokenBuckets[slot].Charge(item->GetSize(), Simulator::Now());
    m_deficit[slot] -= static_cast<int32_t>(item->GetSize());

    Ptr<QueueDisc> inner = edge->GetInnerDiscAt(slot);
    const uint32_t kMax = DiffServEdgeQueueDisc::kMaxInnerSlots;
    if (!inner || inner->GetNPackets() == 0)
    {
        // Drain-to-empty: reset deficit so an idle tin does not accrue
        // save-up credit across the quiet period. Leave the cursor on
        // this slot — when work returns it (the typical single-active-
        // tin case under TCP), the next SelectDequeueSlot starts here
        // rather than walking through every empty slot first.
        m_deficit[slot] = 0;
        return;
    }
    if (m_deficit[slot] <= 0)
    {
        // Round complete for this slot — yield the cursor so other slots
        // get a turn before this one is re-credited. The deficit is
        // intentionally left non-positive; the next visit will add
        // quantum and restore eligibility.
        m_activeSlot = (slot + 1) % kMax;
    }
}

DsTinStats
DsTinShaperDispatcher::GetTinStats(uint32_t tinIdx,
                                   const DiffServEdgeQueueDisc* edge) const
{
    DsTinStats out{};
    if (tinIdx >= DiffServEdgeQueueDisc::kMaxInnerSlots)
    {
        return out;
    }
    out.bytesEnqueued = m_bytesEnqueued[tinIdx];
    out.bytesDequeued = m_bytesDequeued[tinIdx];

    if (edge)
    {
        Ptr<QueueDisc> inner = edge->GetInnerDiscAt(tinIdx);
        if (inner)
        {
            const auto& s = inner->GetStats();
            out.drops = s.nTotalDroppedPackets;
            out.marks = s.nTotalMarkedPackets;
        }
    }
    return out;
}

DsPerFlowStats
DsTinShaperDispatcher::GetPerFlowStats(uint32_t slot,
                                       uint32_t flowId,
                                       const DiffServEdgeQueueDisc* edge) const
{
    DsPerFlowStats out{};
    if (slot >= DiffServEdgeQueueDisc::kMaxInnerSlots || edge == nullptr)
    {
        return out;
    }
    Ptr<QueueDisc> inner = edge->GetInnerDiscAt(slot);
    if (!inner)
    {
        return out;
    }
    if (flowId >= inner->GetNQueueDiscClasses())
    {
        // Inner has no flow classes (flat / non-FQ inner) or flow id
        // is past the live class list. Either way, zero snapshot.
        return out;
    }
    Ptr<QueueDiscClass> qdClass = inner->GetQueueDiscClass(flowId);
    if (!qdClass)
    {
        return out;
    }
    Ptr<QueueDisc> flowQ = qdClass->GetQueueDisc();
    if (!flowQ)
    {
        return out;
    }
    const auto& s = flowQ->GetStats();
    out.bytesEnqueued = s.nTotalEnqueuedBytes;
    out.pktsEnqueued = s.nTotalEnqueuedPackets;
    out.pktsDropped = s.nTotalDroppedPackets;
    out.pktsMarked = s.nTotalMarkedPackets;
    out.bytesRemaining = flowQ->GetNBytes();
    return out;
}

} // namespace diffserv
} // namespace ns3
