# HISTORICAL_BUGS — Bugs surfaced by the DiffServ4NS port

This file documents bugs surfaced while porting DiffServ4NS from ns-2.29
to ns-3. Two categories are recorded:

- **2001-era ns-2 bugs** (BUG-1 through BUG-5) — defects in the original
  ns-2 DiffServ4NS module that shipped in 2001 and were never fixed. The
  ns-3 port deliberately does not reproduce them.
- **2026-era ns-3 upstream bugs** (BUG-6 onward) — defects in ns-3
  mainline that the reconstruction exposed at high offered load.
  Reported upstream with a proposed patch (see `docs/upstream/`).

This record serves two purposes:
1. **Methodology:** demonstrates that the port is a *reasoned* translation,
   not a blind line-by-line copy.
2. **ICNS3 2026 paper:** provides concrete examples of how high-fidelity
   reconstruction surfaces latent bugs in *both* the legacy code being
   ported and the modern simulator framework it is being ported to.

---

## Bug fixes applied in the ns-2.35 port

The ns-2.35 port is framed as "improved DS4" — a bug-fixed reference
implementation against which the ns-3 port is compared, rather than a
faithful reproduction of the 2001 defects. All fixes live in layer 2
under `src/ns-2.35/`, which replaces selected DS4 base
files when patching the ns-2.35 tree.

| Bug | File | Rationale |
|-----|------|-----------|
| BUG-1 | `src-ns235/tools/cbr_traffic.cc` | `set_apptype(PT_CBR)` so CBR traffic is classified correctly by DiffServ policy |
| BUG-2 | `src-ns235/realaudio/realaudio.cc` | `set_apptype(PT_REALAUDIO)` so RealAudio traffic is classified correctly |
| BUG-3 | `src-ns235/tcl/ns-source.tcl` | Replace magic number 27 with `$PT_FTP` variable; correct misstatement in docs (27==PT_FTP, not PT_HTTP) |
| BUG-4 | `src-ns235/diffserv/dsscheduler.cc` | `empty()` check moved before `front()` in SFQ dequeue loop to eliminate C++ UB |
| BUG-5 | `src-ns235/tcl/ns-source.tcl` | `set_apptype` added to all four `Application/FTP` instprocs |
| UDP hdr | `src-ns235/apps/udp.cc` | `sendmsg()` adds 28 bytes (IP 20 + UDP 8) so `hdr_cmn::size()` matches real-world serialisation and aligns with ns-3 |

---

## BUG-1: Dead `set_pkttype(PT_CBR)` in cbr_traffic.cc

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
constructor or Tcl `set_apptype`), not `ptype_`.  So the overwritten
`ptype_` was irrelevant to DiffServ classification.

**ns-3 port:** Application helpers set `DiffServAppTypeTag` correctly.
No transport-type overwrite occurs.

**ns-2.35 port:** Fixed in `src/ns-2.35/tools/cbr_traffic.cc`.
The second line is now `set_apptype(PT_CBR)`, keeping transport type `PT_UDP`
and correctly stamping `app_type_` for DiffServ classification.

---

## BUG-2: Dead `set_pkttype(PT_REALAUDIO)` in realaudio.cc

**File:** `src/ns-2.29/realaudio/realaudio.cc`, lines 129–130

**What happens:**
```cpp
agent_->set_pkttype(PT_REALAUDIO);
agent_->set_pkttype(PT_UDP);   // immediately overwrites PT_REALAUDIO
```

Same pattern as BUG-1.  `PT_REALAUDIO` is set then immediately
overwritten with `PT_UDP`.

**Probable intent:** Same as BUG-1 — the second line should have been
`set_apptype(PT_REALAUDIO)`.

**Why it didn't break:** Same reason — `dsPolicy.cc` uses `app_type_`,
not `ptype_`.

**ns-2.35 port:** Fixed in `src/ns-2.35/realaudio/realaudio.cc`.
The second line is now `set_apptype(PT_REALAUDIO)`. Note: `PT_REALAUDIO=50` in
ns-2.35 (was 49 in ns-2.29), but since this is a symbolic reference it resolves
correctly at compile time.

---

## BUG-3: Magic number 27 in ns-source.tcl

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
not raw integers.  FTP and HTTP have distinct type identifiers.

**ns-2.35 port:** Fixed in `src/ns-2.35/tcl/ns-source.tcl`.
A Tcl variable `set PT_FTP 27 ;# symbolic constant mirroring common/packet.h`
is defined at file scope, and all four `Application/FTP` instprocs use
`$::PT_FTP` instead of the literal `27`. The semantic value is unchanged
(27 == PT_FTP in ns-2.35); the fix is purely for readability and robustness.

