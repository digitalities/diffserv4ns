# Lineage — diffserv-ns3

A twenty-five-year provenance chain from a master's thesis at the University of Pisa (with work performed at Lappeenranta University of Technology), through a personal-website release, a 2006 port from ns-2.1b8a to ns-2.29 re-packaged on SourceForge, twenty years of dormancy, and a return to active development as a port to ns-3 — built on real-network measurements that themselves predate the thesis.

The full validation chain runs:

```
Ferrari 2000 real-network measurements
        ↓
Andreozzi 2001 thesis: ns-2.1b8a reproduction + extension ("DiffServ+")
        ↓ personal-website distribution, 2002–2006 (same code)
        ↓ 2006 port from ns-2.1b8a to ns-2.29 + rename + SourceForge release
DiffServ4NS-0.2 on SourceForge, 2006
        ↓ 20-year dormancy
diffserv-ns3 port, April 2026
```

Each link is independently checkable. Each link traces back to a real-world ground truth that pre-dates the simulation entirely. **The core algorithmic code of the 2001 DiffServ+ module is preserved through every downstream step: the 2006 SourceForge release re-skinned it for the ns-2.29 API without changing the algorithms.**

## Timeline

### May 2000 — Ferrari measurement campaign
Tiziana Ferrari (then at INFN-CNAF) publishes "Differentiated Services: Experiment Report, Phase 2" — a real-network measurement campaign on a deployed DiffServ infrastructure. This is reference [48] in the Andreozzi 2001 thesis and the source of the figures in Appendix A. **It is the founding ground truth for the entire diffserv-ns3 lineage.**

### 2000–2001 — MSc thesis preparation, Pisa and Lappeenranta
Sergio Andreozzi prepares his MSc thesis at the **University of Pisa** (the degree-granting institution), with the work performed at **Lappeenranta University of Technology (LUT), Finland**, under co-supervision. Four supervisors:

- **Professor Luciano Lenzini** (Pisa)
- **Professor Luigi Rizzo** (Pisa) — well-known systems researcher, later author of dummynet and netmap
- **Professor Jari Porras** (LUT)
- **M.Sc. Kari Heikkinen** (LUT) — the day-to-day advisor (acknowledged as having "led this work")

The acknowledgements credit **Tiziana Ferrari** for "critical comments and beneficial suggestions about the simulation results" — establishing direct collaboration with the author of reference [48], who would later be a long-term colleague at INFN/EGI. **Chen-Nee Chuah** provided the VoIP traffic model.

### 5 November 2001 — Thesis PDF created
The thesis PDF (`tesi-5388.pdf`, 2.83 MB, 116 pages, A4) is created with Acrobat PDFMaker 5.0 for Word. Author field reads "tango". Preserved in `provenance/Andreozzi-2001-thesis.pdf`.

**Title:** *DiffServ simulations using the Network Simulator: requirements, issues and solutions*
**Year:** 2001 (Anno Accademico 2000/2001)
**Institution:** Università degli Studi di Pisa, Facoltà di Ingegneria, Corso di Laurea in Ingegneria Informatica (work performed at Lappeenranta University of Technology, Finland)
**Pages:** 116 (with 46 figures, 10 tables, 3 appendices)
**Language:** English

Concurrent with the thesis, the C++ source code for the extended ns-2 module — internally named **"DiffServ+"** in Figure 3.11 of the thesis — is complete, targeting **ns-2.1b8a** (the then-current ns-2 release). The algorithmic core of this 2001 implementation is what will eventually ship on SourceForge five years later as DiffServ4NS, after a 2006 re-skinning for the ns-2.29 API.

### 2002 — Personal-website release
The thesis and the DiffServ+ source code are made available on the author's personal website. The code is openly accessible to anyone who finds the page. This is the typical academic-software distribution model of the era: a tarball link on a university or personal homepage, no formal forge, no version numbering, no licence file, no separate project name distinct from the author's. (Year corrected from 2001 → 2002 by Sergio on 2026-04-26; the personal-website release happened around the time of the ISCC discussion, not concurrent with the thesis defence.)

### January 2002 — Thesis discussed
PDF modification timestamp: 11 January 2002. Discussion follows shortly after.

### July 2002 — ISCC 2002 publication
The thesis findings are published as:

