# Reproducibility Guide

This document lists the exact commands to re-run every DiffServ4NS scenario on
every supported simulator version, along with setup prerequisites.

---

## Directory convention

All simulation output is written to:

```
output/<version>/<scenario>/
```

Where `<version>` is one of `ns2-29`, `ns2-35`, `ns3` and `<scenario>` is one
of `example-1`, `example-2`, `example-2-fullscale`, `example-3`, or
`webtraf-ns235-test`.  Previous run contents are cleaned automatically by the
runner script before each new run.

---

## One-time setup

### ns-2.29 setup

```bash
# Fetch ns-allinone-2.29.3, patch with DiffServ4NS, build inside Docker
./scripts/fetch-ns2-allinone.sh
./scripts/patch-ns2-diffserv.sh
./scripts/build-ns2-allinone-docker.sh
```

### ns-2.35 setup

```bash
# Patch and build ns-allinone-2.35 inside Docker
./scripts/patch-ns2-diffserv-235.sh
./scripts/build-ns2-allinone-235-docker.sh
```

The ns-2.35 binary is pre-built at `ns2/ns-allinone-2.35/ns-2.35/ns`.

### ns-3 setup

```bash
# Clone ns-3-dev at the pinned revision, create symlink, configure and build
./scripts/fetch-ns3.sh
cd ns3/ns-3-dev
./ns3 configure --enable-tests --enable-examples
./ns3 build diffserv
```

The pinned ns-3-dev revision is `cc48bf5c15a4918364abc2b2b060b4056dce09a4`
(nearest release tag `ns-3.47-83-gcc48bf5c1`, 2026-04-10).

---

## Running scenarios

The unified runner is `scripts/run-scenario.sh`:

```
scripts/run-scenario.sh <scenario> <version> [--sim-time <sec>] [--extra-flags "..."]
scripts/run-scenario.sh --all <scenario> [--sim-time <sec>]
```

### Scenario 1 (example-1): EF/BE CBR over PQ/WFQ/SCFQ/SFQ/WF2Qp

```bash
# ns-2.29
bash scripts/run-scenario.sh example-1 ns2-29

# ns-2.35
bash scripts/run-scenario.sh example-1 ns2-35

# ns-3
bash scripts/run-scenario.sh example-1 ns3

# All three back-to-back (prints a summary table)
bash scripts/run-scenario.sh --all example-1
```

Default sim time: 200 s.

### Scenario 2 small-scale (example-2): Premium/Gold/BE with PQ/SCFQ/LLQ

```bash
bash scripts/run-scenario.sh example-2 ns2-29
bash scripts/run-scenario.sh example-2 ns2-35
bash scripts/run-scenario.sh example-2 ns3
bash scripts/run-scenario.sh --all example-2
```

Sim time is hardcoded to 100 s in the ns-2 Tcl script; `--sim-time` is ignored
for the ns-2 version.

### Scenario 2 full-scale (example-2-fullscale): 469-node WRED sweep

```bash
# Smoke run (60 s, WRED parameter set 1)
bash scripts/run-scenario.sh example-2-fullscale ns2-35 --sim-time 60

# Full run: individual WRED parameter sets (5000 s each)
for SET in 1 2 3 4 5 6; do
    bash scripts/run-scenario.sh example-2-fullscale ns2-35 \
        --extra-flags "$SET 5000"
done
```

### Scenario 3 (example-3): Full 5-tier service model (LLQ+SFQ, 771 nodes)

```bash
# Smoke run (60 s)
bash scripts/run-scenario.sh example-3 ns2-35 --sim-time 60

# Full 5000 s run (see "Full-scale Scenario 3 sweep" below)
bash scripts/run-scenario.sh example-3 ns2-35

# ns-3 version
bash scripts/run-scenario.sh example-3 ns3 --sim-time 60
```

### WebTraf smoke test (webtraf-ns235-test, ns-2.35 only)

```bash
bash scripts/run-scenario.sh webtraf-ns235-test ns2-35
```

---

## Full-scale Scenario 3 sweep on ns-2.35

This is the 5000-second full-scale Scenario 3 run on ns-2.35.  Expected
wall-clock time: approximately 60–120 minutes (depends on host CPU speed;
the 771-node topology with VoIP + RealAudio + HTTP + FTP + Telnet is the
most compute-intensive scenario).

To launch in background and capture a log:

```bash
nohup bash scripts/run-scenario.sh example-3 ns2-35 \
    > output/ns2-35/example-3-fullrun.log 2>&1 &
echo "PID=$!"
```

Output will be written to `output/ns2-35/example-3/`.  Monitor progress:

