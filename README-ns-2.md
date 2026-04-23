# DiffServ4NS for ns-2

*Scope: this document covers the **ns-2 archive** of the
DiffServ4NS project (the 2001 ns-2.29 original and the 2026 ns-2.35
port layer). For the project overview see the umbrella
[`README.md`](README.md).*

This archive ships two source variants side-by-side:

- **`src/ns-2.29/`** — the DiffServ4NS module as distributed in the 2006 SourceForge release DiffServ4NS-0.2. The algorithms were designed in 2001 against ns-2.1b8a alongside the author's MSc thesis; the 2006 release re-skinned the C++ source for the ns-2.29 API without changing the algorithms. Preserved read-only.
- **`src/ns-2.35/`** — a 2026 port layer for ns-2.35 (the last stable ns-2 release, 2011) that fixes nine 2001-era bugs (BUG-1..5, BUG-7..10) and corrects the UDP header size, without altering the design.

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

## The 2001 thesis

The 2001 MSc thesis — the authoritative design document for this module — is a separate Zenodo record:

- **Concept DOI** (resolves to latest): [10.5281/zenodo.19662899](https://doi.org/10.5281/zenodo.19662899)
- **Version DOI** (pins the first snapshot): [10.5281/zenodo.19662900](https://doi.org/10.5281/zenodo.19662900)

Key sections for this module:
- Chapter 3.3.3 — the module design specification
- Chapter 4 — three simulation scenarios with results
- Appendix A — validation against real-network measurements

The Zenodo deposit provides the PDF directly; the concept DOI above always resolves to the latest snapshot.

## Installation

See [`docs/installation.md`](docs/installation.md) for full instructions. Quick-start:

- **ns-2.29 Docker build:** `./scripts/fetch-ns2-allinone-229.sh && ./scripts/build-ns2-allinone-229-docker.sh && ./scripts/patch-ns2-diffserv-229.sh && ./scripts/build-ns2-allinone-229-docker.sh`
- **ns-2.35 Docker build:** same pattern with `-235` script suffixes.
- **Legacy (ns-2.29 only, direct-install):** see the Legacy section in [`docs/installation.md`](docs/installation.md).

Verified on macOS (Apple Silicon) and Linux in 2026.

## Repository structure (ns-2 subset)

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
  fetch-ns2-allinone-{229,235}.sh    Download ns-allinone source trees
  build-ns2-allinone-{229,235}-docker.sh   Build ns-2 in Docker (Ubuntu 18.04 + GCC 7)
  patch-ns2-diffserv-{229,235}.sh    Patch DiffServ4NS into the ns-2 source tree
docs/
  installation.md                    Build guide (Docker + legacy; 2.29 and 2.35 paths)
  architecture.md                    Module design overview
```

## Citation

If you use the ns-2 variants of DiffServ4NS in your research, please cite both the peer-reviewed paper and this archival record:

> **Sergio Andreozzi.** *Differentiated services: an experimental vs. simulated case study.*
> Proceedings of the Seventh IEEE Symposium on Computers and Communications (ISCC 2002),
> 1-4 July 2002, Taormina, Italy. Pages 383-390. IEEE Computer Society.
> doi:[10.1109/ISCC.2002.1021705](https://doi.org/10.1109/ISCC.2002.1021705).
> Open-access preprint: [Zenodo 10.5281/zenodo.19665017](https://doi.org/10.5281/zenodo.19665017).

> **Sergio Andreozzi.** *DiffServ4NS for ns-2: the 2001 original and 2026 ns-2.35 port.*
> Zenodo, 2026. Concept DOI: [10.5281/zenodo.19665019](https://doi.org/10.5281/zenodo.19665019).

See [`CITATION.cff`](CITATION.cff) for machine-readable citation metadata.

## License

GPLv2. See [`LICENSE`](LICENSE) for the full text.

Copyright (C) 2001–2026 Sergio Andreozzi. Original Nortel Networks DiffServ framework (C) 2000. The 2026 ns-2.35 port layer is distributed under the same GPLv2 terms as the 2001 original.