---

## BUG-4: SFQ DequeEvent reads FlowQueue.front() before empty() check

**File:** `src/ns-2.29/diffserv/dsscheduler.cc`, lines 485–490

**What happens:**
```cpp
for (int i=0; i<NumQueues; i++) {
    PacketTags=flow[i].FlowQueue.front();   // reads front() BEFORE checking empty()
    if ((!flow[i].FlowQueue.empty())&&(PacketTags.StartTag<MinStartTag)) {
```

The `front()` call on a potentially empty `std::queue` is undefined
behaviour in C++.  The `empty()` check on the next line prevents the
*result* from being used, but a debug-mode STL implementation would
abort at the `front()` call itself.

**Probable intent:** The check order should be reversed — `empty()`
first, then `front()`.  This appears to be a simple oversight in the
loop structure: the author likely tested with queues that were always
non-empty in the scenarios used, so the UB path was never exercised.

**Why it didn't break simulations:** In practice, when a queue is empty,
the `empty()` check on the next line short-circuits the `&&` and the
bogus `PacketTags` value from `front()` is never compared against
`MinStartTag`.  The garbage read is discarded.  On all compilers used
with ns-2 (GCC 2.x through 7.x), `std::queue::front()` on an empty
deque returns a reference to uninitialised memory rather than trapping,
so the program continues without visible error.

**How it was found:** Surfaced during Phase 5 porting (2026-04-15) when
the ns-3 implementation refactored the dequeue path.  The `empty()`
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

**Note:** This bug is in the DiffServ4NS modifications to ns-2
(consistent with BUG-1 through BUG-3), not in pristine ns-2 code.
The `dsSFQ` class was written entirely by DiffServ4NS.

**ns-2.35 port:** Fixed in `src/ns-2.35/diffserv/dsscheduler.cc`.
The loop in `dsSFQ::DequeEvent()` now checks `!flow[i].FlowQueue.empty()` before
calling `flow[i].FlowQueue.front()`, eliminating the undefined behaviour.

---

## BUG-5: Incomplete `set_apptype` patch in Tcl `Application/FTP`

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

**Exact diff** against pristine ns-2.29:

```
50a51
>         [$self agent] set_apptype 27
```

One line added to `start`; nothing added to `send`, `produce`, or
`producemore`.

**Probable intent:** the patch was added in 2001 to make FTP traffic
classify as AF11 in the DiffServ module.  The released examples
(`example-1.tcl`, `example-2.tcl`, `example-3.tcl`) all use the
`$ftp start` / `$ftp stop` pattern exclusively — they never call
`send N` / `produce N`.  The patch covered the only path the
examples exercised.  The sibling methods were simply missed.

**Why it didn't break simulations:** the DiffServ4NS examples never
hit the unpatched paths.  The bug is latent — it only fires when a
downstream user models FTP as finite-size file transfers rather than
unlimited bulk streams.

**Latency:** ~25 years (2001–2026).  First surfaced during the 2026
Scenario 2 full-scale reconstruction, where finite 50 KB FTP
transfers (`$ftp send 50000`) were introduced to match thesis
Table 4.4's DP1 caPL ≈ 0%.  Switching from `start` to `send` caused
DSCP 12 to disappear entirely from the traces and DSCP 50
(BG out-of-profile) counts to spike — characteristic symptom of
silent reclassification to DSCP 0.

**ns-3 port:** not reproduced. ns-3 classification uses packet tags
attached by application helpers, which are set at tag-attach time
regardless of whether the application uses a BulkSendApplication
(unlimited) or an OnOffApplication with a finite `MaxBytes` (finite).

**Workaround applied in the 2026 Scenario 2 reconstruction:**
stamp `app_type_` on the TCP agent at creation time rather than in
the Application layer, so both `start` and `send` paths classify
correctly.  See `ns2/diffserv4ns/examples/example-2-fullscale/utils.tcl`
`ftp_connection` proc.

**How it was found:** a code-path that ran cleanly for 25 years
across every released example abruptly broke when the reconstruction
exercised it — an example of reconstruction-as-verification
surfacing a latent bug that the original author's own validation
suite could not have found.  This is the most striking of the
dormant-bug findings because the original author (Sergio) is still
active on this codebase and did not recall the gap.

