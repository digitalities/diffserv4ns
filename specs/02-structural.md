# Structural Specs — diffserv-ns3

**Tier:** Structural (testable assertions against the implementation)
**Audience:** Implementer and CI
**Format:** Each spec describes one observable property of one component, expressed as a test that should pass. Specs link back to Intent specs (I-N) where applicable.

## S-1: Token bucket arithmetic

**S-1.1** [↪ I-2.3] Given a `TokenBucketMeter` with CIR=500 kbps, CBS=10000 bytes, fed a CBR stream at 500 kbps with 1000-byte packets, in steady state every packet shall be marked GREEN.

**S-1.2** [↪ I-2.3] Given a `TokenBucketMeter` with CIR=500 kbps, CBS=10000 bytes, fed a CBR stream at 1 Mbps with 1000-byte packets, the steady-state GREEN packet ratio shall equal 0.5 ± 0.01.

**S-1.3** [↪ I-2.3] After an idle period of `t` seconds, the bucket level shall increase by `min(CIR * t, CBS - cBucket)` bytes on the next packet's `ApplyMeter` call.

**S-1.4** [↪ I-2.3] The bucket level shall never exceed CBS.

**S-1.5** [↪ I-2.3] The bucket level shall never go negative.

## S-2: srTCM (RFC 2697)

**S-2.1** [↪ I-2.1] Given an `SrTcmMeter` with CIR=1 Mbps, CBS=10000, EBS=20000 starting empty, after 100 ms idle the cBucket shall hold 12500 bytes (overflowing into eBucket which shall hold 0 bytes — overflow is to eBucket, but cBucket caps at CBS=10000 and the remaining 2500 goes to eBucket).

**S-2.2** [↪ I-2.1] Given the conditions above, a 5000-byte packet arriving immediately after shall be marked GREEN, leaving cBucket=5000, eBucket=2500.

**S-2.3** [↪ I-2.1] Given an `SrTcmMeter` with CIR=1 Mbps, CBS=2000, EBS=4000 fed CBR traffic at 1.5 Mbps with 1000-byte packets, the long-run GREEN ratio shall equal CIR/arrival_rate = 0.667 ± 0.02.

**S-2.4** [↪ I-2.1] Given the conditions in S-2.3, the long-run YELLOW ratio shall equal (excess up to PIR-CIR equivalent for srTCM) such that GREEN+YELLOW = (CIR+EBS_refill)/arrival_rate, and the RED ratio shall match the remainder.

**S-2.5** [↪ I-2.1] The colour decision sequence shall match the reference implementation in `dsPolicy.cc:697` line for line for any deterministic input sequence.

## S-3: trTCM (RFC 2698)

**S-3.1** [↪ I-2.2] Given a `TrTcmMeter` with CIR=500 kbps, PIR=1 Mbps, CBS=2000, PBS=4000, fed CBR traffic at 750 kbps with 1000-byte packets, the long-run GREEN ratio shall equal CIR/arrival_rate = 0.667 ± 0.02.

**S-3.2** [↪ I-2.2] Given the conditions above, the long-run YELLOW ratio shall equal (PIR-CIR)/arrival_rate = 0.667 - long-run-RED, and the long-run RED ratio shall be approximately 0 (since arrival rate < PIR).

**S-3.3** [↪ I-2.2] Given a `TrTcmMeter` with CIR=500 kbps, PIR=1 Mbps, fed CBR at 1.5 Mbps, the GREEN ratio shall equal 0.333 ± 0.02 and the RED ratio shall equal 0.333 ± 0.02.

**S-3.4** [↪ I-2.2] When `pBucket < packet_size`, the packet shall be marked RED regardless of `cBucket`.

**S-3.5** [↪ I-2.2] When `cBucket < packet_size <= pBucket`, the packet shall be marked YELLOW and pBucket shall be decremented by packet_size while cBucket shall be unchanged.

**S-3.6** [↪ I-2.2] When `cBucket >= packet_size`, both buckets shall be decremented by packet_size and the packet marked GREEN.

## S-4: TSW2CM and TSW3CM

**S-4.1** [↪ I-2.4] After feeding CBR traffic at rate `R` for at least `10 * winLen`, the EWMA `avgRate` shall be within 5% of `R`.

**S-4.2** [↪ I-2.4] Given a `Tsw2cmMeter` with CIR=1 Mbps and `winLen=1s`, fed CBR at 500 kbps, the long-run GREEN ratio shall equal 1.0.

**S-4.3** [↪ I-2.4] Given a `Tsw2cmMeter` with CIR=1 Mbps, fed CBR at 2 Mbps, the long-run GREEN ratio shall equal CIR/avgRate = 0.5 ± 0.05.

## S-5: Round Robin family

**S-5.1** [↪ I-3.1] `DsRoundRobinScheduler` with N backlogged equal-priority queues shall deliver each queue exactly 1/N of the link capacity in steady state.

**S-5.2** [↪ I-3.2] `DsWeightedRoundRobinScheduler` with weights `w_1, ..., w_N` shall deliver queue `i` a share of `w_i / sum(w)` of link capacity ± 1 max packet size per round.

**S-5.3** [↪ I-3.3] `DsWeightedInterleavedRoundRobinScheduler` with weights `w_1, ..., w_N` shall deliver the same long-run share as WRR but with bounded burstiness — no flow shall be served more than `ceil(w_i / gcd(w))` consecutive packets.

## S-6: Priority Queue

**S-6.1** [↪ I-3.4] `DsPriorityScheduler` shall always serve the highest-priority non-empty queue.

**S-6.2** [↪ I-3.4] When a higher-priority queue is rate-capped at `R`, lower-priority queues shall receive the remaining capacity `C - R` in steady state.

**S-6.3** [↪ I-3.4] An EF queue (priority 0) under offered load `0.3 * C` with cap `0.5 * C` shall deliver all 0.3C and leave 0.7C for lower queues.

## S-7: Weighted Fair Queueing (WFQ)

**S-7.1** [↪ I-3.5] Given N backlogged flows with weights `w_1, ..., w_N` over a link of capacity `C`, each flow `i` shall receive throughput `w_i / sum(w) * C` within ± `MTU/T` over any window of duration `T >= N * MTU * 8 / C`.

**S-7.2** [↪ I-3.5] When a flow becomes idle and then resumes, its first packet shall be served within one max-packet-time of resumption (no penalty for idleness).

**S-7.3** [↪ I-3.5] The virtual time `v_time` shall be monotonically non-decreasing.

**S-7.4** [↪ I-3.5] The scheduler shall not serve packets out of order within a single flow.

## S-8: WF2Q+

**S-8.1** [↪ I-3.6] All assertions S-7.1–S-7.4 apply to `DsWf2qPlusScheduler`.

**S-8.2** [↪ I-3.6] **Eligibility property**: at any service instant, the scheduler shall only consider packets whose virtual start time `S` satisfies `S <= V` (the current virtual time).

