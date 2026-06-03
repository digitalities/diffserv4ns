# Changelog

All notable changes to the `diffserv` module (historically
`diffserv4ns3`, renamed 2026-04-19 per ADR-0036) are documented in
this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
This is a porting project (ns-2 to ns-3), so entries are grouped by development
phase rather than semantic version.  The original ns-2 module is
[DiffServ4NS-0.1](https://sourceforge.net/projects/diffserv4ns/) (2001).

Built and tested against ns-3-dev commit `d2add90b4` (ns-3.48).

---

## [Unreleased]

### Changed

- Re-pinned the ns-3 base to the ns-3.48 stable release tag (`d2add90b4`); full regression + audit suite re-validated, no behavioral changes.

---

## CAKE Tier 2 — ingress mode, autorate-ingress, and memlimit eviction (2026-05-23, ADR-0118)

### What landed

Three behavioural mechanisms from Linux `sch_cake.c` close the remaining
structural parity gap for the v1.1 release.  `DsRateBasedShaperDispatcher`
gains an **ingress shaping mode** (`SetIngressMode`) where per-tin and global
virtual clocks advance on dropped packets as well as on forwarded ones,
matching `sch_cake.c::cake_enqueue` `CAKE_FLAG_INGRESS` behaviour.
`DsHostIsolatedFqCobalt` gains a configurable **`MemLimit` byte cap**
(`DsHostIsolatedFqCobalt::MemLimit` ns-3 attribute) that evicts the
largest-backlog host bucket when the per-tin byte total exceeds the limit,
plus a new `bytesQueued` field on `DsPerHostStats` to track current backlog.
`DsCakeLiveBulkCounter` and the peak-rate tracker are complemented by
`DsCakeLinuxAutorateHook`, a **Linux-faithful peak-rate-EWMA autorate-ingress
closed loop** selectable at helper level via `SetAutorateImpl(AutorateImpl::Linux)`.

### New API surface

- `DsRateBasedShaperDispatcher::SetIngressMode(bool)` / `GetIngressMode()`
- `DsCakeHelper::SetEnableIngressMode(bool)` — helper-level ingress flag
- `DsHostIsolatedFqCobalt::MemLimit` attribute (UintegerValue, bytes; 0 = off)
- `DsPerHostStats::bytesQueued` field (current backlog, decremented on dequeue / eviction)
- `DsCakeLinuxAutorateHook` class (`model/ds-cake-linux-autorate-hook.h`)
- `DsCakeHelper::AutorateImpl` enum: `NoOp` (default), `Linux`
- `DsCakeHelper::SetAutorateImpl(AutorateImpl)` / `GetAutorateImpl()`

### Provenance

`sch_cake.c` at commit `67dc6c56b871` (frozen excerpt under
`provenance/linux-sch-cake-67dc6c56b871/`); iproute2 `q_cake.c` pinned at
`provenance/iproute2-q-cake-62d47c2dbc0eaecdd20c0e19406067488025e92e/`.

---

## CAKE Tier 1 — named link-layer / RTT presets and live bulk-flow counter (2026-05-23, ADR-0117)

### What landed

Four additive features close the gap between the `DsCakeHelper` API and the
named preset surfaces of Linux `tc-cake(8)`.  `ConfigureLinkLayerOverhead`
gains a **`ptm` flag** (T1.1) for PTM 64b/65b linear cell framing.  A
**`LinkPreset` enum** (15 named values from `Raw` to `IpoaLlcsnap`) with a
`SetLinkLayer` dispatcher (T1.2) maps Linux keyword names to their
`(overhead, atm, ptm, mpu)` tuples, sourced byte-for-byte from the
iproute2 `q_cake.c` keyword table.  An **`RttPreset` enum** (8 named
values from `Datacentre` to `Interplanetary`) with a `SetRttPreset`
dispatcher (T1.3) sets `Target` and `Interval` attributes on every
`FqCobaltQueueDisc` inner in one call.  An opt-in
**`DsCakeLiveBulkCounter`** substrate wrapper (T1.4) counts live bulk
flows per tin via an idle-window heuristic, replacing the `ever_seen`
approximation in `DsCakeStatsFormatter`.

### New API surface

- `DsCakeHelper::LinkPreset` enum (15 values: `Raw`, `Conservative`,
  `Ethernet`, `EtherVlan`, `Docsis`, `PppoePtm`, `PppoeVcmux`,
  `PppoeLlcsnap`, `PppoaVcmux`, `PppoaLlc`, `BridgedPtm`, `BridgedVcmux`,
  `BridgedLlcsnap`, `IpoaVcmux`, `IpoaLlcsnap`)
- `DsCakeHelper::SetLinkLayer(Ptr<DiffServEdgeQueueDisc>, LinkPreset)`
- `DsCakeHelper::RttPreset` enum (8 values: `Datacentre`, `Lan`, `Metro`,
  `Regional`, `Internet`, `Oceanic`, `Satellite`, `Interplanetary`)
- `DsCakeHelper::SetRttPreset(Ptr<DiffServEdgeQueueDisc>, RttPreset)`
- `ConfigureLinkLayerOverhead` gains a `bool ptm` parameter (5th positional,
  before the existing `bool raw = false`)
- `DsCakeLiveBulkCounter` class (`model/ds-cake-live-bulk-counter.h`)
- `DsCakeHelper::AttachLiveBulkCounter(Ptr<DiffServEdgeQueueDisc>, Time)`
- `DsCakeHelper::GetLiveBulkCount(Ptr<DiffServEdgeQueueDisc>, uint32_t slot)`

### Provenance

iproute2 `q_cake.c` `cake_link_layer_keywords[]` pinned at
`provenance/iproute2-q-cake-62d47c2dbc0eaecdd20c0e19406067488025e92e/q_cake.c`.
`sch_cake.c` RTT-preset target/interval pairs verified against commit
`67dc6c56b871`.

---

## AQM-eval characterisation suite (2026-04-28 → 2026-05-02, ADR-0048)

### What landed

A new `examples/aqm-eval-runner` binary that sweeps a 13×9 = 117-cell
matrix (13 AQMs across 9 RFC-7928-aligned scenarios) and emits per-cell
CSV + summary output for ellipse-style characterisation plots. Nine of
the AQM cells are mainline ns-3 queue discs (PfifoFast / RED / Adaptive
RED / CoDel / FQ-CoDel / PIE / FQ-PIE / Cobalt / FQ-Cobalt); four are
DS4-aware composites (DsRed / DsL4sWred / DsL4sCoupledOnly / DsCake).
The harness consumes the new `TcpRetransmitTag` (ns-3 mainline patch
`patches/ns3/0002`, MR !2830) for goodput accounting per RFC 7928 §3.2.

### New CLI surface

```cpp
./ns3 run "aqm-eval-runner \
    --scenario=tcp-friendly|steady|... \
    --aqm=ns3::FqCoDelQueueDisc|DsRed|DsL4sWred|DsL4sCoupledOnly|DsCake|... \
    --ecn=on|off|default \
    --simTime=10 \
    --totalRateBps=10000000 \
    --outDir=output/aqm-eval/run1 \
    --manifest=PATH"   # write registry as JSON and exit
```

`--aqm=list` and `--scenario=list` print the registered catalogue and
exit. Unknown values produce a one-line error with the valid choices.

### New source files

- `examples/aqm-eval-runner.cc` — the harness binary
- `examples/aqm-registry.{h,cc}` — `AqmEntry` struct + `AqmRegistry`
  singleton; single source of truth for the 13 AQM cells (dispatch
  name, file tag, display name, family, ECN-support flag, factory
  closure). Adding a cell is one entry in `AqmRegistry::AqmRegistry()`.

### Companion tooling

- `scripts/aqm-eval/aqm-manifest.json` — committed JSON regenerated
  by `aqm-eval-runner --manifest=PATH`; consumed by the Python plot
  scripts as the C++↔Python bridge for family / display-name / ECN
  metadata.
- `scripts/aqm-eval/aqm_manifest.py` — loader for the JSON manifest,
  used by `scripts/aqm-eval/{ellipse-plot,heatmap}.py`.

### Findings catalogued (handbook §13.4, paper §5.4)

- **F-A** — `DsRed` factory naive-instantiation 4-trap chain
  (RIO_C thMin=0; empty PHB table; DROP_TAIL semantics; default qlim).
  Mitigated in `aqm-registry.cc` `MakeDsRed()`.
- **F-C** — `FqPie` tcp-friendly RNG-bistable bandwidth-split lock
  (per-flow PIE drop randomness × DRR feedback under N=2 simultaneous
  start). Algorithmic property, not a defect.
- **F-D** — `FqCobalt` tcp-unresponsive sojourn × DRR coupling
  (per-flow Cobalt drops keep unresponsive queue non-empty when its
  DRR quantum fires; TCP loses rounds). Algorithmic property matching
  Linux `sch_fq_cobalt`.
- **A3-DsCake** — DsCake tcp-friendly low-flow-count hash-FQ artefact
  (8-way set-associative hash places both flows in same bucket under
  N=2). Documented characterisation, no fix recommended.

### --ecn=on|off|default flag (2026-05-02)

Optional override of the constructed qdisc's `UseEcn` attribute,
applied via `Object::SetAttributeFailSafe` after registry construction.
Default (`default`) is a no-op so the 117-cell main matrix reproduces
byte-for-byte. AQMs whose registry entry says `supportsEcn=false`
(PfifoFast, DsRed) emit a stderr note and fall back. Composite
wrappers without a direct `UseEcn` attribute (DsL4s, DsCake) keep
their built-in ECN policy. Summary records `ecn_requested=` and
`ecn_applied=` for traceability.

---

## CAKE Phase 10 — host-isolation (`DsHostIsolatedFqCobalt`) (2026-04-26, ADR-0047)

### What landed

`DsHostIsolatedFqCobalt`, a nested-FQ wrapper that adds CAKE-style
host-isolation on top of mainline `FqCobaltQueueDisc`. Per CAKE
RFC 8290bis §5.3, two-tier hashing keeps each (saddr) host's flows
in a per-host bucket; intra-host flow-FQ runs inside that bucket.
Effect: under multi-host load, each host receives ≥ 1/N of the link
regardless of its individual flow count.

### New API surface

- `DsHostIsolatedFqCobalt` queue disc (model + helper).
- `DsCakeHelper::SetAsCakeDiffserv{3,4,8}` accept a 6th positional
  `enableHostIsolation` flag.

### Validation

- Q-15.9 RRUL multi-host fairness test passes with 5.20× absolute
  improvement and >3× asymmetry vs flow-FQ-only baseline.
- Full extensive suite: 136 / 136 PASS.

### Provenance

ADR-0046 collided in numbering with the BUG-11 (D2-8) ADR landed
the same day; CAKE Phase 10 ADR was renumbered to ADR-0047.

---

## CAKE Phase 9 — Tin-shaping + rate-cap (`DsTinShaperDispatcher`) (2026-04-26, ADR-0045)

### What landed

`DsTinShaperDispatcher` gains share-proportional per-tin token-bucket
rate-capping under deficit-round-robin scheduling. Combined with the
DRR quanta from earlier phases, this delivers the
`tc-cake(8)` work-conserving form's behaviour where each tin receives
its configured share of the link, but unused share spills to other
tins via DRR rather than being capped.

### New API surface

- `DsTinShaperDispatcher::SetRateCap(uint32_t tinIdx, double bps)`
- `DsHybridLlqDispatcher::SetRateCap` + uniform SP/DRR gate
- `DsCakeHelper::SetAsCakeDiffserv{3,4,8}` `enableTinShaping` flag
- `TinTokenBucket` helper struct (uses `__int128` for refill arithmetic
  to avoid overflow on long idle gaps)

### Validation

- S-17.15 (TinTokenBucket unit), S-17.17 + S-17.18 (rate-cap
  invariants), S-17.20 + Q-15.8 (cross-implementation latency vs
  Linux `tc-cake` under tin-shaping). All pass.

---

## Scheduler attribute-driven construction (2026-04-20, ADR-0037)

### Breaking changes (construction API)

All 10 `DsScheduler` subclasses migrated from C++ constructor arguments
to ns-3 attribute-driven configuration. The public API change is:

```cpp
// Before (all scheduler types):
auto s = CreateObject<DsWfqScheduler>(numQueues, linkBandwidthBps);
auto llq = CreateObject<DsLlqScheduler>(numQueues, linkBandwidthBps, "WFQ");

// After:
auto s = CreateObjectWithAttributes<DsWfqScheduler>(
    "NumQueues",     UintegerValue(numQueues),
    "LinkBandwidth", DoubleValue(linkBandwidthBps));

auto llq = CreateObjectWithAttributes<DsLlqScheduler>(
    "NumQueues",     UintegerValue(numQueues),
    "LinkBandwidth", DoubleValue(linkBandwidthBps),
    "FqVariant",     EnumValue(DsLlqScheduler::FqVariant::WFQ));
```

### New attributes

- `DsScheduler::NumQueues` (construct-only, `UintegerValue`, default 1)
- `DsScheduler::LinkBandwidth` (settable any time, `DoubleValue` bps,
  default 0.0)
- `DsPriorityScheduler::WinLen` (construct-only, `DoubleValue` seconds,
  default 1.0)
- `DsLlqScheduler::FqVariant` (construct-only,
  `EnumValue<FqVariant>` WFQ/WF2Qp/SCFQ/SFQ, default WFQ)
- `DsL4sCoupledScheduler::L4sQueueIdx` (construct-only, `UintegerValue`,
  default 0)
- `DsL4sCoupledScheduler::BurstCap` (settable any time, `UintegerValue`,
  default 8)

### Why
`Config::Set` on `LinkBandwidth` now works after construction — unblocks
scenario-wide bandwidth sweeps via a single `Config::Set` or
`ConfigStore::Set` call. Eliminates the attribute-init-after-ctor trap
(see `reference_ns3_attribute_init_after_ctor`) for the scheduler
hierarchy. Aligns the module's construction idiom with the ns-3 spine
(`QueueDisc`, `Application`, `NetDevice`) for App Store / mainline-MR
readiness.

See ADR-0037 for full rationale and
`MIGRATION-from-ns2.md` for the updated ns-2 → ns-3 porting recipe.

---

## Style normalization — ns-3 style-guide baseline (2026-04-19)

### Changed
Three-commit baseline pass against the ns-3 coding style guide at
<https://www.nsnam.org/docs/contributing/html/coding-style.html> using
`ns3/ns-3-dev/utils/check-style-clang-format.py` (clang-format 20):

- `a918ce4` — removed Emacs modelines (`/* -*- Mode:C++; ... */`) from all
  97 C++ source files. Modelines are explicitly forbidden by the ns-3
  style guide.
- `904e4ef` — converted Doxygen `\tag` → `@tag` across 71 files
  (454 substitutions). ns-3 style requires `@` because clang-format
  recognises it as the Doxygen delimiter.
- `b27946f` — applied `check-style-clang-format.py --fix` across all 97
  files for whitespace / alignment / brace normalization against ns-3-dev's
  `.clang-format` (Microsoft base, Allman braces, 4-space indent,
  100-column limit). Net −2 122 lines from tighter alignments replacing
  custom continuation padding.

Post-pass, the tree reports clean on all nine rule categories
(`include-prefixes`, `include-quotes`, `doxygen-tags`, `licenses`,
`emacs`, `whitespace`, `tabs`, `formatting`, `encoding`).

### Why
Pre-App-Store-submission alignment and a clean baseline diff against
which future changes can be reviewed (see
`.claude/commands/cpp-review.md`). Prepares the tree for a future
mainline MR without burying style churn in substantive diffs.

Build clean (`ns3 build diffserv`), `diffserv` TestSuite PASS after
each commit.

## Module rename `diffserv4ns3` → `diffserv` (2026-04-19, ADR-0036)

### Changed
- ns-3 contrib module directory name: `diffserv4ns3` → `diffserv`.
- CMake `LIBNAME`: `diffserv4ns3` → `diffserv`.
- `scripts/fetch-ns3.sh` symlink target: `contrib/diffserv4ns3` → `contrib/diffserv`; retires the legacy symlink idempotently on first run.
- Build / test commands in all user-facing docs:
  `./ns3 build diffserv4ns3` → `./ns3 build diffserv`.
- C++ namespace `ns3::diffserv` unchanged (ADR-0003 §3 remains in force).
- Include-guard prefix `NS3_DIFFSERV_<FILE>_H` unchanged.
- File paths inside `src/ns-3/` unchanged.

### Why
Pre-App-Store-submission alignment. The short name matches the
namespace, matches every other ns-3 contrib (`olsr`, `dsr`, `aodv`,
…), and removes the `4ns3` suffix that added no signal a user sees
before the release Zenodo DOI, paper title, handbook, or per-file
GPL header. See ADR-0036 for the full rationale.

---

## PR3d -- Per-DSCP inner dispatch on DiffServEdgeQueueDisc (2026-04-19, ADR-0035)

### Added
- `DiffServEdgeQueueDisc::kMaxInnerSlots` = 8 parallel inner queueing-discipline slots on a single edge. Slot 0 carries the default (single-inner) path; additional slots host heterogeneous inners dispatched by DSCP.
- New multi-slot API on `DiffServEdgeQueueDisc`:
  - `SetInnerDiscAt(slot, inner)` / `GetInnerDiscAt(slot)` — per-slot install / accessor. Slots fill monotonically; no gaps.
  - `SetDscpToSlot(dscp, slot)` — configure the DSCP-to-slot dispatch map.
  - `GetNumInnerSlots()` — count of populated slots.
- New example `diffserv-hierarchical-l4s.cc` — Briscoe gap-1 demonstrator: DsL4sQueueDisc at slot 0 (EF/DSCP 46 + ECT(1)) + DsRedQueueDisc at slot 1 (BE/DSCP 0), strict-priority dequeue protects EF from BE queue-saturation. Reference numerics + summary plot at `output/comparison/gap1-hierarchical/`.
- Three new tests (`diffserv` EXTENSIVE 81 → 84):
  - **S-13.11** multi-slot DSCP routing — two Red inners at slots 0/1 verify `SetDscpToSlot` routes each DSCP to its configured slot.
  - **S-13.12** multi-slot strict-priority dequeue — enqueue low-priority (slot 1) first, then high-priority (slot 0); verify slot 0 drains completely before slot 1.
  - **S-13.13** backward compatibility — legacy `SetInnerDisc(inner)` call produces exactly one populated slot and preserves the pre-PR3d enqueue/dequeue path.

### Changed
- `DiffServEdgeQueueDisc::m_inner` → `std::array<Ptr<QueueDisc>, kMaxInnerSlots> m_inners`. The composer owns an array of slots indexed 0..7.
- `SetInnerDisc(inner)` re-anchors to `SetInnerDiscAt(0, inner)`; `GetInnerDisc()` shims to `GetInnerDiscAt(0)`. Pre-PR3d callers see identical behaviour.
- `DoEnqueue` looks up `m_dscpToSlot[finalDscp]` and delegates to `m_inners[slot]->Enqueue(item)`. Default DSCP-to-slot map sends every code point to slot 0, preserving the single-inner path.
- `DoDequeue` / `DoPeek` walk populated slots 0..N−1 in strict priority order and return the first non-empty slot's packet.
- `AssignStreams` per-slot cascades (sum across populated slots) + meter-slot cascade (unchanged from PR3b).
- `CheckConfig` asserts `>=1` populated slot and enforces one `QueueDiscClass` child per populated slot.

### Preserved (backward compat — byte-identity gate)
- All shipped examples, tests, and scenarios unchanged.
- S1 CSVs (6 files, both modes) byte-for-byte identical vs reference.
- S2 CSVs byte-for-byte identical with `--offeredBps=9000000`.
- `diffserv-l4s-fqcodel-comparison` 4-mode smoke: 1210 WRED / 1104 coupled-only / 0 fqcodel-inner drops + 455 fqcodel marks (verbatim match pre-PR3d on `17acb81`).

### Explicitly deferred (post-PR3d work)
- **PR3d-next**: configurable dequeue policy (WFQ, round-robin across slots). Current strict-priority is the only scenario-motivated choice for gap 1.
- **PR3d-probe-generalization**: per-slot `QueueStatsProvider` via edge. Shape-B probes default to slot-0 semantics; multi-slot probing uses `GetInnerDiscAt(i)` + direct `dynamic_cast`.
- **PR3d-core-symmetric**: `DiffServCoreQueueDisc` multi-slot widening. Core stays single-inner; no scenario motivates hierarchical core composition.
- **Additional dispatch keys** (ECT(1), 5-tuple, app-type). DSCP-only suffices for gap 1.

See the per-DSCP inner-dispatch decision record (ADR-0035) for full context.

---

## PR3g -- Monitor helper partial generalization (2026-04-19, ADR-0033 §PR3g supplement)

### Changed
- `DiffServMonitorHelper::Install(Ptr<QueueDisc>)` no longer asserts on non-Red inners. Any inner is accepted; Red-specific work (trace-source wiring, scheduler sampling) guards via `DynamicCast<DsRedQueueDisc>` and degrades to no-op + warning on foreign inners.
- `m_disc` member typed up to `Ptr<QueueDisc>`; separate `Ptr<DsRedQueueDisc> m_redDisc` (non-null iff Red) drives the Red-specific paths.
- `SampleQueueLength` uses ns-3 base-class `GetNQueueDiscClasses()` + per-class `GetNPackets()` — works for any inner.
- `SampleDepartureRate` gracefully no-ops on non-Red inners (DsScheduler is RED-specific; L4S has its own DualPI2 controller).
- Trace-source wiring (DsEnqueue / DsDequeue / DsDrop) stays Red-specific; foreign inners log a warning. A generic `QueueTraceProvider` would need design work (L4S's ECT(1)-based signals vs Red's DSCP-aware signals) and is deferred until a concrete scenario motivates the reduction.

### Verified
- `diffserv` EXTENSIVE 81/81, `diffserv-l4s` 12/12, `diffserv-per-flow-classifier` 5/5 (unchanged).
- S1 CSVs (6 files, both modes) byte-for-byte identical.
- S2 CSVs byte-identical with `--offeredBps=9000000`.
- Existing examples behave exactly as before (all use Red inners; Red path unchanged).

---

## PR3e + PR3f -- Strip Red-specific forwarders + QueueStatsProvider (2026-04-19, ADR-0033)

### Changed (PR3e -- Shape-A strip)
- `DiffServEdgeQueueDisc` and `DiffServCoreQueueDisc` no longer expose Red-specific write-time forwarders: `AddPhbEntry`, `SetNumQueues`, `SetNumPrec`, `SetQueueLimit`, `SetMredMode`, `SetScheduler`, `SetMeanPacketSize`, `SetQueueBandwidth`, `ConfigQueue`, `LookupPhb`. Callers configure the inner directly.
- `DiffServHelper::AddPhbEntry`, `SetScheduler`, `ConfigQueue`, `SetMredMode` signatures narrowed from `Ptr<QueueDisc>` (with `DS4_DISPATCH_OR_ABORT` dispatch) to `Ptr<DsRedQueueDisc>` — callers pass the inner disc directly.
- 5 examples + 2 test files + 1 helper migrated to new pattern. ~95 call-site rewrites.

### Added (PR3e)
- `DiffServHelper::InstallRedInner(edge|core)` — sugar for the common case: creates a fresh `DsRedQueueDisc`, wires it as the inner via `SetInnerDisc`, and returns a typed `Ptr<DsRedQueueDisc>` handle.

### Changed (PR3f -- Shape-B via interface)
- Edge/core Shape-B runtime probes (`GetNumQueues`, `GetVirtualQueueLen`, `PrintStats`) now go through the inner-agnostic `QueueStatsProvider` interface via `dynamic_cast<QueueStatsProvider*>(PeekPointer(m_inner))`. L4S and any future inner that implements the interface answer polymorphically.
- `GetScheduler` and `PrintPhbTable` remain Red-specific (truly RED-only concepts).

### Added (PR3f)
- `src/ns-3/model/queue-stats-provider.h` — abstract mixin interface with `GetNumQueues()`, `GetQueueLen(queue, prec)`, `PrintStats()`.
- `DsRedQueueDisc` and `DsL4sQueueDisc` multiply-inherit `QueueStatsProvider`.
- Unit test `S-13.9 Shape-B probes via QueueStatsProvider (PR3f / ADR-0033)` verifies polymorphic access when an edge wraps a `DsL4sQueueDisc` inner.
- ADR-0033 documents the cleanup decision, caller-semantics table, and protocol-audit findings.

### Verified
- `diffserv` EXTENSIVE 81/81 PASS (80 pre-cleanup + S-13.9).
- `diffserv-l4s` 12/12, `diffserv-per-flow-classifier` 5/5.
- S1 CSVs (6 files, both modes) byte-for-byte identical.
- S2 CSVs byte-for-byte identical with `--offeredBps=9000000`.
- `git status ns3/ns-3-dev/`: only the two `patches/ns3/*.patch` modifications.

### Breaking API changes
- Out-of-tree scripts using `edge->Set*` or `core->Set*` Shape-A methods need a one-line update: use `helper.InstallRedInner(edge)` + typed handle, or `DynamicCast<DsRedQueueDisc>(edge->GetInnerDisc())`.

---

## PR3c + BUG-10 follow-up (2026-04-19, ADR-0032)

### Changed
- `DiffServEdgeQueueDisc::m_inner` widened from `Ptr<DsRedQueueDisc>` to `Ptr<QueueDisc>`. `SetInnerDisc` / `GetInnerDisc` signatures widened to match. Enables installing `DsL4sQueueDisc` (or any `QueueDisc` subclass) as the edge's queueing pipeline.
- `DiffServCoreQueueDisc` widened symmetrically.
- Red-specific forwarders on edge/core (`AddPhbEntry`, `SetScheduler`, `ConfigQueue`, `SetMredMode`, `SetNumPrec`, `SetQueueLimit`, `SetMeanPacketSize`, `SetQueueBandwidth`, `SetNumQueues`, `LookupPhb`, `GetVirtualQueueLen`, `GetScheduler`, `GetNumQueues`, `PrintPhbTable`, `PrintStats`) now `DynamicCast<DsRedQueueDisc>` on access; void setters assert on non-Red, const getters return conservative defaults. Foreign inners must be configured via their own API before `SetInnerDisc`.
- `AssignStreams` on edge/core now branches by inner type: Red → DsRedSubQueue leaves (PR3a pattern), L4S → delegates to `DsL4sQueueDisc::AssignStreams`, other → no RNG.
- `DiffServMonitorHelper` asserts on non-Red edge/core inners (sub-queue sampling infrastructure is Red-specific).
- 5 examples (1, 2, 2-fullscale, 3, 3-fullscale) updated to `DynamicCast<DsRedQueueDisc>` the `GetInnerDisc()` return.

### Added
- `DiffServPolicyClassifier::GetUsedMeterTypes()` — returns set of `MeterType` values in the policy table.
- `DiffServEdgeQueueDisc::CheckConfig` pre-materialises meter slots for every MeterType in the policy table, closing the BUG-10 lazy-create timing gap so `edge->AssignStreams(N)` reaches helper-path (non-pre-injected) meters too.
- Unit test `S-13.7 Edge with DsL4sQueueDisc inner (PR3c smoke)` verifies end-to-end enqueue/dequeue through an L4S inner with DSCP stamping + ECT(1) preservation.
- Unit test `S-13.8 Meter cascade reaches helper-path meters (BUG-10 follow-up)` verifies the timing-gap fix via helper-path TSW2CM configuration.
- ADR-0032 documents the widening decision, protocol-audit findings, and caller-semantics table.
- `docs/HISTORICAL_BUGS.md` §BUG-10 extended with a "Follow-up fix" section.

### Verified
- `diffserv` EXTENSIVE 80/80 PASS (78 pre-PR3c + S-13.7 + S-13.8).
- `diffserv-l4s` 12/12, `diffserv-per-flow-classifier` 5/5.
- S1/S2 CSVs byte-identical vs committed references (opt-in design; existing scenarios unchanged).
- `diffserv-l4s-fqcodel-comparison` 4-mode smoke matches PR3b baseline verbatim (378 / 244 / 0 drops + 67 FqCoDel marks).
- `git status ns3/ns-3-dev/`: only the two `patches/ns3/*.patch` modifications.

---

## PR3b -- Meter as strategy (2026-04-18, ADR-0031)

### Changed
- `DiffServPolicyClassifier` no longer owns its meter pool. The private `m_meterPool[7]` array and `GetOrCreateMeter(MeterType)` switch were removed; the classifier now consults an `EdgeMeterProvider` hook to resolve a `Ptr<Meter>` per `MeterType`.
- `ApplyPolicy` dispatch path unchanged externally; internally it calls `m_meterProvider->GetMeter(policy->meter)` instead of the private pool.
- `DiffServEdgeQueueDisc` multiply-inherits `public EdgeMeterProvider` and owns `m_meters[7]`; wires the owned classifier's provider hook to `this` in the constructor.

### Added
- `src/ns-3/model/edge-meter-provider.h` — abstract lookup interface decoupling classifier dispatch from edge-disc type.
- `DiffServEdgeQueueDisc::SetMeter(MeterType, Ptr<Meter>)` — pre-Initialize strategy injection.
- `DiffServEdgeQueueDisc::GetMeter(MeterType)` — lazy-creates default implementations (mirrors pre-PR3b classifier behaviour).
- `DiffServPolicyClassifier::SetMeterProvider(EdgeMeterProvider*)` — non-owning hook wired by the owning edge disc at construction.
- Unit test `S-13.5 Meter strategy injection (ADR-0031)` verifies both injection and lazy-creation paths.
- ADR-0031 documents the decision, protocol-audit findings (including the inherited `Tsw2cm`/`Tsw3cm`/`Fw` `AssignStreams` cascade gap carried over verbatim), and the caller-semantics table.

### Verified
- `diffserv` EXTENSIVE 77/77 PASS (76 pre-PR3b + new S-13.5), `diffserv-l4s` 12/12, `diffserv-per-flow-classifier` 5/5.
- S1 CSVs (6 files, both modes) byte-for-byte identical vs committed references.
- S2 CSVs (`throughput.csv`, `coupling.csv`) byte-for-byte identical with `--offeredBps=9000000`.
- `git status ns3/ns-3-dev/`: only the two expected `patches/ns3/*.patch` modifications.

---

## PR3a -- Edge and core queue disc composition (2026-04-18, ADR-0030)

### Changed
- `DiffServEdgeQueueDisc` re-parented from `DsRedQueueDisc` to `QueueDisc`; composes a single `Ptr<DsRedQueueDisc> m_inner` as the queueing pipeline.
- `DiffServCoreQueueDisc` re-parented in the same shape; no added behaviour over the inner (pure BA delegator per thesis §3.3.1).
- `DsRedQueueDisc::DoEnqueue` now reads `DiffServDscpTag` first, symmetric with its tag-first `DoDequeue`; enables clean composer delegation (+4 LOC).
- `DiffServHelper::{AddPhbEntry, SetScheduler, ConfigQueue, SetMredMode}` widened from `Ptr<DsRedQueueDisc>` to `Ptr<QueueDisc>`, dispatch by DynamicCast.
- `DiffServMonitorHelper::Install` widened from `Ptr<DsRedQueueDisc>` to `Ptr<QueueDisc>`; internally resolves to the inner for trace connection and sub-queue sampling.
- Examples (1, 2, 2-fullscale, 3, 3-fullscale) updated to route per-queue sampling through `edge->GetInnerDisc()->GetQueueDiscClass(i)`.

### Added
- `DiffServEdgeQueueDisc::SetInnerDisc`, `GetInnerDisc`, `AssignStreams` (stream cascade into inner sub-queues).
- `DiffServCoreQueueDisc::SetInnerDisc`, `GetInnerDisc`, `AssignStreams`.
- ADR-0030 documents the composition, caller-semantics table (with PR3b note column), and forward scope.

### Verified
- `diffserv` EXTENSIVE 76/76 PASS, `diffserv-l4s` 12/12, `diffserv-per-flow-classifier` 5/5.
- S1 CSVs (6 files, both modes) byte-for-byte identical vs committed references.
- S2 CSVs byte-for-byte identical with `--offeredBps=9000000`.
- 4-mode FqCoDel comparison example: distinct numerics per mode matching PR2 baseline.

---

## Phase 7 -- Documentation and validation (2026-04-16, in progress)

### Added
- `ns3::diffserv::LoadEmpiricalCdfFromFile` helper (`model/empirical-cdf-loader.{h,cc}`) — parses ns-2's `value count cumprob` 3-column format into an `EmpiricalRandomVariable` (ignoring middle column, matching ns-2 `loadCDF`)
- 4 RealAudio empirical CDFs shipped under `examples/example-3-data/` (userintercdf1, sflowcdf, flowdurcdf, fratecdf) — replaces the previous `UniformRandomVariable(1.1, 8.8) kbps` approximation in scenario 3 Gold traffic (plot-audit follow-up W7b)
- `diffserv-empirical-cdf-loader` test suite: 5 unit tests validating sampled means against analytical CDF means

### Changed
- `diffserv-example-2-fullscale.cc` Telnet traffic generator tuned to emit ~200 kbps aggregate during the 0–50 s warmup, matching ns-2.35 `Application/Telnet` (was ~25 kbps). `PacketSize 64→512` + `OnTime 0.01→0.16` on both DiffServ and stock-RED code paths (plot-audit follow-up W7a)
- `diffserv-example-3-fullscale.cc` Gold/RealAudio traffic generator: 300 users × sflow-sampled sequential flows × flowdur-sampled durations × frate-sampled rates, mirroring ns-2 scenario-3.tcl §Traffic 2

- Port-based classification: `MarkRule` extended with `srcPort`/`dstPort` fields (RFC 2475 section 2.3.1)
- `DiffServHelper::AddMarkRuleWithPorts()` for transport-layer port matching
- ADR-0019: packet-size accounting divergence for fair-queueing schedulers
- ADR-0020: edge enqueue tag removal and RED threshold configuration
- Test S-13.4: port-based mark rule classification
- Test Q-2: example-2 three-class coexistence (port-based)
- Test Q-5: AF drop-precedence differentiation
- Test Q-6: three-class (EF/AF/BE) coexistence under congestion
- Test Q-7: performance regression baseline (throughput + wall-clock + memory)
- 76 tests passing (up from 70)
- `chang-comparison.cc` example: reproduces Chang et al. (SIMULTECH 2015) WFQ/WRR bandwidth ratio validation
- Multi-scheduler comparison script (`scripts/multi-scheduler-comparison.py`): 5 schedulers × 7 pkt sizes
- `doc/diffserv.rst`: ns-3 module reference documentation
- `MIGRATION-from-ns2.md`: ns-2 Tcl → ns-3 C++ migration guide with 30+ command mappings
- Attribution audit: Nortel copyright corrected (31 files derived from pristine ns-2.29, 26 removed)
- Paper validation section: 4-level structure (RFC vectors, cross-simulator, real-network, Chang comparison)
- `DsScfqScheduler::SetLogStream()`: per-packet CSV decision log for SCFQ diagnostics (labels, queue depths, dequeue ratios)
- ADR-0021: root cause analysis of self-clocked FQ OWD divergence (ns-2 UDP agent header overhead)

- `diffserv-example-2.cc`: three-class DiffServ scenario (Premium/Gold/BE) with RIO-C (WRED), TSW2CM, TCP traffic
  - Port-based classification: Telnet=port 23, FTP=port 21 (RFC 2475 §2.3.1)
  - Standard ns-3 apps: `OnOffApplication` (Telnet), `BulkSendApplication` (FTP)
  - 3 schedulers: PQ, SCFQ (weights 3:10:7), LLQ (SFQ, 1.7 Mbps)
  - 6 trace files: ServiceRate, ClassRate, QueueLen, VirQueueLen, OWD, IPDV
- `DsRedQueueDisc::SetQueueBandwidth()` — public API for per-queue RED bandwidth (ns-2 `setQueueBW`)
- `DsRedQueueDisc::GetVirtualQueueLen()` — public API for per-precedence queue length queries (ns-2 `getVirtQueueLen`)
- `kAppTypeTelnet`, `kAppTypeFtp` application-type constants in `diffserv-constants.h`
- Q-2 spec expanded with concrete metrics (Q-2.1 through Q-2.4)
- `diffserv-example-3.cc`: complete five-class service model (thesis §4.3 reconstruction)
  - Premium (EF/VoIP), Gold (AF11/AF12 streaming), Silver (AF21/AF22 Telnet+FTP), Bronze (AF31 HTTP), Best Effort
  - LLQ scheduler with SFQ sub-scheduler (weights 3:3:3:1)
  - RIO-C (Gold), WRED (Silver, Bronze), tail-drop (Premium, BE)
  - Original Tcl script was never published; reconstructed from thesis Table 4.5
- Q-10 spec added for Scenario 3 validation (Q-10.1 through Q-10.5)

### Fixed
- Edge enqueue removes stale `DiffServDscpTag` before re-marking (ADR-0020)
- `DiffServEdgeQueueDisc` meter accounts for wire bytes (including the IP header) so the token-bucket admits the same byte count as ns-2.35 after the UDP-header fix. Closed the Scenario 3 Premium +55.5 % divergence to +0.1 % (commit `0c660ec`).
- Scenario 3 `scenario-3.tcl`: BG-CBR background source is explicitly routed to DSCP 0 (BE) instead of falling through the Premium catch-all classifier (commit `83e4a4c`).

### Removed
- `Ns2CompatSchedulerSize` attribute on `DsRedQueueDisc` and the `--ns2compat` CLI flag on `diffserv-example-1.cc`. Rationale: the attribute was a short-lived bridge added while the ns-2.35 UDP-header accounting was unresolved. After the wire-byte meter fix and the ns-2.35 port's own 28-byte header correction (BUG-5 in `docs/HISTORICAL_BUGS.md`), both simulators agree on the same wire-byte semantics without a compatibility shim. Removing the attribute drops a confusing knob that future contributors would otherwise have to reason about.

### Documented
- `doc/diffserv.rst` updated with all three example scenarios and validation results
- Thesis §4.3 reconstruction note: original Tcl script was never published, Scenario 3 reconstructed from prose and Table 4.5
- Thesis §4.2 vs example-2.tcl clarification: released example-2 is a 13-node scenario, not the 469-node scenario described in the thesis
- Thesis §4.2 full-scale reconstruction (`ns2/diffserv4ns/examples/example-2-fullscale/`, `ns3/diffserv4ns3/examples/diffserv-example-2-fullscale.cc`): 469-node Scenario 2 with 6-way WRED parameter sweep (staggered / partially-overlapped / overlapped from Figure 4.3). Qualitative thesis claims (DP0 < DP1 < DP2 ordering, staggered gives max differentiation, overlapped gives proportional sharing) reproduced in all six sets in both simulators.
- Quantitative Table 4.4 reproduction (updated 2026-04-17 to thesis-exact `numHttp=400`, `simTime=5000`; also corrected to exclude the metric-mismatched goodput column from the tolerance count — see metric note below): **ns-2: 29 / 36 cells within tolerance (80.6 %). ns-3: 29 / 36 cells within tolerance (80.6 %)** under tolerances caPL ≤ 2 pp, boPL ≤ 0.5 pp. Cross-simulator agreement at thesis-exact config: mean |ΔcaPL| = 0.70 pp (was 3.2 pp at the earlier `numHttp=200` workaround), mean |ΔboPL| = 0.03 pp, mean |Δdelivery_ratio| = 0.01. Both simulators agree on tolerance count and on which specific cells are out of tolerance (predominantly DP2 caPL + overlapped-WRED DP1 — the HTTP traffic-model-dominated cells). Remaining divergence from thesis traces to the PagePool/WebTraf approximation via bulk TCP and the unspecified FTP burst profile, documented in `ns2/diffserv4ns/examples/example-2-fullscale/README`. Full comparison in `output/comparison/scenario-2/table-4-4-comparison.md`.
- Metric note: thesis Table 4.4 "goodput" is a per-DSCP TCP retransmission-bytes ratio `TCPbGoTX(x) / (TCPbGoTX(x) + TCPbReTX(x))` measured at the sender (Andreozzi-2001 §3.3.4). Our per-queue `delivery_ratio` (`TxPkts / TotPkts` at AF queue) coincides with it only under static DSCP classification; thesis uses rate-metered classification where retries migrate DPs. `delivery_ratio` is reported against `thesis_goodput` as reference only; it is not counted toward the pass/fail tolerance. Fully thesis-comparable goodput requires per-DSCP TCP retransmit-byte instrumentation (future work).
- Scaling bug surfaced by the Scenario 2 reconstruction: ns-3 mainline `TcpSocketBase::PersistTimeout` unconditionally dereferences the return of `CopyFromSequence`, which can be `nullptr` when the tx buffer has drained past `m_nextTxSequence` — reproducibly SIGSEGVs at ≥ 250 concurrent BulkSend TCPs. Fix is an 8-line null-guard + zero-length probe fallback (RFC 1122 §4.2.2.17), carried as `patches/ns3/0001-tcp-persist-empty-buffer.patch` and auto-applied by `scripts/fetch-ns3.sh`. Upstream MR artifacts at `docs/upstream/ns3-tcp-persist-empty-buffer.md`. Decision recorded in ADR-0022. Documented as `docs/HISTORICAL_BUGS.md` BUG-6. The earlier 2026-04-17 memo that attributed the crash to our Phase 2 `DsRedQueueDisc` was incorrect — verified by reproducing the identical crash frame with stock `ns3::RedQueueDisc` at aggressive thresholds.
- Qualitative validation against thesis: all DiffServ behavioral properties reproduced (LLQ priority, RIO-C/WRED differentiation, SFQ excess bandwidth sharing, TB policing)
- ns-2's deliberate network-layer abstraction: `hdr_cmn::size()` carries application payload only, with no IP/UDP header overhead consuming link bandwidth (users must add 28 bytes manually if desired); ns-3's realistic header model exposes this as EF queue growth under self-clocked FQ schedulers at the fair-share boundary; confirmed by ns-3-users community as a by-design property of ns-2 (ADR-0021)

---

## Phase 6 -- Monitoring and FWMeter (2026-04-15)

### Added
- `FWMeter` (I-2.6) -- Short Flow Differentiating policy, ported from Nortel/Chen-Heidemann `SFDPolicy`; `model/fw-meter.{h,cc}`
- `DiffServStatistics` -- per-DSCP packet/byte counters with arrival/departure/drop tracking; `model/diffserv-statistics.{h,cc}`
- `DiffServMonitorHelper` -- declarative setup for trace-source connections; `helper/diffserv-monitor-helper.{h,cc}`
- `DsEnqueue`, `DsDequeue`, `DsDrop` TracedCallback sources on `DsRedQueueDisc`
- ADR-0018: monitoring via TracedCallback (closes ADR-0008)
- 5 FWMeter tests (S-15.2, S-15.3 statistics tests)
- 70 tests passing (up from 63)
- Feature-complete milestone: all DiffServ4NS-0.1 functionality ported

---

## Phase 5 -- Fair-queueing schedulers (2026-04-15)

### Added
- `DsScfqScheduler` -- Self-Clocked Fair Queueing; `model/ds-scfq-scheduler.{h,cc}` (S-9.1, S-9.2, S-9.3)
- `DsSfqScheduler` -- Start-time Fair Queueing; `model/ds-sfq-scheduler.{h,cc}` (S-10.1, S-10.2, S-10.3)
- `DsWf2qPlusScheduler` -- Worst-case Fair Weighted Fair Queueing; `model/ds-wf2qp-scheduler.{h,cc}` (S-8.1, S-8.2)
- `DsWfqScheduler` -- Weighted Fair Queueing with GPS reference model; `model/ds-wfq-scheduler.{h,cc}` (S-7.1, S-7.3, S-7.4)
- `DsLlqScheduler` -- Low Latency Queueing (PQ + configurable PFQ); `model/ds-llq-scheduler.{h,cc}` (S-11.1, S-11.2, S-11.3)
- `DsScheduler::OnEnqueueWithTime()` and `SetLinkBandwidth()` interface extensions (ADR-0017)
- Q-3 WFQ fairness test and Q-4 WF2Q+ delay comparison test
- ADR-0017: scheduler interface extension for fair-queueing
- 63 tests passing (up from 47)

---

## Phase 4 -- End-to-end validation: example-1 (2026-04-15)

### Added
- `examples/diffserv-example-1.cc` -- thesis Scenario 1 (EF + 4xBE, 10 Mbps bottleneck, PQ scheduler)
- `DiffServSendTimeTag` -- per-packet send-time stamp for OWD/IPDV measurement; `model/diffserv-send-time-tag.{h,cc}`
- ns-2 baseline infrastructure: `fetch-ns2-allinone.sh`, `patch-ns2-diffserv.sh`, `build-ns2-allinone-docker.sh`
- Trace analysis tools: `scripts/parse-traces.py`, `scripts/compare-traces.py`
- ADR-0015: simplex-link mapping to ns-3
- ADR-0016: NetDevice queue structural delay
- Q-tier results: 6/9 metrics within tolerance, 3/9 documented structural divergences (no port defects)
- MVP milestone reached
- 47 tests passing (unchanged from Phase 3)

### Fixed
- BE starvation due to queue disc configuration ordering (pre-configure before Initialize)
- OWD measurement: account for device queue size and subtract transmission time
- Departure rate units corrected from packets/s to bits/s

---

## Phase 3 -- Edge and core routers (2026-04-15)

### Added
- `DiffServEdgeQueueDisc` -- classification, metering, marking on ingress; `model/diffserv-edge-queue-disc.{h,cc}`
- `DiffServCoreQueueDisc` -- DSCP-aware re-marking on core hops; `model/diffserv-core-queue-disc.{h,cc}`
- `DiffServPolicyClassifier` -- multi-field classifier matching MarkRule entries; `model/diffserv-policy-classifier.{h,cc}`
- `MarkRule` -- (srcAddr, dstAddr, protocol, appType) tuple for classification; `model/mark-rule.h`
- `DiffServAppTypeTag` -- packet tag for application-type classification; `model/diffserv-app-type-tag.{h,cc}`
- `DiffServDscpTag` -- packet tag carrying per-hop DSCP; `model/diffserv-dscp-tag.{h,cc}`
- `DiffServHelper` -- typed C++ helper replacing Tcl `addPolicyEntry`/`addPolicerEntry` parsing; `helper/diffserv-helper.{h,cc}`
- ADR-0014: DSCP modification via tag + dequeue const_cast
- S-13, S-14, S-16 tests (8 new tests)
- Edge-core E2E acceptance test (224 packets, DSCP 46 preserved)
- 47 tests passing (up from 39)

---

## Phase 2 -- Queue management and schedulers (2026-04-14 to 2026-04-15)

### Added
- `DsRedSubQueue` -- per-physical-queue with RIO-C, RIO-D, WRED, and DropTail modes; `model/ds-red-sub-queue.{h,cc}`
- `DsRedQueueDisc` -- classful outer queue disc with PHB table and scheduler delegation; `model/ds-red-queue-disc.{h,cc}`
- `DsScheduler` -- abstract base with departure rate metering; `model/ds-scheduler.{h,cc}`
- `DsRoundRobinScheduler`; `model/ds-rr-scheduler.{h,cc}`
- `DsWeightedRoundRobinScheduler`; `model/ds-wrr-scheduler.{h,cc}`
- `DsWeightedInterleavedRoundRobinScheduler`; `model/ds-wirr-scheduler.{h,cc}`
- `DsPriorityScheduler`; `model/ds-pq-scheduler.{h,cc}`
- `MredMode`, `PktResult`, `PhbEntry` constants in `diffserv-constants.h`
- ADR-0012: scheduler strategy pattern
- ADR-0013: direct port of ns-2 RED/RIO
- S-5, S-6, S-12 tests (9 new tests) and E2E toy scenario
- 39 tests passing (up from 30)

---

## Phase 1 -- Meters (2026-04-14)

### Added
- `Meter` abstract base class; `model/meter.{h,cc}`
- `DumbMeter` -- pass-through meter; `model/dumb-meter.{h,cc}`
- `TokenBucketMeter` -- single-rate token bucket; `model/token-bucket-meter.{h,cc}`
- `SrTcmMeter` -- Single Rate Three Colour Marker (RFC 2697); `model/sr-tcm-meter.{h,cc}`
- `TrTcmMeter` -- Two Rate Three Colour Marker (RFC 2698); `model/tr-tcm-meter.{h,cc}`
- `Tsw2cmMeter` -- Time Sliding Window two-colour (RFC 2859); `model/tsw2cm-meter.{h,cc}`
- `Tsw3cmMeter` -- Time Sliding Window three-colour (RFC 2859); `model/tsw3cm-meter.{h,cc}`
- `PolicyEntry`, `PolicerEntry` data structures; `model/policy-entry.h`
- RFC 2697/2698 conformance test vectors (25 vectors); `test/rfc-test-vectors.{h,runner.cc}`
- ADR-0010: explicit time parameter in meters
- ADR-0011: Colour enum over DSCP return
- 30 tests passing (S-1, S-2, S-3, S-4)

---

## Phase 0 -- Project setup (2026-04-14)

### Added
- Repository structure: `ns2/` (read-only 2001 source), `ns3/diffserv4ns3/` (new module), `specs/`, `docs/`, `provenance/`
- Three-tier EDD spec suite: `specs/01-intent.md`, `specs/02-structural.md`, `specs/03-quality.md`
- `scripts/fetch-ns2.sh` -- download ns-allinone-2.29.3
- `scripts/fetch-ns3.sh` -- clone ns-3-dev and create contrib symlink (ADR-0009)
- `docs/NS2_PATCHES.md` -- catalogue of all ns-2.29 modifications in DiffServ4NS
- `docs/HISTORICAL_BUGS.md` -- 2001-era bugs not reproduced in port
- `diffserv-constants.h` -- DSCP code points, queue limits, colour enum
- `CMakeLists.txt` -- ns-3 `build_lib()` configuration
- Module scaffold compiles and test suite registers in ns-3
- ADRs 0001 through 0009
