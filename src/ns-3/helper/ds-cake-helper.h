/*
 * Copyright (C) 2026 Sergio Andreozzi
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Compose CAKE-style multi-tin substrate on a `DiffServEdgeQueueDisc`
 * out of mainline ns-3 primitives (`TbfQueueDisc`, `FqCobaltQueueDisc`
 * with the patched `EnableAckFilter` / `MemLimit` attributes) and DS4
 * substrate components (`DsTinShaperDispatcher`).
 *
 */

#ifndef NS3_DIFFSERV_DS_CAKE_HELPER_H
#define NS3_DIFFSERV_DS_CAKE_HELPER_H

#include "ns3/data-rate.h"
#include "ns3/diffserv-edge-queue-disc.h"
#include "ns3/nstime.h"
#include "ns3/ptr.h"
#include "ns3/queue-disc.h"

#include <cstdint>
#include <iosfwd>
#include <memory>

namespace ns3
{

class NetDevice;

namespace diffserv
{

class DsCakeLiveBulkCounter;
class DsCakeLinuxAutorateHook;

/**
 * @ingroup diffserv
 *
 * @brief Pluggable rate-adjustment hook for the CAKE
 * `autorate-ingress` flag.
 *
 * The hook is invoked by the helper when autorate-ingress is enabled.
 * `ComputeRateDelta` returns a signed delta in bits per second
 * (negative = reduce the configured rate; positive = grow it).
 *
 * The shipped default `DsCakeNoOpAutorateHook` returns zero
 * regardless of input, matching the v1 API-only contract: full
 * RTT-trend inference plus hysteresis is deferred to v2.
 */
class DsCakeAutorateIngressHook
{
  public:
    virtual ~DsCakeAutorateIngressHook() = default;

    /**
     * @brief Return the rate-adjustment delta in bits per second.
     *
     * v1 implementations return zero unconditionally. v2
     * implementations consume RTT-trend state accumulated across
     * calls and return non-zero deltas to track the upstream
     * bottleneck.
     *
     * @param currentRateBps the rate currently applied by the
     *        helper (per-tin or global, depending on the wire-up
     *        site that consumes the hook in v2)
     * @return signed delta in bits per second (negative reduces
     *         the rate; positive grows it; zero is a no-op)
     */
    virtual int64_t ComputeRateDelta(uint64_t currentRateBps) const = 0;
};

/**
 * @ingroup diffserv
 *
 * @brief No-op `DsCakeAutorateIngressHook` shipped as the v1 default.
 *
 * Returns zero for every input, producing byte-identical wire output
 * to the autorate-disabled state. The v2 work item replaces this
 * default with an RTT-trend-tracking subclass.
 */
class DsCakeNoOpAutorateHook : public DsCakeAutorateIngressHook
{
  public:
    int64_t ComputeRateDelta(uint64_t /* currentRateBps */) const override
    {
        return 0;
    }
};

/**
 * @ingroup diffserv
 *
 * @brief Convenience configurators for CAKE-style multi-tin
 * compositions on a `DiffServEdgeQueueDisc`.
 *
 * Each configurator wires a fresh `DiffServEdgeQueueDisc` for one of
 * the three Linux `tc-cake(8)` tin profiles (diffserv3, diffserv4,
 * diffserv8). Per tin: a `FqCobaltQueueDisc` (8-way set-
 * associative FQ + COBALT AQM, `EnableAckFilter` opt-in). The DSCP-to-
 * tin mapping matches the canonical Linux table; the across-tin
 * scheduler is one of:
 *   - `DsTinShaperDispatcher` (DRR with share-proportional quanta) —
 *      default; matches Linux `tc-cake` work-conserving semantics.
 *   - `DsHybridLlqDispatcher` (strict-priority on the latency-
 *      sensitive tin, DRR over the rest) — opt-in via
 *      `enableLlq=true`; delivers sub-30 ms p99 RRUL latency
 *      matching CAKE paper Fig. 4 at the cost of starving lower-
 *      priority tins under SP saturation.
 *
 * The configurator must be called on a freshly-constructed edge,
 * before `Initialize` and before any other inner-slot or DSCP
 * configuration. The edge is ready to `Initialize` on return; the
 * caller installs it into the IP stack via the standard
 * `TrafficControlHelper` path.
 *
 */
class DsCakeHelper
{
  public:
    /**
     * @brief Shaper-path selector for CAKE composition.
     *
     * - TokenBucket (default): per-tin TinTokenBucket POD shared across
     *   work-conserving and LLQ dispatchers.
     * - TbfInner: mainline TbfQueueDisc as per-tin inner via patch 0004.
     *   Aliased by the legacy useInnerTbfShaping=true bool.
     * - RateBased: virtual-clock shaper per Linux sch_cake.c
     *   (67dc6c56b871, cake_advance_shaper @ line 1533;
     *   provenance/linux-sch-cake-67dc6c56b871/sch_cake.c).
     */
    enum class ShaperMode
    {
        TokenBucket,
        TbfInner,
        RateBased,
    };