**S-8.3** [↪ I-3.6] The worst-case service delay for a flow with weight `w_i` and rate `r_i = w_i/sum(w) * C` shall be bounded by `(L_max,i / r_i) + (L_max / C)` where `L_max,i` is the maximum packet size of flow `i` and `L_max` is the maximum across all flows.

## S-9: SCFQ

**S-9.1** [↪ I-3.7] `DsScfqScheduler` shall use the finish time of the packet currently in service as virtual time, not a separate GPS computation.

**S-9.2** [↪ I-3.7] Long-run throughput shares shall match WFQ within ± `MTU/T` for any time window `T`.

**S-9.3** [↪ I-3.7] Packet labels shall be monotonically non-decreasing within a single flow.

## S-10: SFQ (Start-time Fair Queueing)

**S-10.1** [↪ I-3.8] Each packet shall receive a start tag and a finish tag based on the previous packet on its flow.

**S-10.2** [↪ I-3.8] Packets shall be served in increasing order of start tag.

**S-10.3** [↪ I-3.8] Long-run throughput shares shall match WFQ within ± `MTU/T`.

## S-11: LLQ

**S-11.1** [↪ I-3.9] `DsLlqScheduler` configured with one priority queue and a WFQ component shall serve the priority queue strictly first when non-empty.

**S-11.2** [↪ I-3.9] When the priority queue is idle, the WFQ component shall manage the remaining traffic according to S-7.1.

**S-11.3** [↪ I-3.9] The priority queue's rate cap shall be respected — if cap=R is set and offered load > R, only R bandwidth shall be granted to the priority queue.

## S-12: DS-RED queue with virtual queues

**S-12.1** [↪ I-4.3] `DsRedSubQueue` with two drop precedence levels shall drop higher-precedence packets at a higher RED probability than lower-precedence packets at the same average queue length.

**S-12.2** [↪ I-4.3] When the physical buffer is full, all incoming packets shall be dropped regardless of drop precedence (tail drop on overflow).

**S-12.3** [↪ I-4.3] The RED average queue length shall use the same EWMA formula as standard ns-3 RED, with `MEAN_PKT_SIZE = 1000` for the initial implementation.

## S-13: Edge router

**S-13.1** [↪ I-1.5] After a packet has been enqueued and dequeued by the edge router, its IPv4 DSCP field shall match the result of `DiffServPolicyClassifier::ApplyPolicy()`. Internally, the DSCP is stored in a `DiffServDscpTag` at enqueue time and applied to the IPv4 header at dequeue time.

**S-13.2** [↪ I-1.6] A packet not matching any mark rule shall enter the queue with its original DSCP unchanged.

**S-13.3** [↪ I-1.1, I-1.2] Mark rules with specific source or destination addresses shall match only packets with those exact addresses; `ANY_HOST` shall match all.

## S-14: PHB structure

**S-14.1** [↪ I-4.4] An EF configuration (DSCP 46, priority queue, rate cap) shall enforce zero queueing delay for in-profile EF traffic up to the cap.

**S-14.2** [↪ I-4.5] An AF configuration with three drop precedence levels shall apply progressively higher drop probabilities to AFx2 and AFx3 than to AFx1 at any given queue length.

**S-14.3** [↪ I-4.6] A best-effort configuration (DSCP 0) shall use the lowest-priority queue.

## S-15: Statistics collection

**S-15.1** [↪ I-5.6] After running a scenario for time `T`, the average queue length reported per queue shall match a manual integration of trace-source samples within ± 1 packet.

**S-15.2** [↪ I-5.11] After a scenario, `received + dropped = transmitted_by_sender` per DSCP for any closed system.

**S-15.3** [↪ I-5.12] Drop counts attributed to RIO and to buffer overflow shall sum to total drops per DSCP.

## S-16: Helper class

**S-16.1** [↪ I-6.1] A scenario built using only `DiffServHelper` and standard ns-3 helpers shall reproduce example-1 results within the tolerance defined in Q-1.

**S-16.2** [↪ I-6.2] All meter parameters (CIR, CBS, EBS, PIR, PBS) shall be settable and gettable via `Config::Set` and `Config::Get` paths.

## S-17: CAKE substrate (slot dispatcher, tin shaping, hybrid LLQ, host isolation)

The CAKE substrate introduces a pluggable across-slot dispatcher abstraction (`DsSlotDispatcher`) with three concrete strategies (strict-priority, tin-shaping DRR, hybrid LLQ-across-tins), a per-tin token-bucket gate for hard rate caps, and a host-pair-isolated nested-FQ wrapper. The S-17 assertions pin the substrate's behavioural contracts; Q-15 (in `03-quality.md`) pins the end-to-end paper-replication gates.

**Strategy applicability.** The S-17.x specifications below are authored against the `DispatchStrategy::NestedDrrPerHost` host-isolation model (the default; see ADR-0095). They apply natively when that strategy is selected. Under `DispatchStrategy::FlatDrrIlog2HostLoad` the per-bucket inner-disc abstraction is replaced by per-flow Cobalt + ilog2(host_load) quantum scaling; the same external invariants are re-met via a different internal structure. Where a fixture's assertion is testable only under one strategy (notably the ack-filter composition fixtures that require `DynamicCast<FqCobaltQueueDisc>` on the inner queue), the fixture explicitly constructs the queue with `Strategy=NestedDrrPerHost` and the body comment names the architectural dependency.

### S-17.1–4: DsSlotDispatcher contract and tin-shaping DRR

**S-17.1** [↪ I-7.1] With the strict-priority dispatcher, `DsSlotDispatcher::SelectDequeueSlot` shall return the same slot index, in the same byte sequence, as the legacy strict-priority `DiffServEdgeQueueDisc::DoDequeue` path it replaced (byte-identity gate).

**S-17.2** [↪ I-7.1] `DsTinShaperDispatcher` with equal quanta and exact-multiple head sizes shall serve at most one consecutive packet from the same slot (DRR fairness invariant).

**S-17.3** [↪ I-7.1] `DsTinShaperDispatcher::SelectDequeueSlot` shall skip empty slots without consuming deficit; subsequent dispatch sequence shall be byte-identical to the same fixture run with the empty slot omitted.

**S-17.4** [↪ I-7.1] Under mixed-load with three or more saturating slots and equal quanta, the served-byte ratio across slots shall remain within ±2 % of the configured-quantum ratio over a 100-packet window.

### S-17.5–8: DsCakeHelper composition

**S-17.5** [↪ I-7.2] `DsCakeHelper::SetAsCakeDiffserv4` shall produce the Linux `sch_cake` `diffserv4` DSCP-to-tin map verbatim (Bulk/BE/Video/Voice with the documented DSCP set per tin). Reference: `sch_cake.c @ 67dc6c56b871` (provenance/linux-sch-cake-67dc6c56b871/sch_cake.c).

