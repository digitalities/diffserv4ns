# Porting Map: diffserv4ns (ns-2) → ns-3

For each ns-2 class, this document specifies the ns-3 target, the dependencies that need rewriting, and the validation strategy.

## Translation primitives

These five substitutions cover roughly 80% of the mechanical work:

| ns-2 idiom | ns-3 equivalent | Notes |
|---|---|---|
| `Scheduler::instance().clock()` | `Simulator::Now().GetSeconds()` | Use `.GetSeconds()` for double arithmetic |
| `hdr_cmn::access(pkt)->size()` | `packet->GetSize()` | Returns bytes |
| `Packet*` (raw pointer) | `Ptr<Packet>` (smart pointer) | Reference-counted |
| `TclObject` base + `command()` | `Object` base + `TypeId` with attributes | Configuration via `Config::Set` |
| Tcl `argv`-parsing in `command()` | Helper class methods with typed parameters | E.g. `DiffServHelper::AddPolicy(...)` |
| `bind(...)` in constructor | `.AddAttribute(...)` in `GetTypeId()` | Standard ns-3 pattern |

## Class-by-class map

### `dsconsts.h` → `ns3/diffserv-constants.h`

Pure constants. Direct copy with namespace wrapping:

```cpp
namespace ns3 {
namespace diffserv {
constexpr uint32_t MAX_QUEUES    = 9;
constexpr uint32_t MAX_PREC      = 3;
constexpr uint32_t MAX_CP        = 64;
constexpr uint32_t MEAN_PKT_SIZE = 1000;

enum PacketResult { PKT_DROPPED, PKT_ENQUEUED, PKT_EDROPPED, PKT_MARKED };
}
}
```

**Effort:** trivial. **Validation:** none required.

---

### Meters and policers (`dsPolicy.{h,cc}`)

#### `Policy` (abstract base) → `ns3::diffserv::Meter`

```cpp
class Meter : public Object {
public:
  static TypeId GetTypeId();
  virtual ~Meter() = default;
  virtual void   ApplyMeter(PolicyEntry& policy, Ptr<const Packet> pkt) = 0;
  virtual int    ApplyPolicer(PolicyEntry& policy,
                              const PolicerEntry& policer,
                              Ptr<const Packet> pkt) = 0;
};
```