**ns-2.35 port:** Fixed in `src/ns-2.35/tcl/ns-source.tcl`.
All four `Application/FTP` instprocs (`start`, `send`, `produce`,
`producemore`) now call `[$self agent] set_apptype $::PT_FTP` before
initiating data transfer. Combined with the BUG-3 fix, the `27` literal
is replaced by the `$PT_FTP` symbolic variable throughout.

---

## Impact on 2001 example scenarios

This section documents how the bugs above and the ns-2-specific
`app_type_` classification mechanism affect the two shipped example
scenarios.  These findings inform the Phase 4 validation strategy for
the ns-3 port.

### Classification mechanisms used by each example

**Example-1 (`examples/example-1/simulation-1.tcl`):**
- Mark rules use **address-based classification only** with wildcard
  app/packet types (`any any`):
  ```tcl
  $qE1C addMarkRule 46 -1 [$dest(0) id] any any   ;# EF by dest address
  $qE1C addMarkRule  0 -1 [$dest(1) id] any any   ;# BE by dest address
  ```
- All traffic is CBR/UDP.  No application-type matching is needed.
- **BUG-1 through BUG-3 have no effect** on this scenario — the `app_type_`
  field is never consulted.

**Example-2 (`examples/example-2/example-2.tcl`):**
- Mark rules use **application-type string matching** for the Gold
  service class:
  ```tcl
  $qE1C addMarkRule 10 -1 -1 any telnet  ;# Gold: telnet → DSCP 10
  $qE1C addMarkRule 12 -1 -1 any ftp     ;# Gold: FTP    → DSCP 12
  ```
- The Premium and Best Effort rules use address or wildcard matching.
- **BUG-1 is irrelevant** (CBR traffic is only used for Premium/BE, which
  match by address, not app type).
- **BUG-3 is irrelevant** — example-2 uses the string `"ftp"` (mapped to
  `PT_FTP` by `addMarkRule`), not the numeric magic number.  The
  FTP-as-HTTP conflation in `ns-source.tcl` only matters if mark rules
  check for `"http"`, which they don't.
- **The telnet/ftp classification does work correctly** in ns-2 because
  `Application/Telnet` and `Application/FTP` set `app_type_` through
  the standard ns-2 agent mechanism (augmented by DiffServ4NS's
  `set_apptype()` patch to `agent.h`).

### The `addMarkRule` application-type vocabulary

The C++ implementation in `dsEdge.cc:143-189` supports seven string
keywords, mapped to ns-2 `packet_t` enum values:

| Tcl keyword    | C++ constant   | Used in examples? |
|----------------|----------------|-------------------|
| `"telnet"`     | `PT_TELNET`    | example-2         |
| `"ftp"`        | `PT_FTP`       | example-2         |
| `"http"`       | `PT_HTTP`      | no                |
| `"audio"`      | `PT_AUDIO`     | no                |
| `"realaudio"`  | `PT_REALAUDIO` | no                |
| `"cbr"`        | `PT_CBR`       | no                |
| `"any"`        | `PT_NTYPE`     | example-1 & 2     |

At classification time (`dsEdge::mark()`, line 221), the rule's
`appType` is compared against `cmn->app_type()`.  If the rule stores
`PT_NTYPE` (from `"any"`), it matches unconditionally.

### Phase 4 porting implications

1. **Example-1 can be ported without app-type support.**  All
   classification is address-based.  This makes it the natural first
   target for Phase 4.

2. **Example-2 requires an ns-3 equivalent of `app_type_`.**  ns-3
   packets do not carry an application-type field.  The ns-3 port must
   provide an alternative mechanism — most likely a packet tag
   (e.g., `DiffServAppTypeTag`) set by the traffic-generation helper
   and matched by the edge classifier.  This is an additional
   implementation task beyond the scenario translation itself.

3. **The `"cbr"`, `"http"`, `"audio"`, and `"realaudio"` keywords are
   dead vocabulary** — no shipped example uses them.  The ns-3 port
   should support an extensible app-type tag rather than hard-coding
   the same six ns-2-era string values.

---

## BUG-7: Tcl 8.5 `catch {expr X/0}` performance regression (~50 000× slowdown)

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

Not yet verified against the Tcl-core mailing list / bug tracker; the repro
is tight and easy to report upstream if useful.

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

## BUG-8: Incomplete `record_delay` — VoIP OWD/IPDV silently dropped (25-year dormant, **FIXED**)

**File:** `ns2/diffserv4ns/examples/example-3/scenario-3.tcl`,
lines 544–557 (`record_delay` proc) + `voip_connection` helper around
line 270.

