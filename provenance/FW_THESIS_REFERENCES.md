# FWMeter (FW) — Thesis References Analysis

**Date:** 2026-04-15
**Purpose:** Determine what the 2001 thesis says about the FW meter, specifically whether the three penalty modes (deterministic, probabilistic, periodic) were intentional design alternatives or primary-vs-fallback.
**Context:** open design-intent question on the FW meter.

---

## Search Results

### Direct term matches

| Search term | Matches in thesis |
|-------------|-------------------|
| `FairWeighted` / `Fair Weighted` / `Fair-Weighted` | **0** |
| `FW` (standalone token) | **0** |
| `FWPolicy` | **0** |
| `fair weighted meter` / `fair-weighted meter` | **0** |
| `byte threshold` / `per-flow byte` | **0** |
| `downgrade2` | **0** |
| `flow_entry` / `flow timeout` / `FLOW_TIME_OUT` | **0** |
| `deterministic` + `probabilistic` + `periodic` (in proximity) | **0** |
| `penalty` / `penalize` | **0** |
| `bytes_sent` / `byte count` | **0** |

### Indirect matches

| Search term | Matches | Context |
|-------------|---------|---------|
| `per-flow state` | 1 (line 741) | RFC 2475 quote: "without the need for per-flow state" — describes DiffServ's scalability principle, not FW |
| `rate limiter` | 4 | All refer to PQ scheduler rate limiting, not FW |

### Thesis sections that enumerate meters

**Section 3.3.2 "DiffServ support"** (line 1801) lists the meters explicitly:

> **Meter: TSW2CM, TSW3CM, Token Bucket, srTCM and trTCM**

FW is **not listed**.

**Section 3.3.3 "DiffServ module improvements"** (lines 1914–1958) lists all improvements made by the thesis author. Under "marker and meter," the improvement is: "enable marking on a per-packet basis and decouple marking from metering." No new meter type is mentioned.

### UML class diagrams

**Figure 3.8** (line 1768) — "DiffServ module UML Class Diagram" (the Nortel original):
Policy hierarchy shown: `Policy` → `SRTCMPolicy`, `TRTCMPolicy`, `TBPolicy`, `TSW2CMPolicy`, `TSW3CMPolicy`.
**FWPolicy not shown.**

**Figure 3.11** (line 2092) — "DiffServ+ module UML Class Diagram" (the improved module):
Policy hierarchy shown: `Policy` → `DumbPolicy`, `SRTCMPolicy`, `TRTCMPolicy`, `TBPolicy`, `TSW2CMPolicy`, `TSW3CMPolicy`.
**FWPolicy not shown.**

### Future work (Chapter 6)

The thesis lists future work (lines 3287–3303): redesigning dropper/policy, hierarchical scheduling, MPLS integration, network management, per-connection metrics. **No mention of FW or any new meter.**

---

## Key Finding: FW is a Nortel Original, Renamed by Sergio

The pristine ns-2.29 DiffServ module (Nortel Networks, 2000) contains a class called **`SFDPolicy`** at policy slot `#define SFD 6`. Its code is **algorithmically identical** to DiffServ4NS's `FWPolicy`:

- Same `flow_entry` struct (with `fid`, `last_update`, `bytes_sent`, `count`)
- Same `FLOW_TIME_OUT = 5.0` constant
- Same `applyMeter()` logic (linked-list traversal, flow timeout, byte accounting)
- Same `applyPolicer()` logic with the same three penalty modes (`downgrade2` = 0, 1, 2)
- Same comments (e.g., "The coresponding flow is dead/expired")

The only differences in DiffServ4NS's `FWPolicy` vs. pristine `SFDPolicy`:
1. **Renamed**: `SFD` → `FW`, `SFDPolicy` → `FWPolicy`, `SFDPolicer` → `FWPolicer`, `sfdTagger` → `fwTagger`
2. **Removed** `src_id` and `dst_id` fields from `flow_entry` (DiffServ4NS identifies flows by `fid` only)
3. **Removed** unused local variables `fid`, `src_id`, `dst_id` from `applyPolicer()`

The pristine ns-2.29 also contains two other Nortel-original policies not present in DiffServ4NS:
- `EWPolicy` (`#define EW 7`) — Early Warning policy
- `DEWPPolicy` (`#define DEWP 8`) — something related to EW

These were **dropped** by Sergio (slots 7 and 8 removed from the enum), while SFD was **renamed to FW** (slot 6 preserved).

