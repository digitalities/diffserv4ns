# Module Architecture

This page summarises the DiffServ4NS module design as specified in the
2001 thesis (Chapter 3.3.3) and implemented in the source code under
`src/ns-2.29/diffserv/` (the 2001 algorithmic core) and
`src/ns-2.35/diffserv/` (the 2026 port layer).

## Design overview

DiffServ4NS extends the Nortel Networks ns-2 DiffServ module (released
with ns-2.1b8a in 2001) with three categories of improvements:

1. **Router mechanisms** — new schedulers, decoupled marking / metering /
   dropping, per-drop-precedence dropping.
2. **Measurement infrastructure** — end-to-end and per-hop performance
   parameters, including per-DSCP frequency distributions.
3. **Configuration flexibility** — per-packet mark rules with multi-field
   classification (source node, destination node, transport protocol,
   application type).

For the complete specification, see Chapter 3.3.3 of the 2001 MSc thesis
(Zenodo DOI [10.5281/zenodo.19662899](https://doi.org/10.5281/zenodo.19662899)).

## Class structure

The module is organised around five principal classes. Source paths
below point at the 2001 algorithmic core under `src/ns-2.29/diffserv/`;
the `src/ns-2.35/diffserv/` tree re-skins a subset of these for the
ns-2.35 API and applies the bug fixes catalogued in `LINEAGE.md`.

### `dsCore` — core router

The central DiffServ router class. Handles:

- DSCP-based PHB lookup on dequeue
- Queue management via the `dsredq` class below (one per service /
  Behaviour Aggregate)
- Scheduling across per-class queues (pluggable scheduler hierarchy —
  see `dsScheduler` below)
- Per-hop monitoring: queue length (instantaneous / average, per queue
  and per drop-precedence), departure rate, packet counters

**Source:** `src/ns-2.29/diffserv/dsCore.{h,cc}`

### `dsEdge` — edge router

Extends `dsCore` with ingress functions:

- Multi-field classification: source node, destination node, transport
  protocol, application type (via the `addMarkRule` Tcl interface).
- DSCP marking and re-marking.
- Meter / policer dispatch (via `PolicyClassifier` below).

**Source:** `src/ns-2.29/diffserv/dsEdge.{h,cc}`

### `PolicyClassifier` — metering and policing

Maintains two tables:

- **Policy table** — one entry per classification rule (`addPolicyEntry` /
  `addFlowPolicyEntry`). Each entry names a policer type and its
  parameters (CIR, CBS, PIR, PBS, EBS).
- **Policer table** — one entry per (policy type, arrival DSCP) that
  defines the in-profile / out-of-profile DSCP re-marking scheme.

Policer types implemented:

| Name      | Algorithm / RFC                                   |
|-----------|---------------------------------------------------|
| `Dumb`    | DSCP-preserving pass-through                      |
| `TB`      | Token Bucket (two-state)                          |
| `TSW2CM`  | Time Sliding Window, two-colour (RFC 2859)        |
| `TSW3CM`  | Time Sliding Window, three-colour (RFC 2859)      |
| `srTCM`   | Single-Rate Three-Colour Marker (RFC 2697)        |
| `trTCM`   | Two-Rate Three-Colour Marker (RFC 2698)           |
| `FW`      | Flow-Weighted (TSW-variant with per-flow weights) |

**Source:** `src/ns-2.29/diffserv/dsPolicy.{h,cc}`

### `dsredq` — RED queue with drop-precedence levels

Per-class RED queue with multiple drop-precedence levels. Supports:

- DROP mode (tail drop) and MARK mode (ECN-style)
- Configurable RED parameters (`minth`, `maxth`, `maxp`) per
  drop-precedence level
- Virtual queue length tracking
- RIO-C and WRED dropper variants

**Source:** `src/ns-2.29/diffserv/dsredq.{h,cc}`

### `dsScheduler` — scheduler hierarchy

An encapsulated scheduler class replacing the inline scheduling in the
original Nortel module. Available algorithms:

| Scheduler | Algorithm                              | Reference              |
|-----------|----------------------------------------|------------------------|
| RR        | Round Robin                            | —                      |
| WRR       | Weighted Round Robin                   | —                      |
| WIRR      | Weighted Interleaved Round Robin       | —                      |
| PRI       | Strict Priority                        | —                      |
| **WFQ**   | Weighted Fair Queueing                 | Parekh & Gallager, 1993 |
| **WF2Q+** | Worst-case Fair Weighted Fair Queueing | Bennett & Zhang, 1996-1997 |
| **SCFQ**  | Self-Clocked Fair Queueing             | Golestani, 1994         |
| **SFQ**   | Start-time Fair Queueing               | Goyal, Vin & Cheng, 1996 |
| **LLQ**   | Low Latency Queueing (PQ + sub-scheduler) | Cisco, 2001          |

Bold entries are additions by DiffServ4NS. The scheduler hierarchy
(encapsulation + WFQ family + LLQ) is the thesis author's original
contribution over the Nortel base module.

**Source:** `src/ns-2.29/diffserv/dsscheduler.{h,cc}`

## How the pieces fit together

At an **edge router**, an arriving packet passes through:

```
    packet ─► PolicyClassifier ─► (DSCP re-marked) ─► dsCore ─► dsredq[n] ─► dsScheduler ─► link
              (addMarkRule +                          classify      drop           pick
               addPolicyEntry                         DSCP →        decision       next queue
               decide policer)                        queue n        (RED /
                                                                     drop-precedence)
```

At a **core router**, classification is skipped and the packet enters
`dsCore` directly, using its existing DSCP mark:

```
    packet ─► dsCore ─► dsredq[n] ─► dsScheduler ─► link
```

## UML class diagram

The complete class diagram is Figure 3.11 in the 2001 thesis (page 47),
captioned "*DiffServ+ module UML Class Diagram*". It shows every class,
method, and relationship in the module. The class names in the 2001
diagram match the source code in this repository line for line.

The thesis (with the figure) is deposited on Zenodo —
DOI [10.5281/zenodo.19662899](https://doi.org/10.5281/zenodo.19662899).

## Monitoring parameters

DiffServ4NS adds the following measurement capabilities (thesis Section
3.3.3.2):

### End-to-end (UDP)

- **One-Way Delay (OWD):** instantaneous, average, minimum,
  frequency-distributed
- **IP Packet Delay Variation (IPDV):** instantaneous, average, minimum,
  frequency-distributed

### End-to-end (TCP)

- **Goodput:** per-DSCP throughput
- **Round-Trip Time:** instantaneous and frequency-distributed
- **Window Size:** instantaneous and frequency-distributed

### Per-hop

- **Queue length:** per-queue and per-drop-precedence, instantaneous and
  average
- **Maximum Burstiness:** for queue 0 (typically the EF / Premium queue)
- **Departure rate:** per-queue and per-drop-precedence
- **Packet counters:** received, transmitted, dropped (by dropper and by
  buffer overflow), per-DSCP

## What the 2026 port layer (`src/ns-2.35/`) adds

The 2026 port layer under `src/ns-2.35/diffserv/` is additive on top of
the 2001 core. It:

- re-skins the 2001 code for the ns-2.35 C++ API,
- fixes nine latent 2001-era bugs (see the table in
  [`LINEAGE.md`](../LINEAGE.md#the-2026-ns-235-port-layer)),
- extends `PolicyClassifier` with a per-flow variant of `addPolicyEntry`
  so independent three-colour re-marking schemes can coexist on the
  same arrival DSCP,
- isolates the probabilistic-marking RNG stream per policy-pool slot
  (BUG-10), and
- adds the 28-byte UDP header accounting (IP 20 + UDP 8) that the 2001
  code omitted.

Detailed change notes live in
[`src/ns-2.35/CHANGELOG.md`](../src/ns-2.35/CHANGELOG.md).