```bash
tail -f output/ns2-35/example-3-fullrun.log
```

The run is complete when `Simulation complete.` appears in the log.

---

## Notes on ns-2.35 vs ns-2.29 differences

The ns-2.35 port is an **improved DS4**, not a faithful 2001 reproduction.
Differences that affect output:

1. **UDP header size**: ns-2.35's `udp.cc` adds 28 bytes (IP 20 + UDP 8) to
   `hdr_cmn::size()`.  All UDP-based packet sizes in trace files are 28 bytes
   larger than in ns-2.29.  Token Bucket CBS values in `simulation-1.tcl` are
   adjusted automatically via the version probe in `common/apptypes.tcl`.

2. **PT_REALAUDIO = 50**: In ns-2.35, `PT_PBC` was inserted at position 45,
   pushing `PT_REALAUDIO` from 49 to 50.  The `common/apptypes.tcl` file
   handles this automatically.

3. **Bug fixes (BUG-1..5 + BUG-11)**: See `docs/HISTORICAL_BUGS.md` for the catalogue.
   These fixes mean ns-2.35 output will differ from ns-2.29 in code paths
   that exercised the six 2001-era bugs (the five originally identified plus
   BUG-11, the dsRED Tcl shim arg-swap surfaced 2026-04-26).

---

## Packet-type constant portability

The `common/apptypes.tcl` file (sourced by all scenario scripts) sets Tcl
variables for packet-type constants that are version-portable:

| Constant      | ns-2.29 | ns-2.35 |
|---------------|---------|---------|
| PT_CBR        | 2       | 2       |
| PT_TELNET     | 26      | 26      |
| PT_FTP        | 27      | 27      |
| PT_HTTP       | 31      | 31      |
| PT_REALAUDIO  | 49      | **50**  |

PT_REALAUDIO is the only value that differs.  The probe uses the Tcl
patchlevel: ns-2.29 ships Tcl 8.4.11; ns-2.35 ships Tcl 8.5.10.

---

## Reproducing the ICNS3 2026 paper

This section maps each paper section to the exact commands that
regenerate the underlying measurement.  All ns-3 commands assume the
ns-3 setup above is complete (built `diffserv` module on the pinned
revision).  Several sections also require a Linux side: a Lima VM
named `cake-host-fairness` with `sch_cake`, `iperf3`, `jq`, `tshark`,
and `tcpreplay` available.  Provision the VM once with:

```bash
# macOS host (homebrew Lima):
limactl start --name=cake-host-fairness --tty=false template://ubuntu
limactl shell cake-host-fairness sudo apt-get install -y \
    iperf3 jq bc tshark tcpreplay
# Sanity-check sch_cake is loadable:
bash scripts/cake-host-fairness-lima-harness.sh
```

### §5.1 RFC conformance vectors

```bash
cd ns3/ns-3-dev
python3 test.py -s diffserv -v
```

Looks for `TestS_2_*` (sr-TCM RFC 2697), `TestS_3_*` (tr-TCM RFC 2698),
`TestS_4_*` (TSW2CM/TSW3CM RFC 2859), `TestL4S_*` (RFC 9331/9332),
plus Q-tier conformance scenarios.

### §5.2 Cross-simulator equivalence (ns-2.35 ↔ ns-3)

```bash
bash scripts/run-scenario.sh --all example-1     # PQ/WFQ/SCFQ/SFQ/WF2Qp
bash scripts/run-scenario.sh --all example-2     # Premium/Gold/BE with LLQ
bash scripts/run-scenario.sh --all example-3 --sim-time 60
```

Each `--all` invocation emits the corresponding rows of the
cross-simulator table in §5.2.

### §5.3 Independent reproduction — Chang et al. 2015 + Høiland-Jørgensen 2018

Chang scheduling-discipline scenarios are exercised by
`TestQ_16_ChangConvergence` in the ns-3 test suite.  Høiland-Jørgensen
CAKE-paper figures (DiffServ tin throughput shares, intra-tin Jain
fairness, RRUL probe p99 latency) are reproduced by `TestQ_15_*` test
cases; the cross-implementation calibration against Linux
`tc-cake(8)` lives at Q-15.6, Q-15.7, Q-15.8, Q-15.9.

```bash
cd ns3/ns-3-dev
python3 test.py -s diffserv -v -r TestQ_15_CakeDiffServ4TinRatios
python3 test.py -s diffserv -v -r TestQ_15_CakeRrulLatencyTarget
python3 test.py -s diffserv -v -r TestQ_16_ChangConvergence
```

