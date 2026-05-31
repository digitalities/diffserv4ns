/*
 * Copyright (C) 2026 Sergio Andreozzi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Across-slot dispatch strategy for DiffServEdgeQueueDisc — plug-in
 * point for non-strict-priority scheduling across DSCP-keyed slots.
 *
 */

#ifndef NS3_DIFFSERV_DS_SLOT_DISPATCHER_H
#define NS3_DIFFSERV_DS_SLOT_DISPATCHER_H

#include "ns3/object.h"
#include "ns3/ptr.h"

#include <cstdint>

namespace ns3
{

class QueueDiscItem;

namespace diffserv
{

class DiffServEdgeQueueDisc;

/**
 * @ingroup diffserv
 *
 * @brief Per-tin counter snapshot — load-bearing fields for the
 * `tc -s qdisc show` equivalent on a CAKE-composed edge.
 *
 * Fields are wire-byte accumulators (`GetSize()` semantics, including
 * the IPv4 header) plus drop / mark counters aggregated from the
 * inner `QueueDisc::Stats`. v1 contract: bytesEnqueued and
 * bytesDequeued are dispatcher-tracked; drops / marks are read-side
 * from the inner queue disc at `GetTinStats()` call time.
 *
 * Sparse-flow count (Linux `flows_used`) is v1.1 — requires an inner
 * accessor on `FqCobaltQueueDisc` that doesn't exist yet.
 */
struct DsTinStats
{
    uint64_t bytesEnqueued{0}; //!< Total wire bytes admitted on this tin
    uint64_t bytesDequeued{0}; //!< Total wire bytes drained from this tin
    uint64_t drops{0};         //!< Inner-disc-aggregated drop count (packets)
    uint64_t marks{0};         //!< Inner-disc-aggregated mark count (packets)
};

/**
 * @ingroup diffserv
 *
 * @brief Strategy that decides which populated inner slot of a
 * `DiffServEdgeQueueDisc` is serviced next.
 *
 * The edge has a DSCP-keyed multi-slot inner array. The dequeue
 * policy lives behind this Strategy so additional dispatchers
 * (deficit round-robin for CAKE, weighted fair queueing, HTB-style
 * hierarchies) can plug in without editing the edge class.
 *
 * Subclasses override:
 * * `SelectDequeueSlot` — pick the slot to drain (may mutate state)
 * * `PeekSlot` — pick the slot a subsequent dequeue would
 * drain, without mutating dispatcher state
 * * `OnEnqueue` — notification after a successful enqueue
 * * `OnDequeue` — notification after a successful dequeue
 *
 * **RNG discipline.** Dispatchers that own a random-variable stream
 * MUST expose an `AssignStreams(stream) -> consumed` method and be
 * reached by `DiffServEdgeQueueDisc::AssignStreams` so stream
 * isolation (the / 2001-era anti-pattern fix) is preserved.
 * `DsStrictPriorityDispatcher` consumes zero streams; the default
 * cascade is unaffected.
 *
 */
class DsSlotDispatcher : public Object
{
  public:
    /** @brief Get the TypeId for this class. */
    static TypeId GetTypeId();

    ~DsSlotDispatcher() override = default;

    /**
     * @brief Pick the populated inner slot to drain next.
     *
     * May mutate dispatcher state (e.g. advance a DRR round-robin
     * cursor, credit deficit counters). Returns -1 if no slot is
     * ready to yield a packet.
     *
     * @param edge the edge whose slots to consult (never null)
     * @return slot index in `[0, kMaxInnerSlots)`, or -1 if none ready
     */
    virtual int32_t SelectDequeueSlot(DiffServEdgeQueueDisc* edge) = 0;

    /**
     * @brief Pick the slot a subsequent dequeue would drain.
     *
     * MUST NOT mutate dispatcher state. The ns-3 base infrastructure
     * calls `DoPeek` idempotently; a mutating peek would desync state
     * between peek-then-dequeue patterns.
     *
     * @param edge the edge whose slots to consult (never null)
     * @return slot index in `[0, kMaxInnerSlots)`, or -1 if none ready
     */
    virtual int32_t PeekSlot(DiffServEdgeQueueDisc* edge) = 0;

