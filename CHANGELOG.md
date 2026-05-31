# Changelog

All notable user-visible changes to the DiffServ4NS module suite — the
2001 ns-2 original (preserved verbatim under `src/ns-2.29/`), the 2026
ns-2.35 port layer (`src/ns-2.35/`), and the 2026 ns-3 port
(`src/ns-3/`, contrib module name `diffserv`).

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Built and tested against ns-3-dev commit `cc48bf5c1` (ns-3.47 + 83).

---

## v1.0-icns3-submission — 2026-05-24

The first public release of the ns-3 port, accompanying the ICNS3 2026
paper submission.  The substrate composes DiffServ, L4S, and CAKE as
three first-class clients of one ns-3 module.

### Substrate architecture

- Four-primitive substrate (Classify-and-Meter, Mark-and-Route,
  per-class Slot Array, across-slot Service Policy) hosts the three
  clients side by side.
- Per-DSCP inner dispatch on `DiffServEdgeQueueDisc`: up to eight
  parallel inner queueing-discipline slots, indexed by a configurable
  DSCP-to-slot map.  Enables hierarchical compositions where one edge
  routes EF traffic to a DualPI2 inner, AF traffic to a RED inner, and
  best-effort to a separate inner — under one classifier and one PHB
  table.
- `DsSlotDispatcher` strategy abstraction with two concrete
  implementations: `DsStrictPriorityDispatcher` (the default,
  byte-identical to all DiffServ scenarios) and
  `DsTinShaperDispatcher` (deficit round-robin with per-slot quanta,
  used by the CAKE client).

### DiffServ client (canonical, ported from the 2001 module)

- Six metering algorithms: token bucket, srTCM (RFC 2697), trTCM
  (RFC 2698), Time-Sliding-Window two- and three-colour (RFC 2859),
  and a per-flow byte-accounting Fair Weighted meter (ported from
  Chen-Heidemann SFDPolicy).
- Multi-field classification on the edge: address, port, and
  application-type tuples via `DiffServHelper::AddMarkRuleWithPorts`.
- Multi-precedence RED queue management: RIO-C, RIO-D, WRED, and
  drop-tail modes with up to three drop-precedence levels.
- Nine schedulers: round-robin, weighted round-robin, weighted
  interleaved round-robin, strict priority, WFQ, SCFQ, SFQ, WF2Q+,
  and Low-Latency Queueing (LLQ; PQ + configurable fair-queueing
  variant).
- Typed `DiffServHelper` API replaces Tcl `command()` / `argv`
  parsing with `AddPolicyEntry`, `AddPolicerEntry`, `AddMarkRule`,
  and per-queue configuration helpers.

### L4S client (RFC 9332 native)

- `DsL4sQueueDisc` integrates a Dual-Queue Coupled AQM (DualPI2) as
  an inner slot.  ECN-based fast-lane classification (ECT(1) routes
  to the L-queue; all others route to the C-queue); DSCP is left
  free for upstream PHB selection.
- RFC 9332 §A.2 Proportional-Integral controller with reference gains
  α = 0.3 s⁻¹, β = 0.03 s⁻¹.  L-queue mark probability
  p_L = max(k·p′, CE-mark threshold); C-queue drop probability
  p_C = (k·p′)² with default k = 2.  The squared-coupling residual is
  validated to zero.
- Starvation-safe scheduler `DsL4sCoupledScheduler` interleaves
  C-queue bursts when the L-queue's burst credit (default 8 packets)
  is exhausted.
- Twelve structural tests in the `diffserv-l4s` suite cover routing,
  coupling, CE-mark idempotence, and controller dynamics.

### CAKE client (Linux tc-cake equivalent)

- `DsCakeHelper` exposes the five Linux DiffServ profiles
  (`besteffort`, `precedence`, `diffserv3`, `diffserv4`, `diffserv8`)
  with byte-exact `tc-cake(8)` DSCP-to-tin maps.
- Four service modes selectable via two boolean flags (`enableLlq`,
  `enableTinShaping`) spanning the work-conservation × latency-
  protection × cap-enforcement trade-off surface.  The
  `enableLlq && enableTinShaping` mode realises the Cisco MQC LLQ
  pattern.
- Operational coverage of the four canonical CAKE components:
  - **Bandwidth shaping** with link-layer overhead, ATM, MPU, and
    raw-mode accounting (per-tin TBF gate or mainline `TbfQueueDisc`
    composed as the per-tin inner).
  - **COBALT AQM** inherited transitively via mainline
    `FqCobaltQueueDisc`.
  - **DiffServ handling** via the registered presets above.
  - **TCP ACK filter** with conservative and aggressive modes (full
    Linux tc-cake parity); SACK-bearing ACKs preserved in
    conservative mode, dropped in aggressive.
- Eight host-isolation modes exposed via a `Mode` enum:
  triple-isolate (default), srchost, dsthost, hosts, flowblind,
  flows, dual-srchost, dual-dsthost.
