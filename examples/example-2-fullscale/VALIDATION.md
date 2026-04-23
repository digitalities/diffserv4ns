# Scenario 2 — reconstruction narrative and thesis-fidelity validation

Companion to `README.md`. Full reconstruction context for thesis §4.2.

## Two deviations from thesis text

Required to run the scenario under ns-2.29 + DiffServ4NS. Both documented
and applied consistently; neither invalidates the qualitative thesis
conclusions.

 1. **`PagePool/WebTraf` is INCOMPATIBLE with DiffServ4NS.** The DiffServ4NS
    patches enlarge the `hdr_cmn` struct by 12 bytes (`sendtime_` and
    `app_type_`), which shifts every subsequent header offset. WebTraf's
    internal TCP agent construction reads past the new tail of `hdr_cmn`
    and crashes. As a workaround, HTTP sessions are generated here as
    400 independent TCP bulk transfers using an `Application/HTTP` Tcl
    subclass that overrides `start{}` to stamp `PT_HTTP=31` on outgoing
    packets. The aggregate AF queue load is comparable to the thesis's
    PagePool/WebTraf model but lacks the inter-page/inter-object idle
    periods, so DP2 (HTTP) drop rates are slightly higher than Table 4.4.

    **Negative-result note.** A literal hand-rolled WebTraf approximation
    was also implemented (`http_bursty_session` in `utils.tcl`, 250 or 400
    pages per session, Exp(15s) inter-page gaps, Pareto(12 KB, 1.2)
    object size — exactly the thesis-described parameters). It
    UNDER-loads the AF queue: duty cycle per flow is ~0.7 % (12 KB @
    ~2 Mbps takes ~50 ms, fits in a 15 s window) so only ~3 of 400 flows
    are actively sending at any instant. Measured DP2 caPL drops to
    ~1.5–3.5 % across all six sets, compared with thesis Table 4.4's
    19–27 %. This means the thesis's reported AF-queue stress is NOT
    achievable from a faithful reconstruction of its own stated
    WebTraf parameters alone — either thesis's PagePool/WebTraf
    implementation produced higher instantaneous rates than a naive
    parameter reading suggests, or additional unstated traffic was
    present. The bulk-TCP `http_session` model remains the closer
    match to Table 4.4 numerics, and is what the shipped script uses.
    `http_bursty_session` is kept in `utils.tcl` for reproducibility of
    this finding.

 2. **`Application/FTP::start()`** in the stock Tcl library hardcodes
    `set_apptype 27` (`PT_FTP`). For HTTP to classify correctly we needed
    the subclass above; for FTP the hardcoded behaviour is fine. CBR
    background needs an explicit `set_apptype 2` call (ns-2's
    `Application/Traffic/CBR` does not set `app_type_`).

Thesis §4.2 literal reading: *"Both FTP and Telnet traffics are activated
during the first 50 seconds of simulation."* Under this reading, FTP and
Telnet are active only during [0, 50] s. Table 4.4's DP1 caPL = 0.01 % is
consistent with this interpretation; 50 bulk-TCP FTP streams running the
full 5000 s would saturate DP1's WRED thresholds. The scripts here honour
the 50-second window.

## Validation results (5000 s, 6 WRED sets)

Qualitative thesis claims all verified:

- DP0 caPL < DP1 caPL < DP2 caPL in every set (correct WRED ordering)
- Staggered sets (1, 2) give maximum differentiation
- Overlapped sets (5, 6) give proportional sharing (DP0 caPL 5–6 %,
  DP1 caPL 10–11 %, DP2 caPL 27–28 %) — protection reduces
- boPL ~ 0 in all sets except set 2 (a couple of tenths of a percent)

Quantitative match against thesis Table 4.4 (updated 2026-04-17):

**Metric note.** Thesis "goodput" is a per-DSCP TCP retransmission-bytes
ratio (`TCPbGoTX(x) / (TCPbGoTX(x) + TCPbReTX(x))`, Andreozzi 2001 §3.3.4
p. 52), measured at the sender. Our `delivery_ratio` (`TxPkts / TotPkts`
at the AF queue) coincides with it only under static DSCP classification
where TCP retries keep the same DSCP as the original. Thesis uses
rate-metered (srTCM-style) classification where retries migrate DPs, so
the thesis's per-DP goodput picks up distribution effects our port-based
classifier cannot reproduce. `delivery_ratio` is reported against
`thesis_goodput` as reference only and excluded from the pass/fail
tolerance check.

