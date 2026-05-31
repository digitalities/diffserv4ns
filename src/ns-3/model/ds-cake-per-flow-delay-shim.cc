/*
 * Copyright (C) 2026 Sergio Andreozzi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include "ds-cake-per-flow-delay-shim.h"

#include "ns3/double.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/ipv4-header.h"
#include "ns3/ipv4-queue-disc-item.h"
#include "ns3/log.h"
#include "ns3/object-factory.h"
#include "ns3/queue-size.h"
#include "ns3/simulator.h"
#include "ns3/tcp-header.h"
#include "ns3/udp-header.h"

namespace ns3
{
namespace diffserv
{

NS_LOG_COMPONENT_DEFINE("DsCakePerFlowDelayShim");

NS_OBJECT_ENSURE_REGISTERED(DsCakePerFlowDelayShim);

TypeId
DsCakePerFlowDelayShim::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::diffserv::DsCakePerFlowDelayShim")
            .SetParent<QueueDisc>()
            .SetGroupName("Diffserv")
            .AddConstructor<DsCakePerFlowDelayShim>();
    return tid;
}

DsCakePerFlowDelayShim::DsCakePerFlowDelayShim()
    : QueueDisc(QueueDiscSizePolicy::NO_LIMITS)
{
    NS_LOG_FUNCTION(this);
}

DsCakePerFlowDelayShim::~DsCakePerFlowDelayShim()
{
    NS_LOG_FUNCTION(this);
}

void
DsCakePerFlowDelayShim::SetInnerQdisc(Ptr<QueueDisc> inner)
{
    m_inner = inner;
}

void
DsCakePerFlowDelayShim::SetMaxFlowDelay(Time max)
{
    m_maxFlowDelay = max;
}

void
DsCakePerFlowDelayShim::InitializeParams()
{
    m_rng = CreateObject<UniformRandomVariable>();
    m_rng->SetAttribute("Min", DoubleValue(0.0));
    m_rng->SetAttribute("Max", DoubleValue(1.0));
}

bool
DsCakePerFlowDelayShim::CheckConfig()
{
    if (GetNInternalQueues() == 0)
    {
        ObjectFactory factory;
        factory.SetTypeId("ns3::DropTailQueue<QueueDiscItem>");
        factory.Set("MaxSize", QueueSizeValue(QueueSize("1000000p")));
        AddInternalQueue(factory.Create<QueueDisc::InternalQueue>());
    }
    return true;
}

void
DsCakePerFlowDelayShim::DoDispose()
{
    m_perFlowDelay.clear();
    m_inner = nullptr;
    m_rng = nullptr;
    QueueDisc::DoDispose();
}

DsCakePerFlowDelayShim::FlowKey
DsCakePerFlowDelayShim::ExtractFlowKey(Ptr<const QueueDiscItem> item) const
{
    Ptr<const Ipv4QueueDiscItem> ipv4Item =
        DynamicCast<const Ipv4QueueDiscItem>(item);
    if (!ipv4Item)
    {
        return std::make_tuple(0u, 0u, uint16_t{0}, uint16_t{0}, uint8_t{0});
    }
    const Ipv4Header& header = ipv4Item->GetHeader();
    const uint32_t src = header.GetSource().Get();
    const uint32_t dst = header.GetDestination().Get();
    const uint8_t proto = header.GetProtocol();

    uint16_t sport = 0;
    uint16_t dport = 0;
    Ptr<Packet> pkt = item->GetPacket();
    if (pkt && pkt->GetSize() >= 4)
    {
        if (proto == 6) // TCP
        {
            TcpHeader th;
            if (pkt->PeekHeader(th))
            {
                sport = th.GetSourcePort();
                dport = th.GetDestinationPort();
            }
        }
        else if (proto == 17) // UDP
        {
            UdpHeader uh;
            if (pkt->PeekHeader(uh))
            {
                sport = uh.GetSourcePort();
                dport = uh.GetDestinationPort();
            }
        }
    }
    return std::make_tuple(src, dst, sport, dport, proto);
}

Time
DsCakePerFlowDelayShim::GetOrSampleFlowDelay(const FlowKey& key)
{
    auto it = m_perFlowDelay.find(key);
    if (it != m_perFlowDelay.end())
    {
        return it->second;
    }
    const int64_t maxNs = m_maxFlowDelay.GetNanoSeconds();
    const int64_t sampleNs =
        static_cast<int64_t>(static_cast<double>(maxNs) * m_rng->GetValue());
    const Time sampled = NanoSeconds(sampleNs);
    m_perFlowDelay[key] = sampled;
    return sampled;
}

bool
DsCakePerFlowDelayShim::DoEnqueue(Ptr<QueueDiscItem> item)
{
    NS_ASSERT_MSG(m_inner, "Inner qdisc must be set before traffic");
    auto iq = GetInternalQueue(0);
    if (!iq->Enqueue(item))
    {
        return false;
    }
    iq->Dequeue();

    const FlowKey key = ExtractFlowKey(item);
    const Time flowDelay = GetOrSampleFlowDelay(key);
    Simulator::Schedule(flowDelay,
                        &DsCakePerFlowDelayShim::DeliverItem,
                        this,
                        item);
    return true;
}

void
DsCakePerFlowDelayShim::DeliverItem(Ptr<QueueDiscItem> item)
{
    if (m_inner)
    {
        m_inner->Enqueue(item);
        Run();
    }
}

Ptr<QueueDiscItem>
DsCakePerFlowDelayShim::DoDequeue()
{
    return m_inner ? m_inner->Dequeue() : nullptr;
}

Ptr<const QueueDiscItem>
DsCakePerFlowDelayShim::DoPeek()
{
    return m_inner ? m_inner->Peek() : nullptr;
}

} // namespace diffserv
} // namespace ns3
