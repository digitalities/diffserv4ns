# HISTORICAL_BUGS — Bugs surfaced by the DiffServ4NS port

This file documents bugs surfaced while porting DiffServ4NS from ns-2.29
to ns-3. Entries are catalogued under an **area-prefix scheme** that
shows where in the stack each defect lives:

- **N2-N** — ns-2 core defects (the ns-2 runtime / Tcl interpreter / WebTraf)
- **D2-N** — DiffServ4NS-for-ns-2 defects (the headline bucket)
- **N3-N** — ns-3 core defects
- **D3-N** — DiffServ4NS-for-ns-3 defects (intentionally empty for v1)

The scheme replaces the original chronological **BUG-1..10** identifiers
on 2026-04-26 because reader value is much higher with area-grouped IDs
that show *where* a defect lives rather than *when* it was discovered.
Old commit messages, ADRs, and external citations referring to BUG-N
are unchanged; the mapping table below provides the bridge.

This record serves two purposes:
1. **Methodology:** demonstrates that the port is a *reasoned* translation,
   not a blind line-by-line copy.
2. **ICNS3 2026 paper / tech report:** provides concrete examples of how
   high-fidelity reconstruction surfaces latent bugs in *both* the legacy
   code being ported and the modern simulator framework it is being
   ported to.

---

## ID mapping (old chronological -> new area-prefix)

| New ID | Old ID | Layer | Headline |
|--------|--------|-------|----------|
| N2-1 | BUG-7 | ns-2 core (Tcl 8.5) | `catch+expr X/0` ~50 000x slowdown |
| N2-2 | BUG-9 | ns-2 core (WebTraf) | xuanc-2011 destructor + recycle_page_ shared-RV SEGV |
| D2-1 | BUG-5 | DS4 for ns-2 (FTP) | `set_apptype` patch only on `start`, not `send`/`produce`/`producemore` (25-yr dormant) |
| D2-2 | BUG-8 | DS4 for ns-2 (S3 VoIP) | `voip_connection` never set bare global `Sink_` expected by `record_delay` (25-yr dormant) |
| D2-3 | BUG-10 | DS4 for ns-2 & ns-3 (TSW/FW) | shared global RNG stream (opt-in fix in both ports) |
| D2-4 | BUG-4 | DS4 for ns-2 (SFQ) | `FlowQueue.front()` before `empty()` check (UB) |
| D2-5 | BUG-1 | DS4 for ns-2 (CBR) | `set_pkttype(PT_CBR)` dead-overwritten by `set_pkttype(PT_UDP)` |
| D2-6 | BUG-2 | DS4 for ns-2 (RealAudio) | same overwrite pattern in `realaudio.cc` |
| D2-7 | BUG-3 | DS4 for ns-2 (FTP magic int) | magic literal `27` instead of `$PT_FTP` |
| D2-8 | BUG-11 | DS4 for ns-2 (dsRED Tcl shim) | `getStat TCPbReTX/TCPnReTX` arg-swap; biased published goodput by `(MSS-1024)/1024` per packet (25-yr dormant) |
| N3-1 | BUG-6 | ns-3 core (TCP) | `TcpSocketBase::PersistTimeout` null deref (MR !2829) |
| D3-* | (none) | DS4 for ns-3 | empty bucket — methodology story (EDD spec-tier gate) |

The "(formerly BUG-N)" annotations are preserved in each entry's
heading so external references resolve.

---

## Bug fixes applied in the ns-2.35 port

The ns-2.35 port is framed as "improved DS4" — a bug-fixed reference
implementation against which the ns-3 port is compared, rather than a
faithful reproduction of the 2001 defects. All fixes live in layer 2
under `src/ns-2.35/`, which replaces selected DS4 base
files when patching the ns-2.35 tree.

| Bug | File | Rationale |
|-----|------|-----------|
| D2-5 (BUG-1) | `src-ns235/tools/cbr_traffic.cc` | `set_apptype(PT_CBR)` so CBR traffic is classified correctly by DiffServ policy |
| D2-6 (BUG-2) | `src-ns235/realaudio/realaudio.cc` | `set_apptype(PT_REALAUDIO)` so RealAudio traffic is classified correctly |
| D2-7 (BUG-3) | `src-ns235/tcl/ns-source.tcl` | Replace magic number 27 with `$PT_FTP` variable; correct misstatement in docs (27==PT_FTP, not PT_HTTP) |
| D2-4 (BUG-4) | `src-ns235/diffserv/dsscheduler.cc` | `empty()` check moved before `front()` in SFQ dequeue loop to eliminate C++ UB |
| D2-1 (BUG-5) | `src-ns235/tcl/ns-source.tcl` | `set_apptype` added to all four `Application/FTP` instprocs |
| D2-2 (BUG-8) | `src-ns235/tcl/scenario-3.tcl` | bind `global Sink_` in `voip_connection` so `record_delay` emits OWD/IPDV |
| D2-3 (BUG-10) | `src-ns235/diffserv/dsPolicy.{h,cc}`, `dsEdge.cc` | opt-in `Policy::setRngStream()` + `edgeQueue policyRngStream` Tcl command |
| N2-1 (BUG-7) | `src-ns235/tcl/scenario-2.tcl`, `scenario-3.tcl` | replace `catch {expr X/0}` with explicit `if {$pkts<=0}` divisor guard |
| N2-2 (BUG-9) | `src-ns235/webcache/webtraf.cc` | remove the `if (recycle_page_) { Tcl delete ... }` block from `~WebTrafSession` |
| D2-8 (BUG-11) | `src-ns235/diffserv/dsred.cc` | swap return values in `dsREDQueue::getStat` for `TCPbReTX`/`TCPnReTX`; add byte-unit `origBytes`/`retxBytes` aliases for ns-3 parity |
| UDP hdr | `src-ns235/apps/udp.cc` | `sendmsg()` adds 28 bytes (IP 20 + UDP 8) so `hdr_cmn::size()` matches real-world serialisation and aligns with ns-3 |

---

## N2-1 (formerly BUG-7): Tcl 8.5 `catch {expr X/0}` performance regression (~50 000× slowdown)