**Status:** Fixed 2026-04-17 in commit `6ef9589` ("Fill missing
traces/plots: S3 OWD/IPDV + S1 ns-3 IPDV"). Same pattern as BUG-5
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
destination node. It never binds the bare global `Sink_`:

```tcl
proc voip_connection {id src dst start stop} {
    ...
    set sink [new Agent/LossMonitor]
    $ns attach-agent $dst $sink
    $ns connect $udp $sink
    # (pre-fix: no global Sink_ assignment here)
    ...
}
```

The other sinks in the scenario live in arrays (`ra_sink($i)`,
`sink_($i)`); none of them bind a bare-name `Sink_` either.

**Consequence:** `[info exists Sink_]` always evaluated false; the
entire VoIP OWD/IPDV emission branch never fired. `OWD.tr` and
`IPDV.tr` ended up **0-bytes on every Scenario 3 run**, across both
ns-2.29 and ns-2.35 port layers, for the entire 2001–2026 period.
The `$ns at [expr $now + 0.5] "record_delay"` self-rearm kept the
proc running (so no crash or observable error), just with no output
on the VoIP sampling path.

**Exact diff** (commit `6ef9589`, `voip_connection`, +8 lines,
0 lines removed):

```
@@ -271,6 +271,14 @@ proc voip_connection {id src dst start stop} {
     $ns attach-agent $dst $sink
     $ns connect $udp $sink

+    # Expose the first VoIP sink globally so record_delay can read
+    # its owd_/ipdv_ attributes (previously Sink_ was never set,
+    # so OWD.tr and IPDV.tr ended up empty).
+    global Sink_
+    if {![info exists Sink_]} {
+        set Sink_ $sink
+    }
+
     $ns at $start "$voip start"
     $ns at $stop  "$voip stop"
 }
```

The `record_delay` proc itself was left untouched; the fix simply
provides the variable the proc was always looking for.

**Impact of the fix (from commit message):**

- ns-2.29 example-3 fullscale: `OWD.tr` / `IPDV.tr` went from 0 bytes
  to 9,988 rows each.
- ns-2.35 example-3 fullscale: same, both populated.
- Plot inventory gained 2 × 8-file cells under
  `output/comparison/{ns229-vs-ns235,ns235-vs-ns3}/scenario-3/`
  that previously could not be generated.

**Why 25 years dormant:** the scenario ran to completion with no
error; downstream analysis scripts silently handled the empty traces
as "no samples" rather than surfacing them as an anomaly. The pattern
matches BUG-5 exactly — an incomplete 2001 patch whose missing coverage
is invisible without instrumentation of the output files' sizes. Both
bugs were caught during Phase 7's cross-simulator plot audit, which
demanded complete trace coverage across 47 plots.

**Twin defect on the ns-3 side** (separate bug, fixed
independently): `src/ns-3/examples/diffserv-example-3-fullscale.cc`
`RecordDelay()` emitted a cumulative running mean of OWD/IPDV that
converged to the population mean, flattening the per-sample jitter
visible in the trace. Fixed 2026-04-17 in commit `5413404`
(`g_latestOwd` / `g_latestIpdv` introduced; detail in Serena
memory `phase7/s3-owd-tracing-fix`). The two bugs are not the same
— BUG-8 is about the VoIP sample never being read in ns-2; the
ns-3 twin is about the sample being read but averaged into flatness
— but they surfaced together during the same audit and were fixed
in adjacent commits.

---

## BUG-9: WebTraf session-completion SEGV — shared RandomVariable delete (upstream ns-2, **FIXED**)

**Identified:** 2026-04-18, during Phase 7 Scenario 2 per-flow srTCM sweep.
**Status:** Root cause pinned 2026-04-19 (Phase A instrumentation). Fix
lives in `src/ns-2.35/webcache/webtraf.cc` `WebTrafSession::~WebTrafSession`.
Workaround (`numPages=1000`, commit `b1417a4`) reverted to the thesis-faithful
`numPages=250` in the same commit that landed the fix.
**Scope:** Upstream ns-2 bug in `webcache/webtraf.cc`, present in every ns-2
release from 2011 (when xuanc added the `recycle_page_` cleanup path) through
2.35. Not a DiffServ4NS bug, but DS4 hit it because thesis-faithful Scenario 2
parameters land session completion well inside `simTime`.
**Upstream target:** candidate MR for `sourceforge.net/projects/nsnam/`
— flip `tcl/webcache/webtraf.tcl` default `recycle_page_ 1` → `0`, or remove
the destructor delete block entirely. Not filed yet; DS4 fix is self-contained.

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
`create-session`:

```tcl
set interPageRV [new RandomVariable/Exponential]
$interPageRV set avg_ 15
# ...
for {set i 0} {$i < 400} {incr i} {
    $pool create-session $i 250 $startTime \
        $interPageRV $pageSizeRV $interObjRV $objSizeRV
}
```

`WebTrafPool::lookup_rv` stores a raw `RandomVariable*` in each session's
`rvInterPage_/rvPageSize_/rvInterObj_/rvObjSize_` member. All 400 sessions
alias the same 4 `RandomVariable*` pointers.

When the first session to complete its last page enters its destructor,
the `Tcl delete` calls **delete the shared RV objects**. Every other
live session now holds a raw pointer to freed Tcl memory. The next
`sched(rvInterPage_->value())` in any session's `WebTrafSession::donePage`
(inter-page schedule) or `expire` (page-size draw) dereferences the freed
RV → SEGV.

### Evidence chain (Phase A, 2026-04-19)

Reproducer: `smoke-bug9-phaseA.tcl` — 20 sessions × 3 pages, simTime=200 s.
First session to die at t ≈ 10.58 s (Session 2). Instrumented trace at
`output/ns2-35/bug9-phaseA/stderr.log`:

```
[BUG9A] t=10.579691 Session(2)::donePage LAST-PAGE -> doneSession
[BUG9A] t=10.579691 doneSession(2) ENTER sess_ptr=0xaaab2244e720 status=0
[BUG9A] t=10.579691 ~Session(2) donePage_=3 curPage_=3 status=0
[BUG9A] t=10.579691 doneSession(2) RETURN              <-- Session 2 cleanly gone
                                                           (and — critically —
                                                            rvInterPage_ shared
                                                            RV just got Tcl-deleted)
[BUG9A] t=11.210913 Session(7)::donePage pg=... pid=29 donePage_=1 nPage_=3
[BUG9A] t=11.210913 Pool::donePage(29) erased=1 ...
[BUG9A] t=11.210913 Session(7)::donePage BEFORE delete pg=...
[BUG9A] t=11.210913 Session(7)::donePage AFTER delete pg=...
                                                       <-- NEXT line would be
                                                           sched(rvInterPage_->value())
<ns process returns rc=139 — no further trace line>
```

Crash is in Session 7's `sched(rvInterPage_->value())` inside `donePage`'s
inter-page-option branch. The `rvInterPage_` pointer has been freed by
Session 2's destructor 0.63 s earlier.

Hypothesis test: rerun the same reproducer with one extra Tcl line
`PagePool/WebTraf set recycle_page_ 0` — sim completes rc=0, all 20
sessions die cleanly over t ≈ 10-101 s. **Hypothesis confirmed.** See
`output/ns2-35/bug9-phaseA/stderr-recycle0.log`.

Also explains the v1 post-mortem bisection (commits `66e0c42`/`a53e8a2`):
- *Leak session delete* → destructor never runs → RV never freed → no crash.
- *Leak page delete, keep session delete* → session destructor still runs
  → RV still freed → crashes on first subsequent RV access (timing shifts
  with allocator pressure, explaining "SIGSEGV after ~5 sessions").
- *force_cancel before delete* → doesn't touch RVs → no effect.

The v1 "open hypothesis" — TCP agent / Tcl upcall lifecycle — was a red
herring. The upstream `// who deletes the agents?` comment at `:531-532`
is a **different** upstream bug (agent pool leaks) unrelated to the SEGV.

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

The two `// PS: hmm..` comments at `webtraf.cc:531-532` are a *different*
upstream defect (TCP/sink agent pool leaks on `recycle`). Not exercised
by our scenarios (non-fulltcp, dont_recycle_=0). Out of scope for this MR.

---

## FINDING-1: FairWeightedMeter is a Nortel original, not thesis-originated

**Identified:** 2026-04-15, during Phase 6 preparation
**Method:** Systematic comparison of Nortel ns-2.29 `dsPolicy.{h,cc}`
against DiffServ4NS `dsPolicy.{h,cc}`, triggered by spec-driven
porting (the I-2.6 spec references FW, but the thesis does not).

**Finding:** The `FWPolicy` class in DiffServ4NS is a renamed copy of
`SFDPolicy` from the pristine Nortel ns-2 DiffServ module (2000).
The algorithm is byte-for-byte identical.  Changes limited to:

1. Class renamed: `SFDPolicy` → `FWPolicy`, `SFD` → `FW` (slot 6)
2. `flow_entry.src_id` and `flow_entry.dst_id` fields removed
3. Unused local variables cleaned up in `applyPolicer()`

The thesis (Chapter 3.3.2) explicitly enumerates the module's meters
as "TSW2CM, TSW3CM, Token Bucket, srTCM and trTCM" — **FW is absent**.
Neither UML class diagram (Figures 3.8 and 3.11) shows FWPolicy.  No
thesis experiment uses FW.  The three penalty modes (`downgrade2` =
0/1/2) were all present in the Nortel original.

Two other Nortel policies — `EWPolicy` (`#define EW 7`) and
`DEWPPolicy` (`#define DEWP 8`) — were dropped entirely by
DiffServ4NS.  None of these changes were documented until 2026.

**Implications for Phase 6 port:**
- Test vectors for FW must be derived from the source code, not from
  any thesis specification or RFC (there is none).
- All three penalty modes should be tested at equal depth — no mode
  is "primary" or "preferred" by the thesis author.
- The header comment in the ns-3 port should credit Nortel as the
  algorithmic origin: "Algorithm originates in ns-2.29 SFDPolicy
  (Nortel Networks, 2000)."

**Methodology evidence:** Spec-driven LLM-augmented porting surfaces
undocumented inheritance chains in legacy code.  The rename from SFD
to FW hides the structural similarity — a developer reading only
DiffServ4NS would see "FWPolicy" and assume it was part of the
thesis contribution.  Systematic comparison against the upstream
Nortel code revealed the true provenance.  This finding strengthens
the ICNS3 2026 paper's methodology claim about benefits of porting
as a form of code archaeology.

**Full analysis:** `provenance/FW_THESIS_REFERENCES.md`
**Rename documentation:** `docs/NS2_PATCHES.md` §"Renames and removals"

---

## BUG-6: ns-3 TcpSocketBase::PersistTimeout null-pointer dereference

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
only when the window reopens (`tcp-socket-base.cc:1548`) or via
`CancelAllTimers()` during teardown. The crash path is:

1. Sender has queued data. Receiver's rWnd drops to zero (e.g. its app
   pauses consuming).