**S-17.6** [↪ I-7.2] `DsCakeHelper::SetAsCakeDiffserv3` shall produce the Linux `sch_cake` `diffserv3` map verbatim. Reference: `sch_cake.c @ 67dc6c56b871` (provenance/linux-sch-cake-67dc6c56b871/sch_cake.c).

**S-17.7** [↪ I-7.2] `DsCakeHelper::SetAsCakeDiffserv8` shall produce the Linux `sch_cake` `diffserv8` map verbatim. Reference: `sch_cake.c @ 67dc6c56b871` (provenance/linux-sch-cake-67dc6c56b871/sch_cake.c).

**S-17.8** [↪ I-7.2] A single-tin CAKE composition shall pass packets end-to-end through the helper-built `DiffServEdgeQueueDisc` + tin-shaper + inner `DsCakeFlowFilterFqCobalt` chain with byte counts conserved (enqueued = dequeued + dropped).

### S-17.9: ACK-filter API contract (v1)

**S-17.9** [↪ I-7.2] `DsCakeFlowFilterFqCobalt::EnableAckFilter` shall be settable and readable via `Config::Set` and `Config::Get`. Toggling the attribute shall not alter observed dequeue behaviour at v1 (functional implementation deferred to v1.1 alongside the upstream MR adding the equivalent attribute to mainline `FqCobaltQueueDisc`). S-17.10 (functional ordering preservation) is reserved for v1.1.

### S-17.11–14: Hybrid LLQ-across-tins dispatcher

**S-17.11** [↪ I-7.1] When both an SP-designated slot and one or more DRR slots are non-empty, `DsHybridLlqDispatcher::SelectDequeueSlot` shall return the SP slot until it drains.

**S-17.12** [↪ I-7.1] When the SP-designated slot is empty, the dispatcher's selection sequence shall be byte-identical to the pure `DsTinShaperDispatcher` over the same fixture.

**S-17.13** [↪ I-7.1] `DsHybridLlqDispatcher::PeekSlot` shall be side-effect-free across both the SP fast path and the DRR fall-through (two consecutive peeks return the same slot/packet; subsequent dequeue returns that same packet).

**S-17.14** [↪ I-7.1] `DsHybridLlqDispatcher::OnDequeue` shall account deficit only for DRR slots; SP-slot dequeues shall not advance any DRR cursor.

### S-17.15–18, 20: Per-tin TBF rate-cap dispatcher

**S-17.15** [↪ I-7.1] `DsTinTokenBucket` unit math: `HasTokensFor` returns true unconditionally when `rateBps == 0`; `Configure` resets the bucket to a full state (`tokensBytes == burstBytes`); after `Charge(N)`, `HasTokensFor(N)` is false until `N / rateBps` time has elapsed; `Refill` caps at `burstBytes` (no over-fill on long idle).

**S-17.16** [↪ I-7.1] `DsTinShaperDispatcher` with a per-slot rate cap configured shall serve no more than `cap × duration ± 5 %` bytes from that slot over a saturating-input window.

**S-17.17** [↪ I-7.1] When one slot is capped and another is idle, the capped slot's served bytes shall remain within its own cap regardless of idle-slot capacity (no-save-up / no-borrow-from-idle invariant — distinguishes CAKE's `diffserv4` from plain DRR).

**S-17.18** [↪ I-7.1] `DsTinShaperDispatcher::PeekSlot` under an active rate cap shall be side-effect-free: two consecutive peeks return the same packet; subsequent dequeue returns that same packet; no tokens consumed and no DRR cursor advanced.

**S-17.20** [↪ I-7.1] LLQ + tin-shaping composition (Cisco MQC LLQ pattern: SP fast path with hard-cap on the priority slot) shall (a) hold the EF slot to its configured cap under saturating EF input, and (b) deliver the EF remainder to BE without DRR starvation.

*Note:* S-17.19 is intentionally reserved for the v1.1 ACK-filter functional gate.

### S-17.21–23: Host-pair isolation

**S-17.21** [↪ I-7.1] With `enableHostIsolation=false`, `DsHostIsolatedFqCobalt` shall produce a byte-identical dequeue sequence to a stock `FqCobaltQueueDisc` over the same input fixture.

**S-17.22** [↪ I-7.1] With `enableHostIsolation=true`, distinct `(srcIp, dstIp)` host-pairs shall be allocated to distinct nested-FQ buckets; observable through per-bucket trace inspection.

**S-17.23** [↪ I-7.1] When the active host-pair count exceeds `MaxHostPairs`, the least-recently-used host-pair bucket shall be drained-and-recycled (not released-and-reallocated), preserving the inner `FqCobaltQueueDisc`'s `QueueDiscClass` attachment (the `queue-disc.cc:73` re-attachment invariant: a `QueueDiscClass` cannot be detached from one parent and re-attached to another).

### S-17.28: Egress DSCP wash

**S-17.28** [↪ I-7.2] `DiffServEdgeQueueDisc::Wash` is a boolean attribute (default `false`) that, when `true`, zeros the DSCP bits (the high six bits of the IPv4 TOS byte) on every dequeued `Ipv4QueueDiscItem` while preserving the low two ECN bits. With `Wash=false`, the high six bits of TOS on a dequeued item shall match the per-packet DSCP tag set during enqueue (the existing rewrite contract). Mirrors Linux `tc-cake wash` semantics: classification stays in effect inside the qdisc; the egress packet leaves with the IP-precedence/DSCP byte cleared so downstream forwarders see CS0/Default.

### S-17.29: CAKE per-tin `MemLimit` API surface

**S-17.29** [↪ I-7.2] Mainline `FqCobaltQueueDisc::MemLimit` is a uinteger attribute (default `0`) that round-trips through `Config::Set` / `Config::Get` and through the typed `SetMemLimit` / `GetMemLimit` accessors. Mirrors the Linux `tc-cake memlimit BYTES` API surface. With the sentinel default value `0` the gate is disabled and the queue disc is byte-identical to a fresh mainline `FqCobaltQueueDisc`. The packet-count `MaxSize` attribute (default `10240p`) is independent and unaffected by `MemLimit`. The functional byte-counted enqueue gate is asserted by the upstream `FqCobaltQueueDiscMemLimit` test case in the `fq-cobalt-queue-disc` suite — that gate now lives in mainline `FqCobaltQueueDisc` per `patches/ns3/0006-fq-cobalt-ack-filter-memlimit.patch` (filed upstream; pin advances and patch retires once merged). See ADR-0069.

### S-17.30: CAKE link-layer overhead — statistical rate adjustment (v1)

