# DiffServ4NS

[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.19665019.svg)](https://doi.org/10.5281/zenodo.19665019)
[![License: GPL v2](https://img.shields.io/badge/License-GPL_v2-blue.svg)](./LICENSE)
![ns-2 compat](https://img.shields.io/badge/ns--2-2.29%20%7C%202.35-lightgrey)
![status: archival](https://img.shields.io/badge/status-archival-success)

**A Differentiated Services (DiffServ, RFC 2474/2475) module for the ns-2 network simulator.** Edge + core routers; policers (Token Bucket, srTCM, trTCM, TSW2CM, TSW3CM); droppers (WRED, RIO); schedulers (PQ, WFQ, SFQ, SCFQ, LLQ, WF2Q+).

25 years of code: originally designed in 2001 as part of the author's MSc thesis, peer-reviewed distillation published at IEEE [ISCC 2002](https://doi.org/10.1109/ISCC.2002.1021705), re-skinned and released on SourceForge in 2006, forward-ported to ns-2.35 in 2026 with nine latent-bug fixes and a 28-byte UDP header correction. Both ns-2 variants compile cleanly under the documented Docker workflow; the included example scripts run end-to-end.

## Quickstart

Three commands to a working ns-2.29 build with DiffServ4NS applied (Docker required):

```bash
git clone https://github.com/digitalities/diffserv4ns.git && cd diffserv4ns
./scripts/fetch-ns2-allinone-229.sh && ./scripts/patch-ns2-diffserv-229.sh
./scripts/build-ns2-allinone-229-docker.sh
```

Replace `229` with `235` for the ns-2.35 variant. Full install guide in [`docs/installation.md`](docs/installation.md).

## Variants

| Variant | Status | In this repo | Scoped README | Zenodo concept DOI |
|---|---|---|---|---|
| **ns-2.29** | frozen — 2006 SourceForge release (2001 algorithms re-skinned for the ns-2.29 API) | `src/ns-2.29/` | [`README-ns-2.md`](README-ns-2.md) | [10.5281/zenodo.19665019](https://doi.org/10.5281/zenodo.19665019) |
| **ns-2.35** | frozen — 2026 port layer (nine latent-bug fixes + UDP header correction) | `src/ns-2.35/` | [`README-ns-2.md`](README-ns-2.md) | *(shares the same Zenodo record)* |

## Examples

| Example | Scope | Variant |
|---|---|---|
| [`example-1/`](examples/example-1/) | DS-RED with CBR + srTCM policer (2006 original) | ns-2.29 |
| [`example-2/`](examples/example-2/) | AF PHB differentiation, 13 nodes (2006 original) | ns-2.29 |
| [`example-2-fullscale/`](examples/example-2-fullscale/) | AF PHB differentiation, 469-node thesis §4.2 reconstruction | ns-2.29 + ns-2.35 |
| [`example-3/`](examples/example-3/) | Complete service model, 771-node thesis §4.3 reconstruction | ns-2.29 |

## Scholarly context

- **Peer-reviewed reference:** Andreozzi, S. (2002). *Differentiated services: an experimental vs. simulated case study.* In Proc. IEEE ISCC 2002. doi:[10.1109/ISCC.2002.1021705](https://doi.org/10.1109/ISCC.2002.1021705) · open-access preprint on [Zenodo](https://doi.org/10.5281/zenodo.19665017) / [arXiv](https://arxiv.org/abs/2604.20049).
- **Authoritative design document:** Andreozzi, S. (2001). *DiffServ simulations using the Network Simulator: requirements, issues and solutions.* MSc thesis, Lappeenranta University of Technology / University of Pisa. Zenodo: [10.5281/zenodo.19662899](https://doi.org/10.5281/zenodo.19662899).
- **25-year provenance chain:** see [`LINEAGE.md`](LINEAGE.md).

## Citation

Use the concept DOI [10.5281/zenodo.19665019](https://doi.org/10.5281/zenodo.19665019) to cite the archive as a whole (always resolves to the latest version). Machine-readable metadata in [`CITATION.cff`](CITATION.cff); each scoped README includes a scope-specific citation block.

## License

GPLv2. See [`LICENSE`](LICENSE) for the full text.

Copyright (C) 2001–2026 Sergio Andreozzi. Original Nortel Networks DiffServ framework (C) 2000.
