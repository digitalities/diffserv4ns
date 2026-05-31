/*
 * Copyright (C) 2001-2026 Sergio Andreozzi
 * Copyright (C) 2000 Nortel Networks
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Ported from DiffServ4NS dsCore.{h,cc} class coreQueue (2001).
 */

#include "diffserv-core-queue-disc.h"

#include "ds-l4s-queue-disc.h"
#include "ds-red-sub-queue.h"
#include "queue-stats-provider.h"

#include "ns3/log.h"
#include "ns3/queue-disc.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DiffServCoreQueueDisc");

namespace diffserv
{

NS_OBJECT_ENSURE_REGISTERED(DiffServCoreQueueDisc);

TypeId
DiffServCoreQueueDisc::GetTypeId()
{
    static TypeId tid = TypeId("ns3::diffserv::DiffServCoreQueueDisc")
                            .SetParent<QueueDisc>()
                            .SetGroupName("DiffServ")
                            .AddConstructor<DiffServCoreQueueDisc>();
    return tid;
}

DiffServCoreQueueDisc::DiffServCoreQueueDisc()
    : QueueDisc(QueueDiscSizePolicy::NO_LIMITS)
{
    NS_LOG_FUNCTION(this);
}

DiffServCoreQueueDisc::~DiffServCoreQueueDisc()
{
    NS_LOG_FUNCTION(this);
}

void
DiffServCoreQueueDisc::SetInnerDisc(Ptr<QueueDisc> inner)
{
    NS_LOG_FUNCTION(this << inner);
    NS_ASSERT_MSG(GetNQueueDiscClasses() == 0, "SetInnerDisc must be called before Initialize");
    m_inner = inner;
}

Ptr<QueueDisc>
DiffServCoreQueueDisc::GetInnerDisc() const
{
    return m_inner;
}

void
DiffServCoreQueueDisc::EnsureDefaultInner()
{
    if (!m_inner)
    {
        m_inner = CreateObject<DsRedQueueDisc>();
    }
    if (GetNQueueDiscClasses() == 0)
    {
        Ptr<QueueDiscClass> cls = CreateObject<QueueDiscClass>();
        cls->SetQueueDisc(m_inner);
        AddQueueDiscClass(cls);
    }
}

// --- Inner configuration ---
//
// Callers configure the inner via its own API before `SetInnerDisc`.

// --- Runtime probes via the QueueStatsProvider interface ---

Ptr<DsScheduler>
DiffServCoreQueueDisc::GetScheduler() const
{
    if (!m_inner)
    {
        return nullptr;
    }
    Ptr<DsRedQueueDisc> red = DynamicCast<DsRedQueueDisc>(m_inner);
    return red ? red->GetScheduler() : nullptr;
}

uint32_t
DiffServCoreQueueDisc::GetNumQueues() const
{
    auto* stats = dynamic_cast<QueueStatsProvider*>(PeekPointer(m_inner));
    return stats ? stats->GetNumQueues() : 0;
}

int
DiffServCoreQueueDisc::GetVirtualQueueLen(uint32_t queue, uint32_t prec) const
{
    auto* stats = dynamic_cast<QueueStatsProvider*>(PeekPointer(m_inner));
    return stats ? stats->GetQueueLen(queue, prec) : 0;
}

void
DiffServCoreQueueDisc::PrintStats() const
{
    if (!m_inner)
    {
        return;
    }
    Ptr<DsRedQueueDisc> red = DynamicCast<DsRedQueueDisc>(m_inner);
    if (red)
    {
        red->PrintStats();
    }
}

void
DiffServCoreQueueDisc::PrintPhbTable() const
{
    if (!m_inner)
    {
        return;
    }
    Ptr<DsRedQueueDisc> red = DynamicCast<DsRedQueueDisc>(m_inner);
    if (red)
    {
        red->PrintPhbTable();
    }
}

int64_t
DiffServCoreQueueDisc::AssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(this << stream);
    int64_t consumed = 0;
    if (m_inner)
    {
        // Branch on inner type; see edge's AssignStreams for the
        // rationale. The core cascade is the same shape minus the
        // meter-slot cascade (core has no meters).
        Ptr<DsRedQueueDisc> red = DynamicCast<DsRedQueueDisc>(m_inner);
        Ptr<DsL4sQueueDisc> l4s = DynamicCast<DsL4sQueueDisc>(m_inner);
        if (red)
        {
            for (uint32_t i = 0; i < red->GetNQueueDiscClasses(); ++i)
            {
                Ptr<DsRedSubQueue> sub =
                    DynamicCast<DsRedSubQueue>(red->GetQueueDiscClass(i)->GetQueueDisc());
                if (sub)
                {
                    sub->AssignStreams(stream + consumed);
                    ++consumed;
                }
            }
        }
        else if (l4s)
        {
            consumed += l4s->AssignStreams(stream + consumed);
        }
    }
    return consumed;
}

// --- QueueDisc overrides ---

bool
DiffServCoreQueueDisc::DoEnqueue(Ptr<QueueDiscItem> item)
{
    NS_LOG_FUNCTION(this << item);
    // Pure delegation: core is BA-only, no classification or metering.
    // Inner reads the IPv4 header DSCP (no DiffServDscpTag in play) via
    // its own DoEnqueue path. Drop aggregation cascades via the
    // AddQueueDiscClass automatic drop-functor hookup.
    return m_inner->Enqueue(item);
}

Ptr<QueueDiscItem>
DiffServCoreQueueDisc::DoDequeue()
{
    NS_LOG_FUNCTION(this);
    return m_inner ? m_inner->Dequeue() : nullptr;
}

Ptr<const QueueDiscItem>
DiffServCoreQueueDisc::DoPeek()
{
    NS_LOG_FUNCTION(this);
    return m_inner ? m_inner->Peek() : nullptr;
}

bool
DiffServCoreQueueDisc::CheckConfig()
{
    NS_LOG_FUNCTION(this);

    if (GetNInternalQueues() > 0)
    {
        NS_LOG_ERROR("DiffServCoreQueueDisc must not have internal queues");
        return false;
    }

    EnsureDefaultInner();

    NS_ASSERT_MSG(GetNQueueDiscClasses() == 1,
                  "DiffServCoreQueueDisc requires exactly one QueueDiscClass "
                  "child (the inner disc at idx 0); got "
                      << GetNQueueDiscClasses());
    return GetNQueueDiscClasses() == 1;
}

void
DiffServCoreQueueDisc::InitializeParams()
{
    NS_LOG_FUNCTION(this);
}

void
DiffServCoreQueueDisc::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_inner = nullptr;
    QueueDisc::DoDispose();
}

} // namespace diffserv
} // namespace ns3
