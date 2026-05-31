# DiffServ4NS for ns-3 — codename *Stratum*

*Scope: this document covers the **ns-3 substrate** of the DiffServ4NS
project — a QoS substrate composing DiffServ, L4S, and CAKE in
[ns-3](https://www.nsnam.org/). For the ns-2 archive see
[`README-ns-2.md`](README-ns-2.md); for the project overview see the
release [`README.md`](README.md).*

> **Codename: *Stratum*.** This module ships under the provisional
> codename *Stratum* for v1.0. The codename is a stable handle for
> review and citation; the permanent project name and repository home
> will be finalised after community feedback during peer review and
> post-publication discussions. We will commit to one of:
>
> - adopt *Stratum* as the permanent name (keep the embedded structure under DiffServ4NS, or split into a standalone repository);
> - replace the codename with a community-suggested name;
> - drop the codename and revert to a descriptive label.
>
> The code, DOIs, and attribution are stable; only the surface label may
> change. See the project ADR record for the deferral rationale and
> decision deadline (six months post-publication, or earlier on clear
> external signal).

This is the active variant of the DiffServ4NS lineage. Where the ns-2
archive (`src/ns-2.29/` and `src/ns-2.35/`) preserves the 2001 module as
a faithful historical reference, the ns-3 substrate is a new whole that
absorbs the DiffServ algorithms as one of three composed peer
mechanisms.

## What's composed

The substrate integrates three traffic-management architectures on a
single ns-3 edge queue disc:

- **DiffServ** (RFC 2475) — per-class scheduling and policing. Inherits
  the meters, schedulers, and edge / core router patterns from the 2001
  DiffServ4NS module, ported to ns-3 idioms.
- **L4S** (RFC 9330 / 9331 / 9332) — DualPI2 with coupled
  classic / scalable congestion control and ECT(1) classification.
- **CAKE** (`sch_cake` parity) — eight-tin composition with DRR++
  scheduling, per-flow Cobalt AQM, host-pair isolation, ACK-filter,
  optional TBF inner-mode shaping, per-tin diagnostics, and Linux
  `tc-cake(8)` operational vocabulary; 15 named link-layer overhead
  presets (`LinkPreset`), 8 named RTT presets (`RttPreset`), PTM
  framing support, ingress shaping mode, Linux autorate-ingress
  closed loop, per-tin byte-cap eviction (`MemLimit`), and opt-in
  live bulk-flow counter (`DsCakeLiveBulkCounter`).

The composition is the contribution: no single component alone delivers
DiffServ-class-aware-low-latency-with-flow-isolation. The substrate
makes that combination expressible on a single ns-3 edge.

## Features

Extensions over what the 2001 DiffServ4NS module provided:

- **Schedulers:** WFQ (Parekh-style snapshot V(t)), WF2Q+ (Bennett-Zhang
  time-discrete), SCFQ, SFQ, LLQ, plus the original PQ / WRR /
  hierarchical variants.
- **Meters:** the original sr/trTCM and TSW2/3CM, with RNG isolation for
  reproducible per-meter random streams.
- **AQM composition:** L4S DualPI2 inside DiffServ classes; CAKE
  composition orthogonal to DiffServ scheduling; per-tin TBF shaping;
  ACK-filter (conservative + aggressive modes); egress DSCP wash.
- **Classification:** DSCP-based and ECT(1)-based, plus per-flow srTCM
  for fairness-with-policing in the same class.
- **Monitoring:** per-DSCP frequency-distributed OWD and IPDV; per-tin
  byte / packet / drop / mark counters matching `tc -s qdisc show`;
  per-flow goodput.
- **Reproducibility:** the ICNS3 paper figures reproduce from this tree
  via the recipes under `reproducibility/` (when present in the
  release mirror).

## Quickstart

```bash
git clone https://github.com/digitalities/diffserv4ns.git
cd diffserv4ns
./scripts/fetch-ns3.sh                 # pinned ns-3.47 + local patches
cd ns3/ns-3-dev
./ns3 configure --enable-tests --enable-examples
./ns3 build diffserv
./ns3 run diffserv-example-1
python3 test.py -s diffserv -v          # full test suite
```

The fetch script clones the pinned ns-3-dev revision, applies any
in-tree patches under `patches/ns3/`, and creates the
`contrib/diffserv → src/ns-3` symlink that ns-3 expects.

## Citation

Cite the ICNS3 2026 paper:

- **Stratum: A QoS Substrate Composing DiffServ, L4S, and CAKE in
  ns-3.** S. Andreozzi. Workshop on ns-3 (WNS3 / ICNS3) 2026.
- Software Zenodo DOI: 10.5281/zenodo.19986401 *(provisional title;
  reflects codename status)*.
- Paper preprint Zenodo DOI: 10.5281/zenodo.19986696.
- Tech-report Zenodo DOI: 10.5281/zenodo.19986700.

A `CITATION.cff` at the repository root provides machine-readable
metadata; the release's Variants table (in [`README.md`](README.md))
links the ns-3 row to this document and to the software DOI.

## The 2001 thesis

The architectural lineage starts with Sergio Andreozzi's MSc thesis
(University of Pisa, 2001) — the authoritative design document for
the original DiffServ4NS module. The thesis is a separate Zenodo
deposit:

- **Concept DOI:** [10.5281/zenodo.19662899](https://doi.org/10.5281/zenodo.19662899)
- **Version DOI:** [10.5281/zenodo.19662900](https://doi.org/10.5281/zenodo.19662900)

Key sections relevant to the substrate:

- Chapter 3.3.3 — module design specification (inherited algorithms).
- Chapter 4 — three simulation scenarios with results (the Q-tier
  reproductions live in `specs/03-quality.md`).
- Appendix A — validation against real-network measurements.

## Architectural lineage

The substrate is not "DiffServ4NS v3". It is a new whole that includes
the ported DiffServ algorithms as one of three peer components,
composed alongside L4S and CAKE under a single ns-3 edge queue disc.
The 2001 module ([`README-ns-2.md`](README-ns-2.md)) and the 2026
substrate are peer artifacts that share intellectual ancestry but
differ in scope: the former implements DiffServ on ns-2; the latter
composes DiffServ + L4S + CAKE on ns-3.

For the full 25-year chain see [`LINEAGE.md`](LINEAGE.md).

## Licence

GPLv2, matching both ns-3 mainline and the DiffServ4NS-0.1 ancestor.