**Identified:** 2026-04-17, during ns-2.35 Scenario 2 port profiling
**Scope:** Upstream Tcl 8.5 regression — affects any ns-2.35 (or later) user
of the DiffServ4NS periodic-monitoring procs `record_pkt_loss` (and the
equivalent in Scenario 3). Not a DiffServ4NS bug, but the port surfaces it.

### Observation

Scenario 2 (`example-2-fullscale`) on ns-2.35 hangs CPU-pegged for tens of
minutes at simulated time 0, where the same scenario on ns-2.29 completes in
~8 minutes wall clock. Bisection isolated the cause to the monitoring proc
`record_pkt_loss`, which contains ten `catch {expr X*100.0/0}` statements
that fire every simulated second.

### Micro-benchmark (pure Tcl, no ns-2 / DiffServ code)

| Benchmark (10 000 iterations) | Tcl 8.4.11 (ns-2.29) | Tcl 8.5.10 (ns-2.35) | Ratio |
|---|---|---|---|
| `set` | 2 ms | 2 ms | 1× |
| `expr` (safe) | 6 ms | 7 ms | 1.2× |
| `catch {expr safe}` | 6 ms | 7 ms | 1.2× |
| **`catch {expr X/0}`** | **13 ms** | **> 10 min (killed)** | **≥ 46 000×** |

The first three paths scale normally. Only the `catch`-wrapped
division-by-zero explodes.

### Root cause (hypothesised)

Tcl 8.5 introduced (i) a new bytecode compiler, (ii) an arbitrary-precision
numeric object system that promotes int/double on overflow or special values,
and (iii) richer error-dict metadata (`-errorinfo`, `-errorcode`, full return
options dict). The `expr`-raises-error path appears to allocate heavy per-error
objects that Tcl 8.4 did not, and in a tight loop these add up catastrophically.

### Fix (applied 2026-04-17)

Guard the divisor instead of relying on `catch` to swallow division-by-zero.

```tcl
proc loss_pct {q stat_type dscp} {
    set pkts [$q getStat pkts $dscp]
    if {$pkts <= 0} { return 0 }
    return [expr [$q getStat $stat_type $dscp] * 100.0 / $pkts]
}
```

Applied to `examples/example-2-fullscale/scenario-2.tcl::record_pkt_loss` and
`examples/example-3/scenario-3.tcl::record_pkt_loss`. After the fix, Scenario 2
on ns-2.35 runs 5000 s of simulated time in a few minutes of wall clock, as
expected.

### Reproducer

See `scratch/scenario2-profile/catch-expr-test.tcl` for the pure-Tcl
micro-benchmark. To reproduce, run it under both ns-2.29's Tcl 8.4 and
ns-2.35's Tcl 8.5 (via the respective Docker images).

---

## N2-2 (formerly BUG-9): WebTraf session-completion SEGV — shared RandomVariable delete (upstream ns-2, **FIXED**)

**Identified:** 2026-04-18, during Phase 7 Scenario 2 per-flow srTCM sweep.
**Status:** Root cause pinned 2026-04-19 (Phase A instrumentation). Fix
lives in `src/ns-2.35/webcache/webtraf.cc` `WebTrafSession::~WebTrafSession`.
Workaround (`numPages=1000`, commit `b1417a4`) reverted to the thesis-faithful
`numPages=250` in the same commit that landed the fix.
**Scope:** Upstream ns-2 bug in `webcache/webtraf.cc`, present in every ns-2
release from 2011 (when xuanc added the `recycle_page_` cleanup path) through
2.35. Not a DiffServ4NS bug, but DS4 hit it because thesis-faithful Scenario 2
parameters land session completion well inside `simTime`.

### Observation

Any `PagePool/WebTraf` simulation where one or more `WebTrafSession`
instances reach their last page during `simTime` SIGBUSes (rc=135) or
SIGSEGVs (rc=139). Reliably reproduced on ns-2.35 Scenario 2 with
thesis-faithful parameters (250 pages × 400 sessions × Exp(15 s)
inter-page): every 6-set WRED sweep at `simTime=5000` crashed around
t ≈ 3688–3767 s, matching 250 × 15 = 3750 s mean. Traces up to the crash
are written cleanly. Also reproducible in ~11 s with `numPages=3` /
`simTime=200` / 20 sessions (`smoke-bug9-phaseA.tcl`).

### Root cause (2026-04-19 Phase A evidence)

The bug is **not** a use-after-delete on the session/page objects in their
own method frame. It is a **dangling pointer to a shared Tcl-level
`RandomVariable`** that the session destructor deletes.

In `WebTrafSession::~WebTrafSession` (xuanc addition, c. 2011):

```cpp
if (recycle_page_) {
    if (rvInterPage_ != NULL)
        Tcl::instance().evalf("delete %s", rvInterPage_->name());
    if (rvPageSize_ != NULL)
        Tcl::instance().evalf("delete %s", rvPageSize_->name());
    // ...and similarly for rvInterObj_, rvObjSize_
}
```

`recycle_page_` defaults to `1` via `tcl/webcache/webtraf.tcl:42`:
```tcl
PagePool/WebTraf set recycle_page_ 1
```

The idiomatic ns-2 scenario pattern creates **one** Tcl RandomVariable
instance per RV-type and passes that same Tcl name to every session's
`create-session`. All 400 sessions alias the same 4 `RandomVariable*` pointers.

When the first session to complete its last page enters its destructor,
the `Tcl delete` calls **delete the shared RV objects**. Every other
live session now holds a raw pointer to freed Tcl memory. The next
`sched(rvInterPage_->value())` in any session's `WebTrafSession::donePage`
(inter-page schedule) or `expire` (page-size draw) dereferences the freed
RV → SEGV.

### Fix

`src/ns-2.35/webcache/webtraf.cc` `WebTrafSession::~WebTrafSession`:
removed the entire `if (recycle_page_) { Tcl delete ... }` block. The Tcl
interpreter garbage-collects the RandomVariable objects at simulation
exit anyway, so there is no leak concern for any realistic simulation
length. Scenarios that actually do allocate per-session private RVs and
want to reclaim them before sim-end can do so explicitly from Tcl.

The `recycle_page_` attribute stays bound (for compatibility with any
scenario that reads it); it is now functionally a no-op in the destructor.

