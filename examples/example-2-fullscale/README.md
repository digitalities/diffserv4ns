# Example 2 — full-scale Scenario 2 reconstruction

**Author:** Sergio Andreozzi
**Dates:** April 16, 2026
**Notes:** Full-scale reconstruction of thesis Scenario 2 (§4.2) — 469-node
           AF PHB importance differentiation via a 6-way WRED parameter
           sweep over Telnet / FTP / HTTP traffic through an SFQ-scheduled
           DiffServ bottleneck.

This is a 2026 reconstruction by the original author; the 2006 DiffServ4NS
release shipped only the 13-node demonstration in the sibling `example-2/`
directory, never the 469-node thesis scenario. For the full reconstruction
narrative — the two documented deviations from thesis text (HTTP
approximation, FTP calibration), thesis-fidelity validation across all
six WRED parameter sets, and the negative-result `http_bursty_session`
experiment — see [`VALIDATION.md`](VALIDATION.md).

## Files

| File | Purpose |
|------|---------|
| `scenario-2.tcl`           | Main simulation (469 nodes, 5000 s default) |
| `smoke-test.tcl`           | Smoke check (100 s, set 1) |
| `utils.tcl`                | Tcl helpers (`Application/HTTP` subclass, etc.) |
| `wred-parameter-sets.md`   | Verified WRED thresholds from thesis Figure 4.3 |
| `scenario-2-ns235.tcl`     | Port-based variant for ns-2.35 |
| `scenario-2-ns235-srtcm.tcl` | srTCM per-flow variant for ns-2.35 |
| `smoke-test-ns235.tcl`     | Smoke check for the ns-2.35 variant |
| `smoke-bug9-phaseA.tcl`, `smoke-bug9-phaseC2.tcl` | WebTraf BUG-9 regression checks (ns-2.35) |
| `VALIDATION.md`            | Reconstruction narrative + thesis-comparison results |

## Requirements

| Script | ns-2 version |
|--------|--------------|
| `scenario-2.tcl`, `smoke-test.tcl` | 2.29 |
| `scenario-2-ns235.tcl`, `scenario-2-ns235-srtcm.tcl`, `smoke-test-ns235.tcl`, `smoke-bug9-phaseA.tcl`, `smoke-bug9-phaseC2.tcl` | 2.35 |

- DiffServ4NS — <https://github.com/digitalities/diffserv4ns> (Zenodo DOI [10.5281/zenodo.19665019](https://doi.org/10.5281/zenodo.19665019))
- Docker with `ubuntu:18.04` (on macOS / ARM hosts).

## Simulation

From the project root directory:

```bash
docker run --rm \
  -v "$(pwd)/ns2/ns-allinone-2.29.3:/ns-allinone" \
  -v "$(pwd)/ns2/diffserv4ns/examples/example-2-fullscale:/scripts" \
  -v "$(pwd)/output/ns2/example-2-fullscale:/scripts/output/ns2/example-2-fullscale" \
  -w /scripts \
  -e LD_LIBRARY_PATH=/ns-allinone/otcl-1.11:/ns-allinone/lib \
  -e TCL_LIBRARY=/ns-allinone/tcl8.4.11/library \
  ubuntu:18.04 \
  /ns-allinone/ns-2.29/ns scenario-2.tcl SET
```

where `SET` is `1`..`6` (WRED parameter set). Each run takes ~1–2 min wall
clock. Output files `ServiceRate.tr`, `QueueLen.tr`, `PktLoss.tr`,
`run.log` are written to `output/ns2/example-2-fullscale/set-SET/`.

To run the full 6-way sweep, iterate `SET=1..6`, then parse via:

```bash
python3 scripts/scenario2-table44.py \
  > output/ns2/example-2-fullscale/table-4-4-reproduction.md
```

## Topology

```
  servers (n6-n45)
  40 nodes via n2-n5
      \  |  /  /
       n2  n3  n4  n5                clients (n46-n465, 420 nodes)
        \  |  /  /                       |
         \ | / /                          \
          n1 <----[n466]<------ n0 <----+
                                  |      |
                                  |      | DiffServ edge (n0 -> n466):
                                  |      |   3 Mb/s, 20 ms, dsRED
                                  |      |   2 queues, SFQ(17:3)
  n467 <----------------[n0 ]    |      |   AF (85%) + Default (15%)
  n468 <----------------[n466]   |
                                 BG CBR
```

Total 469 nodes. Bottleneck: `n0 -> n466` egress interface.
