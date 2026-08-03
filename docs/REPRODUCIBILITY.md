# Reproducibility Guide

This document lists the exact commands to re-run every DiffServ4NS scenario on
both supported ns-2 versions, along with setup prerequisites.

The reproduction map for the ns-3 QoS substrate *Stratum* is maintained
separately, in
[digitalities/stratum-ns3](https://github.com/digitalities/stratum-ns3).

---

## Directory convention

All simulation output is written to:

```
output/<version>/<scenario>/
```

Where `<version>` is one of `ns2-29`, `ns2-35` and `<scenario>` is one
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

# Both back-to-back (prints a summary table)
bash scripts/run-scenario.sh --all example-1
```

Default sim time: 200 s.

### Scenario 2 small-scale (example-2): Premium/Gold/BE with PQ/SCFQ/LLQ

```bash
bash scripts/run-scenario.sh example-2 ns2-29
bash scripts/run-scenario.sh example-2 ns2-35
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