    void SetShaperMode(ShaperMode mode);
    ShaperMode GetShaperMode() const;

    /**
     * @brief Linux `tc-cake(8)` named link-layer overhead presets.
     *
     * Each enum value maps to a (overhead, atm, ptm, mpu) tuple matching the
     * iproute2 `q_cake.c` keyword table. `SetLinkLayer` resolves the tuple and
     * calls `ConfigureLinkLayerOverhead`. Custom values not covered by the
     * preset table remain accessible via `ConfigureLinkLayerOverhead` directly.
     *
     * \see provenance/iproute2-q-cake-62d47c2dbc0eaecdd20c0e19406067488025e92e/q_cake.c cake_link_layer_keywords[]
     */
    enum class LinkPreset
    {
        Raw,            //!< overhead=0, noatm, noptm, no mpu (Linux `raw`)
        Conservative,   //!< overhead=48, noatm, mpu=64 (Stratum default; differs from Linux conservative which uses ATM)
        Ethernet,       //!< overhead=38, mpu=84
        EtherVlan,      //!< overhead=42 (ethernet + 4), mpu=84 (stacks on Ethernet)
        Docsis,         //!< overhead=18, noatm, mpu=64
        PppoePtm,       //!< overhead=30, ptm
        PppoeVcmux,     //!< overhead=32, atm
        PppoeLlcsnap,   //!< overhead=40, atm
        PppoaVcmux,     //!< overhead=10, atm
        PppoaLlc,       //!< overhead=14, atm
        BridgedPtm,     //!< overhead=22, ptm
        BridgedVcmux,   //!< overhead=24, atm
        BridgedLlcsnap, //!< overhead=32, atm
        IpoaVcmux,      //!< overhead=8, atm
        IpoaLlcsnap,    //!< overhead=16, atm
    };

    /**
     * @brief Apply a named Linux `tc-cake(8)` link-layer preset to @p edge.
     *
     * Resolves the preset to its (overhead, atm, ptm, mpu) tuple and calls
     * `ConfigureLinkLayerOverhead`. Convenience layer over the raw setter;
     * for values not covered by the table, call `ConfigureLinkLayerOverhead`
     * directly.
     *
     * @param edge edge previously composed by `SetAsCake*`
     * @param preset Linux keyword as a strongly-typed enum
     */
    static void SetLinkLayer(Ptr<DiffServEdgeQueueDisc> edge, LinkPreset preset);

    /**
     * @brief Linux `tc-cake(8)` named RTT presets.
     *
     * Each enum value maps to a (target, interval) Time pair matching the
     * iproute2 `q_cake.c` preset table. `SetRttPreset` walks every inner
     * `FqCobaltQueueDisc` on the edge and applies the pair via SetAttribute.
     * Custom values remain accessible via direct SetAttribute calls.
     *
     * `Internet` (5ms / 100ms) is the RFC 8289 CoDel default and matches the
     * implicit default — applying it should be a no-op.
     *
     * Under host-isolation, each tin's `FqCobaltQueueDisc` IS visited by
     * `SetRttPreset` (the patched-mainline disc is a direct inner, no
     * nesting). Apply RTT tuning after or before calling `SetAsCake*` with
     * `enableHostIsolation=true`; both orderings are safe.
     *
     * \see provenance/iproute2-q-cake-62d47c2dbc0eaecdd20c0e19406067488025e92e/q_cake.c presets[]
     * \see RFC 8289 Section 4.2 (CoDel defaults)
     */
    enum class RttPreset
    {
        Datacentre,     //!< target=5us,   interval=100us
        Lan,            //!< target=50us,  interval=1ms
        Metro,          //!< target=500us, interval=10ms
        Regional,       //!< target=1.5ms, interval=30ms
        Internet,       //!< target=5ms,   interval=100ms (RFC 8289 default)
        Oceanic,        //!< target=15ms,  interval=300ms
        Satellite,      //!< target=50ms,  interval=1000ms
        Interplanetary, //!< target=50s,   interval=1000s
    };

