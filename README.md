# DiffServ4NS (ns-2)

[![License: GPL v2](https://img.shields.io/badge/License-GPLv2-blue.svg)](LICENSE)
![ns-2](https://img.shields.io/badge/ns--2-2.29%20%7C%202.35-lightgrey)
[![DOI](https://img.shields.io/badge/DOI-10.5281%2Fzenodo.19665019-blue.svg)](https://doi.org/10.5281/zenodo.19665019)
[![ns-3](https://img.shields.io/badge/ns--3-%E2%86%92%20stratum--ns3-informational)](https://github.com/digitalities/stratum-ns3)

> Looking for the ns-3 version? The QoS substrate **Stratum** — composing DiffServ, L4S, and CAKE —
> lives at [`digitalities/stratum-ns3`](https://github.com/digitalities/stratum-ns3).
> The ns-3 snapshot that used to live in this repository is frozen at tag
> [`v1.0-icns3-submission`](https://github.com/digitalities/diffserv4ns/releases/tag/v1.0-icns3-submission).

**A Differentiated Services module for ns-2, preserved as its author wrote it.** DiffServ4NS implements RFC 2474/2475 DiffServ for the ns-2 network simulator: edge and core routers; policers (Token Bucket, srTCM, trTCM, TSW2CM, TSW3CM); droppers (WRED, RIO); and schedulers (PQ, WFQ, SFQ, SCFQ, LLQ, WF2Q+, RR, WRR, WIRR).

Originally designed in 2001 for the author's MSc thesis, peer-reviewed at IEEE [ISCC 2002](https://doi.org/10.1109/ISCC.2002.1021705), released on SourceForge in 2006, and forward-ported to ns-2.35 in 2026. This repository is the archive of that work.

## Variants

This repository ships two source variants side by side:

| Variant | Status | Path | What it is |
|---|---|---|---|
| **ns-2.29** | frozen | `src/ns-2.29/` | The module as distributed in the 2006 SourceForge release DiffServ4NS-0.2. The algorithms were designed in 2001 against ns-2.1b8a alongside the author's MSc thesis; the 2006 release re-skinned the C++ source for the ns-2.29 API without changing the algorithms. Preserved read-only. |
| **ns-2.35** | frozen | `src/ns-2.35/` | A 2026 port layer for ns-2.35 (the last stable ns-2 release, 2011) that fixes nine 2001-era bugs (BUG-1..5, BUG-7..10) and corrects the UDP header size, without altering the design. |

The port layer is additive: a stock ns-2.35 tree plus the files under `src/ns-2.35/` produces the built module.

## Features

Extensions over the stock ns-2.29 DiffServ module (Nortel Networks):

- **Schedulers:** WFQ, WF2Q+, SCFQ, SFQ, LLQ (in addition to RR, WRR, WIRR, PRI)
- **Marking:** per-packet mark rules based on source node, destination node, transport protocol, and application type
- **Metering:** DSCP-based rate limiter policy
- **Dropping:** out-of-profile traffic dropping on a per-drop-precedence-level basis
- **Monitoring (UDP):** average, instantaneous, minimum, and frequency-distributed OWD and IPDV
- **Monitoring (TCP):** goodput, RTT, and window size on a per-DSCP basis
- **Monitoring (per-hop):** queue length (per-queue and per-drop-precedence), departure rate, packet counters (received, transmitted, dropped by dropper, dropped by overflow)

## Quick start

**ns-2.29 (Docker):**

```bash
git clone https://github.com/digitalities/diffserv4ns.git && cd diffserv4ns
./scripts/fetch-ns2-allinone.sh
./scripts/patch-ns2-diffserv.sh
./scripts/build-ns2-allinone-docker.sh
```

**ns-2.35 (Docker):**

```bash
./scripts/fetch-ns2-allinone-235.sh
./scripts/patch-ns2-diffserv-235.sh
./scripts/build-ns2-allinone-235-docker.sh
```

See [`docs/installation.md`](docs/installation.md) for full instructions, including the legacy ns-2.29 direct-install path, and [`docs/REPRODUCIBILITY.md`](docs/REPRODUCIBILITY.md) for the per-scenario reproduction map.

Verified on macOS (Apple Silicon) and Linux in 2026.

## The 2001 thesis

The 2001 MSc thesis — the authoritative design document for this module — is a separate Zenodo record:

- **Concept DOI** (resolves to latest): [10.5281/zenodo.19662899](https://doi.org/10.5281/zenodo.19662899)
- **Version DOI** (pins the first snapshot): [10.5281/zenodo.19662900](https://doi.org/10.5281/zenodo.19662900)

Key sections for this module:

- Chapter 3.3.3 — the module design specification
- Chapter 4 — three simulation scenarios with results
- Appendix A — validation against real-network measurements

A copy is also in this repository at [`provenance/Andreozzi-2001-thesis.pdf`](provenance/Andreozzi-2001-thesis.pdf).

## Repository structure

```
src/
  ns-2.29/                   2006 SourceForge release (2001 algorithms re-skinned for ns-2.29 API)
    diffserv/                Core module: dsCore, dsred, dsPolicy, dsEdge, dsscheduler
    common/  tcp/  apps/     Modified ns-2 base files (agent, packet, TCP, UDP, telnet)
    tools/ webcache/ realaudio/ tcl/lib/
  ns-2.35/                   2026 port layer (additive; BUG-1..5 + BUG-7..10 fixes, UDP header correction)
    CHANGELOG.md             Port-layer changes relative to ns-2.29 original
    diffserv/  common/ tcp/ apps/ tools/ webcache/ realaudio/ tcl/
examples/
  example-1/                 Scenario 1: scheduler comparison (2001)
  example-2/                 Scenario 2: AF PHB differentiation with WRED (2001)
  example-2-fullscale/       Fullscale variant with ns-2.35 + srTCM scenarios
  example-3/                 Scenario 3: large-scale mixed traffic (2001)
  webtraf-ns235-test/        WebTraf regression tests for the ns-2.35 port
  common/apptypes.tcl        Shared application-type constants
scripts/
  fetch-ns2-allinone[-235].sh            Download the ns-allinone source tree
  build-ns2-allinone[-235]-docker.sh     Build ns-2 in Docker (Ubuntu 18.04 + GCC 7)
  patch-ns2-diffserv[-235].sh            Patch DiffServ4NS into the ns-2 source tree
  run-scenario.sh                        Unified scenario runner (ns-2.29 / ns-2.35 backends)
  run-s3-fullscale-ns229.sh              Scenario 3 fullscale drivers
  run-s3-fullscale-ns235.sh
  run-ns235-scenario2-*.sh               Scenario 2 sweeps on the ns-2.35 port
  parse-traces.py                        Trace files to a common CSV schema
  scenario3-comparison.py                Pair-wise Scenario 3 comparison
  d2-8-regression.sh                     Regression check cited by docs/HISTORICAL_BUGS.md
docs/
  installation.md            Build guide (Docker + legacy; 2.29 and 2.35 paths)
  architecture.md            Module design overview
  REPRODUCIBILITY.md         Per-scenario reproduction map
  INVENTORY.md               Inventory and analysis of the original module
  NS2_PATCHES.md             Catalogue of modifications to stock ns-2
  HISTORICAL_BUGS.md         2001-era bugs and their disposition
provenance/
  Andreozzi-2001-thesis.pdf  The 2001 MSc thesis (design document)
  LINEAGE.md                 Full provenance chain
```

## Scholarly context

- **Peer-reviewed reference (2002):** Andreozzi, S. *Differentiated services: an experimental vs. simulated case study.* Proc. IEEE ISCC 2002. doi:[10.1109/ISCC.2002.1021705](https://doi.org/10.1109/ISCC.2002.1021705). Open-access preprint: [Zenodo 10.5281/zenodo.19665017](https://doi.org/10.5281/zenodo.19665017).
- **Authoritative design document (2001):** Andreozzi, S. *DiffServ simulations using the Network Simulator.* MSc thesis. Zenodo: [10.5281/zenodo.19662899](https://doi.org/10.5281/zenodo.19662899).
- **Provenance chain:** [`LINEAGE.md`](LINEAGE.md).

## Citation

If you use DiffServ4NS in your research, please cite both the peer-reviewed paper and this archival record:

> **Sergio Andreozzi.** *Differentiated services: an experimental vs. simulated case study.*
> Proceedings of the Seventh IEEE Symposium on Computers and Communications (ISCC 2002),
> 1-4 July 2002, Taormina, Italy. Pages 383-390. IEEE Computer Society.
> doi:[10.1109/ISCC.2002.1021705](https://doi.org/10.1109/ISCC.2002.1021705).

> **Sergio Andreozzi.** *DiffServ4NS for ns-2: the 2001 original and 2026 ns-2.35 port.*
> Zenodo, 2026. Concept DOI: [10.5281/zenodo.19665019](https://doi.org/10.5281/zenodo.19665019).

Machine-readable metadata is in [`CITATION.cff`](CITATION.cff).

## Acknowledgements

- **Sergio Andreozzi** (2001-2006, 2026): original DiffServ4NS author and maintainer.
- **Nortel Networks** (2000): the ns-2 diffserv module that DiffServ4NS extended (Farhan Shallwani, Jeremy Ethridge, Peter Pieda, Mandeep Baines).
- **Xuan Chen, ISI** (2001): ns-2 integration of the Nortel module.

## License

GPLv2. See [`LICENSE`](LICENSE) for the full text.

Copyright (C) 2001-2026 Sergio Andreozzi. Original Nortel Networks DiffServ framework (C) 2000. The 2026 ns-2.35 port layer is distributed under the same GPLv2 terms as the 2001 original.