**S-17.30** [↪ I-2] `DsCakeHelper::ConfigureLinkLayerOverhead(edge, overhead, atm, mpu)` invoked after a `SetAsCake*` profile composed with `useInnerTbfShaping=true` shall downscale every per-tin inner `TbfQueueDisc::Rate` by `gamma = E[wire_bytes(s)] / E[s]` over the bimodal Internet mix `{(64B, 0.5), (1500B, 0.5)}`, where `wire_bytes(s) = max(s + overhead, mpu)` for `atm=false` and `wire_bytes(s) = max(ceil((s + overhead) / 48) * 53, mpu)` for `atm=true`. This mirrors the per-packet adjustment performed by Linux `cake_overhead()` in `sch_cake.c` at commit `16d7fed7` (the CAKE-paper reference deposit; Hoiland-Jorgensen et al. 2018, Zenodo `10.5281/zenodo.1226887`). The post-adjustment per-tin rate must equal `share × totalRate / gamma` within ±0.5 % relative error. The v1 contract is statistical-mode — accurate in steady state for the bimodal mix; per-packet wire-byte adjustment at TBF dequeue is v1.1 follow-up.

### S-17.31: CAKE raw mode — suppress overhead correction

**S-17.31** [↪ I-2] `DsCakeHelper::ConfigureLinkLayerOverhead(edge, overhead, atm, mpu, raw=true)` shall short-circuit before any rate-adjustment pass and leave every per-tin inner `TbfQueueDisc::Rate` exactly equal to its post-`SetAsCake*` value, regardless of the `overhead`, `atm`, or `mpu` arguments. Mirrors the Linux `tc-cake raw` flag, which interprets the configured `bandwidth` as the raw IP-layer rate and disables PHY-framing correction. The `raw` argument has a default value of `false`, preserving call-site source compatibility for the four-argument form introduced by S-17.30.

### S-17.32: CAKE per-tin diagnostics — `GetTinStats(slot)` (v1)

**S-17.32** [↪ I-7] `DiffServEdgeQueueDisc::GetTinStats(slot)` shall return a `DsTinStats` snapshot of per-tin counters: `bytesEnqueued`, `bytesDequeued`, `drops`, `marks`. Wire-byte fields are dispatcher-tracked (`DsTinShaperDispatcher` / `DsHybridLlqDispatcher` increment in `OnEnqueue` / `OnDequeue` using `QueueDiscItem::GetSize()`); drops / marks are read-side from the inner `QueueDisc::Stats` at call time (`nTotalDroppedPackets` / `nTotalMarkedPackets`). The default `DsStrictPriorityDispatcher` returns a zeroed snapshot — non-CAKE compositions are unaffected. Out-of-range slot indices yield a zeroed snapshot rather than aborting. v1 contract surfaces the four counters above; sparse-flow count (Linux `flows_used`) is v1.1, gated on a `DsCakeFlowFilterFqCobalt::GetNFlows()` accessor that does not yet exist. Mirrors the Linux `tc -s qdisc show` per-tin reporting that operators rely on for CAKE health checks.

### S-17.33: Set-associative flow hash — structural-equivalence audit (mainline ns-3)

**S-17.33** [↪ I-7.2] Mainline ns-3 `FqCobaltQueueDisc::EnableSetAssociativeHash` (with `SetWays = 8`) shall realise the 8-way set-associative flow-bucket lookup specified by Hoiland-Joergensen et al. 2018 §3 and implemented in Linux `sch_cake.c` at the CAKE-paper deposit commit `16d7fed7`. Structural equivalence of the lookup logic (modulo-flows decomposition into outer / inner indices; linear probe within each set; tag-match-or-reuse acceptance) is recorded in ADR-0066. The underlying 5-tuple → 32-bit hash function diverges at the bit level (Linux `jhash_3words` vs ns-3 `Ipv4QueueDiscItem::Hash` over the platform `Hasher`, default Murmur3); the divergence is design-neutral with respect to set-associativity and is accepted permanently. The fixture `SetAssociativeHashStructuralPropertiesTest` shall verify (a) the `EnableSetAssociativeHash` and `SetWays` attributes are settable and readable; (b) same-flow → same-bucket determinism (tag-preservation invariant: a second arrival of an already-seen 5-tuple lands in the same queue index it landed in on the first arrival) under both modes; and (c) under default sizing (`m_flows = 1024`, `SetWays = 8`) and a small-cardinality distinct-flow input, distinct flows are spread across distinct queue indices in both modes. Byte-exact equivalence to Linux is **not** asserted (and is not achievable under the divergent hash function). The substrate claim is satisfied at the algorithmic-design level.

### S-17.35: Host-isolation Srchost mode — src-only bucket key

**S-17.35** [↪ I-7.2] `DsHostIsolatedFqCobalt::SetMode(HostIsolationMode::Srchost)` shall derive the outer-DRR bucket key from the source IP only — flows from the same source IP to distinct destination IPs shall collapse into a single host-pair bucket. Mirrors the Linux `tc-cake srchost` semantic as defined by `cake_flow_names[CAKE_FLOW_SRC_IP]` in iproute2 `q_cake.c @ 87c66f79d8b0` (line 52) and the `tc-cake(8) @ 87c66f79d8b0` man page's FLOW ISOLATION PARAMETERS section (line 460), both frozen under `provenance/linux-iproute2-87c66f79d8b0/` per ADR-0093. The asymmetric per-source quantum modulation that distinguishes `srchost` from `dual-srchost` in `cake_get_flow_quantum` is deferred to a later v1.x release per ADR-0065's "Read for shape; derive ns-3-native" rule for stateful algorithms; in v1.1 `DualSrchost` therefore aliases `Srchost` (asserted by S-17.38).

### S-17.36: Host-isolation Dsthost mode — dst-only bucket key

**S-17.36** [↪ I-7.2] `DsHostIsolatedFqCobalt::SetMode(HostIsolationMode::Dsthost)` shall derive the outer-DRR bucket key from the destination IP only — flows from distinct source IPs to the same destination IP shall collapse into a single host-pair bucket. Mirrors the Linux `tc-cake dsthost` semantic. `HostIsolationMode::Hosts` is the Linux alias of `Dsthost` and shall produce identical behaviour. `DualDsthost` aliases `Dsthost` in v1.1 for the same reason given in S-17.35.

### S-17.37: Host-isolation Flowblind / Flows modes — single-bucket with divergent inner type

**S-17.37** [↪ I-7.2] `DsHostIsolatedFqCobalt::SetMode(HostIsolationMode::Flowblind)` and `SetMode(HostIsolationMode::Flows)` shall both collapse every arrival to a single host-pair bucket regardless of `(srcIP, dstIP)`. The two modes diverge on inner-queue-disc type: under `Flowblind` the bucket's inner shall be a stock ns-3 `CobaltQueueDisc` (single FIFO with Cobalt AQM, no flow hashing — Linux `CAKE_FLOW_NONE`); under `Flows` the bucket's inner shall be an `FqCobaltQueueDisc` retaining 5-tuple flow fairness without host isolation (Linux `CAKE_FLOW_FLOWS`). The single-bucket invariant — `GetActiveHostPairs() == 1` after enqueuing arrivals from any number of distinct `(srcIP, dstIP)` pairs — holds for both modes.