    /**
     * @brief Apply a named Linux `tc-cake(8)` RTT preset to every inner
     *        `FqCobaltQueueDisc` on @p edge.
     *
     * Walks the edge's inner slots; for any slot whose inner is a
     * `FqCobaltQueueDisc` (direct or TBF-wrapped), sets `Target` and
     * `Interval` attributes from the preset table. Slots that don't
     * contain a `FqCobaltQueueDisc` (for example, L4S inner discs) are
     * silently skipped.
     *
     * @param edge edge previously composed by `SetAsCake*`
     * @param preset Linux keyword as a strongly-typed enum
     */
    static void SetRttPreset(Ptr<DiffServEdgeQueueDisc> edge, RttPreset preset);

    /**
     * @brief Back-compat alias: maps to @c ShaperMode::TbfInner.
     *
     * Preserves the legacy boolean toggle exposed before the
     * @c ShaperMode enum was introduced. When @p enable is false, the
     * mode falls back to @c TokenBucket if the current mode was
     * @c TbfInner; @c RateBased is preserved unchanged.
     *
     * @param enable true selects @c TbfInner; false reverts @c TbfInner
     *               to @c TokenBucket
     */
    void SetUseInnerTbfShaping(bool enable);

    /**
     * @brief Set the aggregate (global-clock) rate in bits per second.
     *
     * Drives the global clock for @c ShaperMode::RateBased and the
     * aggregate-rate cap for @c TokenBucket / @c TbfInner compositions.
     *
     * @param rateBps aggregate shaping rate
     */
    void SetGlobalRateBps(uint64_t rateBps);

    /**
     * @brief Set a uniform per-tin rate (bits per second) used by
     * @c BuildDispatcher in @c RateBased mode.
     *
     * @param rateBps per-tin shaping rate; applied to every configured tin
     */
    void SetTinRateBpsAll(uint64_t rateBps);

    /**
     * @brief Set the number of tins instantiated by @c BuildDispatcher
     * in @c RateBased mode (default 4 — the @c diffserv4 layout).
     *
     * @param n number of tins
     */
    void SetTinCount(uint32_t n);

    /**
     * @brief Construct a CAKE shaper dispatcher whose concrete type is
     * determined by the current @c ShaperMode.
     *
     * Returns a fully-configured @c DiffServEdgeQueueDisc (for
     * @c TokenBucket / @c TbfInner — the inner FQ layout from
     * @c SetAsCakeDiffserv4) or a @c DsRateBasedShaperDispatcher
     * (for @c RateBased), populated from @c m_globalRateBps,
     * @c m_uniformTinRateBps and @c m_tinCount.
     *
     * @return the configured queue disc, ready for installation
     */
    Ptr<QueueDisc> BuildDispatcher();

    /**
     * @brief Build the dispatcher and install it as the root qdisc on
     * @p device via the node's @c TrafficControlLayer.
     *
     * @param device target NetDevice
     * @return the installed queue disc
     */
    Ptr<QueueDisc> BuildAndInstall(Ptr<NetDevice> device);

    /**
     * @brief Compose CAKE diffserv4 (4 tins: Bulk, BE, Video, Voice)
     * on @p edge.
     *
     * Tin shares match Linux `tc-cake(8)` defaults:
     * Bulk 6.25%, BE 100%, Video 50%, Voice 25%. The DSCP map matches
     * Linux's `cake_diffserv4` profile (CS1+LE+AF11 -> Bulk; default
     * -> BE; AF12+AF13+CS2+CS3+AF21..AF43 -> Video; CS4..CS7+EF+VA
     * -> Voice).
     *
     * @param edge fresh `DiffServEdgeQueueDisc`; must be pre-Initialize
     * @param totalRate aggregate rate driving per-tin TBF caps
     * @param enableAckFilter opt-in TCP ACK filter on each tin
     *        (default false; v1 contract is no-op until upstream MR
     *         lands)
     * @param enableLlq opt-in hybrid LLQ-across-tins dispatcher; when
     *        true, the Voice tin (slot 3, EF/CS5/CS4/CS6/CS7/VA) is
     *        served strict-priority and other tins remain DRR (default
     *        false — pure DRR per Linux `tc-cake` work-conserving)
     * @param enableTinShaping opt-in hard per-tin rate caps; when true,
     *        each tin's serve rate is capped at `share × totalRate`,
     *        matching Linux `tc-cake bandwidth N <profile>` (the
     *        production default). Composes orthogonally with
     *        @p enableLlq — `enableLlq && enableTinShaping` produces
     *        the Cisco MQC LLQ pattern (priority class with hard cap).
     *        Default false → no per-tin rate cap (DRR-only behaviour).
     * @param enableHostIsolation opt-in Linux `tc-cake(8)` triple-isolate
     *        semantics: each per-tin inner is a patched-mainline
     *        `FqCobaltQueueDisc` with `EnableHostIsolation=true` and
     *        `HostIsolationMode=Triple` (attributes from
     *        `patches/ns3/0016-fq-cobalt-host-isolation.patch`).
     *        A single host with N concurrent flows receives the same
     *        total share as a host with one flow, mirroring
     *        `sch_cake.c` (67dc6c56b871, per-side-max keying).
     *        Composes orthogonally with @p enableLlq and
     *        @p enableTinShaping (the across-tin layer is unchanged).
     *        ACK-filter knobs are forwarded to the mainline disc's
     *        `EnableAckFilter` / `EnableAckFilterAggressive` attributes.
     *        Default false → no host-pair isolation on top of the
     *        per-tin FQ.
     */
    static void SetAsCakeDiffserv4(Ptr<DiffServEdgeQueueDisc> edge,
                                   DataRate totalRate,
                                   bool enableAckFilter = false,
                                   bool enableLlq = false,
                                   bool enableTinShaping = false,
                                   bool enableHostIsolation = false,
                                   bool useInnerTbfShaping = false,
                                   bool enableAckFilterAggressive = false,
                                   bool useDualPi2Inner = false);