### §5.4 Protocol-robustness anchor (cake-host-fairness figure)

The per-protocol `share_A` figure at the $(16, 1)$ and $(16, 16)$
cells over CUBIC, NewReno, BBR, and UDP requires both the ns-3 and the
Linux netns sides.

```bash
# Stratum side: per-flow goodput sweep
export PROTOCOL=cubic
bash scripts/cake-host-fairness-stratum-sweep.sh
export PROTOCOL=newreno && bash scripts/cake-host-fairness-stratum-sweep.sh
export PROTOCOL=bbr     && bash scripts/cake-host-fairness-stratum-sweep.sh
export PROTOCOL=udp     && bash scripts/cake-host-fairness-stratum-sweep.sh

# Linux side: same sweep through the Lima cake-host-fairness VM
for PROTO in cubic newreno bbr udp; do
    PROTOCOL=$PROTO bash scripts/cake-host-fairness-lima-sweep.sh
done

# Aggregate + plot
python3 scripts/cake-host-fairness-concat.py \
    --in output/ns3/cake-host-fairness/sweep-perflow-stratum.csv \
         output/ns3/cake-host-fairness/sweep-perflow-linux.csv \
    --out output/ns3/cake-host-fairness/sweep-perflow-all.csv
python3 scripts/aggregate-cells.py \
    --in output/ns3/cake-host-fairness/sweep-perflow-all.csv \
    --out output/ns3/cake-host-fairness/sweep-cells.csv
python3 scripts/plot-cake-host-fairness.py \
    --in output/ns3/cake-host-fairness/sweep-cells.csv \
    --out output/ns3/cake-host-fairness/cake-host-fairness-protocols.pdf
```

Expected output: 8/8 cells (4 protocols × 2 N/M combinations × 2
implementations).  Each protocol's `share_A` value should match
the §5.4 figure within the 3-replica noise floor.

### §5.5 Trace-replay (forward direction: Linux → Stratum qdisc)

Capture a (16,1) CUBIC reference run on Linux, then feed the
captured `cake_enqueue` pcap into the Stratum qdisc.  Demonstrates
that Stratum's qdisc reproduces Linux's `share_A` when given the
same input timeline.

```bash
# Linux-side capture (one-time per anchor)
bash scripts/lima/trace-replay-capture-harness.sh /tmp/trace-capture
# Yields: /tmp/trace-capture/linux-trace.pcap (cake_enqueue arrivals)
#         /tmp/trace-capture/share_A.txt (sanity: should be ~0.5146)

# Stratum-side replay: feed the captured pcap to the ns-3 example
cd ns3/ns-3-dev
./ns3 run "cake-host-fairness-sweep \
    --nFlowsA=16 --nFlowsB=1 --tcpVariant=cubic --bandwidth=100Mbps \
    --duration=30 --rngRun=1 --replayPcap=/tmp/trace-capture/linux-trace.pcap \
    --output=/tmp/trace-replay-fwd.csv"
```

Expected `share_A` ≈ 0.52 (Linux band) when fed Linux's input
pattern; matches paper §5.5 row 1, forward direction.

### §5.5 Trace-replay (reverse direction: Stratum → Linux sch_cake)

The inverse: capture Stratum's deterministic arrival pattern, replay
it through Linux `sch_cake`.

```bash
# Stratum-side capture
cd ns3/ns-3-dev
./ns3 run "cake-host-fairness-sweep \
    --nFlowsA=16 --nFlowsB=1 --tcpVariant=cubic --bandwidth=100Mbps \
    --duration=30 --rngRun=1 \
    --captureCakeEnqueuePcap=/tmp/stratum-trace.pcap \
    --output=/tmp/stratum-baseline.csv"

# Linux-side replay
STRATUM_PCAP=/tmp/stratum-trace.pcap \
    bash scripts/lima/path-b-stratum-pcap-to-linux-cake.sh
```

Expected `share_A` ≈ 0.77 (Stratum band) when Linux processes
Stratum's deterministic input — matches paper §5.5 row 1, reverse
direction.

### §5.5 Probe #1 — 5D arrival-trace comparison

