# DiffServ4NS

**A Differentiated Services and QoS substrate spanning two simulator generations — ns-2 (2001 original + 2026 port) and ns-3 (active substrate composing DiffServ, L4S, and CAKE).**

DiffServ4NS was written in 2001 as part of an MSc thesis at Lappeenranta University of Technology (Finland) and the University of Pisa (Italy). It extends the stock ns-2 DiffServ module (Nortel Networks, 2000) with five fair-queueing schedulers, multiple traffic meters, per-DSCP monitoring, and a composable edge/core router architecture.

> **Two eras, one lineage.** The **ns-2** code (versions 2.29 and 2.35) is preserved for historical reference — the ns-2.29 C++ source has not changed since 2001, and the ns-2.35 port is additive. Active development has moved to an **ns-3** substrate (codename *Stratum*) that composes Differentiated Services, L4S, and CAKE as three first-class clients of one QoS module. See the [ns-3 README](../README-ns-3.md) and [`src/ns-3/`](../src/ns-3/).

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

- [ns-3 substrate (active development)](../README-ns-3.md) — *Stratum*: DiffServ + L4S + CAKE in ns-3
- [ns-2 installation guide](installation.md) (ns-2.29 + ns-2.35, Docker)
- [Module architecture](architecture.md)
- Thesis Chapter 3.3.3 (in the Zenodo thesis record above) — the module design specification
- [25-year lineage](../LINEAGE.md)
- [Source code on GitHub](https://github.com/digitalities/diffserv4ns)

## Citation

> **Sergio Andreozzi.** *Differentiated services: an experimental vs. simulated case study.*
> ISCC 2002, Taormina, Italy. IEEE. doi:[10.1109/ISCC.2002.1021705](https://doi.org/10.1109/ISCC.2002.1021705)
