# DiffServ4NS — Inventory and Port Analysis

**Source:** DiffServ4NS, archived on SourceForge (https://sourceforge.net/projects/diffserv4ns/). Two near-identical SourceForge tarballs exist (0.1 and 0.2); the C++ source code is byte-identical between them and unchanged from the 2001 thesis. The 0.2 packaging is a strict superset (it adds gnuplot scripts that example-2 needs) and is the canonical reference under `ns2/diffserv4ns/`.
**Original development:** 2001, as the implementation backing the author's MSc thesis at Lappeenranta University of Technology / University of Pisa. See `provenance/LINEAGE.md` for the full provenance chain. **No code changes have been made to `src/diffserv/` since 2001.**
**SourceForge release:** 29 June 2006 (as DiffServ4NS-0.1, GPLv2). Re-packaging of the 2001 thesis code, originally distributed via the author's personal website 2001–2006.
**Author:** Sergio Andreozzi (2001), built on the Nortel Networks ns-2 diffserv module (2000)
**Licence:** GPLv2 (compatible with ns-3, which is also GPLv2 — straightforward upstreaming)
**Target platform of original:** ns-2 version 2.29
**Target platform of port:** ns-3 (current mainline)

> **Note:** the architectural design that the 2006 release implements was originally laid out in Sergio's 2001 master's thesis at Lappeenranta University of Technology / Università di Pisa (*DiffServ simulations using the Network Simulator: requirements, issues and solutions*). Figure 3.11 of that thesis (page 48) contains the full UML class diagram of what was then called the "DiffServ+" module, which became DiffServ4NS unchanged. The thesis is in this repository at `provenance/Andreozzi-2001-thesis.pdf`. Chapter 3.3.3 of the thesis is the original design document for everything in `src/diffserv/`. See `provenance/LINEAGE.md` for the full provenance chain.

## 1. Tarball contents

The diffserv core under `src/diffserv/` totals 4,265 lines of C++. The wider archive contains modified versions of stock ns-2 base files (tcp, agent, packet, apps, webcache) which are not part of the port — only the diffserv directory is original work.

| File | LOC | Role |
|---|---|---|
| `dsPolicy.cc` / `.h` | 947 + 272 | Meters, markers, policers, policy and policer tables |
| `dsred.cc` / `.h` | 717 + 219 | DS-RED queue base class with multi-queue and multi-precedence |
| `dsscheduler.cc` / `.h` | 627 + 291 | All schedulers (RR, WRR, WIRR, PQ, WFQ, WF2Q+, SCFQ, SFQ, LLQ) |
| `dsredq.cc` / `.h` | 406 + 153 | Per-queue RED logic with virtual queues for drop precedence |
| `dsEdge.cc` / `.h` | 273 + 121 | Edge router: BA classification and marking rules |
| `dsCore.cc` / `.h` | 120 + 107 | Core router: inherits dsREDQueue with PHB behaviour |
| `dsconsts.h` | 12 | Constants: MAX_QUEUES=9, MAX_PREC=3, MAX_CP=64, MEAN_PKT_SIZE=1000 |

Two complete example scenarios are included under `examples/`:

- **example-1**: 5-source / edge / core / 5-destination topology with CBR traffic, srTCM policer, gnuplot output for queue length, OWD, and IPDV
- **example-2**: 393-line scenario script with utilities

## 2. Feature inventory (from the dsCore.h author header)

The original `dsCore.h` lists the additions made on top of the Nortel original. This list functions as a built-in feature spec.

### 2.1 Marking
- Mark rules based on source node
- Mark rules based on destination node
- Mark rules based on transport protocol type
- Mark rules based on application type

### 2.2 Schedulers (eight implementations)
- `dsRR` — Round Robin
- `dsWRR` — Weighted Round Robin
- `dsWIRR` — Weighted Interleaved Round Robin
- `dsPQ` — Priority Queue with rate cap
- `dsWFQ` — Weighted Fair Queueing (full GPS reference system)
- `dsWF2Qp` — Worst-case Fair Weighted Fair Queueing+
- `dsSCFQ` — Self-Clocked Fair Queueing
- `dsSFQ` — Start-time Fair Queueing
- `dsLLQ` — Low Latency Queueing (priority + PFQ composite)

### 2.3 Meters and policers
- `DumbPolicy` — pass-through
- `TSW2CMPolicy` — Time Sliding Window Two Colour Marker
- `TSW3CMPolicy` — Time Sliding Window Three Colour Marker
- `TBPolicy` — Token Bucket
- `SRTCMPolicy` — Single Rate Three Colour Marker (RFC 2697)
- `TRTCMPolicy` — Two Rate Three Colour Marker (RFC 2698)
- `FWPolicy` — Fair Weighted with per-flow state

### 2.4 New policy: DSCP-based rate limiter

### 2.5 Monitoring (per-DSCP statistics)

**For UDP traffic:**
- One-Way Delay (OWD) — average, instantaneous, minimum, frequency distribution
- IP Delay Variation (IPDV) — average, instantaneous, minimum, frequency distribution

**For TCP traffic (per DSCP):**
- Goodput
- Round-Trip Time — instantaneous and frequency distribution
- Window Size — instantaneous and frequency distribution

**Per-hop parameters:**
- Instantaneous and average queue length per queue and per (queue, drop precedence)
- Maximum burstiness for queue 0
- Departure rate per queue and per (queue, drop precedence)
- Received, transmitted, and dropped packet counts per DSCP (absolute and percentage); droppers and buffer overflow tracked separately

## 3. Class hierarchy (porting topology)

```
dsREDQueue (base)
├── edgeQueue        — BA classifier + mark rules + policy classifier
└── coreQueue        — pure PHB forwarding

PolicyClassifier     — owns policy table + policer table
└── policy_pool[]: Policy*
    ├── DumbPolicy
    ├── TSW2CMPolicy
    ├── TSW3CMPolicy
    ├── TBPolicy
    ├── SRTCMPolicy   ← RFC 2697
    ├── TRTCMPolicy   ← RFC 2698
    └── FWPolicy      ← per-flow state, flow_table

dsScheduler (base)
├── dsRR
├── dsWRR
├── dsWIRR
├── dsPQ              — also reused by dsLLQ
├── dsWFQ : Handler   — uses ns-2 event scheduler
├── dsWF2Qp
├── dsSCFQ
├── dsSFQ
└── dsLLQ             — composite: dsPQ + (one of WFQ/WF2Q+/SCFQ/SFQ)
```

## 4. Key data structures

### `policyTableEntry`
Holds the runtime state for one (source, destination) policy. Fields:

- Flow identification: `sourceNode`, `destNode`, `policy_index`
- Type: `policer`, `meter`, `codePt` (in-profile DSCP)
- Single-rate parameters: `cir`, `cbs`, `cBucket` (current committed bucket)
- Excess parameters: `ebs`, `eBucket`
- Two-rate parameters: `pir`, `pbs`, `pBucket`
- TSW state: `arrivalTime`, `avgRate`, `winLen`

This is the per-flow state object the meters update on every packet.

### `policerTableEntry`
- `policer` type, `initialCodePt`, `downgrade1` (yellow), `downgrade2` (red), `policy_index`

### Constants
- `MAX_QUEUES = 9` — physical RED queues per device
- `MAX_PREC = 3` — virtual RED queues per physical queue (drop precedence levels — matches AF1/AF2/AF3)
- `MAX_CP = 64` — DSCP code points
- `MEAN_PKT_SIZE = 1000` bytes — for RED calculations

## 5. Algorithm reference: what the meters actually compute

### srTCM (`SRTCMPolicy`, dsPolicy.cc:651–714)

**Token accumulation** (`applyMeter`):
```
elapsed = now - arrivalTime
tokens  = cir * elapsed
if cBucket + tokens <= cbs:
    cBucket += tokens
else:
    overflow = (cBucket + tokens) - cbs
    cBucket  = cbs
    eBucket  = min(eBucket + overflow, ebs)
arrivalTime = now
```

**Marking** (`applyPolicer`):
```
if cBucket >= size:
    cBucket -= size
    return GREEN (initialCodePt)
elif eBucket >= size:
    eBucket -= size
    return YELLOW (downgrade1)
else:
    return RED (downgrade2)
```

This is a textbook RFC 2697 implementation (colour-blind mode). Maps directly to RFC 2697 test vectors.

### trTCM (`TRTCMPolicy`, dsPolicy.cc:717–778)

**Token accumulation** (two independent buckets):
```
elapsed   = now - arrivalTime
cBucket   = min(cBucket + cir * elapsed, cbs)
pBucket   = min(pBucket + pir * elapsed, pbs)
arrivalTime = now
```

**Marking**:
```
if pBucket < size:
    return RED (downgrade2)
elif cBucket < size:
    pBucket -= size
    return YELLOW (downgrade1)
else:
    cBucket -= size
    pBucket -= size
    return GREEN (initialCodePt)
```

Textbook RFC 2698 colour-blind implementation.

### Token Bucket (`TBPolicy`, dsPolicy.cc:604–651)

Single bucket, two-colour marker. Same structure as the cBucket arithmetic in srTCM but without the excess bucket.

### TSW2CM / TSW3CM (`TSW2CMPolicy`/`TSW3CMPolicy`, dsPolicy.cc:507–602)

Time Sliding Window estimators. Uses `avgRate` and `winLen` in the policy entry to compute an EWMA rate estimate; marking is probabilistic when the estimate exceeds CIR (and PIR for 3CM). Reference: RFC 2859 (TSW family).

### Fair Weighted (`FWPolicy`, dsPolicy.cc:780–947)

Maintains a per-flow table (`flow_table` of `flow_entry` records). On each packet, looks up the flow by fid, updates `bytes_sent` and `count`, and computes a fair share. More complex than the others — has a destructor and printable flow table.

## 6. Honest porting effort assessment

Three factors revise my earlier estimate:

**Upward — the scheduler family.** Eight schedulers, three of which (WFQ, WF2Q+, SCFQ) involve virtual-time bookkeeping where subtle errors give plausible-looking but incorrect results. ns-3 mainline traffic-control does not have these — your port would be the first to bring WF2Q+ to ns-3. Each needs its own conformance test.

**Upward — monitoring infrastructure.** The per-DSCP frequency distributions for OWD, IPDV, goodput, RTT, and window size are a substantial body of code that lives outside the queue discipline. In ns-3 this maps to FlowMonitor extensions or custom trace sources, not a one-to-one port.

**Downward — the code is well-structured.** Clean class hierarchy with proper virtual dispatch through `Policy::applyMeter`/`applyPolicer` and `dsScheduler::EnqueEvent`/`DequeEvent`. The token-bucket arithmetic in `dsPolicy.cc` is self-contained — it touches `Scheduler::instance().clock()` and `hdr_cmn::access(pkt)->size()`, nothing more. These two calls become `Simulator::Now()` and `packet->GetSize()` in ns-3. That is the entire ns-2-to-ns-3 dependency surface for the meters.

### Revised effort estimate: 4–8 weeks focused work

| Phase | Duration | Scope |
|---|---|---|
| 1 | Week 1 | Meters and markers (TBPolicy, srTCM, trTCM, TSW2CM/3CM) as standalone C++ classes with unit tests against RFC 2697/2698 vectors |
| 2 | Weeks 2–3 | DS-RED queue disc and simple schedulers (RR, WRR, WIRR, PQ); map onto ns-3 QueueDisc and PrioQueueDisc patterns |
| 3 | Weeks 3–5 | Fair-queueing schedulers (WFQ, WF2Q+, SCFQ, SFQ, LLQ) — the genuinely hard part |
| 4 | Weeks 5–6 | Edge/core router composition, BA + MF classifier, FWPolicy with per-flow state |
| 5 | Weeks 6–7 | Monitoring layer and per-DSCP statistics |
| 6 | Week 8 | Example scenarios ported to ns-3, end-to-end validation against original ns-2 outputs |

### Minimal viable port: 2–3 weeks
Drop the exotic schedulers (keep only PQ + WRR + WFQ) and the full monitoring layer (use FlowMonitor as-is). Land a working DiffServ edge/core in ns-3 that runs example-1.

## 7. Three independent validation oracles

This is what makes the port unusually well-suited to evaluation-driven development:

1. **The `dsCore.h` feature list** — Sergio's own 2006 specification, directly mappable to Intent-tier specs.
2. **RFC 2697 / 2698 test vectors** — formal conformance assertions for srTCM and trTCM behaviour, independent of any implementation.
3. **example-1 simulation outputs** — running the original ns-2 module on the example produces gnuplot traces (queue length, OWD, IPDV) that the ns-3 port should reproduce within statistical tolerance.

Three independent oracles means an agentic loop can iterate against all three without a human in every cycle.

## 8. Notes for the port

- **No SVN history needed.** The tarball contains `.svn` directories from the original working copy, but treat them as historical curiosities — git is the working VCS.
- **GPLv2 throughout.** Both diffserv4ns and ns-3 are GPLv2, so contributing the port upstream to ns-3 contrib is straightforward. As original author, Sergio retains the ability to relicense if needed.
- **The `Scheduler::instance().clock()` and `hdr_cmn::access(pkt)->size()` calls are the only ns-2 dependencies in the meter code.** Everything else is plain C++ arithmetic on doubles.
- **The Tcl interface (`command()` methods) does not port.** Configuration in ns-3 is via `TypeId` attributes and helper classes. The `addPolicyEntry` / `addPolicerEntry` argv-parsing methods become C++ helper methods like `DiffServHelper::AddPolicy(source, dest, policer, cir, cbs, ...)`.
- **`MAX_QUEUES = 9` is generous for AF + EF + BE.** Standard DiffServ needs 6 (one per AF class plus EF plus BE), so the data structures have headroom.