2. Persist timer probes 1 byte at a time; receiver ACKs probes but does
   not open the window. `m_firstByteSeq` advances.
3. Eventually `m_nextTxSequence == TailSequence()` — the buffer holds
   no more unsent data, the app has not supplied more.
4. The next persist firing calls `CopyFromSequence(1, seq)`, which
   returns `nullptr`. Crash.

**How we found it.** `diffserv-example-2-fullscale --numHttp=400
--simTime=100` crashed reliably. An isolation experiment
(`--stockQueue=true` vs `--stockQueue=false`) initially pointed at our
code, but rerunning stock `ns3::RedQueueDisc` with aggressive thresholds
(`MinTh=5 MaxTh=15 MaxP=0.5`, matching our DP2 WRED aggression)
reproduced the *identical* crash frame. The variable is drop aggression,
not queue-disc choice. Backtrace under `lldb` landed in
`TcpSocketBase::PersistTimeout`. See
`output/ns3/diagnostics/scaling-crash-isolation.md` for the full log.

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
artifacts (issue text, patch, regression test, commit message) are at
`docs/upstream/ns3-tcp-persist-empty-buffer.md`.

**Verification.**
- Before patch: `tcp-zero-window-test` crashes, 34 other TCP test
  suites pass.
- After patch: all 35 TCP test suites pass, our 76-case `diffserv`
  suite passes, `diffserv-example-2-fullscale --numHttp=400 --simTime=100`
  runs to completion.

