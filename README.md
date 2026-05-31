# DiffServ4NS

[![License: GPL v2](https://img.shields.io/badge/License-GPLv2-blue.svg)](LICENSE)
![ns-2](https://img.shields.io/badge/ns--2-2.29%20%7C%202.35%20%C2%B7%20archival-lightgrey)
![ns-3](https://img.shields.io/badge/ns--3-substrate%20%C2%B7%20active-orange)

**A 25-year Differentiated Services lineage spanning two network simulators.** DiffServ4NS began as a DiffServ (RFC 2474/2475) module for **ns-2** — edge + core routers; policers (Token Bucket, srTCM, trTCM, TSW2CM, TSW3CM); droppers (WRED, RIO); schedulers (PQ, WFQ, SFQ, SCFQ, LLQ, WF2Q+) — and is carried forward into an **ns-3 QoS substrate** (codename *Stratum*) that composes DiffServ, L4S, and CAKE on a single edge queue disc.

Originally designed in 2001 for the author's MSc thesis, peer-reviewed at IEEE [ISCC 2002](https://doi.org/10.1109/ISCC.2002.1021705), released on SourceForge in 2006, forward-ported to ns-2.35 in 2026, and re-imagined as the ns-3 substrate for the WNS3 / ICNS3 2026 paper. Built using Evaluation-Driven Development — a layered Intent → Structural → Quality spec suite gates every change.

## Variants

This repository hosts the whole lineage. Each variant has its own scoped README:

| Variant | Status | In this repo | Scoped README |
|---|---|---|---|
| **ns-2.29** | frozen — 2006 SourceForge release (2001 algorithms on the ns-2.29 API) | `src/ns-2.29/` | [`README-ns-2.md`](README-ns-2.md) |
| **ns-2.35** | frozen — 2026 port layer (latent-bug fixes + UDP header correction) | `src/ns-2.35/` | [`README-ns-2.md`](README-ns-2.md) |
| **ns-3** | **active** — QoS substrate composing DiffServ + L4S + CAKE (codename *Stratum*) | `src/ns-3/` | [`README-ns-3.md`](README-ns-3.md) |

## Quick start

**ns-3 substrate** (the active variant):

```bash
git clone https://github.com/digitalities/diffserv4ns.git && cd diffserv4ns
./scripts/fetch-ns3.sh                              # pinned ns-3 + local patches
cd ns3/ns-3-dev
./ns3 configure --enable-tests --enable-examples
./ns3 build diffserv && python3 test.py -s diffserv -v
```

**ns-2 archive** (Docker):

```bash
./scripts/fetch-ns2-allinone.sh && ./scripts/patch-ns2-diffserv.sh
./scripts/build-ns2-allinone-docker.sh
```

See [`docs/REPRODUCIBILITY.md`](docs/REPRODUCIBILITY.md) for the full per-scenario reproduction map, and the scoped READMEs above for per-variant detail.

## Scholarly context

- **ns-3 substrate (2026):** *Stratum: A QoS Substrate Composing DiffServ, L4S, and CAKE in ns-3.* S. Andreozzi, Workshop on ns-3 (WNS3 / ICNS3) 2026 — see [`README-ns-3.md`](README-ns-3.md) for the scoped citation and DOIs.
- **Peer-reviewed reference (2002):** Andreozzi, S. *Differentiated services: an experimental vs. simulated case study.* Proc. IEEE ISCC 2002. doi:[10.1109/ISCC.2002.1021705](https://doi.org/10.1109/ISCC.2002.1021705).
- **Authoritative design document (2001):** Andreozzi, S. *DiffServ simulations using the Network Simulator.* MSc thesis. Zenodo: [10.5281/zenodo.19662899](https://doi.org/10.5281/zenodo.19662899).
- **25-year provenance chain:** [`LINEAGE.md`](LINEAGE.md).

## Citation

Machine-readable metadata is in [`CITATION.cff`](CITATION.cff). Each scoped README carries a scope-specific citation block — the ns-3 substrate and the ns-2 archive have distinct Zenodo records.

## Acknowledgements

- **Sergio Andreozzi** (2001–2006, 2026): original DiffServ4NS author and maintainer.
- **Nortel Networks** (2000): the ns-2 diffserv module that DiffServ4NS extended (Farhan Shallwani, Jeremy Ethridge, Peter Pieda, Mandeep Baines).
- **Xuan Chen, ISI** (2001): ns-2 integration of the Nortel module.
- **Pasquale Imputato and Stefano Avallone** (2016): the ns-3 traffic-control layer this substrate builds on.

## License

GPLv2. See [`LICENSE`](LICENSE) for the full text.

Copyright (C) 2001–2026 Sergio Andreozzi. Original Nortel Networks DiffServ framework (C) 2000.