### What "SFD" likely stands for

The acronym "SFD" is not expanded in the ns-2.29 source code or documentation. Given the algorithm (per-flow byte counting with proportional/deterministic/periodic downgrading of excess flows), plausible expansions are:
- **S**ource-based **F**air **D**ropping
- **S**imple **F**low **D**ifferentiation
- **S**tateful **F**low **D**iscipline

The meaning of "FW" is unknown. The expansion is not recorded in the source code, the thesis, the SourceForge project, or the ns-users mailing list archive. The author (Sergio Andreozzi) does not recall the intended meaning. No external source uses the term "FWPolicy" — it exists only in the DiffServ4NS codebase.

---

## Summary

| Question | Answer |
|----------|--------|
| Total mentions of FW in the thesis | **0** |
| Chapters/sections discussing FW | **None** |
| Three penalty modes described? | **Not in the thesis** — they exist only in the source code (inherited from Nortel's SFDPolicy) |
| Any experimental scenario using FW? | **No** — all thesis experiments use TB, srTCM, or trTCM |
| Thesis recommends a specific mode? | **No** — the thesis does not discuss FW at all |
| Origin of FW | **SFD (Short Flow Differentiating)** algorithm by Chen & Heidemann (USC/ISI). Implemented in ns-2 DiffServ module as `SFDPolicy` (~2000). Renamed to `FWPolicy` in DiffServ4NS (2001). Published as a paper in 2003. |
| Code changed vs. Nortel original? | **Minimal** — algorithmic logic identical, only renamed + removed `src_id`/`dst_id` from flow_entry |

---

## SFD Acronym Resolved

**SFD = Short Flow Differentiating.** The algorithm originated as research by Xuan Chen and John Heidemann at USC/ISI (the same institution where ns-2 was developed). Published as:

> Chen, X. and Heidemann, J. "Preferential treatment for short flows to reduce web latency." *Computer Networks*, 2003, 41(6):779–794.

The algorithm gives preferential treatment to short TCP flows (web request/response patterns) to reduce user-perceived web latency. Per the abstract: "SFD algorithm reduces the transmission latency of short flows and the response time to retrieve representative web pages by about 30%. Using web traces, we demonstrate that 99% web pages would be transferred faster. SFD penalizes long flows, but the penalty is well bounded."

### Chronology

```
~2000     SFDPolicy implemented in ns-2 DiffServ module (USC/ISI)
2001      Sergio develops DiffServ4NS, renames SFD → FW
2003      Chen & Heidemann publish the SFD paper in Computer Networks
2005/2006 DiffServ4NS re-released on SourceForge against ns-2.29 baseline
```

The rename happened **before** the paper was published. Sergio could not have known the published algorithm name because it did not yet exist as a publication. The rename from SFD to FW (meaning unknown) effectively severed the code from its research lineage — which only became traceable once the paper appeared two years later.

### Fork-divergence evidence (added 2026-04-16)

A natural question is whether the `SFDPolicy → FWPolicy` rename could have come from ns-2 mainline between Sergio's original 2001 work (on `ns-2.1b8`) and the 2005/2006 SourceForge re-release (on `ns-2.29`), rather than from Sergio himself. The hypothesis is motivated by the ns-2 `CHANGES.html` entry of 27 Nov 2003: *"Change the name of dumbPolicy to nullPolicy for consistency with diffserv documentations (suggested by Alexander Sayenko)."* — which proves that upstream ns-2 did perform policy-class renames in this window.

Three pieces of evidence **disprove the hypothesis**:

1. **ns-2.29-pristine still has `SFDPolicy`.** The `SFD → FW` rename never happened upstream:
   ```
   ns2/ns-2.29-pristine/diffserv/dsPolicy.h:51  #define SFD 6
   ns2/ns-2.29-pristine/diffserv/dsPolicy.h:55  enum policerType { nullPolicer, …, SFDPolicer, EWPolicer, DEWPPolicer, };
   ns2/ns-2.29-pristine/diffserv/dsPolicy.h:207 class SFDPolicy : public Policy { … };
   ```

2. **DiffServ4NS kept the *older* `dumbPolicy` name,** i.e. it did **not** pick up the Nov 2003 upstream rename:
   ```
   src/ns-2.29/diffserv/dsPolicy.h:99   #define DUMB 0
   src/ns-2.29/diffserv/dsPolicy.h:107  enum policerType { dumbPolicer, TSW2CMPolicer, …, FWPolicer };
   src/ns-2.29/diffserv/dsPolicy.h:187  class DumbPolicy : public Policy { … };
   src/ns-2.29/diffserv/dsPolicy.cc:155 if (strcmp(argv[3], "Dumb") == 0) {   // Tcl token still "Dumb"
   ```
   If DiffServ4NS had been re-synced against ns-2.29 mainline after Nov 2003, `dumbPolicy` would now be `nullPolicy`. It isn't.

3. **DiffServ4NS lacks `EWPolicer`/`DEWPPolicer`,** which ns-2.29 mainline added after 2001 (enum slots 7 and 8). Another divergence marker showing the branch never pulled upstream changes.

**Conclusion.** DiffServ4NS is a **divergent 2001 fork** of the `ns-2.1b8` DiffServ module. Sergio maintained it through 2006 *without* re-merging against ns-2.29 upstream. The `SFD → FW` rename is therefore a DiffServ4NS-internal edit, not an upstream event. The original Phase-6-prep finding stands.

Summary of fork vs. upstream divergence after 2001:

| Change | Upstream ns-2 mainline | DiffServ4NS |
|---|---|---|
| `SFDPolicy` → `FWPolicy` | ❌ never happened (still `SFDPolicy` in ns-2.29) | ✅ renamed by Sergio in 2001 |
| `dumbPolicy` → `nullPolicy` (Nov 2003, upstream) | ✅ applied | ❌ never merged — still `dumbPolicy` |
| `EWPolicer`, `DEWPPolicer` added | ✅ present in ns-2.29 | ❌ not present |
| `src_id`/`dst_id` removed from `flow_entry` | ❌ | ✅ Sergio's simplification |

### ns-2 webtraf.tcl evidence

The ns-2.29 `webtraf.tcl` modifications include SFD-specific controls (e.g. `FLOW_SIZE_TH_=15` packets ≈ 15KB, `FLOW_SIZE_OPS_` traffic mix). A comment at line 62 reads: "options to control traffic based on flow size, used to evaluate SFD algorithm." This confirms SFD was research code integrated into the ns-2 DiffServ module at USC/ISI.

### Implications for attribution

The ns-3 port should preserve the name FW (for DiffServ4NS compatibility) but document the SFD provenance and Chen & Heidemann citation in the header comment. The algorithm is **not RFC-based** and maintains per-flow state, which deviates from RFC 2475's per-flow-stateless principle for DiffServ.

---

## Implications for the port

1. **No thesis-level specification exists for FW.** The algorithm's specification is the source code itself (both `SFDPolicy` in ns-2.29-pristine and `FWPolicy` in DiffServ4NS). Test vectors must be derived from the code.

2. **All three penalty modes were present in the original SFD implementation.** They are not "design alternatives" selected by the thesis author — they are inherited functionality. The three modes were likely designed for different operational scenarios: deterministic for strict compliance, probabilistic for statistical fairness, periodic for simple burst control.

3. **The rename from SFD to FW is now documented.** See `docs/NS2_PATCHES.md` §"Renames and removals".

4. **No experimental validation of FW exists** in the thesis or in any known DiffServ4NS documentation. The meter was inherited, renamed, and carried forward without specific testing in the thesis context.

5. **The `src_id`/`dst_id` removal** in DiffServ4NS's `flow_entry` is consistent with Sergio's decision to identify flows by `fid` only (simplification).

6. **The SFD provenance discovery strengthens the methodology paper.** The provenance chain (DiffServ4NS:FW → ns-2:SFD → Chen & Heidemann 2003) demonstrates that spec-driven LLM-augmented porting can reconnect renamed code to its published research lineage.

---

## Cross-reference

- Chen & Heidemann 2003: *Computer Networks* 41(6):779–794
- Pristine ns-2.29 SFDPolicy: `ns2/ns-2.29-pristine/diffserv/dsPolicy.{h,cc}`
- DiffServ4NS FWPolicy: `src/ns-2.29/diffserv/dsPolicy.{h,cc}`
- Thesis text: `provenance/Andreozzi-2001-thesis.txt`
- Thesis section 3.3.2 (meter enumeration): line 1801
- Thesis section 3.3.3 (module improvements): lines 1914–1958
- Thesis Figure 3.8 (original UML): line 1768
- Thesis Figure 3.11 (improved UML): line 2092
- Rename documentation: `docs/NS2_PATCHES.md` §"Renames and removals"