**Methodology evidence:** High-fidelity reconstruction functions as a
verification pass on the *simulator framework*, not only on the new
model being built. Together with BUG-5 (the dormant 2001 DiffServ4NS
`Application/FTP::send` Tcl patch), this is the second latent bug
the Scenario 2 full-scale reconstruction has surfaced — one in the
2001 layer and one in the 2026 ns-3 mainline layer.

**Full analysis:**
- `output/ns3/diagnostics/scaling-crash-isolation.md` — diagnostic log
- `docs/upstream/ns3-tcp-persist-empty-buffer.md` — upstream artifacts
- `docs/adr/0022-ns3-tcp-persist-timer-upstream-patch.md` — decision
  record for the local patch

---

## BUG-10: TSW/FW meters silently share the global default RNG stream (2001-era ns-2)

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

**Symptom:**
The probabilistic colour decisions made by TSW2CM / TSW3CM / FW
(random-drop/mark beyond CIR per RFC 2859 §2) are reproducible within
a single build, but the per-meter stream position drifts whenever an
unrelated RNG-consuming object is added upstream (a flow generator,
a different scheduler, etc.). Two simulations with the same TSW
configuration but different unrelated objects produce different
colour vectors, even though RFC 2859 semantics should be identical.

**What happens:**
ns-2's `Random::uniform(a, b)` is a file-scope wrapper over a single
shared `RNG` instance (`defaultrng_`). Constructing a new TSW2CMPolicy
does not allocate it a dedicated stream; it just shares the global one
with every other default-stream consumer. ns-2 provides a per-object
`RNG` class and stream-seeding idiom (`new RNG(RAW_SEED_SOURCE, N)`)
but none of the DiffServ4NS 2001 meter code uses it.