- Optional features: `Wash` toggle (zeroes egress DSCP while
  preserving ECN), `MemLimit` byte-counted enqueue cap,
  `EnableSetAssociativeHash` (inherited from mainline FqCobalt),
  per-tin diagnostic counters via `GetTinStats`.

### Composability worked example

- A single edge configured for the Briscoe gap-1 composition:
  DualPI2 (slot 0, EF, ECT(1)) + WRED (slot 1, AF) + DropTail
  (slot 2, BE) under strict-priority dispatch with one classifier
  and one PHB table.  See `examples/diffserv-hierarchical-l4s.cc`.

### Validation

- Validation along four independent dimensions:
  - **RFC conformance**: 25 vectors covering RFC 2697 srTCM and
    RFC 2698 trTCM, framework-independent ground truth.
  - **Cross-simulator equivalence**: paired runs against the
    companion ns-2.35 port reproduce the 2001 reference module's
    Scenario 1 (PQ baseline), Scenario 2 (multi-class WRED sweep),
    and Scenario 3 (771-node varybell with five service classes).
  - **Independent reproduction**: Chang et al. 2015 (SIMULTECH) WFQ
    and WRR bandwidth ratios reproduced; CAKE paper figures 4, 5,
    and 8 reproduced against the authors' published Flent reference
    data (`10.5281/zenodo.1226887`).
  - **Real-network inheritance**: TF-TANT 2001 measurement chain
    inherited transitively for throughput.
- Module ships 136 automated tests across four suites: `diffserv`,
  `diffserv-l4s`, `diffserv-per-flow-classifier`,
  `diffserv-cake-q15`.

### Multi-AQM characterisation harness

- New `examples/aqm-eval-runner` binary that sweeps a 13×9 = 117-cell
  matrix (13 AQMs across nine RFC 7928-aligned scenarios) and emits
  per-cell CSV output for ellipse-style characterisation plots.
- AQM cells include nine mainline ns-3 queue discs (PfifoFast, RED,
  Adaptive RED, CoDel, FQ-CoDel, PIE, FQ-PIE, Cobalt, FQ-Cobalt) and
  four DiffServ-aware composites (DsRed, DsL4s with WRED inner, L4S
  coupled-only, DsCake).
- A substrate-registry pattern (`DsRegistry<EntryT>` template,
  `AqmRegistry`, `SchedulerRegistry`) is the single source of truth
  for the registered cells; the manifest is consumed by the Python
  plot scripts.

### Tooling and reproducibility

- `scripts/fetch-ns3.sh` clones ns-3-dev at the pinned commit and
  auto-applies local patches under `patches/ns3/` (e.g., the TCP
  PersistTimeout null-guard, the TcpRetransmitTag accessor, the
  TbfQueueDisc nested-dequeue guard, the FqCobalt ACK-filter +
  MemLimit additions).  Patches retire as their upstream
  merge-requests merge and the pinned ns-3-dev revision advances.
- `MIGRATION-from-ns2.md` provides a side-by-side ns-2 Tcl → ns-3
  C++ migration guide with thirty-plus command mappings.
- Three-tier Evaluation-Driven Development spec suite under `specs/`
  (intent / structural / quality) makes the module's behaviour
  machine-checkable.

### Mainline-bound contributions

Each mainline-relevant artefact lands as a local patch in
`patches/ns3/` paired with a concurrent upstream merge request,
retired on merge:

- `0001-tcp-persist-empty-buffer.patch` — null-guard in
  `TcpSocketBase::PersistTimeout` for an empty transmit buffer.
- `0002-tcp-retransmit-tag.patch` — public accessor for the TCP
  retransmit count (consumed by the multi-AQM harness for goodput
  accounting per RFC 7928 §3.2).
- `0004-tbf-nested-dequeue-guard.patch` — defers the pacing wake to
  the parent qdisc when `TbfQueueDisc` is nested as an inner.
- `0006-fqcobalt-ack-filter-memlimit.patch` — adds
  `EnableAckFilter`, `EnableAckFilterAggressive`, and a byte-counted
  `MemLimit` cap to mainline `FqCobaltQueueDisc`; the implementation
  walks inner-flow queues and parses TCP headers via raw-byte access
  to avoid a circular module dependency.

### Companion ns-2 source tree

The repository also carries the 2001 ns-2 original (preserved verbatim
under `src/ns-2.29/`) and the 2026 ns-2.35 port layer
(`src/ns-2.35/`).  The ns-2.35 layer is additive to a stock ns-2.35
source tree: it carries the 2001 algorithms unchanged, folds in
compiler warning hygiene from upstream ns-2.35, adds 28-byte UDP
header accounting that the 2001 code omitted, and fixes nine latent
2001-era defects (catalogued in `LINEAGE.md`).

---

## See also

- [`LINEAGE.md`](LINEAGE.md) — twenty-five-year provenance chain.
- [`CITATION.cff`](CITATION.cff) — citation metadata for the software,
  the 2001 thesis, and the 2002 ISCC paper.
- The companion archive record on Zenodo carries the ns-2 sources
  (DOI [`10.5281/zenodo.19665019`](https://doi.org/10.5281/zenodo.19665019)).