    /**
     * @brief Compose CAKE diffserv3 (3 tins: Bulk, Latency-Sensitive, BE)
     * on @p edge.
     *
     * Tin shares: Bulk 6.25%, Latency-Sensitive 50%, BE 100%. The DSCP
     * map matches Linux's `cake_diffserv3` profile.
     *
     * @param edge fresh `DiffServEdgeQueueDisc`; must be pre-Initialize
     * @param totalRate aggregate rate driving per-tin TBF caps
     * @param enableAckFilter opt-in TCP ACK filter on each tin
     * @param enableLlq opt-in hybrid LLQ-across-tins dispatcher; when
     *        true, the Latency-Sensitive tin (slot 1) is served
     *        strict-priority and other tins remain DRR
     * @param enableTinShaping opt-in hard per-tin rate caps; when true,
     *        each tin's serve rate is capped at `share × totalRate`,
     *        matching Linux `tc-cake bandwidth N <profile>` (the
     *        production default). Composes orthogonally with
     *        @p enableLlq — `enableLlq && enableTinShaping` produces
     *        the Cisco MQC LLQ pattern (priority class with hard cap).
     *        Default false → no per-tin rate cap (DRR-only behaviour).
     * @param enableHostIsolation opt-in Linux `tc-cake(8)` triple-isolate
     *        semantics: each per-tin inner is a patched-mainline
     *        `FqCobaltQueueDisc` with `EnableHostIsolation=true` and
     *        `HostIsolationMode=Triple` (attributes from
     *        `patches/ns3/0016-fq-cobalt-host-isolation.patch`).
     *        A single host with N concurrent flows receives the same
     *        total share as a host with one flow, mirroring
     *        `sch_cake.c` (67dc6c56b871, per-side-max keying).
     *        Composes orthogonally with @p enableLlq and
     *        @p enableTinShaping (the across-tin layer is unchanged).
     *        ACK-filter knobs are forwarded to the mainline disc's
     *        `EnableAckFilter` / `EnableAckFilterAggressive` attributes.
     *        Default false → no host-pair isolation on top of the
     *        per-tin FQ.
     */
    static void SetAsCakeDiffserv3(Ptr<DiffServEdgeQueueDisc> edge,
                                   DataRate totalRate,
                                   bool enableAckFilter = false,
                                   bool enableLlq = false,
                                   bool enableTinShaping = false,
                                   bool enableHostIsolation = false,
                                   bool useInnerTbfShaping = false,
                                   bool enableAckFilterAggressive = false);