### S-17.38: Host-isolation mode attribute — round-trip and Dual-mode aliasing

**S-17.38** [↪ I-7.2] The `Mode` attribute on `DsHostIsolatedFqCobalt` shall round-trip every value of `HostIsolationMode` (eight enum values: `Triple`, `Srchost`, `Dsthost`, `Hosts`, `Flowblind`, `Flows`, `DualSrchost`, `DualDsthost`) via `Config::Set` / `Config::Get` and via the typed `SetMode` / `GetMode` accessors. The mode names mirror iproute2's `cake_flow_names[]` table (iproute2 `q_cake.c @ 87c66f79d8b0`, lines 51-58) and the `tc-cake(8) @ 87c66f79d8b0` man page's FLOW ISOLATION PARAMETERS section (line 460), both frozen under `provenance/linux-iproute2-87c66f79d8b0/` per ADR-0093. The default value shall be `HostIsolationMode::Triple`. `DualSrchost` shall alias `Srchost` and `DualDsthost` shall alias `Dsthost` — distinguished only by bucket-key derivation, not by quantum modulation. The Linux `cake_get_flow_quantum` per-host modulation realised in the kernel `sch_cake` flat-DRR architecture is a separate substrate investigation deferred pending architectural design work; an earlier outer-DRR quantum scaling hypothesis (divide-by-flowsEverSeen branch) was empirically falsified.

### S-17.47: Per-flow counter accessor on the tin-shaping dispatcher

**S-17.47** [↪ I-7.2] After enqueuing M packets across N distinct flows (5-tuple) into a single tin slot s, `DsTinShaperDispatcher::GetPerFlowStats(s, f, edge)` for every active flow `f` (where `f` is the flow's index in the inner queue disc's `QueueDiscClass` list) shall return:

- `bytesEnqueued` matching the cumulative wire-byte count admitted to that flow's per-flow queue.
- `pktsEnqueued` matching the count of packets enqueued to that flow.
- `bytesRemaining` matching the live backlog (in bytes) of that flow's per-flow queue at call time.
- `pktsDropped` and `pktsMarked` reflecting the inner per-flow `QueueDisc::Stats::nTotalDroppedPackets` / `nTotalMarkedPackets`.

Aggregated across all `f` in `[0, inner->GetNQueueDiscClasses())`, the totals shall match the sum of inner per-flow `nTotalEnqueuedBytes`. Out-of-range slot, slot whose inner is non-FQ (no `QueueDiscClass` list), or out-of-range `f` shall return a zero-initialized `DsPerFlowStats` rather than aborting.

### S-17.48: Per-host counter accessor on the host-isolated wrapper

**S-17.48** [↪ I-7.2] After enqueuing M packets across N distinct flows from host pair `(src, dst)` to a `DsHostIsolatedFqCobalt` instance in `HostIsolationMode::Triple`, `GetPerHostStats(src, dst)` shall return:

- `bytesTotal` matching the bucket's inner `QueueDisc::GetStats().nTotalEnqueuedBytes`.
- `flowsActive` matching the bucket's inner `GetNQueueDiscClasses()` (the count of distinct flows ever observed in the bucket; this carries the documented substrate-fidelity gap with Linux's live `bulk_flow_count`).
- `bytesPerFlowAvg` equal to `bytesTotal / max(flowsActive, 1)` (truncating integer division).

A query for a host pair with no live bucket shall return a zero-initialized `DsPerHostStats` rather than aborting.

### S-17.45: Q-15.10 RRUL latency fixture — substrate-replicated cake-rrul scenario

**S-17.45** [↪ I-7.2] An EXTENSIVE-tier ns-3 test fixture in `src/ns-3/test/diffserv-cake-q15-test-suite.cc` shall replicate the cake-rrul scenario (50 Mbit/s bottleneck, 80 ms base RTT, 4 saturating TCP up + 4 saturating TCP down + 3 EF UDP probes at 200 ms cadence, 60 s duration, DsCakeHelper RateBased shaper) in-process and assert the EF-probe p99 OWD over the [10 s, 60 s] measurement window stays within the gate documented in Q-15.10. The paper threshold is locked in `kQ15_10_RrulFig9P99LatencyCeilingMs`; the runtime assertion may track an empirical calibration band when the in-process scenario diverges from paper, provided the gap is documented in the fixture's Doxygen block per the Q-15.2 / Q-15.7 calibration pattern.

### S-17.46: Q-15.11 UDP cross-traffic isolation fixture — Voice-vs-BE isolation ratio

**S-17.46** [↪ I-7.2] An EXTENSIVE-tier ns-3 test fixture in `src/ns-3/test/diffserv-cake-q15-test-suite.cc` shall offer a saturating UDP CBR flow on the Best-Effort tin (DSCP 0) alongside a saturating TCP flow plus three EF UDP probes on the Voice tin (DSCP 46) over a single 50 Mbit/s / 80 ms bottleneck for 60 s, and assert the isolation ratio (UDP-tin achieved throughput in Mbit/s, divided by EF-probe OWD jitter in milliseconds, jitter = `p99(OWD) − min(OWD)`) strictly greater than `kQ15_11_IsolationRatioMbpsPerMs` (= 5.0). Sanity preconditions: at least 100 EF probe samples in the measurement window; UDP achieved throughput at least 5 Mbit/s.

### S-17.49: CAKE conservative preset — defensive overhead defaults

**S-17.49** [↪ I-2] `DsCakeHelper::SetAsCakeConservative(edge)` invoked after a `SetAsCake*` profile composed with `useInnerTbfShaping=true` shall apply the Linux `tc-cake(8)` `conservative` preset: per-packet overhead 48 bytes, minimum-packet-unit floor 64 bytes, ATM cell framing disabled (`atm=false`). The call shall be observably equivalent to `ConfigureLinkLayerOverhead(edge, 48, false, 64, false)` and shall trigger the same `gamma`-downscale of every per-tin TBF rate as S-17.30, with the bimodal-mix `gamma = E[wire_bytes(s)]/E[s]` evaluated at `(overhead=48, atm=false, mpu=64)` over `{(64B, 0.5), (1500B, 0.5)}`. The post-call relative error of every per-tin `TbfQueueDisc::Rate` against the closed-form expected rate `share × totalRate / gamma` shall be within ±0.5 %.

### S-17.50: Host-isolation Flowblind mode — inner is `CobaltQueueDisc`, no per-flow classes

**S-17.50** [↪ I-7.2] Under `HostIsolationMode::Flowblind`, after enqueuing arrivals from at least four distinct `(srcIP, dstIP)` pairs, the single host-pair bucket's inner queue disc shall satisfy: `DynamicCast<CobaltQueueDisc>(inner)` non-null; `DynamicCast<FqCobaltQueueDisc>(inner)` null; `inner->GetNQueueDiscClasses() == 0`; and `inner->GetNPackets()` equal to the total number of packets admitted regardless of 5-tuple. Together with S-17.37 this realises the Linux `tc-cake flowblind` (`CAKE_FLOW_NONE`) semantic — the substrate disables per-flow fairness within the single bucket, so all 5-tuples share one FIFO with Cobalt AQM.

