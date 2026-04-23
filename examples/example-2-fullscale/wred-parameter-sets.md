# WRED Parameter Sets — thesis Figure 4.3

**Source:** `provenance/Andreozzi-2001-thesis.pdf` PDF page 72 (thesis printed page 66), Figure 4.3 "WRED parameter sets".

**Verified against PDF:** 2026-04-16 by rendering the page with `pdftoppm -r 300` and cropping each of the six subplots, then visually reading the numeric labels adjacent to each marker.

**Max drop probability (fixed for all sets, inferred from the ordinate label of each knee point in the figure; also stated in thesis §4.2 body text):**

- DP0 max_p = 0.1  (green triangle, lowest drop precedence, least aggressive)
- DP1 max_p = 0.2  (yellow square)
- DP2 max_p = 0.5  (red diamond, highest drop precedence, most aggressive)

## Legend mapping (verified from figure marker geometry)

Every subplot shows three curves with the same max_p pattern:

- The **red diamond** curve rises to 0.5 at its max-threshold (then jumps vertically to 1.0). Highest drop pressure → **DP2**.
- The **yellow square** curve rises to 0.2 at its max-threshold. Medium drop pressure → **DP1**.
- The **green triangle** curve rises to 0.1 at its max-threshold. Lowest drop pressure → **DP0**.

This mapping is the opposite of the column order the ASCII-extraction candidate table used: in Set 1 the ASCII table assigned DP0 = (30, 50) (yellow curve), DP1 = (50, 70) (green curve), DP2 = (70, 100) (misread — 100 is the right-edge tail of the green curve post-saturation, not a threshold). The figure actually places DP2 at the *lowest* thresholds because DP2 has the highest drop probability.

## Parameter table (verified from PDF)

All thresholds are in units of packets (as used by the ns-2 `RED` queue implementation in DiffServ4NS-0.2).

| Set | Structure            | DP0 (min,max) | DP1 (min,max) | DP2 (min,max) | Verified |
|-----|----------------------|---------------|---------------|---------------|----------|
| 1   | staggered            | (50, 70)      | (30, 50)      | (10, 30)      | yes      |
| 2   | staggered            | (65, 95)      | (35, 65)      | (5, 35)       | yes      |
| 3   | partially overlapped | (45, 65)      | (30, 50)      | (15, 35)      | yes      |
| 4   | partially overlapped | (40, 60)      | (30, 50)      | (20, 40)      | yes      |
| 5   | overlapped           | (20, 60)      | (20, 60)      | (20, 60)      | yes      |
| 6   | overlapped           | (20, 80)      | (20, 80)      | (20, 80)      | yes      |

## Discrepancies from ASCII-extraction candidates

The ASCII extraction was unreliable on two counts:

1. **Column order was inverted** for all six sets. The ASCII table listed thresholds in ascending order as DP0/DP1/DP2, but the figure's legend pairs the lowest thresholds with the highest-max_p (red, = DP2), not DP0. Every cell in the ASCII table needs to be re-assigned. For example, Set 1 candidate said DP0=(30,50) DP1=(50,70) DP2=(70,100); the figure actually shows DP2=(10,30) DP1=(30,50) DP0=(50,70).

2. **"Overlapped" sets (5 and 6) were completely mis-extracted.** The ASCII extraction gave degenerate pairs (60, 60) and (80, 80), which would make the drop region zero-width. The figure shows all three curves with a common min=20 and a common max=60 (Set 5) or max=80 (Set 6); the curves differ only in max_p. This is what makes the configuration "overlapped" — the active-dropping regions coincide.

3. **Set 1 DP2 min-threshold** was read as 70 by the ASCII extraction but is actually 10 in the figure. The "70" and "100" labels the ASCII extraction picked up are the green-curve tail points (min=50, max=70 for DP0, then the horizontal line extends to x=100 after max_th).

4. **Set 2 DP2 min-threshold** — ASCII said 65 but figure shows 5.

5. **Set 3 DP1 min-threshold** — ASCII said 35 but figure clearly shows 30 (the DP1 yellow square at drop=0 sits directly above the 30 tick, aligned with the DP2 max-threshold; DP1's min-threshold coincides with DP2's max-threshold, which is what "partially overlapped" means for this set).

All DP0 max-thresholds in sets 1–4 extend as a horizontal line from the knee (at max_p) to x=100. The "100" tail label on the green curve is a right-edge display artefact (the chart's x-axis range), **not** a max-threshold. The ASCII extraction confused this tail with a real threshold.

## Notes

- In the figure each subplot has an x-axis labelled "minimum and maximum threshold" and a y-axis labelled "drop probability". Markers are placed at four locations per curve: (0, 0) origin anchor, (min_th, 0), (max_th, max_p), and (max_th, 1.0) after the vertical jump. Numeric labels next to each marker report the x-coordinate.
- Each curve also continues horizontally to x=100 after reaching drop=1.0. This tail is not a parameter — it is the display convention showing that once the queue fills past max_th, every packet of that precedence is dropped.
- Set 1 matches RFC 2597-style "staggered" AF thresholds (no overlap between DPs). Set 2 is the same structure scaled and shifted. Sets 3 and 4 have the DP2 max-threshold equal to the DP1 min-threshold and the DP1 max-threshold equal to the DP0 min-threshold, creating a "partially overlapped" regime where adjacent precedence levels share the knee-jump. Sets 5 and 6 are fully overlapped: all three curves share min_th and max_th and differ only in the max drop probability at the knee.
- The sets are listed in order of increasing aggregation of the drop region. Set 1 (widest stagger, DP2 filling 10–30, DP0 filling 50–70) is the strongest differentiation; Set 6 (all three sharing 20–80) is the weakest, approaching a uniform RED.