> **Sergio Andreozzi.** *Differentiated services: an experimental vs. simulated case study.*
> Proceedings of the Seventh IEEE Symposium on Computers and Communications (ISCC 2002), 1–4 July 2002, Taormina, Italy. Pages 383–390.
> IEEE Computer Society. ISBN 0-7695-1671-8.
> IEEE Xplore: https://ieeexplore.ieee.org/document/1021705

The published paper is a distillation of the thesis. The thesis itself is the complete artefact — including the detailed UML class diagrams, three full simulation scenarios, and the side-by-side real-vs-simulated figures in Appendix A that are this project's validation oracle.

### 2002–2006 — Personal-website era
The DiffServ+ source code remains continuously available on the personal website. **No code changes are made.** The four years between the thesis and the SourceForge release are a period of distribution-channel maturation — open-source packaging conventions, public forges, and the cultural norm of releasing research code as a named project (rather than a tarball under your own name) are still consolidating during this period.

### 16 May 2006 — SourceForge registration
Project `diffserv4ns` registered on SourceForge under the username `andreozzi`. Licence: GPLv2. The module is renamed from "DiffServ+" to "DiffServ4NS" for public release — a more searchable, less-ambiguous name suitable for a public forge.

### 29 June 2006 — DiffServ4NS-0.1 released
First SourceForge release: 228 KB tarball, 4,265 lines of C++ in the diffserv core, with two example scenarios. **The module is ported from ns-2.1b8a to ns-2.29** — the algorithms are unchanged from 2001, but the C++ source files are re-skinned for the ns-2.29 API (Packet / Header / OTcl glue evolved between 1b8a and 2.29).

Other changes from the personal-website version are cosmetic: a README explaining installation on ns-2.29 and the rename from "DiffServ+" to "DiffServ4NS" for public release. The licence is unchanged — GPLv2 from the 2001 personal-website tarball forward.

### Shortly after — DiffServ4NS-0.2 released
A second SourceForge upload, never formally marked as a release. **The C++ source code is byte-identical to 0.1 — no diffs in `src/diffserv/`.** The only changes are:

- README rewritten to add the SourceForge download URL and clean up install instructions
- `examples/example-1/simulation-1.tcl` extended with two lines (a Dumb-policy entry for a second DSCP)
- `examples/example-2/clean` corrected to remove a stale cleanup line
- Eight gnuplot script files added under `examples/example-2/` (`ServiceRate.p`, `ClassRate.p`, `goodput.p`, `ipdv.p`, `owd.p`, `pktLoss.p`, `queue.p`, `virqueue.p`) — these were the plotting scripts the example needed but which had been left out of the 0.1 tarball

0.2 is functionally a packaging fix: the gnuplot scripts that example-2 needed in order to actually produce its plots had been omitted from 0.1 by mistake. **For the port, 0.2 is the canonical reference because it's a strict superset of 0.1 with no algorithmic differences.**

### 2006–2013 — Maintenance window (no maintenance)
The SourceForge project remains downloadable and indexed. **No further code changes ever occur.** Sergio's career moves toward EGI, federated research infrastructures, and grid/cloud governance work.

### 18 April 2013 — Last activity on SourceForge
Final recorded activity. The project enters dormancy as ns-2 itself enters legacy status and ns-3 takes over as the active simulator platform.

### 2013–2026 — Dormancy
Twenty years of dormancy total, counting from the 2001 implementation. Twenty from the personal-website release. Thirteen from SourceForge.

### April 2026 — ns-3 port begins
Twenty-five years after the 2001 implementation, Sergio returns to the module to port it to ns-3 mainline. The port uses **Evaluation-Driven Development (EDD)** — a spec-driven methodology Sergio developed for autonomous LLM coding agents in 2025–2026, applying tiered specification suites (Intent, Structural, Quality) as formalised constraints.

Three independent oracles validate the port:

1. **The 2001 thesis Chapter 3.3.3 design** + **the dsCore.h author header in the SourceForge release** → Intent specs
2. **RFC 2697 / 2698 / 2859 conformance vectors** → Structural specs
3. **Ferrari 2000 real-network measurements**, reproduced in **Appendix A of the 2001 thesis** as Figures A.1, A.3, A.5, A.7 → Quality specs (gold standard)