### S-17.51: CAKE diagnostic text dump — `tc -s qdisc show cake` mirror

**S-17.51** [↪ I-7] `DsCakeHelper::PrintTcStats(os, edge)` (delegating to `DsCakeStatsFormatter::Print`) shall write to @p os a human-readable diagnostic dump for a CAKE-composed `DiffServEdgeQueueDisc` whose section ordering and section-key vocabulary mirrors iproute2 `q_cake.c @ 87c66f79d8b0::cake_print_xstats` (line 617), frozen under `provenance/linux-iproute2-87c66f79d8b0/` per ADR-0093. The output shall contain, at a minimum: a header line beginning with `qdisc cake` and including the `tins N` token; an aggregate `Sent` line with `bytes`, `pkt`, `dropped`, `requeues` keys; an aggregate `backlog` line; one per-tin block per populated inner slot containing the keys `tin <i> kind=`, `thresh`, `bytes_enqueued`, `bytes_dequeued`, `drops`, `marks`, `ever_seen`, and `backlog`. The `ever_seen` field deliberately differs from the Linux name `bulk_flow_count`: stock ns-3 `FqCobaltQueueDisc` exposes only an append-only class list (`GetNQueueDiscClasses()`), so the substrate cannot honestly report the live `bulk_flow_count` Linux's `sch_cake` tracks. The discrepancy is documented in the per-tin output line itself and in the formatter's class Doxygen. The output is structural, not byte-exact: future iproute2 cosmetic changes (whitespace, decimal precision, label re-ordering within a section) shall not regress the fixture. A null edge pointer shall produce a single-line diagnostic of the form `qdisc cake (null)` and return without aborting.

### S-17.52: Path-α/β/γ shaper comparison panel

**S-17.52** [↪ I-7] An EXTENSIVE-tier ns-3 fixture in `src/ns-3/test/diffserv-cake-q15-test-suite.cc` shall drive a single 4-tin TCP saturation scenario (the existing `Q15Scenario6Run` helper, 1 Gbps P2P, 100 Mbit/s aggregate cap, 4 long-lived BulkSend flows over 30 s) through all three `DsCakeHelper::ShaperMode` paths — α (`TokenBucket`, default in-dispatcher token-bucket gate; the helper composes this with `enableTinShaping=false` so neither per-tin TBF caps nor per-tin token-bucket gates are wired in), β (`RateBased`, virtual-clock per-tin shaper plus global clock), γ (`TbfInner`, mainline `TbfQueueDisc` as per-tin inner via `patches/ns3/0004`) — and characterise the path-choice landscape via aggregate-throughput ratios. The fixture shall assert: (a) `|β/γ − 1| ≤ 0.02` (β tracks γ within ±2 % under the symmetric regime — restated for completeness, identical to S-17.41); (b) `α/γ > 1.5` AND `α/β > 1.5` (α materially diverges from β/γ by more than 1.5× under the default helper composition because `enableTinShaping=false` lets traffic through at link rate rather than enforcing the 100 Mbit/s aggregate cap — this is the reviewer-defensive "when does path choice matter?" characterisation, demonstrating that α under the default helper is NOT a drop-in cap-enforcing replacement for β/γ); (c) the S-17.44 bound restated within the panel: under `RateBased` (β) the aggregate egress saturates at the cap (`< 102 Mbit/s`, `> 95 Mbit/s`). Per-tin gating for α (`enableTinShaping=true`, in-dispatcher `TinTokenBucket` caps) requires either a helper-API extension or a fixture that calls `SetAsCakeDiffserv4` directly with `enableTinShaping=true`; both are deferred and documented inline in the fixture's Doxygen block per the calibration discipline established by S-17.45 / Q-15.2.

### S-17.53: CAKE `autorate-ingress` API contract — skeleton

**S-17.53** [↪ I-7] `DsCakeHelper::SetEnableAutorateIngress(bool)` and the paired `GetEnableAutorateIngress() const` accessor shall implement the Linux `tc-cake(8)` `autorate-ingress` flag at API level, mirroring the flag's authoritative definition in the `tc-cake(8) @ 87c66f79d8b0` man page's OTHER PARAMETERS section (line 600) and the parser entry in iproute2 `q_cake.c @ 87c66f79d8b0::cake_parse_opt` (line 91), both frozen under `provenance/linux-iproute2-87c66f79d8b0/` per ADR-0093. The contract is structural and asserted by a QUICK-tier ns-3 fixture in `src/ns-3/test/diffserv-test-suite.cc` named `CakeAutorateIngressApiContractTest`. After invoking `SetEnableAutorateIngress(true)`: (a) `GetEnableAutorateIngress()` returns `true`; (b) `GetAutorateIngressHook()` returns a non-null pointer to a `DsCakeAutorateIngressHook` whose `ComputeRateDelta(N)` returns `0` for at least three values spanning `{1 000 000, 100 000 000, 1 000 000 000}` bps; (c) under a deterministic 4-tin diffserv4 scenario driving a fixed packet sequence through a CAKE-composed `DiffServEdgeQueueDisc`, the per-packet enqueue and dequeue counts captured with the flag enabled (no-op hook installed) shall equal those captured with the flag disabled (the byte-identity contract). After invoking `SetEnableAutorateIngress(false)`: `GetEnableAutorateIngress()` returns `false` and `GetAutorateIngressHook()` returns `nullptr`. The closed-loop RTT-trend tracker plus hysteresis logic that Linux `sch_cake.c::cake_calc_rate_estimator` implements is **deferred to v2** per ADR-0071 and is not asserted by S-17.53; the v2 work item replaces the no-op `DsCakeAutorateIngressHook` with an inference subclass and wires `ComputeRateDelta` into the rate-update sites of paths α / β / γ without an API redesign.

### S-17.54: `DsCakeHelper::SetAsCakeAlphaTinShaped` enables α tin-shaping

