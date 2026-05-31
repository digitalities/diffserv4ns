/*
 * Copyright (C) 2026 Sergio Andreozzi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#ifndef NS3_DIFFSERV_DS_CAKE_INPUT_JITTER_SHIM_H
#define NS3_DIFFSERV_DS_CAKE_INPUT_JITTER_SHIM_H

#include "ns3/event-id.h"
#include "ns3/nstime.h"
#include "ns3/queue-disc.h"
#include "ns3/random-variable-stream.h"

#include <deque>

namespace ns3
{
namespace diffserv
{

/**
 * Pass-through queue disc that adds a small per-packet random delay
 * before forwarding to an inner queue disc. Used as an experimental
 * probe of CAKE host-isolation's sensitivity to input timing jitter
 * (Probe #7, 2026-05-21).
 *
 * On DoEnqueue, items are pushed into a FIFO internal queue with a
 * per-packet release time `max(now + U(0, MaxJitter),
 * prev_releaseAt + U(0, MaxJitter))`. The FIFO clamp matches NAPI-like
 * semantics: packets exit in arrival order with bounded per-packet
 * additional delay; no overtaking. On DoDequeue, due items are
 * forwarded to the inner qdisc and one packet is dequeued from inner.
 *
 * Set the inner qdisc via SetInnerQdisc() before traffic; if MaxJitter
 * is zero, the shim bypasses the buffer and forwards immediately
 * (zero-overhead pass-through).
 */
class DsCakeInputJitterShim : public QueueDisc
{
  public:
    static TypeId GetTypeId();
    DsCakeInputJitterShim();
    ~DsCakeInputJitterShim() override;

    /// Inner queue disc receiving released items. Must be set before traffic.
    void SetInnerQdisc(Ptr<QueueDisc> inner);
    /// Maximum jitter applied per packet. 0 = bypass (immediate forward).
    void SetMaxJitter(Time max);

  protected:
    bool DoEnqueue(Ptr<QueueDiscItem> item) override;
    Ptr<QueueDiscItem> DoDequeue() override;
    Ptr<const QueueDiscItem> DoPeek() override;
    bool CheckConfig() override;
    void InitializeParams() override;
    void DoDispose() override;

  private:
    Ptr<QueueDisc> m_inner;
    Time m_maxJitter{NanoSeconds(0)};
    Ptr<UniformRandomVariable> m_rng;

    /// Per-packet release timestamps. The N-th entry is the release time
    /// of the N-th item in GetInternalQueue(0). Each entry is an
    /// independent sample (now + U(0, maxJitter)) — no FIFO clamp.
    /// HOL ordering of the internal FIFO queue means a later-arrived
    /// item with smaller sampled delay still waits behind earlier
    /// items, but does NOT inherit their delay (no accumulation).
    std::deque<Time> m_releaseTimes;

    /// Pending wake-up to run the queue disc when the next item becomes
    /// due. Cancelled and rescheduled when an earlier-due item arrives.
    EventId m_wakeEvent;

    /// Flush items whose releaseAt <= now from the internal queue to
    /// the inner qdisc.
    void FlushDueItems();

    /// Schedule a Run() at the front item's release time. No-op if no
    /// items are pending or a wake event is already scheduled.
    void RescheduleWake();

    /// EventId handler — calls Run() on this qdisc to trigger dequeue.
    void OnWake();
};

} // namespace diffserv
} // namespace ns3

#endif // NS3_DIFFSERV_DS_CAKE_INPUT_JITTER_SHIM_H