### Verification

- **C1 smoke** — `smoke-bug9-phaseA.tcl` (numPages=3, 20 sessions, 200 s):
  pre-fix rc=139 at t ≈ 11 s; post-fix rc=0 with all 20 sessions completing
  cleanly by t ≈ 101 s.
- **C2 smoke** — `smoke-bug9-phaseC2.tcl` (numPages=10, 20 sessions, 300 s):
  post-fix rc=0.
- **C3 full sweep** — `scripts/run-ns235-scenario2-sweep.sh` and
  `run-ns235-scenario2-srtcm-sweep.sh` at `SIM_TIME=5000`, numPages=250
  (thesis-faithful), 400 sessions. All 12 sets (6 port-based + 6 srTCM)
  per-sim rc=0, no rc=135 / rc=139 anywhere in the sweep logs.

### Upstream artefact

Candidate upstream fix (not yet filed): flip the default in
`tcl/webcache/webtraf.tcl`:
```tcl
PagePool/WebTraf set recycle_page_ 0   ;# was 1 — SEGV under shared-RV usage
```
This makes the dangerous behaviour opt-in. Scenarios using per-session
private RVs can still enable it. Zero-risk default flip.

---

## D2-1 (formerly BUG-5): Incomplete `set_apptype` patch in Tcl `Application/FTP` (25-year dormant)

**File:** `ns2/ns-allinone-2.29.3/ns-2.29/tcl/lib/ns-source.tcl`,
lines 50–68 (patched by DiffServ4NS; pristine does not contain
`set_apptype`).

**What happens:** the DiffServ4NS patch adds exactly one line to this
file to propagate the new `app_type_` field (DiffServ4NS's addition
to `hdr_cmn`) into the outgoing packet header when an FTP flow
starts:

```tcl
Application/FTP instproc start {} {
        [$self agent] set_apptype 27      ;# <-- DiffServ4NS patch (PT_FTP)
        [$self agent] send -1
}
```

The sibling methods that also initiate data transmission are NOT
patched:

```tcl
Application/FTP instproc send {nbytes} {
        [$self agent] send $nbytes        ;# no set_apptype — SILENT GAP
}
Application/FTP instproc produce { pktcnt } {
        [$self agent] advance $pktcnt     ;# no set_apptype — SILENT GAP
}
Application/FTP instproc producemore { pktcnt } {
        [$self agent] advanceby $pktcnt   ;# no set_apptype — SILENT GAP
}
```

Consequence: `$ftp send N`, `$ftp produce N`, and `$ftp producemore N`
all transmit FTP data without the `app_type_` field being stamped.
The DiffServ classifier (`dsPolicy.cc`) reads `app_type_` on each
packet; when the field is zero (`PT_NTYPE`), the classifier falls
through to its default rule, and the packets get DSCP 0 (Default)
instead of DSCP 12 (AF11/FTP).

**Probable intent:** the patch was added in 2001 to make FTP traffic
classify as AF11 in the DiffServ module. The released examples
(`example-1.tcl`, `example-2.tcl`, `example-3.tcl`) all use the
`$ftp start` / `$ftp stop` pattern exclusively — they never call
`send N` / `produce N`. The patch covered the only path the
examples exercised. The sibling methods were simply missed.

**Why it didn't break simulations:** the DiffServ4NS examples never
hit the unpatched paths. The bug is latent — it only fires when a
downstream user models FTP as finite-size file transfers rather than
unlimited bulk streams.

**Latency:** ~25 years (2001–2026). First surfaced during the 2026
Scenario 2 full-scale reconstruction, where finite 50 KB FTP
transfers (`$ftp send 50000`) were introduced to match thesis
Table 4.4's DP1 caPL ≈ 0%. Switching from `start` to `send` caused
DSCP 12 to disappear entirely from the traces and DSCP 50
(BG out-of-profile) counts to spike — characteristic symptom of
silent reclassification to DSCP 0.

**ns-3 port:** not reproduced. ns-3 classification uses packet tags
attached by application helpers, which are set at tag-attach time
regardless of whether the application uses a BulkSendApplication
(unlimited) or an OnOffApplication with a finite `MaxBytes` (finite).

**ns-2.35 port:** Fixed in `src/ns-2.35/tcl/ns-source.tcl`.
All four `Application/FTP` instprocs (`start`, `send`, `produce`,
`producemore`) now call `[$self agent] set_apptype $::PT_FTP` before
initiating data transfer. Combined with the D2-7 fix, the `27` literal
is replaced by the `$PT_FTP` symbolic variable throughout.

---

## D2-2 (formerly BUG-8): Incomplete `record_delay` — VoIP OWD/IPDV silently dropped (25-year dormant, **FIXED**)

**File:** `ns2/diffserv4ns/examples/example-3/scenario-3.tcl`,
lines 544–557 (`record_delay` proc) + `voip_connection` helper around
line 270.