**S-17.54** [↪ I-7] `DsCakeHelper::SetAsCakeAlphaTinShaped(edge, totalRate, ...)` shall compose path α (in-dispatcher TokenBucket across tins) with per-tin caps enabled (`enableTinShaping=true`) while preserving the TokenBucket dispatcher choice (`useInnerTbfShaping=false`). The contract is asserted by an EXTENSIVE-tier ns-3 fixture in `src/ns-3/test/diffserv-cake-q15-test-suite.cc` that drives the 4-tin TCP saturation scenario from S-17.52 (1 Gbps P2P, 100 Mbit/s aggregate cap, 4 long-lived BulkSend flows over 30 s) under three compositions: α with tin-shaping enabled (this preset), β (`RateBased`), γ (`TbfInner`). The fixture shall assert that aggregate goodput under α-with-tin-shaping falls within ±5 % of γ (`|α/γ − 1| ≤ 0.05`) AND within ±5 % of β (`|α/β − 1| ≤ 0.05`) at the 100 Mbit/s cap. This inverts S-17.52's deliberate divergence assertion: with tin-shaping enabled, α joins β and γ in cap-enforcement; without it (the default helper composition asserted by S-17.52), α diverges by more than 1.5×. The ±5 % tolerance reflects measured steady-state deviations of < 1 % for α/β and α/γ pairs; the bound provides regression headroom without admitting silent loss of the cap-enforcing equivalence.

### S-17.55: cake-host-fairness-sweep probe reproduces 4-vs-1 nested-DRR CUBIC baseline

**S-17.55** [↪ I-7.2] An EXTENSIVE-tier ns-3 test fixture in `src/ns-3/test/diffserv-cake-host-fairness-smoke-test.cc` shall replicate the parameterised host-fairness probe configuration emitted by the `cake-host-fairness-sweep` example (4 bulk TCP CUBIC flows from host A; 1 bulk TCP CUBIC flow from host B; 100 Mbit/s bottleneck at 20 ms one-way delay; 30 s duration; `DsCakeHelper::SetAsCakeDiffserv4` composed with `enableTinShaping=true` and `enableHostIsolation=true`; per-tin inner `DsHostIsolatedFqCobalt` configured with `Mode=Triple` and `Strategy=NestedDrrPerHost`) and assert that `share_A = goodput_A / (goodput_A + goodput_B)` falls within the band `[0.74, 0.78]` over a single `RngRun=1` replica. The band is wider than the measured single-replica goodput share of 0.7604 to absorb single-replica RNG jitter; the multi-replica sweep aggregator uses k=3 replicas and a tighter band. This fixture guards against probe-side regressions on the version-controlled `cake-host-fairness-sweep` example.

## S-flent-sink-host-column-emitted

> **References:** I-10

After a 2-second smoke run of `cake-host-isolation --isolation=triple
--length=2 --output=<temp_dir>`, every `tcp_up_flow*.csv` in
`<temp_dir>` has a 4-column header `t,bytes_delta,goodput_mbps,host`
and ≥1 data row.

## S-flent-sink-host-attribution-correct

> **References:** I-10

After the same 2-second smoke run, every data row of
`tcp_up_flow{0,1,2,3}.csv` has `host == "A"`, and every data row of
`tcp_up_flow4.csv` has `host == "B"`.

## S-flent-sink-backwards-compat-no-hostid

> **References:** I-10

After a 2-second smoke run of any DsFlentCsvSink-using binary that
does NOT pass a hostId (e.g., `cake-tcp-4up-squarewave --length=2
--output=<temp_dir>`), the emitted `tcp_up_flow*.csv` files have the
same 4-column header `t,bytes_delta,goodput_mbps,host` with an empty
trailing `host` field per row.

### S-meter-base-trace-registered

**S-meter-base-trace-registered** [↪ I-8] `Meter::GetTypeId()` shall advertise a trace source named `MeterColour`
with signature `(Colour, uint32_t, Time)` discoverable via
`TypeId::LookupTraceSourceByName`. The signature shall match the
declared `TracedCallback` member precisely.

### S-meter-trace-srtcm

**S-meter-trace-srtcm** [↪ I-8] When a controlled input sequence is fed through a `SrTcmMeter` instance
with known expected colour sequence (per RFC 2697 §2.2), the
`MeterColour` trace shall fire exactly once per input packet, in order,
with the expected `(colour, classId)` pair. `classId` matches the
`PolicyEntry.classId` field of the meter's input.

### S-meter-trace-trtcm

**S-meter-trace-trtcm** [↪ I-8] As S-meter-trace-srtcm, with `TrTcmMeter` and RFC 2698 §2.2 expected
colour sequence.

### S-meter-trace-tsw2cm

**S-meter-trace-tsw2cm** [↪ I-8] As S-meter-trace-srtcm, with `Tsw2cmMeter` and the project's TSW2CM
reference vector (sources: `src/ns-2.29/diffserv/dsPolicy.cc` TSW logic).

### S-meter-trace-tsw3cm

**S-meter-trace-tsw3cm** [↪ I-8] As S-meter-trace-srtcm, with `Tsw3cmMeter` and RFC 2859-style expected
colour sequence.

### S-meter-trace-byteacct

**S-meter-trace-byteacct** [↪ I-8] As S-meter-trace-srtcm, with `ByteAcctMeter`. Expected colour sequence
sourced from the existing byte-acct unit tests.

### S-meter-trace-fw

**S-meter-trace-fw** [↪ I-8] As S-meter-trace-srtcm, with `FwMeter`. classId defaults to 0 if the
framework-meter has no per-class state; verify against actual class
field during execution.

### S-meter-trace-tokenbucket

**S-meter-trace-tokenbucket** [↪ I-8] As S-meter-trace-srtcm, with `TokenBucketMeter`. `classId=0`
unconditionally (single-class meter).

### S-meter-trace-dumb

**S-meter-trace-dumb** [↪ I-8] As S-meter-trace-srtcm, with `DumbMeter`. All emissions are
`Colour::GREEN, classId=0`.

### S-example-1-perclass-owd

**S-example-1-perclass-owd** [↪ I-9] After a 6-second smoke run of `diffserv-example-1`, the run directory
shall contain `OWD-ef.tr` and `OWD-be.tr`. Each file shall be
non-empty, parseable as whitespace-separated `time owd_seconds` rows,
with `time` monotone non-decreasing.

### S-example-1-perclass-flowrate

**S-example-1-perclass-flowrate** [↪ I-9] After a 6-second smoke run, `FlowRate.csv` shall exist with the header
row `time,classId,rate_kbps`, contain ≥10 sample rows, expose two
distinct `classId` values (0 = EF, 1 = BE) at each timestamp, and
report `rate_kbps` values within the plausibility band [0, link
capacity in kbps].

### S-example-1-metercolour-aggregate

**S-example-1-metercolour-aggregate** [↪ I-9] After a 6-second smoke run, `MeterColour.csv` shall exist with header
row `time,classId,green,yellow,red`, contain ≥10 sample rows, and
report non-negative integer counts whose per-class running sum across
windows is consistent (within ±1 packet) with the total ingress packet
count observed at the meter.

## S-l4s-piControl-fires-at-nominal-load (intent: I-12)

After running `diffserv-l4s-s2-equivalence` at its v1.7-tuned defaults for a short
sim (`simTime=3`), the emitted `coupling.csv` shall satisfy:

