/*
 * Copyright (C) 2026 Sergio Andreozzi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#ifndef NS3_DIFFSERV_DS_CAKE_PER_FLOW_DELAY_SHIM_H
#define NS3_DIFFSERV_DS_CAKE_PER_FLOW_DELAY_SHIM_H

#include "ns3/nstime.h"
#include "ns3/queue-disc.h"
#include "ns3/random-variable-stream.h"

#include <map>
#include <tuple>

namespace ns3
{
namespace diffserv
{

/**
 * Pass-through queue disc that adds a sustained per-flow delay before
 * forwarding to an inner queue disc. Each flow's delay is sampled from
 * U(0, MaxFlowDelay) on first observation of that flow's 5-tuple, then
 * re-used for every subsequent packet of that flow.
 *
 * Unlike per-packet random jitter, this delay is constant per flow for
 * the lifetime of the simulation. It introduces sustained per-flow
 * phase decoherence on the forward path: flows do not share a single
 * coherent RTT, so their TCP ACK clocks tick on independent grids.
 *
 * Set the inner qdisc via SetInnerQdisc() before traffic; if
 * MaxFlowDelay is zero, the shim bypasses the buffer and forwards
 * immediately (zero-overhead pass-through).
 *
 * The internal queue is used solely to satisfy the ns-3
 * `nTotalReceivedPackets == nDroppedBeforeEnqueue + nTotalEnqueuedPackets`
 * invariant: items are enqueued (incrementing the trace counter) and
 * immediately dequeued. Each item's actual delivery to the inner qdisc
 * is scheduled independently via Simulator::Schedule, so packets do not
 * HOL-block each other — a fast-delay packet is delivered at its own
 * scheduled time regardless of what slower-delay items preceded it.
 */
class DsCakePerFlowDelayShim : public QueueDisc
{
  public:
    static TypeId GetTypeId();
    DsCakePerFlowDelayShim();
    ~DsCakePerFlowDelayShim() override;

    /// Inner queue disc receiving items after their per-flow delay
    /// elapses. Must be set before traffic.
    void SetInnerQdisc(Ptr<QueueDisc> inner);

    /// Upper bound on per-flow delay. Per-flow delays are sampled from
    /// U(0, max). 0 = bypass (immediate forward).
    void SetMaxFlowDelay(Time max);

  protected:
    bool DoEnqueue(Ptr<QueueDiscItem> item) override;
    Ptr<QueueDiscItem> DoDequeue() override;
    Ptr<const QueueDiscItem> DoPeek() override;
    bool CheckConfig() override;
    void InitializeParams() override;
    void DoDispose() override;

  private:
    using FlowKey =
        std::tuple<uint32_t, uint32_t, uint16_t, uint16_t, uint8_t>;

    Ptr<QueueDisc> m_inner;
    Time m_maxFlowDelay{NanoSeconds(0)};
    Ptr<UniformRandomVariable> m_rng;
    std::map<FlowKey, Time> m_perFlowDelay;

    FlowKey ExtractFlowKey(Ptr<const QueueDiscItem> item) const;
    Time GetOrSampleFlowDelay(const FlowKey& key);
    void DeliverItem(Ptr<QueueDiscItem> item);
};

} // namespace diffserv
} // namespace ns3

#endif // NS3_DIFFSERV_DS_CAKE_PER_FLOW_DELAY_SHIM_H