    /**
     * @brief Compose CAKE diffserv8 (8 tins, full DS class hierarchy)
     * on @p edge.
     *
     * Tin index follows DS class number: Tin 0 = CS0/Default,
     * Tin 1 = CS1+LE, Tin 2 = CS2+AF1x, Tin 3 = CS3+AF2x,
     * Tin 4 = CS4+AF3x, Tin 5 = CS5+AF4x, Tin 6 = CS6+EF+VA,
     * Tin 7 = CS7. Tin shares are progressive (Tin 0 = 100%, Tin 1
     * = 6.25%, Tin 2..5 = 50%, Tin 6..7 = 25%).
     *
     * @param edge fresh `DiffServEdgeQueueDisc`; must be pre-Initialize
     * @param totalRate aggregate rate driving per-tin TBF caps
     * @param enableAckFilter opt-in TCP ACK filter on each tin
     * @param enableLlq opt-in hybrid LLQ-across-tins dispatcher; when
     *        true, Tin 6 (CS6/EF/VA) is served strict-priority and
     *        other tins remain DRR
     * @param enableTinShaping opt-in hard per-tin rate caps; when true,
     *        each tin's serve rate is capped at `share × totalRate`,
     *        matching Linux `tc-cake bandwidth N <profile>` (the
     *        production default). Composes orthogonally with
     *        @p enableLlq — `enableLlq && enableTinShaping` produces
     *        the Cisco MQC LLQ pattern (priority class with hard cap).
     *        Default false → no per-tin rate cap (DRR-only behaviour).
     * @param enableHostIsolation opt-in Linux `tc-cake(8)` triple-isolate
     *        semantics: each per-tin inner is a patched-mainline
     *        `FqCobaltQueueDisc` with `EnableHostIsolation=true` and
     *        `HostIsolationMode=Triple` (attributes from
     *        `patches/ns3/0016-fq-cobalt-host-isolation.patch`).
     *        A single host with N concurrent flows receives the same
     *        total share as a host with one flow, mirroring
     *        `sch_cake.c` (67dc6c56b871, per-side-max keying).
     *        Composes orthogonally with @p enableLlq and
     *        @p enableTinShaping (the across-tin layer is unchanged).
     *        ACK-filter knobs are forwarded to the mainline disc's
     *        `EnableAckFilter` / `EnableAckFilterAggressive` attributes.
     *        Default false → no host-pair isolation on top of the
     *        per-tin FQ.
     */
    static void SetAsCakeDiffserv8(Ptr<DiffServEdgeQueueDisc> edge,
                                   DataRate totalRate,
                                   bool enableAckFilter = false,
                                   bool enableLlq = false,
                                   bool enableTinShaping = false,
                                   bool enableHostIsolation = false,
                                   bool useInnerTbfShaping = false,
                                   bool enableAckFilterAggressive = false);

    /**
     * @brief Compose CAKE besteffort (single tin, no DiffServ
     * classification) on @p edge.
     *
     * All DSCP code points map to tin 0; share is 1.0; the across-tin
     * dispatcher serves the lone tin. Matches Linux `tc-cake besteffort`
     * which disables tin classification entirely. The flag arguments
     * compose orthogonally: @p enableAckFilter, @p enableTinShaping,
     * @p enableHostIsolation, and @p useInnerTbfShaping affect the per-
     * tin inner queue disc; @p enableLlq is a no-op (only one tin).
     *
     * @param edge fresh `DiffServEdgeQueueDisc`; must be pre-Initialize
     * @param totalRate aggregate rate driving the per-tin TBF cap
     * @param enableAckFilter opt-in TCP ACK filter on the lone tin
     * @param enableLlq ignored (silently); kept in the signature for
     *        orthogonality with the multi-tin presets
     * @param enableTinShaping opt-in hard rate cap on the lone tin
     * @param enableHostIsolation opt-in patched-mainline
     *        `FqCobaltQueueDisc` with host-isolation on the lone tin
     * @param useInnerTbfShaping opt-in mainline `TbfQueueDisc` as the
     *        per-tin shaper (token-bucket-on-each-tin composition)
     */
    static void SetAsCakeBestEffort(Ptr<DiffServEdgeQueueDisc> edge,
                                    DataRate totalRate,
                                    bool enableAckFilter = false,
                                    bool enableLlq = false,
                                    bool enableTinShaping = false,
                                    bool enableHostIsolation = false,
                                    bool useInnerTbfShaping = false,
                                    bool enableAckFilterAggressive = false);