The port aims to bring to ns-3 mainline several capabilities that do not currently exist there: WFQ, WF2Q+, SCFQ, SFQ as schedulers; the TSW meters; per-DSCP frequency-distribution monitoring; and a composable DiffServ edge/core router.

### 16 April 2026 — Full-scale thesis Scenario 2 reconstruction
A Tcl script not shipped with the 2006 SourceForge release nor tracked in its SVN repository is reconstructed from the 2001 thesis Chapter 4 prose, Table 4.3 / 4.5, and Figure 4.3:

- **`ns2/diffserv4ns/examples/example-2-fullscale/`** — thesis Scenario 2 (§4.2, 469 nodes, AF PHB importance differentiation via a 6-way WRED parameter sweep, SFQ scheduler). Qualitative validation of all Appendix B claims (DP ordering, staggered vs overlapped differentiation). Quantitative Table 4.4 match is 29 of 54 cells within tolerance, constrained by two documented model approximations: (a) PagePool/WebTraf is incompatible with DiffServ4NS (crashes due to hdr_cmn struct enlargement) so HTTP is reconstructed as bulk TCP; (b) the thesis's FTP burst profile is not specified, so it is implemented as bulk FTP active only during the first 50s per §4.2 literal reading. See `ns2/diffserv4ns/examples/example-2-fullscale/README`.

This directory is demarcated as a 2026 reconstruction (not original 2001/2006 code) via an explicit DISCLAIMER section in its README. It sits alongside the authentic 2006 release artefacts (`example-1/`, `example-2/`) in `ns2/diffserv4ns/examples/`.

