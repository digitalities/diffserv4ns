# DiffServ4NS (ns-2)

**A Differentiated Services module for ns-2 — the 2001 original and the 2026 ns-2.35 port.**

DiffServ4NS was written in 2001 as part of an MSc thesis at Lappeenranta University of Technology (Finland) and the University of Pisa (Italy). It extends the stock ns-2 DiffServ module (Nortel Networks, 2000) with five fair-queueing schedulers, multiple traffic meters, per-DSCP monitoring, and a composable edge/core router architecture.

> **Archived, not abandoned.** Both ns-2 variants are preserved for historical reference: the ns-2.29 C++ source has not changed since 2001, and the ns-2.35 port is additive. The successor for current work is the ns-3 QoS substrate *Stratum*, which composes Differentiated Services, L4S, and CAKE as three first-class clients of one module. It lives at [digitalities/stratum-ns3](https://github.com/digitalities/stratum-ns3).

## Features

| Category | Additions over stock ns-2.29 |
|----------|------------------------------|
| **Schedulers** | WFQ, WF2Q+, SCFQ, SFQ, LLQ |
| **Marking** | Per-packet rules by source, destination, protocol, app type |
| **Metering** | DSCP-based rate limiter policy |
| **Dropping** | Out-of-profile drop on per-drop-precedence basis |
| **Monitoring (UDP)** | OWD, IPDV: average, instantaneous, min, frequency-distributed |
| **Monitoring (TCP)** | Goodput, RTT, window size per DSCP |
| **Monitoring (per-hop)** | Queue length, departure rate, packet counters |

## Browse the thesis

The 2001 MSc thesis is the authoritative design document for this module. It is archived as a separate Zenodo record: concept DOI [10.5281/zenodo.19662899](https://doi.org/10.5281/zenodo.19662899).

## Quick links

- [Installation guide](installation.md) (ns-2.29 + ns-2.35, Docker)
- [Module architecture](architecture.md)
- [Reproduction map](REPRODUCIBILITY.md) — per-scenario instructions
- Thesis Chapter 3.3.3 (in the Zenodo thesis record above) — the module design specification
- [Provenance chain](../LINEAGE.md)
- [Source code on GitHub](https://github.com/digitalities/diffserv4ns)
- [Stratum for ns-3](https://github.com/digitalities/stratum-ns3) — the successor substrate

## Citation

> **Sergio Andreozzi.** *Differentiated services: an experimental vs. simulated case study.*
> ISCC 2002, Taormina, Italy. IEEE. doi:[10.1109/ISCC.2002.1021705](https://doi.org/10.1109/ISCC.2002.1021705)
