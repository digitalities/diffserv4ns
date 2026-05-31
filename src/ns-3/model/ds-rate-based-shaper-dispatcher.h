/*
 * Copyright (C) 2026 Sergio Andreozzi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef NS3_DIFFSERV_DS_RATE_BASED_SHAPER_DISPATCHER_H
#define NS3_DIFFSERV_DS_RATE_BASED_SHAPER_DISPATCHER_H

#include "ds-rate-based-global-clock.h"
#include "ds-rate-based-tin-clock.h"

#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/queue-disc.h"

#include <array>
#include <memory>
#include <vector>

namespace ns3
{
namespace diffserv
{

// Forward declaration — avoids pulling ds-cake-helper.h into the model layer.
class DsCakeLinuxAutorateHook;

/**
 * @brief CAKE rate-based virtual-clock shaper dispatcher.
 *
 * Parallels `DsTinShaperDispatcher` but consumes a per-tin
 * `RateBasedTinClock` instead of `TinTokenBucket`. Each tin holds an
 * independent virtual clock; an additional aggregate-rate clock binds
 * total egress per the Linux sch_cake dual-clock model
 * (provenance/linux-sch-cake-67dc6c56b871/sch_cake.c @ 67dc6c56b871).
 *
 * Eligibility for dequeue from a tin requires:
 *   `Now() >= max(tin.tNext, global.tNext)`
 *
 * On dequeue the per-tin clock advances via the three-branch logic from
 * `cake_advance_shaper`; the global clock advances unconditionally by
 * `adj_len/global_rate`. When no tin is eligible, the dispatcher
 * schedules one outstanding `SelfWake` at:
 *   `min over backlogged tins of max(tin.tNext, global.tNext)`.
 *
 * Full DRR rotation and dual-clock wiring land in Tasks 3.2-3.3.
 */
class DsRateBasedShaperDispatcher : public QueueDisc
{
  public:
    /** @brief Get the TypeId for this class. */
    static TypeId GetTypeId();

    DsRateBasedShaperDispatcher();
    ~DsRateBasedShaperDispatcher() override;

    /**
     * @brief Configure a tin slot's per-tin clock parameters.
     *
     * Must be called once per tin before the dispatcher starts.
     *
     * @param slot     zero-based tin index
     * @param rateBps  per-tin shaping rate in bits per second
     * @param overhead signed per-packet wire-byte overhead
     * @param mpu      minimum packet unit in bytes
     * @param framing  ATM/PTM/none cell-rounding mode
     */
    void ConfigureTin(uint32_t slot,
                      uint64_t rateBps,
                      int32_t overhead,
                      uint32_t mpu,
                      RateBasedTinClock::FramingMode framing);

    /**
     * @brief Configure the aggregate global clock rate.
     *
     * @param rateBps aggregate shaping rate in bits per second
     */
    void ConfigureGlobal(uint64_t rateBps);

    /**
     * @brief LLQ-first toggle: when true, the LLQ slot is examined ahead
     *        of the DRR rotation.
     *
     * @param enabled true to enable LLQ-first dequeue priority
     */
    void SetEnableLlq(bool enabled);

    /**
     * @brief Set the DSCP-codepoint -> tin-slot mapping.
     *
     * Drives DSCP-based classification when this dispatcher is installed
     * as a standalone root qdisc (via @c DsCakeHelper::BuildAndInstall).
     * @p dscp must be a valid 6-bit code point; @p slot must be a
     * configured tin index.
     *
     * @param dscp 6-bit DSCP code point (0..63)
     * @param slot zero-based tin index
     */
    void SetDscpToSlot(uint8_t dscp, uint32_t slot);

    /**
     * @brief Toggle Linux `tc-cake(8)` `ingress` mode.
     *
     * When enabled, the per-tin and global clocks advance on packet drops
     * (overflow at the dispatcher boundary) as well as on forwarded
     * packets. Matches `sch_cake.c::cake_enqueue` calling
     * `cake_advance_shaper(..., true)` when CAKE_FLAG_INGRESS is set.
     *
     * Default: false (egress shaping — clocks advance only on dequeue).
     *
     * Note: AQM-decided drops inside the inner FqCobaltQueueDisc are not
     * visible to this dispatcher in v1; ingress accounting covers
     * overflow drops at the dispatcher boundary only.
     *
     * @param enabled true to enable ingress-mode clock charging on drop
     * \see provenance/linux-sch-cake-67dc6c56b871/sch_cake.c::cake_enqueue
     */
    void SetIngressMode(bool enabled);

    /** @brief Return the current ingress-mode setting. */
    bool GetIngressMode() const;

    /**
     * @brief Attach a Linux-faithful autorate hook.
     *
     * When non-null, the hook's `OnEnqueue` is called on every accepted
     * packet, allowing it to maintain EWMA state across the arrival
     * stream. The hook must outlive this dispatcher.
     *
     * @param hook pointer to an instantiated `DsCakeLinuxAutorateHook`
     *             (nullptr disables autorate observation)
     */
    void SetAutorateHook(std::shared_ptr<DsCakeLinuxAutorateHook> hook);

    /**
     * @brief Return the cumulative bytes charged to a per-tin clock.
     *
     * In egress mode, counts forwarded bytes only (charged on dequeue).
     * In ingress mode, also counts dropped bytes at the dispatcher
     * boundary (overflow path in DoEnqueue).
     *
     * @param slot tin index
     * @return total bytes charged to the per-tin clock for @p slot
     */
    uint64_t GetTinBytesCharged(uint32_t slot) const;

  protected:
    bool DoEnqueue(Ptr<QueueDiscItem> item) override;
    Ptr<QueueDiscItem> DoDequeue() override;
    Ptr<const QueueDiscItem> DoPeek() override;
    bool CheckConfig() override;
    void InitializeParams() override;

  private:
    /** @brief Deferred-wakeup callback: drives the dequeue loop. */
    void OnSelfWake();

    /**
     * @brief Compute the next earliest eligibility time across backlogged
     *        tins; schedule a SelfWake event there if none is armed.
     */
    void MaybeArmSelfWake();

    /**
     * @brief Look up a tin slot from the item's IPv4 DSCP, or 0 if the
     *        item is non-IPv4 / out of range.
     *
     * @param item the QueueDiscItem being classified
     * @return tin slot index (0 by default, never negative)
     */
    int32_t ClassifyByDscp(Ptr<QueueDiscItem> item) const;

    std::vector<RateBasedTinClock> m_tinClocks;   //!< Per-tin virtual-clock state
    RateBasedGlobalClock m_globalClock;            //!< Aggregate-rate virtual clock
    EventId m_selfWakeEvent;                       //!< Outstanding deferred-wakeup event
    bool m_enableLlq{false};                       //!< LLQ-first dequeue priority
    bool m_ingressMode{false};                     //!< Ingress-mode flag (charge on drop)
    uint32_t m_drrCursor{0};                       //!< DRR round-robin cursor (tin index)
    std::array<uint8_t, 64> m_dscpToSlot{};        //!< DSCP -> tin slot (zero-init)
    std::vector<uint64_t> m_bytesCharged;           //!< Cumulative bytes charged per tin
    std::shared_ptr<DsCakeLinuxAutorateHook> m_autorateHook; //!< Shared autorate hook (Linux variant)
};

} // namespace diffserv
} // namespace ns3

#endif /* NS3_DIFFSERV_DS_RATE_BASED_SHAPER_DISPATCHER_H */