**Why it's a bug:**
Research-grade reproducibility requires stream isolation per
decision-making component, not just per simulation. Adding a new
application or swapping a scheduler silently re-randomises every
observed TSW/FW colour sequence — the scenario author has no way to
hold RNG consumption fixed for the meter while varying the rest of
the setup. The fix landed here doesn't change any *average*
RFC-conformance property; it adds the *capability* to isolate the
stream when the scenario author needs that.

**Inheritance into ns-3 port:**
`src/ns-3/model/tsw2cm-meter.{h,cc}`, `tsw3cm-meter.{h,cc}`,
`fw-meter.{h,cc}` each own a `Ptr<UniformRandomVariable>` member.
Tsw2cm and Tsw3cm had `AssignStreams` methods (never called through
the classifier → edge path); FwMeter had no `AssignStreams` at all.
Under PR3a's composed edge shape, the meters live in the
`DiffServPolicyClassifier::m_meterPool[]` — a member of the classifier,
not a `QueueDiscClass` child — so the standard `ns3::QueueDisc`
`AssignStreams` cascade never reached them. Default-stream inheritance
was identical to ns-2.

**Inheritance into ns-2.35 port:**
`src/ns-2.35/diffserv/dsPolicy.cc` preserves the three
`Random::uniform(0.0, 1.0)` sites verbatim from ns-2.29. Same
anti-pattern.

**Fix (opt-in, preserves pre-fix default behaviour):**

*ns-2.35* (`src/ns-2.35/diffserv/dsPolicy.{h,cc}`, `dsEdge.cc`):
- Base class `Policy` gains an optional `RNG *rng_` and helper
  `double Policy::uniform()` that falls through to
  `Random::uniform(0.0, 1.0)` when `rng_` is `NULL` (default).
- The three `Random::uniform(0.0, 1.0)` sites call `this->uniform()`.
- `Policy::setRngStream(int stream)` creates a dedicated RNG seeded via
  `new RNG(RNG::RAW_SEED_SOURCE, stream)`.
- `PolicyClassifier::setPolicyRngStream(const char *policyName, int)`
  routes to the named pool slot (`"TSW2CM"`, `"TSW3CM"`, `"FW"`).
- `edgeQueue` Tcl command exposes `$edge policyRngStream <name> <stream>`.
- **Scenarios that never call `policyRngStream` observe identical
  pre-fix output** (the fallback path preserves `Random::uniform`).

*ns-3* (`src/ns-3/model/fw-meter.{h,cc}`, `diffserv-edge-queue-disc.cc`):
- Added `FWMeter::AssignStreams(int64_t)` (matching the Tsw2cm/Tsw3cm
  idiom).
- `DiffServEdgeQueueDisc::AssignStreams` now cascades into any
  installed `Tsw2cm` / `Tsw3cm` / `Fw` meter slot via DynamicCast
  guards. Non-probabilistic meters (Dumb, TokenBucket, SrTcm, TrTcm)
  consume no stream.
- **Scenarios that never call `edge->AssignStreams(N)` observe
  identical pre-fix output** (meter RNGs still use the default stream).

**ns-2.29 is not modified.** The frozen original preserves the bug for
historical fidelity, consistent with BUG-1 through BUG-5.

**Categorisation:**
Quality-of-implementation defect (reproducibility hardening), not a
numeric-correctness defect. RFC 2859 §2 expectations are met in
distribution either way; the fix unlocks scenario-author control over
stream isolation without changing means.

**Verification:**
- ns-3 unit test `S-13.6 Edge AssignStreams cascades into meter
  slots (BUG-10)` asserts that two edges with the same injected seed
  produce identical TSW2CM colour vectors, and that different seeds
  produce different vectors. Grows `diffserv` EXTENSIVE 77→78.
- S1/S2 CSVs and Q-tier references unchanged (opt-in design; scenarios
  do not invoke the new API).