### 2026 — Scenario 3 SVN discovery; Software Heritage as redundant archive
Initially Scenario 3 was reconstructed from thesis prose (April 2026) following the same path as Scenario 2 above. The mistake was working only from the 0.1/0.2 tarball releases — the original Tcl driver was in fact present in the [SourceForge SVN repository](https://sourceforge.net/p/diffserv4ns/code/HEAD/tree/examples/) all along; it was just never packaged into any tarball release. Once we checked the SVN tree, the thesis-prose reconstruction was **replaced** with the original-author Tcl script, and Scenario 3 validation now anchors against authentic 2001 code.

- **`ns2/diffserv4ns/examples/example-3/`** — thesis Scenario 3 (§4.3, 771 nodes, 5-class Olympic + VoIP service model, LLQ scheduler). The Tcl driver is the original-author script recovered from the SourceForge SVN tree (see the SVN-discovery note above).

[Software Heritage](https://www.softwareheritage.org/) independently archives the SourceForge SVN tree (and many other forges); for this artefact, its role is forward-looking — long-term insurance against SourceForge shutdown — rather than recovery. The discovery happened on SourceForge itself.

Two practical lessons:
1. **Tarball releases and the underlying source-control tree can diverge silently.** When reconstructing legacy software, check both — VCS-only files are invisible to anyone reconstructing from released artefacts.
2. **Independent archival paths matter for future preservation, not present recovery.** Multiple permanent archives (SourceForge SVN + Software Heritage) protect a single-author project against forge attrition, even if the recovery itself happens on the original forge.

## What the thesis contains that the SourceForge release does not

The thesis is more than historical context. It contains specific artefacts not in the source code release:

1. **Chapter 3.3.3 — DiffServ module improvements**: a complete prose specification of every mechanism added on top of the Nortel ns-2 base. Reads as a contemporaneous design document, written alongside the code.
2. **Figure 3.11 — DiffServ+ module UML Class Diagram**: a full class diagram showing every class, every method, and every relationship. **The class names in this 2001 diagram match the class names in the SourceForge release line for line.**
3. **Section 3.3.3.2 — Measurements and performance analysis**: the rationale for *why* each performance parameter was added, not just what was added.
4. **Chapter 4 — Three complete simulation scenarios**: topology, traffic settings, configuration parameters, results. Scenario 1 explicitly reproduces the Ferrari [48] test bed.
5. **Appendix A — eight figures pairing real-network measurements with simulated reproduction**: Figures A.1, A.3, A.5, A.7 are from Ferrari 2000; A.2, A.4, A.6, A.8 are the thesis simulation results. The visual similarity of each pair is the original validation evidence. A page (Figure A.5 vs A.6) is preserved as `provenance/figure-A5-A6-real-vs-simulated.jpg`.
6. **The complete reference list (52 entries)**: every algorithm citation needed for the schedulers, every IETF RFC and draft current as of 2001.

For the port, **the thesis sits above the source code in the authority hierarchy**. The source is a 2001 snapshot frozen in amber; the thesis is the contemporaneous explanation of what that snapshot was meant to do. When the source is ambiguous, the thesis tells you the design intent. There is no other source of truth — no later commentary, no maintenance notes, no v0.2 changelog, because the code never changed.

## Why this matters for the port

Three properties make this an unusually clean candidate for spec-driven, agent-assisted porting:

1. **Single author, single design moment.** The code, the thesis, the SourceForge release, and the port are all the work of one person. There is no maintainer drift, no inherited design decisions to unpick, no mystery commits to investigate. When Claude Code asks "why is this class structured this way?", the answer exists and is in Chapter 3 of the thesis.

2. **Single ground truth.** Ferrari 2000 is the only measurement source. Every validation claim — in 2001, in 2002, in 2006, and in 2026 — traces back to it. The Q-tier specs in `specs/03-quality.md` are anchored to the same figures the thesis used.

3. **Frozen reference.** The 2001 algorithmic core is preserved through every downstream step. The 2006 SourceForge release re-skinned the files for the ns-2.29 API without changing the algorithms; 0.1 and 0.2 differ only in packaging. This means the thesis prose can be cited as authoritative for the behaviour of *every* line of the source code, with no risk that some later revision invalidated the cross-reference.

## Conceptual continuity

The 2001 thesis was about **discovering where simulation diverges from reality**, then closing the gap. The methodology had three steps:

1. Take a real-network measurement campaign as ground truth (Ferrari 2000)
2. Reproduce the same scenario in simulation; observe where the simulator falls short
3. Extend the simulator until it can reproduce the measurements faithfully

EDD is the same methodology at a different level. Take a specification suite as ground truth. Reproduce the spec in code; observe where the implementation falls short. Iterate until the implementation passes the spec.

Both are gap-finding methodologies built on careful comparison against an external oracle. The 2001 oracle was a real DiffServ deployment; the 2026 oracle is a tiered spec suite plus the 2001 reference implementation plus the IETF RFCs. The continuity is not coincidental — Sergio's later work on federated research infrastructures (EGI, GLUE 2.0, GAIA-X vocabularies) operates on the same principle at a different level: defining shared specifications so that heterogeneous systems can be composed and verified against a common reference. The Operations Research framing he applies to agentic AI — humans define constraints, agents navigate solution spaces autonomously — is the same shape: separate the *what* from the *how*, formalise the *what*, let the *how* be discovered and validated against it.

So this port is not just an exercise in moving 4,000 lines of C++ from one simulator to another. It is a full-circle return: the same person, the same problem domain, the same methodological commitment, applying twenty-five years of accumulated craft to a module he wrote as an MSc student — and which, even then, was already grounded in real measurements made by a colleague who would later become a long-term collaborator.

## For the WNS3 paper

If a publication comes out of the port, the lineage gives it a distinctive framing:

> *In 2000, Tiziana Ferrari published a measurement campaign on a real DiffServ deployment. In 2001, Sergio Andreozzi's MSc thesis at the University of Pisa (work performed at Lappeenranta University of Technology, Finland) used those measurements as ground truth to identify gaps in the ns-2 DiffServ module, designed and implemented an extended module ("DiffServ+") against ns-2.1b8a whose UML class diagram defined every class that would later ship publicly, and demonstrated that the extended simulator could reproduce the real measurements within tight tolerances. The work was published at ISCC 2002, and the source code was immediately made available on the author's personal website. In 2006, the module was ported from ns-2.1b8a to ns-2.29 and released on SourceForge as DiffServ4NS-0.2 under GPLv2 — a re-skinning for the evolved ns-2 API at the moment when "research code on a public forge with a project name and a licence" had become the new norm. The algorithmic core never changed. The module was dormant for twenty years. This paper presents a port to ns-3, performed by the original author twenty-five years after the original implementation, using Evaluation-Driven Development with an LLM coding agent. The port closes specific gaps in ns-3 mainline (WFQ family, TSW meters, per-DSCP monitoring), validates against the original Ferrari measurements via the thesis appendix figures, and serves as a case study of how a contemporaneous thesis can function as the founding specification for an LLM-assisted reimplementation a quarter-century later — across the entire history of how academic software has been distributed.*

That last clause is the unusual angle: the project literally bridges three eras of academic software distribution (personal website → public forge → LLM-assisted port from a frozen reference), with a single author and a single ground truth running through all three.

## References

1. **Ferrari, T.** (2000). *Differentiated Services: Experiment Report, Phase 2.* v 1.1, May 2000. The founding measurement source.
2. **Andreozzi, S.** (2001). *DiffServ simulations using the Network Simulator: requirements, issues and solutions.* MSc thesis, Università degli Studi di Pisa, Facoltà di Ingegneria (work performed at Lappeenranta University of Technology, Finland), Anno Accademico 2000/2001. 116 pages. Preserved as `provenance/Andreozzi-2001-thesis.pdf`.
3. **Andreozzi, S.** (2002). *Differentiated services: an experimental vs. simulated case study.* ISCC 2002, Taormina, Italy. IEEE 0-7695-1671-8. https://ieeexplore.ieee.org/document/1021705
4. **DiffServ4NS** project on SourceForge. https://sourceforge.net/projects/diffserv4ns/ — preserved as `ns2/diffserv4ns/` (using the 0.2 tarball, which is a strict packaging-only superset of 0.1).
5. **RFC 2474** — Definition of the Differentiated Services Field (DS Field) in the IPv4 and IPv6 Headers
6. **RFC 2475** — An Architecture for Differentiated Services
7. **RFC 2597** — Assured Forwarding PHB Group
8. **RFC 2598 / RFC 3246** — An Expedited Forwarding PHB
9. **RFC 2697** — A Single Rate Three Color Marker (Heinanen, Guérin)
10. **RFC 2698** — A Two Rate Three Color Marker (Heinanen, Guérin)
11. **RFC 2859** — A Time Sliding Window Three Colour Marker
12. **Imputato, P. & Avallone, S.** (2016). *Design and Implementation of the Traffic Control Module in ns-3.* WNS3 2016. ACM. doi:10.1145/2915371.2915382
13. **Imputato, P. & Avallone, S.** (2017). *Traffic Differentiation and Multiqueue Networking in ns-3.* WNS3 2017.

### Scheduler references (from thesis bibliography)

14. **Parekh, A. K. & Gallager, R. G.** (1993). *A Generalized Processor Sharing Approach to Flow Control in Integrated Services Networks: The Single-Node Case.* IEEE/ACM Trans. on Networking Vol. 1, No. 3. — WFQ foundation.
15. **Bennett, J.C.R. & Zhang, H.** (1996). *WF2Q: Worst-case fair weighted fair queueing.* Proc. IEEE INFOCOM '96. — WF2Q.
16. **Bennett, J.C.R. & Zhang, H.** (1997). *Hierarchical Packet Fair Queueing Algorithms.* IEEE/ACM Trans. on Networking Vol. 5, No. 5. — WF2Q+.
17. **Golestani, S. J.** (1994). *A self-clocked fair queueing scheme for broadband applications.* Proc. IEEE INFOCOM '94. — SCFQ.
18. **Goyal, P., Vin, H. M. & Cheng, H.** (1996). *Start-time Fair Queueing: A scheduling algorithm for integrated services packet switching networks.* Proc. SIGCOMM 1996. — SFQ.
19. **Cisco Systems** (2001). *Low Latency Queueing.* Cisco IOS documentation. — LLQ.

### iproute2 userspace snapshots

- `provenance/linux-iproute2-87c66f79d8b0/q_cake.c` — frozen iproute2 userspace CAKE qdisc plugin at commit `87c66f79d8b09779c01c07122b0846f83b566dc1` (2026-05-07). Authoritative for flow-isolation mode names (`cake_flow_names[]`), `tc -s qdisc show cake` xstats output format, and `autorate-ingress` flag spelling. Cited by ADR-0093.
- `provenance/iproute2-q-cake-62d47c2dbc0eaecdd20c0e19406067488025e92e/q_cake.c` — frozen iproute2 userspace CAKE qdisc plugin at commit `62d47c2dbc0eaecdd20c0e19406067488025e92e` (fetched 2026-05-23). Authoritative for `cake_link_layer_keywords[]` (link-layer overhead preset table, T1.2 LinkPreset) and `presets[]` (RTT preset table, T1.3 RttPreset). Cited by ADR-0117.