    /**
     * @brief Compose CAKE precedence (8 tins keyed on the top three bits
     * of the DSCP code point) on @p edge.
     *
     * DSCP `d` maps to tin `d >> 3` — that is, all eight DSCPs sharing
     * the same IP precedence (CS0..CS7) land in the same tin. Matches
     * Linux `tc-cake precedence` which disables fine-grained AFxx
     * classification and falls back to the legacy IP-precedence model.
     *
     * Tin shares scale linearly with precedence (tin 0 = 0.125, tin 7
     * = 1.0), mirroring the relative ordering of Linux's
     * `cake_class_quanta_precedence` table. The exact share scaling is
     * a v1 calibration choice — verify against the current Linux
     * `iproute2` master before relying on absolute share ratios.
     *
     * The LLQ slot defaults to tin 7 (CS7, network control / signalling)
     * when @p enableLlq is true — the same convention CAKE uses to
     * elevate network-control traffic above bulk under hybrid SP+DRR.
     *
     * @param edge fresh `DiffServEdgeQueueDisc`; must be pre-Initialize
     * @param totalRate aggregate rate driving per-tin TBF caps
     * @param enableAckFilter opt-in TCP ACK filter on each tin
     * @param enableLlq opt-in hybrid LLQ-across-tins; tin 7 (CS7) is
     *        served strict-priority when true
     * @param enableTinShaping opt-in hard per-tin rate caps
     * @param enableHostIsolation opt-in patched-mainline
     *        `FqCobaltQueueDisc` with host-isolation on each tin
     * @param useInnerTbfShaping opt-in mainline `TbfQueueDisc` as the
     *        per-tin shaper (token-bucket-on-each-tin composition)
     */
    static void SetAsCakePrecedence(Ptr<DiffServEdgeQueueDisc> edge,
                                    DataRate totalRate,
                                    bool enableAckFilter = false,
                                    bool enableLlq = false,
                                    bool enableTinShaping = false,
                                    bool enableHostIsolation = false,
                                    bool useInnerTbfShaping = false,
                                    bool enableAckFilterAggressive = false);

    /**
     * @brief Compose path-α (in-dispatcher TokenBucket) on @p edge with
     *        per-tin shaping enabled.
     *
     * Default α composition (e.g. via `SetAsCakeDiffserv4` with
     * `enableTinShaping=false` — the wiring used by `BuildDispatcher` in
     * `ShaperMode::TokenBucket`) does NOT cap aggregate or per-tin rates;
     * this preset turns on per-tin caps so α matches the cap-enforcing
     * behaviour of β (rate-based shaper) and γ (inner-TBF) compositions.
     *
     * Composes the four-tin diffserv4 layout (Bulk / BE / Video / Voice)
     * with TokenBucket-via-dispatcher for the across-tin layer (no
     * inner-TBF) and hard per-tin caps inside each tin via the in-
     * dispatcher `TinTokenBucket` gate.
     *
     * Equivalent to `SetAsCakeDiffserv4(edge, totalRate, ...,
     *                                   enableTinShaping=true,
     *                                   ...,
     *                                   useInnerTbfShaping=false)`.
     *
     * @param edge fresh `DiffServEdgeQueueDisc`; must be pre-Initialize
     * @param totalRate aggregate rate driving per-tin token-bucket caps
     * @param enableAckFilter opt-in TCP ACK filter on each tin
     * @param enableLlq opt-in hybrid LLQ-across-tins dispatcher
     * @param enableHostIsolation opt-in patched-mainline
     *        `FqCobaltQueueDisc` with host-isolation per tin
     */
    static void SetAsCakeAlphaTinShaped(Ptr<DiffServEdgeQueueDisc> edge,
                                        DataRate totalRate,
                                        bool enableAckFilter = false,
                                        bool enableLlq = false,
                                        bool enableHostIsolation = false,
                                        bool enableAckFilterAggressive = false);

    /**
     * @brief Apply the Linux `tc-cake(8)` `conservative` preset to @p edge.
     *
     * The `conservative` preset targets unknown link technologies by
     * applying defensive overhead defaults: per-packet overhead 48
     * bytes, minimum-packet-unit floor 64 bytes, ATM cell framing
     * disabled. Equivalent to a call to
     * `ConfigureLinkLayerOverhead(edge, 48, false, false, 64, false)` — the
     * preset is a thin convenience wrapper that names the arguments rather
     * than requiring the call site to spell out each one.
     *
     * Composes additively on top of any prior `SetAsCake*` profile;
     * call after the profile and before `Initialize`. Has the same
     * pre-Initialize discipline and per-tin-TBF dependency as
     * `ConfigureLinkLayerOverhead`.
     *
     * @param edge edge previously composed by `SetAsCake*` with
     *        `useInnerTbfShaping=true`.
     */
    static void SetAsCakeConservative(Ptr<DiffServEdgeQueueDisc> edge);