- ns-2.35 scenarios (examples-2, examples-3) unchanged by default;
  optional seeding via the new `policyRngStream` Tcl command.

### Follow-up fix (2026-04-19): lazy-create timing gap

The initial BUG-10 fix (commit `e4cb556`) implemented the cascade via
`DynamicCast` over the edge's `m_meters[0..6]` slots — correct for
scenarios that pre-inject via `edge->SetMeter(type, meter)` before
`AssignStreams`. However, scenarios that use the helper API
(`helper.AddTsw2cmPolicy(edge, ...)`) don't pre-inject: they add a
policy entry with `meter = MeterType::TSW2CM` and let the classifier
lazy-create the meter on first `ApplyPolicy` call. Under that path:

1. `edge->AssignStreams(N)` iterates `m_meters[]`, finds all slots
   `nullptr` (nothing pre-created), cascade reaches zero meters.
2. First packet arrives → classifier calls `provider->GetMeter(TSW2CM)`
   → lazy-create fires → new `Tsw2cmMeter` uses global default stream.
3. User's explicit `AssignStreams(N)` silently has no effect on the
   helper-path meter.

The follow-up closes this timing gap by pre-materialising meter slots
in `DiffServEdgeQueueDisc::CheckConfig` for every MeterType referenced
in the policy table. Specifically:

- `DiffServPolicyClassifier::GetUsedMeterTypes()` new method returns the
  set of `MeterType` values appearing in `m_policyTable`.
- `DiffServEdgeQueueDisc::CheckConfig` scans that set and calls
  `GetMeter(type)` per used type, lazy-creating defaults so the slots
  are populated when `AssignStreams` runs.

The fix preserves the opt-in byte-identity guarantee: scenarios that
never call `AssignStreams` still lazy-create on first packet and use
the global default stream, matching pre-BUG-10 behaviour. Only the
ordering of lazy-creation changes (CheckConfig time instead of first-
packet time) — deterministic in all cases.

**Verification (follow-up):**
- New unit test `S-13.8 Meter cascade reaches helper-path meters (BUG-10
  follow-up)` asserts that two edges wired via `AddTsw2cmPolicy` with
  the same `AssignStreams` seed produce identical colour vectors, and
  that different seeds produce different vectors. Grows `diffserv`
  EXTENSIVE 78→79.
- S1/S2 CSVs remain byte-identical; Q-tier TSW-using scenarios
  (examples 2 and 3) unaffected because they don't call `AssignStreams`.

---

## Notes

BUG-1 through BUG-3 were identified during the NS2_PATCHES analysis
(2026-04-14) by diffing `ns2/ns-2.29-pristine/` against
`src/ns-2.29/`.  See `docs/NS2_PATCHES.md` for the complete
patch catalogue.

BUG-4 was identified during Phase 5 porting (2026-04-15) when the
ns-3 implementation of `DsSfqScheduler` refactored the dequeue loop
and the unsafe access order was noticed during code review.  This
demonstrates a secondary benefit of porting: the act of translating
code to a new platform surfaces latent bugs that survived decades of
use in the original.

BUG-5 was identified during Phase 7 Scenario 2 reconstruction
(2026-04-16) when the 2001 `Application/FTP` Tcl patch was audited
as part of the app-type classification analysis.

BUG-6 was identified during Phase 7 Scenario 2 reconstruction
(2026-04-17). Unlike BUG-1 through BUG-5, BUG-6 is a defect in the
modern simulator framework (ns-3 mainline), not in the 2001 ns-2
module — reconstruction exposed it by operating at a higher offered
load than any shipped ns-3 example.

FINDING-1 was identified during Phase 6 preparation (2026-04-15) by
searching the thesis text for FW references and comparing the
DiffServ4NS source against the Nortel original.  Unlike BUG-1 through
BUG-6 (which are code defects), FINDING-1 is a provenance discovery:
undocumented code heritage surfaced by systematic analysis.

FINDING-2 (2026-04-17, Phase 7 ns-2.35 port) confirms that
`PagePool/WebTraf` works correctly on ns-2.35+DS4 — the OFFSET-macro
crash present on ns-2.29+DS4 does not occur. See
`docs/WEBTRAF_NS235_FINDING.md` for the full analysis. Key companion
finding: native WebTraf requires a one-instproc `alloc-tcp` override to
stamp `set_apptype 31` (PT_HTTP) so DiffServ mark rules for `"any http"`
fire correctly; without this, all WebTraf TCP agents carry app_type=0
and fall through to the Best Effort queue.
