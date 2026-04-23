# Example 1

**Author:** Sergio Andreozzi
**Dates:** June 27, 2006
**Notes:** A DS-RED script that uses CBR traffic agents and the srTCM policer.

## 1. Requirements

- ns-2 (either version 2.29 or version 2.35 — the script uses only
  APIs preserved in both DiffServ4NS variants)
- DiffServ4NS — <https://github.com/digitalities/diffserv4ns> (Zenodo DOI [10.5281/zenodo.19665019](https://doi.org/10.5281/zenodo.19665019))
- gnuplot

## 2. Simulation

From the command line (assumes `ns` and `gnuplot` are on PATH):

```
source start
```

For Docker-based execution on macOS / Apple Silicon, see [Running simulations](../../docs/installation.md#running-simulations) in the installation guide.

## 3. Topology

```
 ----                                                    ----
| s1 |------------|                       |-------------| d1 |
 ---- 100Mb/s     |                       |      100Mb/s ----
        1ms       |                       |        1ms
 ----             |                       |              ----
| s2 |--------    |                       |     --------| d2 |
 ---- 100Mb/s \   |                       |    / 100Mb/s ----
        1ms    \  |                       |   /    1ms
 ----           \----        ----        ----/           ----
| s3 |----------| e1 |------|core|------| e2 |----------| d3 |
 ---- 100Mb/s   /----  2Mb/s ----  2Mb/s ----\   100Mb/s ----
        1ms    /  |    5ms         3ms    |   \    1ms
 ----         /   |                       |    \         ----
| s4 |--------    |                       |     --------| d4 |
 ---- 100Mb/s     |                       |      100Mb/s ----
        1ms       |                       |        1ms
 ----             |                       |              ----
| s5 |------------|                       |-------------| d5 |
 ---- 100Mb/s                                    100Mb/s ----
        1ms                                        1ms
```