    /**
     * @brief Configure Linux `tc-cake(8)` link-layer overhead correction
     * on @p edge — additive on top of any prior `SetAsCake*` profile.
     *
     * The v1 contract is **statistical**: each per-tin TBF rate is
     * downscaled by the expected wire-byte / IP-byte ratio
     * `gamma = E[wire(s)] / E[s]` over the bimodal Internet mix
     * `{(64B, 0.5), (1500B, 0.5)}`. Per-packet correctness — adjusting
     * the burst charged at TBF dequeue — is v1.1 follow-up; the v1
     * surface validates against `S-17.30` and matches Linux
     * `tc-cake bandwidth N overhead M [atm] [mpu K]` steady-state
     * throughput within ±2 % for the default mix.
     *
     * Maps the five Linux flags onto helper arguments:
     *   - `overhead N`  -> @p overhead bytes
     *   - `atm`         -> @p atm = true (53/48 ATM cell framing)
     *   - `ptm`         -> @p ptm = true (PTM 64b/65b linear framing)
     *   - `mpu N`       -> @p mpu bytes (minimum-packet-unit floor)
     *   - `raw`         -> @p raw = true (suppress overhead correction)
     *
     * Must be called **after** the relevant `SetAsCake*` profile and
     * **before** `Initialize`.
     *
     * @param edge edge previously composed by `SetAsCake*`
     * @param overhead bytes added to each packet (Linux `overhead N`)
     * @param atm ATM cell framing (Linux `atm`); when false uses the
     *        raw `s + overhead` accounting
     * @param ptm PTM 64b/65b framing (Linux `ptm`); when true uses the
     *        `adj = adj + (adj + 63) / 64` linear cell-tax. Mutually
     *        exclusive with `atm`; asserting both is undefined.
     *        \see provenance/linux-sch-cake-67dc6c56b871/sch_cake.c::cake_overhead
     * @param mpu minimum-packet-unit floor (Linux `mpu N`); 0 disables
     * @param raw when true, suppress overhead-driven rate adjustment
     *        entirely (Linux `raw`); other arguments are ignored.
     *        Default false.
     */
    static void ConfigureLinkLayerOverhead(Ptr<DiffServEdgeQueueDisc> edge,
                                           uint32_t overhead,
                                           bool atm,
                                           bool ptm,
                                           uint32_t mpu,
                                           bool raw = false);

    /**
     * @brief Emit a `tc -s qdisc show cake`-equivalent text dump for
     * @p edge to @p os.
     *
     * Thin delegate to `DsCakeStatsFormatter::Print`. Walks the
     * edge's populated tins, pulls per-tin counters via
     * `DiffServEdgeQueueDisc::GetTinStats`, and writes a Linux-
     * compatible report. Intended for diagnostic dumps in scenario
     * scripts and integration tests; not on the hot path.
     *
     * Substrate fidelity gaps are documented in the user handbook's
     * CAKE chapter "Diagnostic output" subsection; the most prominent
     * is the bulk-flow counter (`ever_seen`)
     * which differs from Linux's live `bulk_flow_count` because the
     * stock ns-3 `FqCobaltQueueDisc` exposes only an append-only
     * class list.
     *
     * @param os destination stream
     * @param edge CAKE-composed edge queue disc (nullable; null
     *        produces a single-line null diagnostic)
     * @see specs/02-structural.md S-17.51
     */
    void PrintTcStats(std::ostream& os, Ptr<const QueueDisc> edge) const;

    /**
     * @brief Attach a `DsCakeLiveBulkCounter` to every inner
     *        `FqCobaltQueueDisc` on @p edge.
     *
     * After this call, `GetLiveBulkCount(edge, slot)` returns a live
     * count for each slot instead of the `ever_seen` approximation used
     * by `DsCakeStatsFormatter`. The counter objects are stored as
     * aggregated `Object`s on the respective inner discs and survive
     * for the lifetime of those discs.
     *
     * Must be called **after** `Initialize`. Calling on a null edge or
     * on a slot that does not wrap a `FqCobaltQueueDisc` is silently
     * skipped.
     *
     * @param edge CAKE-composed edge queue disc
     * @param idleWindow passed to `DsCakeLiveBulkCounter::Attach`;
     *        `Time(0)` (default) auto-derives `8 × Interval`
     */
    static void AttachLiveBulkCounter(Ptr<DiffServEdgeQueueDisc> edge,
                                      Time idleWindow = Time(0));

