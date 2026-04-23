# Lineage

A twenty-five-year provenance chain from a master's thesis at Lappeenranta University of Technology and the University of Pisa, through a personal-website release, a 2006 port from ns-2.1b8a to ns-2.29 released on SourceForge, twenty years of dormancy, and a 2026 forward-port to the last stable ns-2 release.

```
Andreozzi 2001 thesis: ns-2.1b8a reproduction + extension ("DiffServ+")
        | personal-website distribution, 2001-2006 (same code)
        | 2006 port from ns-2.1b8a to ns-2.29 + rename + SourceForge release
DiffServ4NS-0.2 on SourceForge, 2006
        | 20-year dormancy
DiffServ4NS ns-2.35 port layer, April 2026
```

Each link is independently checkable. The **core algorithmic code** of the 2001 DiffServ+ module is preserved through every downstream step: the 2006 SourceForge release re-skinned it for the ns-2.29 API without changing the algorithms, and the 2026 ns-2.35 port layer under `src/ns-2.35/` is additive — it fixes latent 2001-era bugs and corrects the UDP header size (28 bytes), without altering the 2001 design. The validation reference for every release is the comparison of simulator results against real-network experiments (see Appendix A of the 2001 thesis).

## What the thesis contains that the source release does not

1. **Chapter 3.3.3** — complete prose specification of every mechanism added on top of the Nortel ns-2 base
2. **Figure 3.11** — full UML class diagram matching the source code line for line
3. **Section 3.3.3.2** — rationale for each performance parameter
4. **Chapter 4** — three complete simulation scenarios with topologies, traffic settings, and results
5. **Appendix A** — eight figures pairing real-network measurements with simulated reproduction
6. **Complete reference list** (52 entries) — every algorithm citation for the schedulers, every IETF RFC

The 2001 thesis is archived as a separate Zenodo record: concept DOI [10.5281/zenodo.19662899](https://doi.org/10.5281/zenodo.19662899).

## The 2026 ns-2.35 port layer

In April 2026, twenty-five years after the original implementation, the original author forward-ported the DiffServ4NS module from ns-2.29 to **ns-2.35** (the last stable ns-2 release, 2011). The port layer under `src/ns-2.35/` is additive to a stock ns-2.35 source tree: it carries the 2001 DiffServ4NS algorithms unchanged, folds in compiler warning-hygiene from upstream ns-2.35, and adds the 28-byte UDP header accounting (IP 20 + UDP 8) that the 2001 code omitted.

Nine latent 2001-era bugs are fixed in the port layer:

| ID | Component affected | Summary |
|----|--------------------|---------|
| BUG-1 | ns-2 core | Dead `set_pkttype(PT_CBR)` in `cbr_traffic.cc` |
| BUG-2 | ns-2 core | Dead `set_pkttype(PT_REALAUDIO)` in `realaudio.cc` |
| BUG-3 | ns-2 core | Magic-number `27` in `ns-source.tcl` |
| BUG-4 | DiffServ4NS module | `SFQ::DequeEvent` reads `FlowQueue.front()` before `empty()` check |
| BUG-5 | ns-2 core | Incomplete `set_apptype` patch in Tcl `Application/FTP` |
| BUG-7 | DiffServ4NS module | Tcl 8.5 `catch {expr X/0}` slowdown (~50 000×) in DS4 monitoring procs |
| BUG-8 | Simulation files | Incomplete `record_delay` in `scenario-3.tcl` — VoIP OWD / IPDV silently dropped |
| BUG-9 | ns-2 core | WebTraf session-completion SEGV (shared `RandomVariable` delete) |
| BUG-10 | DiffServ4NS module | TSW / FW meters silently share the global default RNG stream |

## Zenodo deposits

| Record | Concept DOI | Scope |
|---|---|---|
| DiffServ4NS ns-2 archive (this repository) | [10.5281/zenodo.19665019](https://doi.org/10.5281/zenodo.19665019) | 2001 design on ns-2.29 (2006 SourceForge port) + 2026 ns-2.35 port |
| 2002 ISCC preprint (AAM) | [10.5281/zenodo.19665017](https://doi.org/10.5281/zenodo.19665017) | Peer-reviewed distillation of the 2001 thesis |

## References

1. **Andreozzi, S.** (2001). *DiffServ simulations using the Network Simulator: requirements, issues and solutions.* MSc thesis, LUT / University of Pisa.
2. **Andreozzi, S.** (2002). *Differentiated services: an experimental vs. simulated case study.* ISCC 2002, Taormina, Italy. IEEE. doi:[10.1109/ISCC.2002.1021705](https://doi.org/10.1109/ISCC.2002.1021705). Open-access preprint: [Zenodo 10.5281/zenodo.19665017](https://doi.org/10.5281/zenodo.19665017), [arXiv:2604.20049](https://arxiv.org/abs/2604.20049).