    /**
     * @brief Notification fired after a successful inner `Enqueue`.
     *
     * Default: no-op. Stateful subclasses (e.g. DRR-across-slots that
     * must track newly-non-empty slots) override.
     *
     * The edge pointer is supplied so subclasses can introspect inner
     * state (e.g. `edge->GetInnerDiscAt(slot)->GetNPackets()`) without
     * having to cache a separate back-reference.
     *
     * @param slot the slot that received the packet
     * @param item the enqueued item (never null on the success path)
     * @param edge the owning edge disc (never null)
     */
    virtual void OnEnqueue(uint32_t /*slot*/,
                           Ptr<QueueDiscItem> /*item*/,
                           DiffServEdgeQueueDisc* /*edge*/)
    {
    }

    /**
     * @brief Notification fired after a successful
     * `DiffServEdgeQueueDisc::DoDequeue`.
     *
     * Default: no-op. Stateful subclasses override to decrement
     * deficit counters, advance cursors, or reset per-slot state when
     * the inner drained to empty.
     *
     * The edge pointer is supplied for the same reason as on
     * `OnEnqueue`.
     *
     * @param slot the slot that yielded the packet
     * @param item the dequeued item (never null on the success path)
     * @param edge the owning edge disc (never null)
     */
    virtual void OnDequeue(uint32_t /*slot*/,
                           Ptr<QueueDiscItem> /*item*/,
                           DiffServEdgeQueueDisc* /*edge*/)
    {
    }

    /**
     * @brief Read-only per-tin diagnostic snapshot.
     *
     * Default implementation returns a zeroed `DsTinStats`; concrete
     * dispatchers that track per-tin counters (`DsTinShaperDispatcher`,
     * `DsHybridLlqDispatcher`) override. The `edge` argument is
     * provided so subclasses can read drop / mark counters from the
     * inner disc's `QueueDisc::Stats` at call time without caching
     * shadow state.
     *
     * @param tinIdx the slot whose counters to return
     * @param edge the owning edge disc (nullable; subclasses that read
     *        inner-stats must guard)
     * @return per-tin counters (`bytesEnqueued`, `bytesDequeued`,
     *         `drops`, `marks`)
     */
    virtual DsTinStats GetTinStats(uint32_t /*tinIdx*/,
                                   const DiffServEdgeQueueDisc* /*edge*/) const
    {
        return DsTinStats{};
    }
};

/**
 * @ingroup diffserv
 *
 * @brief Strict-priority across slots — lowest populated slot drains
 * first; higher slots yield only when all lower slots are empty.
 *
 * Default dispatcher installed by the `DiffServEdgeQueueDisc`
 * constructor. Implements a byte-identical strict-priority loop so
 * existing scenarios (L4S at slot 0 + Red at slot 1, hierarchical-L4S
 * gap-1 example, single-slot backward-compat paths) observe no
 * behavioural change.
 *
 * **Stateless.** `SelectDequeueSlot` and `PeekSlot` share the walk;
 * `OnEnqueue` / `OnDequeue` remain no-ops. No RNG; no participation in
 * `AssignStreams` cascade.
 */
class DsStrictPriorityDispatcher : public DsSlotDispatcher
{
  public:
    /** @brief Get the TypeId for this class. */
    static TypeId GetTypeId();

    DsStrictPriorityDispatcher() = default;

    int32_t SelectDequeueSlot(DiffServEdgeQueueDisc* edge) override;
    int32_t PeekSlot(DiffServEdgeQueueDisc* edge) override;

  private:
    /**
     * @brief Walk populated slots in index order; return the first
     * slot whose inner reports `GetNPackets() > 0`, or -1.
     */
    int32_t FirstNonEmpty(DiffServEdgeQueueDisc* edge) const;
};

} // namespace diffserv
} // namespace ns3

#endif // NS3_DIFFSERV_DS_SLOT_DISPATCHER_H