    /**
     * @brief Return the live bulk-flow count for tin @p slot on @p edge.
     *
     * Returns the count from the `DsCakeLiveBulkCounter` previously
     * attached via `AttachLiveBulkCounter`. Returns 0 if no counter
     * is attached on @p slot or if @p edge is null.
     *
     * @param edge CAKE-composed edge queue disc
     * @param slot tin index (0-based)
     * @return live bulk-flow count, or 0 if no counter is attached
     */
    static uint32_t GetLiveBulkCount(Ptr<DiffServEdgeQueueDisc> edge, uint32_t slot);

    /**
     * @brief Enable or disable the Linux `tc-cake(8)`
     * `autorate-ingress` flag at API level.
     *
     * The current contract is API-only: the boolean round-trips and
     * the no-op hook (`DsCakeNoOpAutorateHook`) is installed as the
     * rate-adjustment default when @p enable is true. The hook
     * returns zero for every input, so per-packet enqueue and
     * dequeue paths produce byte-identical output to the disabled
     * case. The closed-loop RTT-trend tracker plus hysteresis logic
     * is a future deliverable; the hook is the integration surface
     * that future implementations plumb into without an API
     * redesign.
     *
     * @param enable true installs the no-op hook (and the boolean
     *               toggles to true); false clears the hook and the
     *               toggle reverts to false
     */
    void SetEnableAutorateIngress(bool enable);

    /**
     * @brief Return the current state of the `autorate-ingress` flag.
     *
     * @return true if autorate-ingress is enabled
     * @see SetEnableAutorateIngress
     */
    bool GetEnableAutorateIngress() const;

    /**
     * @brief Access the currently-installed autorate-ingress hook.
     *
     * v1 returns either nullptr (flag disabled) or the no-op hook
     * (flag enabled). v2 implementations may swap the hook via a
     * future setter; the accessor is provided so test fixtures can
     * verify the no-op contract directly.
     *
     * @return pointer to the installed hook, or nullptr when the flag
     *         is disabled
     */
    const DsCakeAutorateIngressHook* GetAutorateIngressHook() const;

    /**
     * @brief Autorate hook implementation selector.
     *
     * - NoOp: v1 contract — returns zero, byte-identical to disabled.
     * - Linux: peak-rate-EWMA per sch_cake.c (Tier 2 work item).
     *
     * The selection takes effect the next time `SetEnableAutorateIngress(true)`
     * is called. Changing the selector while autorate is already enabled
     * requires disabling and re-enabling the flag.
     *
     * \see provenance/linux-sch-cake-67dc6c56b871/sch_cake.c::cake_enqueue
     */
    enum class AutorateImpl
    {
        NoOp,
        Linux,
    };

    /**
     * @brief Select which autorate hook implementation to install.
     *
     * @param impl NoOp for the v1 default or Linux for the Linux-faithful
     *             peak-rate-EWMA implementation
     */
    void SetAutorateImpl(AutorateImpl impl);

    /** @brief Return the current autorate implementation selector. */
    AutorateImpl GetAutorateImpl() const;

    /**
     * @brief Enable Linux `tc-cake(8)` `ingress` mode on the path-beta shaper.
     *
     * Effective only when ShaperMode is RateBased. No effect on
     * TokenBucket or TbfInner compositions.
     *
     * When enabled, the per-tin and global clocks advance on packet drops
     * (overflow at the dispatcher boundary) as well as on forwarded
     * packets. Matches `sch_cake.c::cake_enqueue` CAKE_FLAG_INGRESS
     * behaviour.
     *
     * @param enabled true to enable ingress-mode clock charging on drop
     * \see provenance/linux-sch-cake-67dc6c56b871/sch_cake.c::cake_enqueue
     */
    void SetEnableIngressMode(bool enabled);

    /** @brief Return the current ingress-mode setting. */
    bool GetEnableIngressMode() const;

  private:
    ShaperMode m_shaperMode{ShaperMode::TokenBucket};
    uint64_t m_globalRateBps{0};         //!< Aggregate-rate in bits per second
    uint64_t m_uniformTinRateBps{0};     //!< Uniform per-tin rate (RateBased mode)
    uint32_t m_tinCount{4};              //!< Number of tins (RateBased mode default = 4)
    bool m_enableAutorateIngress{false}; //!< Linux `autorate-ingress` flag
    bool m_enableIngressMode{false};     //!< Linux `ingress` flag for path-beta shaper
    AutorateImpl m_autorateImpl{AutorateImpl::NoOp}; //!< Autorate implementation selector
    std::shared_ptr<DsCakeAutorateIngressHook> m_autorateHook; //!< Shared hook — co-owned with dispatcher
};

} // namespace diffserv
} // namespace ns3

#endif // NS3_DIFFSERV_DS_CAKE_HELPER_H