**Status:** Fixed 2026-04-17 in commit `6ef9589` ("Fill missing
traces/plots: S3 OWD/IPDV + S1 ns-3 IPDV"). Same pattern as D2-1
(incomplete 2001 scenario code: a hook expects a variable that a
sibling code path never sets).

**What happens:** the `record_delay` proc expects a plain-name global
`Sink_` to be bound to the first VoIP `LossMonitor` sink:

```tcl
proc record_delay {} {
    global Sink_ OWD IPDV
    ...
    if {[info exists Sink_]} {
        set voip_owd  [expr 1000*[$Sink_ set owd_]]
        set voip_ipdv [expr 1000*[$Sink_ set ipdv_]]
        puts $OWD  "$now $voip_owd"
        puts $IPDV "$now $voip_ipdv"
    }
    $ns at [expr $now + 0.5] "record_delay"
}
```

But `voip_connection` — the proc that creates VoIP flows — only ever
assigns the sink to a local `$sink` variable and attaches it to the
destination node. It never binds the bare global `Sink_`. The other
sinks in the scenario live in arrays (`ra_sink($i)`, `sink_($i)`); none
of them bind a bare-name `Sink_` either.

**Consequence:** `[info exists Sink_]` always evaluated false; the
entire VoIP OWD/IPDV emission branch never fired. `OWD.tr` and
`IPDV.tr` ended up **0-bytes on every Scenario 3 run**, across both
ns-2.29 and ns-2.35 port layers, for the entire 2001–2026 period.
The `$ns at [expr $now + 0.5] "record_delay"` self-rearm kept the
proc running (so no crash or observable error), just with no output
on the VoIP sampling path.

**Fix** (commit `6ef9589`, `voip_connection`, +8 lines, 0 lines removed):
bind `global Sink_` to the first created sink. The `record_delay` proc
itself was left untouched; the fix simply provides the variable the proc
was always looking for.

**Impact of the fix:**

- ns-2.29 example-3 fullscale: `OWD.tr` / `IPDV.tr` went from 0 bytes
  to 9,988 rows each.
- ns-2.35 example-3 fullscale: same, both populated.
- Plot inventory gained 2 × 8-file cells under
  `output/comparison/{ns229-vs-ns235,ns235-vs-ns3}/scenario-3/`
  that previously could not be generated.

**Why 25 years dormant:** the scenario ran to completion with no
error; downstream analysis scripts silently handled the empty traces
as "no samples" rather than surfacing them as an anomaly. The pattern
matches D2-1 exactly — an incomplete 2001 patch whose missing coverage
is invisible without instrumentation of the output files' sizes. Both
bugs were caught during Phase 7's cross-simulator plot audit, which
demanded complete trace coverage across 47 plots.

**Twin defect on the ns-3 side** (separate bug, fixed independently):
`src/ns-3/examples/diffserv-example-3.cc` `RecordDelayFull()` (under `--scale=full`)
emitted a cumulative running mean of OWD/IPDV that converged to the
population mean, flattening the per-sample jitter visible in the trace.
Fixed 2026-04-17 in commit `5413404` (`g_latestOwd` / `g_latestIpdv`
introduced; detail in Serena memory `phase7/s3-owd-tracing-fix`). The
two bugs are not the same — D2-2 is about the VoIP sample never being
read in ns-2; the ns-3 twin is about the sample being read but averaged
into flatness — but they surfaced together during the same audit and
were fixed in adjacent commits.

---

## D2-3 (formerly BUG-10): TSW/FW meters silently share the global default RNG stream (2001-era ns-2)

**Category:** 2001-era ns-2 bug. Inherited identically into ns-3.

**Locations:**
- `src/ns-2.29/diffserv/dsPolicy.cc:538` — `TSW2CMPolicy::applyPolicer`
- `src/ns-2.29/diffserv/dsPolicy.cc:590` — `TSW3CMPolicy::applyPolicer`
- `src/ns-2.29/diffserv/dsPolicy.cc:895` — `FWPolicy::applyPolicer`
  (probabilistic penalty mode)

Direct calls to `Random::uniform(0.0, 1.0)` in all three sites. ns-2's
`Random::uniform` draws from the **global default stream**: a single
shared sequence whose position advances as any RNG-consuming object in
the simulation draws from it.

**Symptom:** The probabilistic colour decisions made by TSW2CM /
TSW3CM / FW (random-drop/mark beyond CIR per RFC 2859 §2) are
reproducible within a single build, but the per-meter stream position
drifts whenever an unrelated RNG-consuming object is added upstream
(a flow generator, a different scheduler, etc.). Two simulations with
the same TSW configuration but different unrelated objects produce
different colour vectors, even though RFC 2859 semantics should be
identical.

**Why it's a bug:** Research-grade reproducibility requires stream
isolation per decision-making component, not just per simulation. The
fix landed here doesn't change any *average* RFC-conformance property;
it adds the *capability* to isolate the stream when the scenario author
needs that.

**Fix (opt-in, preserves pre-fix default behaviour):**

*ns-2.35* (`src/ns-2.35/diffserv/dsPolicy.{h,cc}`, `dsEdge.cc`):
- Base class `Policy` gains an optional `RNG *rng_` and helper
  `double Policy::uniform()` that falls through to
  `Random::uniform(0.0, 1.0)` when `rng_` is `NULL` (default).
- `Policy::setRngStream(int stream)` creates a dedicated RNG.
- `edgeQueue` Tcl command exposes `$edge policyRngStream <name> <stream>`.
- **Scenarios that never call `policyRngStream` observe identical
  pre-fix output** (the fallback path preserves `Random::uniform`).

*ns-3* (`src/ns-3/model/fw-meter.{h,cc}`, `diffserv-edge-queue-disc.cc`):
- Added `FWMeter::AssignStreams(int64_t)` (matching the Tsw2cm/Tsw3cm
  idiom).
- `DiffServEdgeQueueDisc::AssignStreams` now cascades into any
  installed `Tsw2cm` / `Tsw3cm` / `Fw` meter slot via DynamicCast
  guards.
- **Scenarios that never call `edge->AssignStreams(N)` observe
  identical pre-fix output.**

**ns-2.29 is not modified.** The frozen original preserves the bug for
historical fidelity, consistent with D2-4 through D2-7.

**Verification:**
- ns-3 unit test `S-13.6 Edge AssignStreams cascades into meter
  slots` asserts that two edges with the same injected seed produce
  identical TSW2CM colour vectors, and that different seeds produce
  different vectors.
- ns-3 unit test `S-13.8 Meter cascade reaches helper-path meters`
  asserts the cascade survives the lazy-create path used by the
  helper API (follow-up fix 2026-04-19).
- S1/S2 CSVs and Q-tier references unchanged (opt-in design;
  scenarios do not invoke the new API).

---

## D2-4 (formerly BUG-4): SFQ DequeEvent reads `FlowQueue.front()` before `empty()` check

**File:** `src/ns-2.29/diffserv/dsscheduler.cc`, lines 485–490

**What happens:**
```cpp
for (int i=0; i<NumQueues; i++) {
    PacketTags=flow[i].FlowQueue.front();   // reads front() BEFORE checking empty()
    if ((!flow[i].FlowQueue.empty())&&(PacketTags.StartTag<MinStartTag)) {
```

The `front()` call on a potentially empty `std::queue` is undefined
behaviour in C++. The `empty()` check on the next line prevents the
*result* from being used, but a debug-mode STL implementation would
abort at the `front()` call itself.

**Probable intent:** The check order should be reversed — `empty()`
first, then `front()`. This appears to be a simple oversight in the
loop structure: the author likely tested with queues that were always
non-empty in the scenarios used, so the UB path was never exercised.

**Why it didn't break simulations:** In practice, when a queue is empty,
the `empty()` check on the next line short-circuits the `&&` and the
bogus `PacketTags` value from `front()` is never compared against
`MinStartTag`. The garbage read is discarded. On all compilers used
with ns-2 (GCC 2.x through 7.x), `std::queue::front()` on an empty
deque returns a reference to uninitialised memory rather than trapping,
so the program continues without visible error.

**How it was found:** Surfaced during Phase 5 porting (2026-04-15) when
the ns-3 implementation refactored the dequeue path. The `empty()`
check was moved before `front()` as a natural part of the ns-3 coding
style, and the original ordering was flagged during code review.

**Latency:** ~25 years (2001–2026).

**ns-3 port** (`ds-sfq-scheduler.cc:101-108`):
```cpp
for (uint32_t i = 0; i < m_numQueues; ++i)
{
    if (!m_flow[i].flowQueue.empty() &&
        m_flow[i].flowQueue.front().startTag < minStartTag)
    {
```

The `empty()` check now precedes `front()`, eliminating the undefined
behaviour.

**ns-2.35 port:** Fixed in `src/ns-2.35/diffserv/dsscheduler.cc`.
The loop in `dsSFQ::DequeEvent()` now checks `!flow[i].FlowQueue.empty()`
before calling `flow[i].FlowQueue.front()`, eliminating the UB.

---

## D2-5 (formerly BUG-1): Dead `set_pkttype(PT_CBR)` in cbr_traffic.cc

**File:** `src/ns-2.29/tools/cbr_traffic.cc`, lines 93–96

**What happens:**
```cpp
agent_->set_pkttype(PT_CBR);
agent_->set_pkttype(PT_UDP);   // immediately overwrites PT_CBR
```

The first `set_pkttype(PT_CBR)` is dead code — it is unconditionally
overwritten by `set_pkttype(PT_UDP)` on the very next line.

**Probable intent:** The second line was likely meant to be
`set_apptype(PT_CBR)` (setting the *application* type for DiffServ
classification), while keeping the *transport* type as `PT_UDP`.
The `set_apptype()` method was added by DiffServ4NS to `agent.h`, so
this looks like a simple method-name error.

**Why it didn't break simulations:** The DiffServ classifier
(`dsPolicy.cc`) reads `app_type_` (set elsewhere via the Agent
constructor or Tcl `set_apptype`), not `ptype_`. So the overwritten
`ptype_` was irrelevant to DiffServ classification.

**ns-3 port:** Application helpers set `DiffServAppTypeTag` correctly.
No transport-type overwrite occurs.

**ns-2.35 port:** Fixed in `src/ns-2.35/tools/cbr_traffic.cc`.
The second line is now `set_apptype(PT_CBR)`, keeping transport type `PT_UDP`
and correctly stamping `app_type_` for DiffServ classification.

---

## D2-6 (formerly BUG-2): Dead `set_pkttype(PT_REALAUDIO)` in realaudio.cc

**File:** `src/ns-2.29/realaudio/realaudio.cc`, lines 129–130

**What happens:**
```cpp
agent_->set_pkttype(PT_REALAUDIO);
agent_->set_pkttype(PT_UDP);   // immediately overwrites PT_REALAUDIO
```

Same pattern as D2-5. `PT_REALAUDIO` is set then immediately
overwritten with `PT_UDP`.

**Probable intent:** Same as D2-5 — the second line should have been
`set_apptype(PT_REALAUDIO)`.

**Why it didn't break:** Same reason — `dsPolicy.cc` uses `app_type_`,
not `ptype_`.

**ns-2.35 port:** Fixed in `src/ns-2.35/realaudio/realaudio.cc`.
The second line is now `set_apptype(PT_REALAUDIO)`. Note: `PT_REALAUDIO=50` in
ns-2.35 (was 49 in ns-2.29), but since this is a symbolic reference it resolves
correctly at compile time.

---

## D2-7 (formerly BUG-3): Magic number 27 in ns-source.tcl

**File:** `src/ns-2.29/tcl/lib/ns-source.tcl`, line 51

**What happens:**
```tcl
[$self agent] set_apptype 27
```

The FTP application sets its `app_type` to the raw integer `27`.

**Correction (2026-04-17):** Earlier versions of this document stated that
27 is the value of `PT_HTTP`. This was **incorrect**. Verification against
`ns2/ns-allinone-2.35/ns-2.35/common/packet.h` confirms:

```
PT_FTP  = 27   (line 115)
PT_HTTP = 31   (line 119)
```

Both ns-2.29 and ns-2.35 assign `PT_FTP=27`. The value 27 is therefore
semantically correct for FTP classification — the original code was
right, just fragile due to using a magic number.

**Problems:**
1. Uses a magic number instead of a symbolic constant — fragile if the
   enum is reordered in a future ns-2 release.

**Why it didn't break:** 27 == `PT_FTP`, so classification was always
semantically correct. The DiffServ policy rules configured to match 27
for FTP worked as intended.

**ns-3 port:** Application types use string or TypeId-based identifiers,
not raw integers. FTP and HTTP have distinct type identifiers.

**ns-2.35 port:** Fixed in `src/ns-2.35/tcl/ns-source.tcl`.
A Tcl variable `set PT_FTP 27 ;# symbolic constant mirroring common/packet.h`
is defined at file scope, and all four `Application/FTP` instprocs use
`$::PT_FTP` instead of the literal `27`. The semantic value is unchanged
(27 == PT_FTP in ns-2.35); the fix is purely for readability and robustness.

---

## D2-8 (formerly BUG-11): dsRED Tcl shim arg-swap — TCPbReTX/TCPnReTX (25-year dormant, **FIXED**)

**Discovered:** 2026-04-26, during the pre-deposit dual-metric audit
prep for the v1.0 tech-report. Surfaced by code-reading the existing
`tcp->reason()`-driven counter at `dsred.cc:256-259` while preparing
to add a parallel `hdr_cmn.tcp_retx_` mechanism (subsequently
identified as redundant — see
`docs/superpowers/plans/2026-04-26-bug11-shim-swap-execution.md`).

**File:** `src/ns-2.35/diffserv/dsred.cc:388-393` (and verbatim in the
frozen `src/ns-2.29/diffserv/dsred.cc`).

**What happens:**

The Tcl-visible `getStat <name> <codepoint>` shim dispatches:

```cpp
if (strcmp(argv[2], "TCPbReTX") == 0)
   return (s_TCPnReTX);     // BUG: returns the COUNT for a BYTES query
if (strcmp(argv[2], "TCPbGoTX") == 0)
   return (s_TCPbGoTX);     // OK
if (strcmp(argv[2], "TCPnReTX") == 0)
   return (s_TCPbReTX);     // BUG: returns BYTES for a COUNT query
```

The local variables loaded at lines 374-376 are correct; only the
dispatch is swapped. `TCPbGoTX` was already correct. Counter writes
themselves at `dsred.cc:256-259` are correct; only the Tcl read
path corrupts the data.

**Active consumers (pre-fix):** `example-2.tcl`,
`scenario-2-ns235.tcl`, `scenario-2-ns235-srtcm.tcl` — all compute
goodput as `origBytes / (origBytes + retxBytes)` after multiplying
both KB totals by 1024, so the swap biases published `retxBytes` by
`(MSS - 1024) / 1024` per packet (small for ~1024-byte packets,
larger for 1500-byte ~+46% or 512-byte ~-50% MSS).

**Numerical impact:** Scenario 2 fullscale set 1 post-fix, DSCP 10
shifts retxBytes by -1.6%, DSCP 14 by +8.0%; per-DSCP goodput
shifts < 0.002 absolute, so the existing tech-report Fig 4
calibration narrative remains qualitatively valid.

**Fix:** swap return values + add `origBytes`/`retxBytes` byte-unit
aliases (~14 LOC including comments) for cross-implementation
naming/units parity with ns-3's `DiffServStatistics::RecordOrigBytes`
/ `RecordRetxBytes`. ns-2.29 frozen original is **not** modified
(2001 release-faithful reference per project policy).

**Verification:** new Tcl regression scenario at
`src/ns-2.35/test/d2-8-regression.tcl` drives a small TCP/Reno
flow over a tight `dsRED/edge` bottleneck, queries all five `getStat`
dispatches, and asserts arithmetic identities that the pre-fix swap
would violate. PASS 2026-04-26 against the patched ns-2.35 build.
Runner script at `scripts/d2-8-regression.sh` invokes via the
same Ubuntu 18.04 + GCC 7 Docker workflow.

**Categorisation:** Tcl-shim correctness defect. Underlying C++
counters were always correct; only the Tcl read path was corrupting
data. Scenarios that never invoked `getStat TCPbReTX` / `TCPnReTX`
are unaffected.

**The 25-year-dormant siblings:** D2-8 is the fourth 2001-era defect
that has been silently corrupting or dropping output across the
canonical ns-2 example set:

| New ID | Old ID | Year added | Year surfaced | What was wrong |
|---|---|---|---|---|
| D2-1 | BUG-5 | 2001 | 2026-04-18 | FTP `set_apptype` missing on 3 of 4 instprocs |
| D2-2 | BUG-8 | 2001 | 2026-04-25 | VoIP `record_delay` ran on one trace path only |
| N2-2 | BUG-9 | 2001 (xuanc-2011 destructor revision) | 2026-04-19 | WebTraf `recycle_page_=1` shared-RV double-free |
| D2-8 | BUG-11 | 2001 | 2026-04-26 | Tcl shim arg-swap on 2 of 3 retx-stat queries |

Each was dormant because the affected path was not in the canonical
example set; modernization-era CI surfaced each one only when a
different scenario or audit pass exercised it.

---

## N3-1 (formerly BUG-6): ns-3 TcpSocketBase::PersistTimeout null-pointer dereference

**File:** `ns3-dev/src/internet/model/tcp-socket-base.cc`, line 4133
(on the pinned revision `cc48bf5c15a4918364abc2b2b060b4056dce09a4`,
2026-04-10)

**Scope:** This bug is in **ns-3 mainline**, not in DiffServ4NS or in
our port. The Scenario 2 full-scale reconstruction surfaced it by
operating at a higher offered load than any shipped ns-3 example.

**What happens:**
```cpp
Ptr<Packet> p = m_txBuffer->CopyFromSequence(1, m_tcb->m_nextTxSequence)
                  ->GetPacketCopy();
```

`TcpTxBuffer::CopyFromSequence()` returns `nullptr` when
`SizeFromSequence(seq) == 0`, i.e. when the tx buffer holds no data at
or past `m_nextTxSequence`. `PersistTimeout()` dereferences the return
unconditionally, so the simulator SIGSEGVs at
`ns3::Ptr<ns3::Packet>::operator->()` with `this=0x8` (the offset of
`m_packet` inside a zero-address `TcpTxItem`).

**Trigger pattern.** The persist timer is armed whenever an ACK arrives
with `rWnd=0` and the event is not already pending. It is cancelled
only when the window reopens or via `CancelAllTimers()` during
teardown. The crash path is:

1. Sender has queued data. Receiver's rWnd drops to zero.
2. Persist timer probes 1 byte at a time; receiver ACKs probes but does
   not open the window. `m_firstByteSeq` advances.
3. Eventually `m_nextTxSequence == TailSequence()` — the buffer holds
   no more unsent data, the app has not supplied more.
4. The next persist firing calls `CopyFromSequence(1, seq)`, which
   returns `nullptr`. Crash.

**Fix.** Null-guard the `CopyFromSequence()` return and fall back to a
zero-length probe segment. RFC 1122 section 4.2.2.17 permits a
zero-window probe of either one octet of old data or no data. A
zero-length segment elicits an ACK from the peer carrying the current
receive window, which is what persist-probing requires. Patch is eight
lines in `tcp-socket-base.cc` plus a regression test
(`TcpPersistEmptyTxBufferTest` in `tcp-zero-window-test.cc`).

**Upstream status.** Applied locally (ADR-0022) to unblock the
thesis-exact Scenario 2 sweep. **Issue
[#1326](https://gitlab.com/nsnam/ns-3-dev/-/work_items/1326) and merge
request
[!2829](https://gitlab.com/nsnam/ns-3-dev/-/merge_requests/2829) filed
upstream 2026-04-18; awaiting maintainer review.** Full submission
artifacts at `docs/upstream/ns3-tcp-persist-empty-buffer.md`.

**Methodology evidence:** High-fidelity reconstruction functions as a
verification pass on the *simulator framework*, not only on the new
model being built. Together with D2-1 (the dormant 2001 DiffServ4NS
`Application/FTP::send` Tcl patch), this is the second latent bug
the Scenario 2 full-scale reconstruction has surfaced — one in the
2001 DS4 layer and one in the 2026 ns-3 mainline layer.

---

## D3-1 (DiffServ4NS-for-ns-3): `DsWfqScheduler` perceived-ratio degrades monotonically with weight asymmetry (Q-16 replication, **FIXED 2026-05-03**)

**Identified:** 2026-05-03, during Q-16 chang2015 GPS-convergence
replication (specs/03-quality.md Q-16).

**Symptom:** At T = 10 Mbps bottleneck = 5 Mbps (Chang's §V scenario)
the perceived throughput ratio R₀/R₁ tracks the configured weight
ratio at w₁/w₂ = 1 (0.11 % error) but degrades monotonically as the
weight asymmetry grows:

| w₁/w₂ | perceived | error |
|---|---|---|
| 1  | 1.00 | 0.11 % |
| 2  | 1.63 | 18.66 % |
| 7  | 4.19 | 40.18 % |
| 10 | 2.95 | 70.53 % |

The other four fair-queueing schedulers (WRR, WF2Q+, SCFQ, SFQ) at
the same scenario stay within single-digit % error across all four
weight ratios, ruling out topology / TCP / classification as the
cause. Localised to `DsWfqScheduler::SetParam` or its virtual-time
update path.

**Status:** Reproducible across runs and across transport (TCP +
UDP CBR). Scope: structural defect in the WFQ algorithm shared
verbatim with the original 2001 ns-2 implementation — this is a
**25-year-dormant D2-class bug inherited into the ns-3 port**, not
a porting defect. The original 2001 use cases never exercised the
saturating-asymmetric regime against an independent reference.

**Root cause (2026-05-03 trace).** Under saturating asymmetric load
the GPS-event handler `HandleGpsDeparture` reaches `m_sum == 0` on
essentially every packet (every ~1.6 ms in the Chang scenario),
because the high-weight queue's `B` count drains to 0 between
enqueues while the low-weight queue is GPS-idle (`B==0`,
`pgps.size()==50`). `Reset()` then zeroes per-queue `finishT` while
`m_sessionDelay = now` shifts the PGPS time-base. Subsequent
enqueues compute `finishT = max(0, vTime=0) + increment` —
correctly weighted *internally* — but the still-pending PGPS
entries sit in `std::queue<double>` (FIFO, not priority), so
`SelectNextQueue` compares OLDEST-pushed values, not minimum. After
a few thousand resets the dequeue ratio converges to ~1:1
regardless of the configured weight ratio.

**Fix path (NOT applied).** Two minimal experiments in the
2026-05-03 session demonstrated that simple changes break the
algorithm: (a) swapping `pgps` to `std::priority_queue<double, …,
greater<>>` keeps tag ordering correct but does not address the
Reset cascade (still 50–90 % error); (b) gating Reset on
`pgps.empty()` for all queues prevents the cascade but starves the
low-weight flow entirely (100 % error — `finishT` grows
unboundedly and tags low-weight packets out of contention). The
correct fix likely requires restructuring the GPS-event /
PGPS-tracking decoupling to use a single virtual-time discipline
similar to SCFQ — which passes the same Q-16 stress at ≤ 1 %
error. Treat as a multi-day algorithmic redesign, not a one-line
patch. The `pgps` container, the Reset trigger, and the
finish-time-rebase strategy must be designed together.

**Workaround (pre-fix).** Use SCFQ, WF2Q+, or SFQ for asymmetric
weighting — all three pass Q-16 cleanly (≤ 1 % error at ratio = 10).

**Resolution (2026-05-03).** `DsWfqScheduler` rewritten to **true
Parekh-Gallager 1993 PGPS**: virtual time `V(t)` is a piecewise-
linear function over busy-set epochs (Eq. 10), recomputed on demand
from a snapshot triple `(t_epoch, V_epoch, sumPhiBusy)`. The
snapshot is refreshed at every busy-set transition; one subtraction
and one division per query, no accumulation drift. Per-flow finish
tags computed once at enqueue per Eq. 3 — `F_i^k =
max(F_i^{k-1}, V(t_arrival)) + L_i^k / (φ_i · r)` — and never reset;
the per-flow tag sequence is monotone non-decreasing by construction.
Removed: GPS-event simulation, `m_sum`, `m_idle`, `m_sessionDelay`,
the `Reset()` trigger on `m_sum == 0`, the dual GPS/PGPS finish-tag
queues. See ADR-0049 for the full decision.

**Verification.** Q-17.1 (Parekh-Gallager Theorem 1 conformance):
WFQ symmetric `max(F̂ − F) = 1.456 ms` ≪ strict bound `L_max/r =
8.224 ms`, **0/581 strict violations**. Q-16.2 Chang convergence at
saturating-asymmetric weight ratios PASS for the 4 gated schedulers
(WF2Q+, SCFQ, SFQ, WRR); WFQ excluded from the in-process gate
because TCP+RED+ratio=2 surfaces inherent GPS forfeit-share dynamics
that the runner-sweep at 300 s window resolves correctly.

**Caveat (cross-sim audit 2026-05-03,
`output/comparison/ns235-vs-ns3/scenario-1/STAMP`).** The fix
exposes that the 2001 example-1 scenario was implicitly load-bearing
on D3-1 for EF QoS: when EF traffic is CIR-shaped at exactly its
weighted share (300 kbps = 3/20 × 2 Mbps), pure Parekh-Gallager WFQ
plateaus the EF queue at ~29 packets and EF OWD at ~412 ms, vs ~20 ms
under the D3-1-defective ns-2.35 WFQ. Throughput shares remain
correct in both. The recommendation is to use **WF2Q+ (eligibility
predicate, OWD ~37 ms)** or **PQ (strict priority, OWD ~14 ms)** for
EF QoS in DiffServ; both are available in the module and are the
standard literature answer for "rate = share" provisioning. WFQ in
example-1 becomes a documented characterisation rather than a QoS
demonstration.

**Companion observation (under investigation):** `DsWeightedRoundRobinScheduler`
shows a separate, possibly distinct, anomaly at T = 50 Mbps where
perceived-ratio error jumps from single-digit % at T = 10 to 17–77 %
across all weight ratios. Both anomalies surfaced from the same
chang2015 sweep and will be tracked in this entry until root-cause
analysis distinguishes them.

---

## D3-2 onward: no entries

The remainder of the ns-3 port surfaces no further catalogued bugs
for v1. The Evaluation-Driven Development (EDD) workflow that gates
each class addition (spec-tier I/S/Q assertions written before code;
comparison against ns-2 outputs and RFC vectors before declaring
done) caught porting issues at write time, so they never propagated
into the ns-3 source as durable bugs. The methodology narrative is
in tech report §3.6.

The mostly-empty bucket is a deliberately curated absence rather
than an artefact of incomplete review.

---

## FINDING-1: FairWeightedMeter is a Nortel original, not thesis-originated

**Identified:** 2026-04-15, during Phase 6 preparation
**Method:** Systematic comparison of Nortel ns-2.29 `dsPolicy.{h,cc}`
against DiffServ4NS `dsPolicy.{h,cc}`, triggered by spec-driven
porting (the I-2.6 spec references FW, but the thesis does not).

**Finding:** The `FWPolicy` class in DiffServ4NS is a renamed copy of
`SFDPolicy` from the pristine Nortel ns-2 DiffServ module (2000).
The algorithm is byte-for-byte identical. Changes limited to:

1. Class renamed: `SFDPolicy` → `FWPolicy`, `SFD` → `FW` (slot 6)
2. `flow_entry.src_id` and `flow_entry.dst_id` fields removed
3. Unused local variables cleaned up in `applyPolicer()`

The thesis (Chapter 3.3.2) explicitly enumerates the module's meters
as "TSW2CM, TSW3CM, Token Bucket, srTCM and trTCM" — **FW is absent**.
Neither UML class diagram (Figures 3.8 and 3.11) shows FWPolicy. No
thesis experiment uses FW.

Two other Nortel policies — `EWPolicy` (`#define EW 7`) and
`DEWPPolicy` (`#define DEWP 8`) — were dropped entirely by
DiffServ4NS. None of these changes were documented until 2026.

**Methodology evidence:** Spec-driven LLM-augmented porting surfaces
undocumented inheritance chains in legacy code. The rename from SFD
to FW hides the structural similarity — a developer reading only
DiffServ4NS would see "FWPolicy" and assume it was part of the
thesis contribution. Systematic comparison against the upstream
Nortel code revealed the true provenance.

**Full analysis:** `provenance/FW_THESIS_REFERENCES.md`
**Rename documentation:** `docs/NS2_PATCHES.md` §"Renames and removals"

---

## Discovery notes

- D2-5, D2-6, D2-7 (formerly BUG-1, BUG-2, BUG-3) were identified during
  the NS2_PATCHES analysis (2026-04-14) by diffing
  `ns2/ns-2.29-pristine/` against `src/ns-2.29/`. See
  `docs/NS2_PATCHES.md` for the complete patch catalogue.

- D2-4 (formerly BUG-4) was identified during Phase 5 porting
  (2026-04-15) when the ns-3 implementation of `DsSfqScheduler`
  refactored the dequeue loop and the unsafe access order was noticed
  during code review. This demonstrates a secondary benefit of porting:
  the act of translating code to a new platform surfaces latent bugs
  that survived decades of use in the original.

- D2-1 (formerly BUG-5) was identified during Phase 7 Scenario 2
  reconstruction (2026-04-16) when the 2001 `Application/FTP` Tcl patch
  was audited as part of the app-type classification analysis.

- N3-1 (formerly BUG-6) was identified during Phase 7 Scenario 2
  reconstruction (2026-04-17). Unlike the D2 bucket, N3-1 is a defect
  in the modern simulator framework (ns-3 mainline), not in the 2001
  ns-2 module — reconstruction exposed it by operating at a higher
  offered load than any shipped ns-3 example.

- D2-2 (formerly BUG-8) was identified 2026-04-17 during the same
  Phase 7 cross-simulator plot audit that surfaced D2-1; the two are
  structural twins.

- N2-1 (formerly BUG-7) was identified during ns-2.35 Scenario 2
  port profiling 2026-04-17.

- N2-2 (formerly BUG-9) was identified 2026-04-18 during the Phase 7
  Scenario 2 per-flow srTCM sweep; root cause pinned 2026-04-19.

- D2-3 (formerly BUG-10) was identified 2026-04-19 during the post-PR3c
  RNG-isolation pass.

- FINDING-1 was identified during Phase 6 preparation (2026-04-15) by
  searching the thesis text for FW references and comparing the
  DiffServ4NS source against the Nortel original. Unlike the BUG-N /
  N2/D2/N3 entries (which are code defects), FINDING-1 is a provenance
  discovery: undocumented code heritage surfaced by systematic analysis.

- FINDING-2 (2026-04-17, Phase 7 ns-2.35 port) confirms that
  `PagePool/WebTraf` works correctly on ns-2.35+DS4 — the OFFSET-macro
  crash present on ns-2.29+DS4 does not occur. See
  `docs/WEBTRAF_NS235_FINDING.md` for the full analysis. Key companion
  finding: native WebTraf requires a one-instproc `alloc-tcp` override to
  stamp `set_apptype 31` (PT_HTTP) so DiffServ mark rules for `"any http"`
  fire correctly; without this, all WebTraf TCP agents carry app_type=0
  and fall through to the Best Effort queue.