The base class becomes pure virtual with `Object` inheritance for the ns-3 type system. `policyTableEntry` becomes `PolicyEntry` (a struct, not an Object — it's plain runtime state). `Ptr<const Packet>` because the meter never modifies the packet, only reads its size.

#### `policyTableEntry` → `PolicyEntry` struct

Direct field-for-field translation, but use `Time` for `arrivalTime` instead of `double`:

```cpp
struct PolicyEntry {
  Ipv4Address sourceNode;     // was nsaddr_t
  Ipv4Address destNode;
  uint32_t    policyIndex;
  PolicerType policer;
  MeterType   meter;
  uint8_t     codePoint;      // in-profile DSCP

  DataRate cir, pir;          // typed instead of raw double
  uint64_t cbs, ebs, pbs;     // bytes
  double   cBucket, eBucket, pBucket;   // current bucket levels
  Time     arrivalTime;
  double   avgRate;           // TSW state
  Time     winLen;            // TSW state
};
```

Using `DataRate` and `Time` instead of raw doubles is more idiomatic ns-3 and gives free unit conversion.

#### `DumbPolicy` → `DumbMeter`

Trivial pass-through. Keep as a sanity-check baseline. **Effort:** 1 hour.

#### `TBPolicy` → `TokenBucketMeter`

```cpp
void TokenBucketMeter::ApplyMeter(PolicyEntry& p, Ptr<const Packet>) {
  Time now = Simulator::Now();
  double tokens = p.cir.GetBitRate() / 8.0 *
                  (now - p.arrivalTime).GetSeconds();
  p.cBucket = std::min(p.cBucket + tokens, double(p.cbs));
  p.arrivalTime = now;
}

int TokenBucketMeter::ApplyPolicer(PolicyEntry& p,
                                    const PolicerEntry& pol,
                                    Ptr<const Packet> pkt) {
  double size = pkt->GetSize();
  if (p.cBucket >= size) { p.cBucket -= size; return pol.initialCodePt; }
  return pol.downgrade2;
}
```

**Validation:** unit test — feed a 1 Mbps stream into a 500 kbps bucket with a 10 kB burst, assert the conformance ratio matches analytical expectation within 1%.

#### `SRTCMPolicy` → `SrTcmMeter` (RFC 2697)

Translation is line-for-line from the analysis in `INVENTORY.md` §5. The reference implementation in `dsPolicy.cc:651–712` is correct and clean.

**Validation:** RFC 2697 conformance vectors. The RFC provides specific (CIR, CBS, EBS, traffic pattern) → (green, yellow, red) expectations.

**Effort:** 4 hours including tests.

#### `TRTCMPolicy` → `TrTcmMeter` (RFC 2698)

Same — line-for-line translation from `dsPolicy.cc:717–778`.

**Validation:** RFC 2698 conformance vectors.

**Effort:** 4 hours including tests.

#### `TSW2CMPolicy` / `TSW3CMPolicy` → `Tsw2cmMeter` / `Tsw3cmMeter`

EWMA-based rate estimator with probabilistic marking. Reference: RFC 2859. The state lives in `PolicyEntry::avgRate` and `PolicyEntry::winLen`.

**Validation:** statistical — feed CBR traffic at known rates, confirm the EWMA estimate converges within `winLen` and the marking probabilities match the RFC formula.

**Effort:** 1 day each (the probabilistic marking needs an `Ptr<UniformRandomVariable>` member).

#### `FWPolicy` → `FairWeightedMeter`

The most complex meter. Maintains a `flow_table` keyed by flow id with per-flow state (`bytes_sent`, `count`, `last_update`, linked list). In ns-3, replace the linked list with `std::unordered_map<FlowId, FlowState>`.

The flow-id lookup currently uses ns-2's `hdr_flags` field. In ns-3 this is the FlowMonitor classifier or a custom 5-tuple hash.

**Validation:** harder — no RFC reference, only the original ns-2 behaviour. Use the example scenarios as the oracle.

**Effort:** 2–3 days.

#### `PolicyClassifier` → `DiffServPolicyClassifier`

Holds the policy table and policer table, exposes `Mark(Ptr<Packet>)` to the edge router. Becomes a plain C++ class (not an `Object`) owned by the `EdgeQueueDisc`.

The Tcl `addPolicyEntry` / `addPolicerEntry` argv-parsers become typed methods called from `DiffServHelper`:

```cpp
classifier.AddPolicy(source, dest, MeterType::SRTCM,
                     DataRate("1Mbps"),  // cir
                     8000,               // cbs
                     16000);             // ebs
classifier.AddPolicer(policyIndex, initialDscp, yellowDscp, redDscp);
```

**Effort:** 1 day.

---

### DS-RED (`dsred.{h,cc}`, `dsredq.{h,cc}`)

#### `dsREDQueue` → `DsRedQueueDisc : public QueueDisc`

This is the parent of both edge and core router queues. Represents a multi-queue device with up to `MAX_QUEUES` physical queues, each with up to `MAX_PREC` virtual queues for drop precedence.

ns-3 mapping:

- The outer `DsRedQueueDisc` extends `QueueDisc` and holds `MAX_QUEUES` child `DsRedSubQueue` instances.
- Each `DsRedSubQueue` is itself a small queue disc holding `MAX_PREC` virtual queues with RIO-style drop precedence (RED with In/Out).
- Scheduling between physical queues is delegated to a `DsScheduler` member.
- The classification of incoming packets to a physical queue is done by reading the DSCP and consulting a DSCP→queue mapping.

This composition is similar to ns-3's existing `PfifoFastQueueDisc` (which has 3 bands) and `MqQueueDisc` (multi-queue), but with deeper structure.

**Reference to study:** `src/traffic-control/model/red-queue-disc.{h,cc}` for the RED algorithm itself, and `src/traffic-control/model/prio-queue-disc.{h,cc}` for the multi-band scheduling pattern.

#### `dsREDSubQueue` (`dsredq.cc`) → `DsRedSubQueue`

The per-queue logic with virtual queues for drop precedence. Implements the RED drop probability calculation per virtual queue but a shared physical buffer. This is essentially RIO (RED with In and Out).

**Effort:** 1 week for both classes including unit tests.

---

### Edge and core routers (`dsEdge.{h,cc}`, `dsCore.{h,cc}`)

#### `edgeQueue` → `DiffServEdgeQueueDisc : public DsRedQueueDisc`

Adds:
- `MarkRulesTable` — array of `MarkRule { dscp, srcAddr, dstAddr, protocol, appType, srcPort, dstPort }`
- `PolicyClassifier policy` — the meter/marker pipeline
- Override `Enqueue()` to (1) classify packet via mark rules, (2) apply meters/policers to compute final DSCP, (3) write DSCP to IPv4 header, (4) hand off to parent enqueue logic

The mark rule matching is a linear scan in the original (with `MAX_MARK_RULES = 20`). Keep that in the port — premature optimisation otherwise.

The DSCP write becomes:

```cpp
Ipv4Header ipHeader;
packet->RemoveHeader(ipHeader);
ipHeader.SetDscp(Ipv4Header::DscpType(newDscp));
packet->AddHeader(ipHeader);
```

**Effort:** 3 days.

#### `coreQueue` → `DiffServCoreQueueDisc : public DsRedQueueDisc`

Trivial — the core router just inherits the parent's PHB forwarding behaviour. The original `dsCore.cc` is only 120 lines and mostly delegates.

**Effort:** half a day.

---

### Schedulers (`dsscheduler.{h,cc}`)

This is the largest and most algorithmically-sensitive part of the port. Each scheduler becomes a class derived from a common `DsScheduler` base, owned by `DsRedQueueDisc`.

```cpp
class DsScheduler : public Object {
public:
  static TypeId GetTypeId();
  virtual ~DsScheduler() = default;
  virtual void Reset() {}
  virtual void OnEnqueue(uint32_t queueIndex, Ptr<const Packet> pkt) = 0;
  virtual int  SelectNextQueue() = 0;   // returns -1 if all idle
  virtual void SetParam(uint32_t queueIndex, double value) {}
};
```

#### `dsRR` → `DsRoundRobinScheduler`
Straightforward. Track which queues are non-empty, rotate through them. **Effort:** 4 hours.

#### `dsWRR` → `DsWeightedRoundRobinScheduler`
Each queue has an integer weight; serve `weight[i]` packets from queue `i` in each round. **Effort:** 4 hours.

#### `dsWIRR` → `DsWeightedInterleavedRoundRobinScheduler`
Interleaved variant where slices are spread across the round rather than served in bursts. **Effort:** 1 day.

#### `dsPQ` → `DsPriorityScheduler`
Strict priority with optional rate cap per queue. Reused by `DsLLQ`. **Effort:** 1 day.

#### `dsWFQ` → `DsWfqScheduler` (HARD)

Implements full Weighted Fair Queueing with a GPS reference clock. The original uses ns-2's `Handler` pattern to schedule events on the GPS reference system; in ns-3 this becomes `Simulator::Schedule()` callbacks.

The state per flow:
- `weight_` — fair share
- `B` — set of active queues in the GPS reference system (bitmask)
- `GPS` queue and `PGPS` queue (queues of finish times)
- `GPSfinish_t`, `PGPSfinish_t`, `finish_t`

Plus global state: `v_time` (virtual time), `last_vt_update`, `sum` (sum of weights of active queues), `idle` flag.

**Reference:** Demers, Keshav, Shenker (1989) — the original WFQ paper.

**Validation:** the canonical WFQ test — N flows with weights `w_1, ..., w_N` over a backlogged link should each receive bandwidth `w_i / sum(w) * C` within an error bound proportional to maximum packet size.

**Effort:** 5–7 days.

#### `dsWF2Qp` → `DsWf2qPlusScheduler` (HARD)

WF2Q+ is the improved version of WFQ with tighter delay bounds. Tracks per-flow `S` (start time) and `F` (finish time) plus a global virtual time `V`. Eligible-set semantics: only packets with `S <= V` are candidates.

**Reference:** Bennett & Zhang (1996) — WF2Q paper; Stiliadis & Varma — WF2Q+ refinement.

**Validation:** same as WFQ plus the eligibility property — no flow should be served before its packet becomes eligible.

**Effort:** 5–7 days.

#### `dsSCFQ` → `DsScfqScheduler` (MEDIUM-HARD)

Self-Clocked Fair Queueing — uses the finish time of the packet currently in service as the virtual time, avoiding the GPS computation. Simpler than WFQ but still needs careful labelling.

**Reference:** Golestani (1994).

**Effort:** 3–4 days.

#### `dsSFQ` → `DsSfqScheduler` (MEDIUM)

Start-time Fair Queueing. Each packet gets a start tag and a finish tag based on the previous packet on its flow. Servicing order is by start tag.

**Reference:** Goyal, Vin, Cheng (1996).

**Effort:** 2–3 days.

#### `dsLLQ` → `DsLlqScheduler`

Composite: a strict-priority queue (using `DsPriorityScheduler`) for low-latency traffic plus a fair-queueing scheduler (configurable WFQ/WF2Q+/SCFQ/SFQ) for everything else. Implementation just delegates.

**Effort:** 2 days (after the components exist).

---

## Dependency-ordered build sequence

Build bottom-up so that each layer is fully tested before the next:

1. **Constants and types** (`diffserv-constants.h`, `policy-entry.h`)
2. **Meters** (`Meter` base, `DumbMeter`, `TokenBucketMeter`, `SrTcmMeter`, `TrTcmMeter`)
3. **TSW meters** (`Tsw2cmMeter`, `Tsw3cmMeter`)
4. **Policy classifier** (`DiffServPolicyClassifier`)
5. **Sub-queue with RIO** (`DsRedSubQueue`)
6. **DS-RED queue disc base** (`DsRedQueueDisc`)
7. **Simple schedulers** (`DsRoundRobinScheduler`, `DsWeightedRoundRobinScheduler`, `DsWeightedInterleavedRoundRobinScheduler`, `DsPriorityScheduler`)
8. **Edge and core queue discs** (`DiffServEdgeQueueDisc`, `DiffServCoreQueueDisc`)
9. **Helper class** (`DiffServHelper`)
10. **Example-1 scenario port** — first end-to-end milestone
11. **Fair queueing schedulers** (WFQ, WF2Q+, SCFQ, SFQ)
12. **LLQ composite**
13. **Fair Weighted meter** (`FairWeightedMeter`)
14. **Monitoring layer** (per-DSCP statistics, frequency distributions)
15. **Example-2 scenario port** — second end-to-end milestone

Phases 1–10 are the **minimal viable port** target (2–3 weeks). Phases 11–15 are the **full port** (6–8 weeks total).

## What needs studying before writing code

Mandatory reading before starting the port:

1. **ns-3 traffic-control module documentation** — `https://www.nsnam.org/docs/models/html/traffic-control-layer.html`
2. **`src/traffic-control/model/queue-disc.{h,cc}`** — the QueueDisc base class
3. **`src/traffic-control/model/red-queue-disc.{h,cc}`** — closest reference for the RED algorithm
4. **`src/traffic-control/model/prio-queue-disc.{h,cc}`** — multi-band scheduling pattern
5. **`src/traffic-control/examples/traffic-control.cc`** — Imputato's basic usage example
6. **Imputato & Avallone, WNS3 2016** — "Design and Implementation of the Traffic Control Module in ns-3"
7. **Imputato & Avallone, WNS3 2017** — "Traffic Differentiation and Multiqueue Networking in ns-3"
8. **RFC 2697** (srTCM) and **RFC 2698** (trTCM) for the meter conformance vectors
9. **RFC 2474, 2475, 2597, 2598** for the DiffServ architecture itself (refresher)

Plus, for the fair-queueing schedulers, the original papers cited in each scheduler section above.
