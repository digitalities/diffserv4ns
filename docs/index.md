# DiffServ4NS

**An extended Differentiated Services module for the Network Simulator 2 (ns-2).**

DiffServ4NS was written in 2001 as part of an MSc thesis at Lappeenranta University of Technology (Finland) and the University of Pisa (Italy). It extends the stock ns-2 DiffServ module (Nortel Networks, 2000) with five fair-queueing schedulers, multiple traffic meters, per-DSCP monitoring, and a composable edge/core router architecture.

> **Archival repository.** This code targets ns-2 versions 2.29 (2001 original) and 2.35 (2026 port layer) and is preserved for historical reference. The ns-2.29 C++ source has not changed since 2001; the ns-2.35 port is additive and does not modify the 2001 design.

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

- [Installation guide](installation.md) (historical, for ns-2.29)
- [Module architecture](architecture.md)
- Thesis Chapter 3.3.3 (in the Zenodo thesis record above) — the module design specification
- [25-year lineage](../LINEAGE.md)
- [Source code on GitHub](https://github.com/digitalities/diffserv4ns)

## Citation

> **Sergio Andreozzi.** *Differentiated services: an experimental vs. simulated case study.*
> ISCC 2002, Taormina, Italy. IEEE. doi:[10.1109/ISCC.2002.1021705](https://doi.org/10.1109/ISCC.2002.1021705)