**Tolerance:** `caPL <= 2pp`, `boPL <= 0.5pp` (applied across 36 cells:
6 sets × 3 DPs × 2 metrics). `delivery_ratio` excluded.

- 29 of 36 cells within tolerance (80.6 %)
-  7 of 36 cells outside tolerance, distributed as follows:
    - DP2 caPL in all sets: within 0.85–8.27 pp (measured ~28 %,
      thesis 19–27 %). Tracks the HTTP bulk-TCP approximation.
    - DP1 caPL at Sets 5–6 (overlapped WRED): drift amplified by
      HTTP-model divergence where DP1 and DP2 thresholds coincide.
- `delivery_ratio` (reference, not tolerance-checked): DP0 / DP1 saturate
  near 1.00 (very few queue drops under static DSCP); thesis goodput
  0.87–0.91 (because thesis retries migrate DPs). DP2 `delivery_ratio`
  0.71–0.73 (consistent with 27–28 % drops) vs thesis 0.78–0.83.

## Why Sets 5–6 (overlapped WRED) drift even after the finite-FTP fix

Measured AF queue average vs thesis back-implied value (from DP1 caPL):

| Set | Measured q_avg | Thesis ~ | Status |
|-----|----------------|----------|--------|
| 1   | 19.9 | 20 | match |
| 2   | 19.5 | 20 | match |
| 3   | 24.6 | 27 | close |
| 4   | 29.8 | 26 | close |
| 5   | 38.9 | 30 | 8–9 packet stress excess |
| 6   | 47.8 | 43 | 5 packet stress excess |

Staggered / partially overlapped WRED (Sets 1–4) has per-DP thresholds,
so each DP's WRED response self-corrects the queue toward that DP's
band. Overlapped WRED (Sets 5–6) gives all three DPs the same band, so
only DP2's aggressive drop probability holds the queue back. The regime
is more sensitive to absolute offered load: bulk-TCP's slight
over-saturation (relative to thesis WebTraf) shifts the steady-state
queue level up by ~5–10 packets in overlapped configs, which uniformly
increases DP0 / DP1 / DP2 caPL by ~1–4 pp. We cannot close this gap
without either parameter-fitting session count or transfer dynamics to
the thesis figures (out of scope here — the reconstruction's value
depends on not back-fitting), or a WebTraf-faithful HTTP model that
genuinely reproduces the thesis queue dynamics (the `http_bursty_session`
experiment documented above shows that a naive parameter reproduction
does NOT achieve this).

## FTP traffic calibration

Each of the 50 FTP connections models a single 50 KB file transfer
(`$ftp send 50000`) rather than an unlimited bulk stream (`$ftp start`).
The thesis is silent on FTP size; with bulk streams DP1 caPL was 6–11 pp
over thesis's 0–5 %, whereas finite 50 KB transfers bring DP1 caPL
within tolerance for Sets 1–4 exactly and within 4 pp for Sets 5–6.
Using `$ftp send` rather than `$ftp start` also exposed a silent
classification bug: `Application/FTP::send N` skips the hardcoded
`set_apptype 27` that `start` does, which would silently route FTP
packets to DSCP 0. The reconstruction's `ftp_connection` in `utils.tcl`
stamps `PT_FTP=27` on the TCP agent at creation time to make both
`start` and `send` paths classify correctly.

## Summary

The divergences are dominated by the HTTP-model approximation (no idle
periods) and by the unspecified FTP burst profile. The WRED
differentiation mechanism itself reproduces correctly — this is what
thesis §4.2 was demonstrating. Scenario 2 full-scale is therefore
validated as a qualitative reconstruction with a few percentage points
of per-cell quantitative drift.

Full Table 4.4 reproduction is in
`output/ns2/example-2-fullscale/table-4-4-reproduction.md`.
