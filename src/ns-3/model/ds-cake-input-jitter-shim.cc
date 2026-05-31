/*
 * Copyright (C) 2026 Sergio Andreozzi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include "ds-cake-input-jitter-shim.h"

#include "ns3/double.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/log.h"
#include "ns3/object-factory.h"
#include "ns3/queue-size.h"
#include "ns3/simulator.h"

#include <algorithm>

namespace ns3
{
namespace diffserv
{

NS_LOG_COMPONENT_DEFINE("DsCakeInputJitterShim");

NS_OBJECT_ENSURE_REGISTERED(DsCakeInputJitterShim);

TypeId
DsCakeInputJitterShim::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::diffserv::DsCakeInputJitterShim")
            .SetParent<QueueDisc>()
            .SetGroupName("Diffserv")
            .AddConstructor<DsCakeInputJitterShim>();
    return tid;
}

DsCakeInputJitterShim::DsCakeInputJitterShim()
    : QueueDisc(QueueDiscSizePolicy::NO_LIMITS)
{
    NS_LOG_FUNCTION(this);
}

DsCakeInputJitterShim::~DsCakeInputJitterShim()
{
    NS_LOG_FUNCTION(this);
}

void
DsCakeInputJitterShim::SetInnerQdisc(Ptr<QueueDisc> inner)
{
    m_inner = inner;
}

void
DsCakeInputJitterShim::SetMaxJitter(Time max)
{
    m_maxJitter = max;
}

void
DsCakeInputJitterShim::InitializeParams()
{
    m_rng = CreateObject<UniformRandomVariable>();
    m_rng->SetAttribute("Min", DoubleValue(0.0));
    m_rng->SetAttribute("Max", DoubleValue(1.0));
}

bool
DsCakeInputJitterShim::CheckConfig()
{
    // The shim itself doesn't enforce a packet limit; it relies on the
    // inner qdisc (CAKE) for buffer management. The internal queue here
    // is purely for ns-3's stats invariant (one Enqueue increment per
    // accepted packet) and exists only when jitter is requested.
    if (GetNInternalQueues() == 0)
    {
        ObjectFactory factory;
        factory.SetTypeId("ns3::DropTailQueue<QueueDiscItem>");
        // Sized very large; the shim is meant to delay packets by tens
        // to hundreds of microseconds, never to drop them.
        factory.Set("MaxSize", QueueSizeValue(QueueSize("1000000p")));
        AddInternalQueue(factory.Create<QueueDisc::InternalQueue>());
    }
    return true;
}

void
DsCakeInputJitterShim::DoDispose()
{
    m_wakeEvent.Cancel();
    m_releaseTimes.clear();
    m_inner = nullptr;
    m_rng = nullptr;
    QueueDisc::DoDispose();
}

bool
DsCakeInputJitterShim::DoEnqueue(Ptr<QueueDiscItem> item)
{
    NS_ASSERT_MSG(m_inner, "Inner qdisc must be set before traffic");
    // Always route through the internal queue so ns-3's
    //   nTotalReceivedPackets == nDroppedBeforeEnqueue + nEnqueuedPackets
    // invariant holds (the internal-queue trace callback drives the
    // nTotalEnqueuedPackets increment). The shim's caller installs us
    // only when m_maxJitter > 0; if jitter happens to be zero, every
    // item gets releaseAt == now and OnWake flushes immediately.
    auto iq = GetInternalQueue(0);
    if (!iq->Enqueue(item))
    {
        return false;
    }
    const Time now = Simulator::Now();
    const Time perPacketJitter = m_maxJitter * m_rng->GetValue();
    // Independent per-packet release time. We do NOT clamp to a
    // monotonic FIFO sequence — clamping would accumulate jitter into a
    // rate-limiter when packet inter-arrival is shorter than maxJitter
    // (rate = 1/maxJitter pkt/s). Instead, items leave the internal
    // FIFO queue in arrival order; if a later item's releaseAt is
    // earlier than a still-waiting predecessor's, the later item is
    // HOL-blocked until the predecessor's releaseAt passes. This
    // matches NAPI semantics: random per-packet latency, no
    // overtaking, no accumulation.
    const Time releaseAt = now + perPacketJitter;
    m_releaseTimes.push_back(releaseAt);
    RescheduleWake();
    return true;
}

void
DsCakeInputJitterShim::FlushDueItems()
{
    const Time now = Simulator::Now();
    auto iq = GetInternalQueue(0);
    while (!m_releaseTimes.empty() && m_releaseTimes.front() <= now)
    {
        Ptr<QueueDiscItem> due = iq->Dequeue();
        if (!due)
        {
            break;
        }
        m_releaseTimes.pop_front();
        m_inner->Enqueue(due);
    }
}

void
DsCakeInputJitterShim::RescheduleWake()
{
    if (m_releaseTimes.empty())
    {
        return;
    }
    const Time front = m_releaseTimes.front();
    const Time delta = front - Simulator::Now();
    if (delta <= Time(0))
    {
        // Already overdue; flush immediately on the next dequeue.
        return;
    }
    if (m_wakeEvent.IsPending())
    {
        m_wakeEvent.Cancel();
    }
    m_wakeEvent =
        Simulator::Schedule(delta, &DsCakeInputJitterShim::OnWake, this);
}

void
DsCakeInputJitterShim::OnWake()
{
    FlushDueItems();
    if (!m_releaseTimes.empty())
    {
        RescheduleWake();
    }
    // Re-run the queue disc to push the newly-released items toward the
    // netdevice. Without this nudge, the netdevice may have already
    // gone idle waiting for a packet that's now ready.
    Run();
}

Ptr<QueueDiscItem>
DsCakeInputJitterShim::DoDequeue()
{
    if (m_maxJitter.IsZero())
    {
        return m_inner ? m_inner->Dequeue() : nullptr;
    }
    // Defensive flush in case the netdevice polled between OnWake events.
    FlushDueItems();
    return m_inner ? m_inner->Dequeue() : nullptr;
}

Ptr<const QueueDiscItem>
DsCakeInputJitterShim::DoPeek()
{
    return m_inner ? m_inner->Peek() : nullptr;
}

} // namespace diffserv
} // namespace ns3
