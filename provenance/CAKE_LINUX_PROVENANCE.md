# CAKE Linux Provenance

DiffServ4NS-CAKE is implemented as a *client* of the DiffServ4NS substrate.
The substrate's architecture (dispatchers, subclass patterns, helpers,
component registries) is DiffServ4NS-original. The CAKE *feature*
semantics — what each `tc-cake(8)` flag does, what its defaults are, what
behavioural rules its components follow — are specified by the canonical
Linux implementation (`net/sched/sch_cake.c`). This document maps the
inheritance.

## Reference release

The implementation was developed against Linux v6.6 LTS
(`net/sched/sch_cake.c` lineage from the original CAKE deposit, commit
`16d7fed7` "sch_cake: Add Common Applications Kept Enhanced (CAKE)
qdisc", with intervening upstream maintenance commits).

CAKE upstream authors: Jonathan Morton, Toke Høiland-Jørgensen, Kevin
Darbyshire-Bryant, Ryan Mounce. Reference paper: Hoiland-Joergensen,
Taht, Morton et al. (2018), *"Piece of CAKE: A Comprehensive Queue
Management Solution for Home Gateways"*.

## Inheritance scope

DiffServ4NS reads `sch_cake.c` as a **specification source** for CAKE
feature semantics. Inheritance is at the level of **data, constants,
formulas, and semantic intents** — never as source-code translation. The
implementation is written in idiomatic ns-3 C++; the substrate plumbing
that hosts each feature is DiffServ4NS-original.

The boundary:

- **Inherited from `sch_cake.c`:** what CAKE features mean (knob names,
  tables, defaults, formulas, semantic rules).
- **DiffServ4NS-original:** how CAKE features are realised in ns-3
  (which dispatcher hosts them, which subclass exposes them, which
  helper API configures them, which test harness validates them).

## Per-feature inheritance map

Each row pairs a layer-2 inheritance (left, what was read from Linux)
with the layer-1 substrate primitive that hosts it (right, what is
DiffServ4NS-original).

| CAKE feature | Inherited from `sch_cake.c` | Substrate primitive that hosts it |
|---|---|---|
| DSCP-to-tin maps (`diffserv3`, `diffserv4`, `diffserv8`, `precedence`, `besteffort`) | constant tables, byte-identical values | `DsCakeHelper::SetAsCake*` family; `DsTinShaperDispatcher` tin-routing |
| Tin share weights (diffserv3 / diffserv4) | proportional values (`1.0`, `0.5`, `0.25`, `0.0625`) | `DsTinShaperDispatcher` DRR quanta |
| ACK filter classification | TCP-ACK candidacy rules (no SYN/FIN/RST/URG, zero payload, ACK flag set) | mainline ns-3 `FqCobaltQueueDisc::EnableAckFilter` (raw TCP byte parsing on `QueueDiscItem::GetPacket()`; polymorphic 5-tuple matching via `QueueDiscItem::Hash`); patch `patches/ns3/0006-fq-cobalt-ack-filter-memlimit.patch`, filed upstream; ADR-0069 |
| ACK filter conservative variant | SACK-bearing ACKs preserved | `EnableAckFilter=true, EnableAckFilterAggressive=false` on mainline `FqCobaltQueueDisc` |
| ACK filter aggressive variant | SACK-bearing ACKs admitted as scan triggers and drop targets | `EnableAckFilterAggressive=true` on mainline `FqCobaltQueueDisc` |
| Wash semantic | egress DSCP zero, ECN preserved | `Wash` attribute on `DiffServEdgeQueueDisc` |
| Host-isolation mode names (`triple-isolate` / `srchost` / `dsthost` / `hosts` / `flowblind` / `flows` / `dual-srchost` / `dual-dsthost`) + per-host quantum-recycle definition for `triple-isolate` | enum of bucket-key derivation rules; `triple-isolate` semantic: a host with N flows receives the same total share as a host with one flow | `DsHostIsolatedFqCobalt` nested-FqCobalt wrapper exposing `HostIsolationMode` enum + `Mode` attribute (default `Triple`); fixtures S-17.21..23 (Triple) + S-17.35..38 (other 7 modes). Per-host quantum modulation in `cake_get_flow_quantum` implemented via outer-DRR quantum scaling under Dual modes: bucket-effective quantum = `m_outerQuantumBytes / max(N_flows, 1)` (recorded in ADR-0068 errata 2026-05-09). Substrate fidelity note: `N_flows` is read via the inner `FqCobaltQueueDisc::GetNQueueDiscClasses()`, which counts distinct flows ever observed in the bucket rather than Linux's live `bulk_flow_count`; bounded experiments are unaffected, long-lived buckets may over-divide quantum, and exposing the live count is a deferred mainline-API extension. |
| Set-associative flow hash | 8-way set-associative lookup design (Hoiland-Joergensen et al. 2018 §3); structural-equivalence audit landed (ADR-0066). Underlying 5-tuple → 32-bit hash diverges at the bit level (Linux `jhash_3words`; ns-3 `Ipv4QueueDiscItem::Hash` over `Hasher` / Murmur3) — design-equivalent for set-associativity. | mainline ns-3 `FqCobaltQueueDisc::EnableSetAssociativeHash` + `SetWays`, exercised transitively by `DsHostIsolatedFqCobalt`; structural-property fixture S-17.33 |
| COBALT AQM target / interval | `5 ms` / `100 ms` defaults | mainline ns-3 `FqCobaltQueueDisc` (transitive) |
| MemLimit default + byte gate | `rate × 100 ms` default (floored at `4 MB`); `q->buffer_used + s_len > q->buffer_limit` triggers `cake_drop()` at enqueue | mainline ns-3 `FqCobaltQueueDisc::MemLimit` (byte gate enforced in `DoEnqueue` against `GetNBytes()`; drop reason `MEMLIMIT_DROP`); patch `0006`; ADR-0069 |
| `cake_overhead()` formula (raw / ATM / MPU) | per-packet wire-byte arithmetic | `DsCakeHelper::ConfigureLinkLayerOverhead` (statistical-mode rate adjustment) |
| `cake_advance_shaper` per-tin three-branch advance + `cake_enqueue` hard idle-reset (two-site catchup); per-tin and global virtual clocks | line-for-line correspondence with `sch_cake.c`; per-tin `time_next_packet`, global `q->time_next_packet`, per-packet `adj_len` from `cake_calc_overhead` | `RateBasedTinClock` POD + `RateBasedGlobalClock` POD inside `DsRateBasedShaperDispatcher`; selectable via `DsCakeHelper::ShaperMode::RateBased` (path β); fixtures S-17.39..44 |
| `tc-cake(8)` knob names + units | `overhead`, `atm`, `mpu`, `raw`, `wash`, `memlimit`, `bandwidth`, `ack-filter`, `ack-filter-aggressive`, `triple-isolate` | DiffServ4NS attributes use the same names where applicable |
| `split-gso` — enqueue-time GSO/GRO super-packet segmentation | `skb_shinfo->gso_segs`-driven segment expansion before per-flow enqueue | _(deferred — kernel-coupled; ns-3 has no GSO/GRO equivalent and no upstream proposal exists; see docs/superpowers/plans/2026-05-07-cake-op-flags-deferral.md)_ |
| `fwmark MASK` — netfilter conntrack mark used as a per-tin classification key | `skb->mark` set via netfilter conntrack or iptables `MARK` rules; CAKE consumes the masked value as a tin override | _(deferred — kernel-coupled; needs Ds5tupleMarkTag substrate primitive and a marking helper; design proposal post-paper Phase 2+)_ |

## Out-of-scope features (not inherited; not implemented in v1)

The remaining `tc-cake(8)` flags below depend on Linux kernel
facilities that have no direct ns-3 equivalent:

- `autorate-ingress` — adaptive shaping based on observed throughput.
- `ingress`-mode shaping — depends on `tc-mirred` ingress-qdisc semantics.

Both are catalogued as future-release work. (The previously-listed
`split-gso` and `fwmark` flags have been promoted to the inheritance
map above with explicit deferral labels; their per-flag architectural
findings live in the 2026-05-07 op-flags-deferral plan and ADR-0067.)

## DiffServ4NS-original (no Linux counterpart)

The following architectural primitives are DiffServ4NS-original; the
Linux CAKE qdisc is a single monolithic class with no equivalent
decomposition:

- The substrate boundary itself (DiffServ4NS as a substrate; CAKE as
  one client of that substrate, alongside L4S, classical AF/EF, and
  legacy DiffServ profiles).
- `DsTinShaperDispatcher` — work-conserving DRR across tins as a
  substrate primitive.
- `DsHybridLlqDispatcher` — LLQ-across-tins composition (Cisco MQC
  pattern).
- The per-tin TBF dispatcher — strict per-tin rate cap with mainline
  `TbfQueueDisc` as the inner qdisc.
- TBF-as-inner-qdisc pattern — a one-line guard in
  `TbfQueueDisc::DoDequeue` defers the pacing wake to the parent qdisc
  when TBF is nested. Carried as a local patch with an upstream MR
  prepared for review.
- `DsHostIsolatedFqCobalt` — nested-FqCobalt wrapper that delivers
  triple-isolate semantics by composition rather than by reimplementing
  per-flow queueing.
- Edge / core asymmetric queue-disc split — different queue-disc
  subclasses for the two router roles per RFC 2475 §2.3.1; predates
  Linux CAKE by 13 years and comes from the 2001 DiffServ4NS lineage.
- Substrate-component registries (AQMs and schedulers) and the
  manifest-driven loader pattern that exposes them.
- The `DsCakeHelper` API surface and preset family.
- The path-α / path-β / path-γ trio for per-tin rate capping,
  selectable via `DsCakeHelper::ShaperMode`:
  in-dispatcher token bucket (path α, default `ShaperMode::TokenBucket`),
  rate-based virtual clock with `cake_advance_shaper`-equivalent
  three-branch advance + `cake_enqueue`-equivalent hard snap-to-now on
  tin-empty transitions (path β, `ShaperMode::RateBased`), and mainline
  `TbfQueueDisc` as the inner qdisc (path γ, `ShaperMode::TbfInner`).
  Path β additionally binds an aggregate egress cap via the global
  virtual clock; per-packet `adj_len` flows from `cake_calc_overhead`
  in all three paths.

## Attribution

Files containing functionality directly informed by `sch_cake.c` carry
an attribution line in their GPL header:

```
// Functional semantics informed by Linux net/sched/sch_cake.c
// (GPL-2.0-or-later; Jonathan Morton, Toke Høiland-Jørgensen,
//  Kevin Darbyshire-Bryant, Ryan Mounce et al.)
```

DiffServ4NS source files are licensed `GPL-2.0`. `sch_cake.c` is
licensed `GPL-2.0-or-later`, which is strictly more permissive;
inheritance is licence-compatible. For files that import Linux
source-level fragments (none currently), the file would carry the
upstream copyright header alongside the existing DiffServ4NS / Nortel /
Imputato-Avallone lines.

## Maintenance

This document is updated whenever a new CAKE feature is implemented in
DiffServ4NS-CAKE. Each new row records the feature name, the inherited
spec elements, and the substrate primitive that hosts the feature.

When `sch_cake.c` upstream changes a value, formula, or semantic that
DiffServ4NS-CAKE has inherited, the maintenance task is:

1. Note the upstream change here (with the upstream commit SHA).
2. Decide whether DiffServ4NS-CAKE follows the change. Default: yes
   for bug fixes; case-by-case for behavioural changes.
3. Update the implementation if following; document the divergence
   explicitly if not.

This document is a living artefact of the layer-2 inheritance.
Layer-1 architectural decisions are recorded as the substrate's
architectural decision records.