- `(pPrime > 0).sum() / len(pPrime) > 0.05` — controller fires on a non-trivial sample fraction
- For samples where `pPrime > 0`: the RFC 9332 §4.1 coupling formula holds approximately —
  `abs(pC - (k*pPrime)**2) / max(pC, 1e-6) < 0.10` (within 10% of formula prediction)
  AND `abs(pL - min(2*pPrime, 1)) / max(pL, 1e-6) < 0.10`
  where `k=2` (default coupling factor)

The criterion is **coupling-formula verification**, not throughput-ratio equivalence
(the latter is structurally unobservable with non-responsive UDP CBR flows). At
moderate offered ratio (~1.5× bottleneck), `pPrime` operates in the `[1e-5, 1e-2]`
range where the formula relationship is most clearly demonstrable.

Verified by: `TestS_l4s_piControl_fires_at_nominal_load` in `l4s-scenario-validation-test.cc`.

## S-l4s-s1-latency-arm-differentiation (intent: I-12)

After running `diffserv-l4s-s1-latency --mode=l4s-on --simTime=3` and
`--mode=l4s-off --simTime=3` at default parameters, the EF probe arms shall
satisfy:

- `mean(owd_ef_l4s_on) < 10 ms` (priority routing functional in L4S mode)
- `mean(owd_ef_l4s_off) < 10 ms` (priority routing functional in classic mode)

An earlier draft criterion (15% mean / 20% P95 reduction in AF OWD) was
structurally unachievable because both modes give the EF probe identical priority
access; AF arm differentiation requires Scalable congestion control (e.g.
`TcpDctcp` with `UseEct0=false` as the L4S sender), not UDP CBR.

The relaxed criterion verifies that the scenario's priority wiring is correct in both
modes — a meaningful but weaker assertion than the original. A future cycle with
responsive flows (deferred to v1.8+) can demonstrate the throughput-equivalence
narrative properly.

Verified by: `TestS_l4s_s1_latency_arm_differentiation` in `l4s-scenario-validation-test.cc`.

## S-aqm-envelope-axis-in-mbps (intent: I-13)

Rendering the `aqm-eval-runner` recipe shall produce an SVG whose y-axis tick
labels parse as Mbps (e.g., '5', '10', '15'), not as ECDF [0, 1] values with a
scientific-notation scale factor. The recipe YAML shall declare
`y_unit_convert: bps_to_mbps` and `ylabel: "Aggregate goodput (Mbps)"`.

Verified by: `test_aqm_envelope_axis_in_mbps` in `scripts/test_plot_recipe.py`.

## S-cake-host-asymmetric-capture-bounded (intent: I-11)

Under asymmetric TCP offered load (host A with N_A flows, host B with N_B
flows, N_A > N_B, identical RTT, identical TCP variant) sharing one
besteffort tin via `DsHostIsolatedFqCobalt` configured in any of
{`Triple`, `DualSrchost`, `DualDsthost`} host-isolation modes, the host-A
bucket's byte-share over the steady-state window converges to `[0.70,
0.80]`.

This band is an emergent property of the nested-DRR architecture under
bursty inner TCP — when the lower-flow-count bucket transiently empties
between TCP bursts, the outer DRR cursor returns to the higher-flow-count
bucket. The asymmetry is the documented consequence of nesting per-bucket
FQ inside a per-host outer DRR; it is **not** a mirror of Linux
`sch_cake` semantics. Linux `sch_cake` uses flat per-flow DRR with
`quantum = flow_quantum >> ilog2(host_load)` scaling (see
`cake_get_flow_quantum` @ line 688 in
provenance/linux-sch-cake-67dc6c56b871/sch_cake.c) and has no outer
DRR over per-host buckets; per-host fairness in Linux emerges from the
divisor, not from a nested dispatcher.

**Spec history:**

- v1.10: spec introduced as Triple-mode "sticky-cursor matching Linux
  `sch_cake`" contract. Framing later retracted as unsupported.
- v1.15: framing re-grounded to "emergent property of nested-DRR
  architecture"; scope widened to cover `DualSrchost` / `DualDsthost`
  (behaviourally equivalent to Triple after `EffectiveOuterQuantum`
  divide-branch removal).

## S-cake-host-isolated-ack-filter-per-bucket-conservative (intent: I-11)

With `InnerEnableAckFilter=true` and `InnerEnableAckFilterAggressive=false`
on `DsHostIsolatedFqCobalt`, the ACK filter operates within each
host-pair-bucket: monotonic plain TCP ACKs in the same 5-tuple
collapse to one survivor (newest ACK number); cross-bucket
comparison is structurally moot because 5-tuple identity uniquely
determines both bucket assignment and flow class within the bucket.

For the canonical case of two host-pair-buckets each receiving three
monotonic plain ACKs (Ack#100, #200, #300), each bucket's inner
`FqCobaltQueueDisc` retains exactly one ACK (Ack#=300) with
cumulative drops=2 per bucket; total survivors=2, total drops=4,
all with drop reason `ACK_FILTER_DROP`.

This contract closes the v1.1 deferred-item "ACK filter
× host-isolation composes" for the conservative-mode arm.

Verified by: `DsHostIsolatedFqCobaltAckFilterPerBucketTest` in
`host-isolated-fq-cobalt-ack-filter-test.cc`.

## S-cake-host-isolated-ack-filter-per-bucket-aggressive (intent: I-11)

With `InnerEnableAckFilterAggressive=true` (in addition to
`InnerEnableAckFilter=true`) on `DsHostIsolatedFqCobalt`, SACK-bearing
ACKs in each per-host-pair-bucket become both scan triggers AND drop
targets in the per-bucket ACK filter scan. Mainline
`FqCobaltQueueDisc`'s aggressive-mode semantic is preserved through
the wrapper's attribute propagation pattern.

For the canonical case of a single host-pair-bucket receiving three
ACKs in order — Ack#100 plain, Ack#200 with SACK option, Ack#300
plain — under aggressive mode the SACK ACK becomes a drop target:
Ack#100 is dropped when Ack#200 arrives (older drop target); Ack#200
is then dropped when Ack#300 arrives (older drop target under
aggressive mode, despite SACK bearing). Result: 1 survivor (Ack#300),
2 cumulative drops, all with drop reason `ACK_FILTER_DROP`.

This contract closes the v1.1 deferred-item "ACK filter
× host-isolation composes" for the aggressive-mode arm.

Verified by: `DsHostIsolatedFqCobaltAckFilterAggressiveTest` in
`host-isolated-fq-cobalt-ack-filter-test.cc`.

## How to use this file

Each S-N.M assertion translates to a unit or integration test in the ns-3 test framework. Tests are named `BriefDescriptionTest` (one-line `\brief` and `\see specs/02-structural.md S-N.M` in the class Doxygen block) and located in `src/diffserv/test/`. The CI gate is: all S-tests pass, with statistical assertions allowed up to their declared tolerance.

When implementing a class, the developer (or Claude Code) should look up the S-tests that reference the corresponding I-spec and use them as the implementation target.