Quantifies the per-dimension divergence between Stratum and Linux
arrival patterns.  Requires both pcaps (Stratum's from the previous
step plus Linux's from the trace-replay capture harness):

```bash
# Extract per-packet features from each pcap
python3 scripts/analysis/arrival-trace-5d-extract.py \
    --pcap /tmp/stratum-trace.pcap   --out /tmp/stratum-features.csv
python3 scripts/analysis/arrival-trace-5d-extract.py \
    --pcap /tmp/trace-capture/linux-trace.pcap --out /tmp/linux-features.csv

# Pairwise comparison across 5 dimensions
python3 scripts/analysis/arrival-trace-5d-compare.py \
    --stratum /tmp/stratum-features.csv \
    --linux   /tmp/linux-features.csv \
    --out /tmp/verdict-rng1.csv

# (Repeat for rngRun=2, 3, then aggregate)
python3 scripts/analysis/arrival-trace-5d-aggregate.py \
    --in /tmp/verdict-rng1.csv /tmp/verdict-rng2.csv /tmp/verdict-rng3.csv \
    --out /tmp/verdict-aggregate.csv
```

Expected: dimension 4 (host-B silence fraction at 5 ms) is the
load-bearing dimension (Wasserstein ≈ 0.34); dimensions 1, 3 are
below 0.21; dimensions 2, 5 are noisy.

### §5.5 Probe #2 — adversarial phase initialisation

Tests whether the deterministic phase coherence is an
initial-alignment artefact by staggering flow start times.

```bash
cd ns3/ns-3-dev
mkdir -p /tmp/probe-phase-init
for STAGGER in 0.0 2.5 10.0; do
    for RNG in 1 2 3; do
        ./ns3 run "cake-host-fairness-sweep \
            --nFlowsA=16 --nFlowsB=1 --tcpVariant=cubic \
            --bandwidth=100Mbps --duration=30 --rngRun=$RNG \
            --phaseStaggerMs=$STAGGER \
            --output=/tmp/probe-phase-init/stagger-$STAGGER-rng-$RNG.csv"
    done
done

# Aggregate
python3 scripts/stratum-bridge/aggregate-phase-init.py \
    /tmp/probe-phase-init
```

Expected `share_A`: 0.7619 (stagger=0, aligned baseline);
0.7597 (2.5 ms = 1 RTT spread); 0.7611 (10 ms = 4 RTT spread).
All within 0.0022 of baseline → initial-alignment hypothesis
falsified; synchronisation is self-reinforcing.

### §7 Stratum-bridge prototype

Validates the full-scenario delegation backend: a Stratum scenario
configuration is mechanically translated into an equivalent Linux
netns testbed that reproduces the Linux band.

```bash
# Emit + run all 8 scenarios (4 protocols × 2 cells)
mkdir -p /tmp/sb-output
for s in scripts/stratum-bridge/scenarios/*.yaml; do
    name=$(basename "$s" .yaml)
    python3 scripts/stratum-bridge/emit-netns.py "$s" > /tmp/emit-${name}.sh
    OUT_DIR=/tmp/sb-output/${name} bash /tmp/emit-${name}.sh
done

# Aggregate against the bundled reference ground truth
python3 scripts/stratum-bridge/aggregate-sweep.py /tmp/sb-output
```

Expected: 8/8 cells PASS within ±0.01 of the bundled reference
`share_A` values (max Δ ≈ 0.008).  Reproduces the paper §7
prototype-validation paragraph numerically.

Wall-clock: ~20 minutes for the full 8-cell sweep with the
`cake-host-fairness` Lima VM.

See `scripts/stratum-bridge/README.md` for the scenario IR schema
and emitter design.

### §8 Composing CAKE and L4S — composition fairness

One scalable (DCTCP) and one classic (Cubic) flow share a single
DiffServ tin on a 40 Mbit/s, 50 ms-RTT bottleneck.  The sweep runs
the same workload through three roots — the CAKE client (per-tin
DualPI2 inner), the bare L4S client (standalone DualPI2), and the
GPRT reference — and reports Jain's Fairness Index per qdisc.

Requires the `diffserv-l4s-dualpi2-gprt-parity` example to be built:

```bash
cd ns3/ns-3-dev
./ns3 build diffserv-l4s-dualpi2-gprt-parity
cd -

# 8-seed JFI sweep across the three qdiscs (prints a per-qdisc table)
bash scripts/l4s-cake-composition-fairness-sweep.sh

# Throughput-parity sweep: CAKE per-tin DualPI2 vs standalone DualPI2
bash scripts/l4s-dualpi2-gprt-parity-sweep.sh
```

Expected: all three roots land at JFI ≈ 0.99 (the CAKE per-tin inner
matches the bare DualPI2 and GPRT reference); the two flows split the
tin within the per-seed noise floor.  The result is locked by a
regression test in the `diffserv-l4s` suite:

```bash
cd ns3/ns-3-dev
python3 test.py -s diffserv-l4s -v
```

Per-seed summaries are written under
`output/cake-l4s-fairness/<qdisc>-<seed>/summary.txt`.
