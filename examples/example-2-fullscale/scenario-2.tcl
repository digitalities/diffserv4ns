# Copyright (C) 2001-2026  Sergio Andreozzi
#
# DISCLAIMER: This is a 2026 reconstruction of thesis Scenario 2
# (Section 4.2 — "full-scale web traffic scenario"). The original 2001
# DiffServ4NS release did not ship a Tcl script for this scenario; the
# thesis describes the topology, the WRED parameter sweep and the traffic
# mix, but the concrete simulation script was never published. This file
# recreates the scenario as faithfully as the thesis text and Figure 4.3
# allow, using the working Scenario 3 reconstruction
# (examples/example-3/scenario-3.tcl) as a pattern template.
#
# Topology (469 nodes, thesis §4.2 + ns-2.29 varybell.tcl):
#   n0       : server-side DiffServ edge router
#   n1       : client-side router
#   n2..n5   : 4 server access routers
#   n6..n45  : 40 web servers (10 per access router)
#   n46..n465: 420 web clients
#   n466     : bottleneck intermediate (n0 -> n466 is the DiffServ egress)
#   n467     : background CBR source
#   n468     : background CBR sink
#
# DiffServ configuration (Table 4.3):
#   Q0 (AF)     : 3 drop precedences, WRED, SFQ weight 17, 85% BW
#   Q1 (Default): 2 drop precedences, TokenBucket metered, SFQ weight 3
#   Scheduler   : SFQ on 3 Mbps aggregate
#
# WRED sweep (thesis Figure 4.3, verified against the PDF — see
# wred-parameter-sets.md for the verification record):
#   Set  : 1..6, selectable via argv[0]
#
# Usage:
#   ns scenario-2.tcl [paramSet [testTime]]
#     paramSet : 1..6  (WRED threshold set; default 1)
#     testTime : seconds (default 5000, can also be overridden by
#                setting the global `overrideTestTime` before sourcing).
# ---------------------------------------------------------------------------

source ../common/apptypes.tcl
source utils.tcl

# HTTP traffic-model RVs for bursty WebTraf approximation (thesis §4.2).
# Deterministic seed above (ns-random 12345) makes samples reproducible.
set interPageRV [new RandomVariable/Exponential]
$interPageRV set avg_ 15
set objSizeRV [new RandomVariable/ParetoII]
$objSizeRV set avg_ 12
$objSizeRV set shape_ 1.2

set ns [new Simulator]

# ---------------------------------------------------------------------------
# Run-time parameters
# ---------------------------------------------------------------------------
# WRED parameter set selector: argv[0], default 1
set paramSet 1
if { $argc >= 1 } { set paramSet [lindex $argv 0] }
if { $paramSet < 1 || $paramSet > 6 } {
    puts "ERROR: paramSet must be in 1..6 (got $paramSet)"
    exit 1
}

# Simulation length: argv[1], or the calling-context override
# `overrideTestTime`, or the default 5000 s.
if { [info exists overrideTestTime] } {
    set testTime $overrideTestTime
} elseif { $argc >= 2 } {
    set testTime [lindex $argv 1]
} else {
    set testTime 5000
}

puts "scenario-2: paramSet=$paramSet testTime=${testTime}s"

# Seed the ns-random generator so the random link delays/bandwidths are
# reproducible across runs (same seed -> same topology).
ns-random 12345

# ---------------------------------------------------------------------------
# WRED threshold arrays — verified against thesis Figure 4.3.
# Format: "min_th max_th max_p"
# DP0 is the most protected (highest thresholds, lowest max_p);
# DP2 is the most aggressive (lowest thresholds, highest max_p).
# See wred-parameter-sets.md for the PDF-verification record.
# ---------------------------------------------------------------------------
array set wredDP0 {
    1 "50 70 0.1"
    2 "65 95 0.1"
    3 "45 65 0.1"
    4 "40 60 0.1"
    5 "20 60 0.1"
    6 "20 80 0.1"
}
array set wredDP1 {
    1 "30 50 0.2"
    2 "35 65 0.2"
    3 "30 50 0.2"
    4 "30 50 0.2"
    5 "20 60 0.2"
    6 "20 80 0.2"
}
array set wredDP2 {
    1 "10 30 0.5"
    2 "5 35 0.5"
    3 "15 35 0.5"
    4 "20 40 0.5"
    5 "20 60 0.5"
    6 "20 80 0.5"
}

# ---------------------------------------------------------------------------
# Output directory and trace files
# ---------------------------------------------------------------------------
set outDir "output/ns2/example-2-fullscale/set-$paramSet"
file mkdir $outDir

set ServiceRate [open "$outDir/ServiceRate.tr" w]
set QueueLen    [open "$outDir/QueueLen.tr"    w]
set PktLoss     [open "$outDir/PktLoss.tr"     w]

# ---------------------------------------------------------------------------
# Node allocation (469 nodes)
# ---------------------------------------------------------------------------
set numNodes 469
for {set i 0} {$i < $numNodes} {incr i} {
    set n($i) [$ns node]
}
puts "Topology: $numNodes nodes created"

# ---------------------------------------------------------------------------
# Topology (derived from ns-2.29 tcl/ex/varybell.tcl + thesis §4.2)
# ---------------------------------------------------------------------------

# DiffServ bottleneck: n0 -> n466 (edge), n466 -> n0 (core, minimal)
$ns simplex-link $n(0)   $n(466) 3Mb 20ms dsRED/edge
$ns simplex-link $n(466) $n(0)   3Mb 20ms dsRED/core

# Client-side: n466 <-> n1
$ns duplex-link  $n(466) $n(1)   3Mb 20ms DropTail

# Server access routers (n0 <-> n2-n5)
$ns duplex-link $n(0) $n(2) 1.5Mb 20ms DropTail
$ns duplex-link $n(0) $n(3) 1.5Mb 30ms DropTail
$ns duplex-link $n(0) $n(4) 1.5Mb 40ms DropTail
$ns duplex-link $n(0) $n(5) 1.5Mb 60ms DropTail

# Server links: n6-n45 <-> one of n2-n5 (10 servers per access router,
# uniform(10,100) ms delay). Using [ns-random] % 91 + 10 for the delay.
for {set i 0} {$i < 40} {incr i} {
    set base  [expr $i / 10 + 2]
    set delay [expr [ns-random] % 91 + 10]
    $ns duplex-link $n($base) $n([expr $i + 6]) 10Mb ${delay}ms DropTail
}

# Client links: n46-n465 <-> n1 (uniform(22,32) Mb BW, uniform(10,100) ms delay)
for {set i 0} {$i < 420} {incr i} {
    set bw    [expr [ns-random] % 11 + 22]
    set delay [expr [ns-random] % 91 + 10]
    $ns duplex-link $n(1) $n([expr $i + 46]) ${bw}Mb ${delay}ms DropTail
}

# Background CBR endpoints: n0 <-> n467 (source), n466 <-> n468 (sink)
$ns duplex-link $n(0)   $n(467) 10Mb 1ms DropTail
$ns duplex-link $n(466) $n(468) 10Mb 1ms DropTail

# ---------------------------------------------------------------------------
# DiffServ configuration (Table 4.3)
#   Q0: AF  — 3 drop precedences, WRED, SFQ weight 17
#   Q1: BE  — 2 drop precedences, Token-Bucket metered, SFQ weight 3
# ---------------------------------------------------------------------------
set qE1C [[$ns link $n(0)   $n(466)] queue]
set qCE1 [[$ns link $n(466) $n(0)  ] queue]

# 2 physical queues on the egress edge queue
$qE1C set numQueues_ 2

# Scheduler: SFQ on 3 Mbps aggregate, weights 17 (AF) : 3 (BE) = 85% : 15%
$qE1C setSchedularMode SFQ 3000000
$qE1C addQueueWeight 0 17
$qE1C addQueueWeight 1  3

# --- Q0: Assured Forwarding (3 drop precedences) -----------------------
$qE1C setQSize   0 100
$qE1C setNumPrec 0   3

# Mark rules: Telnet -> DSCP 10 (DP0), FTP -> DSCP 12 (DP1), HTTP -> DSCP 14 (DP2)
$qE1C addMarkRule 10 -1 -1 any telnet
$qE1C addMarkRule 12 -1 -1 any ftp
$qE1C addMarkRule 14 -1 -1 any http

# Dumb policy/policer — classifier has already pre-marked the DSCP
$qE1C addPolicyEntry  10 Dumb
$qE1C addPolicerEntry Dumb 10
$qE1C addPolicyEntry  12 Dumb
$qE1C addPolicerEntry Dumb 12
$qE1C addPolicyEntry  14 Dumb
$qE1C addPolicerEntry Dumb 14

# PHB: DSCP -> (queue, drop_prec)
$qE1C addPHBEntry 10 0 0
$qE1C addPHBEntry 12 0 1
$qE1C addPHBEntry 14 0 2

# RED/WRED configuration for Q0
$qE1C meanPktSize 1000
$qE1C setQueueBW  0 2550000        ;# 85% of 3 Mbps
$qE1C setMREDMode WRED 0

# Per-precedence thresholds from the WRED sweep arrays
eval $qE1C configQ 0 0 $wredDP0($paramSet)
eval $qE1C configQ 0 1 $wredDP1($paramSet)
eval $qE1C configQ 0 2 $wredDP2($paramSet)

# --- Q1: Best Effort / Default (2 drop precedences) --------------------
$qE1C setQSize   1  50
$qE1C setNumPrec 1   2

# Token-Bucket meter for unmarked (DSCP 0) traffic: CIR 500 kbps, CBS 10 KB
$qE1C addPolicyEntry  0 TokenBucket 500000 10000
$qE1C addPolicerEntry TokenBucket 0 50

# PHB: conforming -> (1,0), non-conforming -> (1,1)
$qE1C addPHBEntry  0 1 0
$qE1C addPHBEntry 50 1 1

# Tail-drop per drop-precedence on Q1
$qE1C setMREDMode DROP 1
$qE1C configQ 1 0 50
$qE1C configQ 1 1 -1

# --- Core reverse queue (n466 -> n0): minimal --------------------------
$qCE1 set numQueues_ 1
$qCE1 setMREDMode DROP
$qCE1 setQSize   0 60
$qCE1 setNumPrec 0  1
$qCE1 configQ    0 0 60
$qCE1 addPHBEntry 0 0 0

puts "DiffServ: 2-queue SFQ(17:3) configured, WRED set $paramSet"
puts "   Q0 DP0 = $wredDP0($paramSet)"
puts "   Q0 DP1 = $wredDP1($paramSet)"
puts "   Q0 DP2 = $wredDP2($paramSet)"

# ---------------------------------------------------------------------------
# Traffic 1: Telnet (50 connections, first 50 s)
# ---------------------------------------------------------------------------
puts "Creating 50 Telnet connections (active 0-50 s per thesis §4.2)..."
for {set i 0} {$i < 50} {incr i} {
    set src       [expr ($i % 40) + 6]
    set dst       [expr ($i % 50) + 46]
    set startTime [expr $i * 1.0]
    telnet_connection [expr $i + 1000] $n($src) $n($dst) $startTime 1.0 50.0
}

# ---------------------------------------------------------------------------
# Traffic 2: FTP (50 connections, active 0-50 s per thesis §4.2)
# ---------------------------------------------------------------------------
puts "Creating 50 FTP connections (50KB file transfers staggered 0-50 s per thesis §4.2)..."
# Each FTP connection is modelled as a single 50KB file download rather
# than an unlimited bulk stream. The thesis is silent on the size; 50KB
# was chosen because bulk transfers produce DP1 caPL 6-11pp over thesis's
# 0-5% band, whereas finite 50KB transfers bring DP1 caPL into the
# thesis range. See README §HTTP-model divergence for the rationale.
for {set i 0} {$i < 50} {incr i} {
    set src       [expr ($i % 40) + 6]
    set dst       [expr ($i % 50) + 46]
    set startTime [expr $i * 1.0]
    ftp_connection [expr $i + 2000] $n($src) $n($dst) $startTime 50.0 50000
}

# ---------------------------------------------------------------------------
# Traffic 3: HTTP (400 "sessions", staggered 0.25 s apart)
#
# Thesis §4.2 specifies 400 HTTP sessions originally driven by WebTraf
# with an exponential inter-session time (mean ~1 s). WebTraf crashes
# under DiffServ4NS (see utils.tcl http_session header); we substitute
# 400 independent TCP bulk transfers classified as HTTP via the
# Application/HTTP subclass. Start times are deterministic
# 0.25 s steps as a reproducible approximation of Exp(1 s).
# ---------------------------------------------------------------------------
puts "Creating 400 HTTP sessions (bulk-TCP approximation — see README §HTTP-model divergence)..."
# The thesis specifies a PagePool/WebTraf model with Exp(15s) inter-page
# idle gaps and Pareto(12KB, 1.2) object sizes. A literal hand-rolled
# reconstruction of that model (see http_bursty_session in utils.tcl) was
# tested and UNDER-loads the AF queue: duty cycle per flow ~0.7% means
# only ~3 flows are actively sending at any instant out of 400, AF queue
# runs at ~65% utilisation, and DP2 caPL drops to ~3% vs thesis's 25%.
#
# The bulk-TCP approximation (http_session) keeps flows continuously
# sending, matches thesis Table 4.4 within ~3-8pp on DP2, and is
# retained as the primary HTTP generator. The discrepancy between the
# thesis-described WebTraf params and the Table 4.4 values is
# documented as a finding in the README. http_bursty_session is kept
# in utils.tcl for reproducibility of the negative result.
for {set i 0} {$i < 400} {incr i} {
    set src       [expr ($i % 40) + 6]
    set dst       [expr ($i % 420) + 46]
    set startTime [expr $i * 0.25]
    http_session  [expr $i + 3000] $n($src) $n($dst) $startTime [expr $testTime - 1]
}

# ---------------------------------------------------------------------------
# Traffic 4: Background CBR (n467 -> n468, entire simulation)
# ---------------------------------------------------------------------------
puts "Creating background CBR..."
cbr_connection 9000 0 $n(467) $n(468) 0.0 512 500000b 0 $testTime

# ---------------------------------------------------------------------------
# Monitoring / trace writers
# ---------------------------------------------------------------------------

# Per-DSCP byte counters (ServiceRate.tr): one entry per second, columns
#   time  AF-DP0(dscp10)  AF-DP1(dscp12)  AF-DP2(dscp14)  BE-in(0)  BE-oop(50)
proc record_service_rate {} {
    global qE1C ServiceRate
    set ns [Simulator instance]
    set now [$ns now]

    set r10 [expr [$qE1C getDepartureRate 0 0]/1000]
    set r12 [expr [$qE1C getDepartureRate 0 1]/1000]
    set r14 [expr [$qE1C getDepartureRate 0 2]/1000]
    set r00 [expr [$qE1C getDepartureRate 1 0]/1000]
    set r50 [expr [$qE1C getDepartureRate 1 1]/1000]

    puts $ServiceRate "$now $r10 $r12 $r14 $r00 $r50"
    $ns at [expr $now + 1] "record_service_rate"
}

# Per-queue instantaneous length (QueueLen.tr):
#   time  Q0-len  Q1-len
proc record_queue_len {} {
    global qE1C QueueLen
    set ns [Simulator instance]
    set now [$ns now]

    set q0 [$qE1C getQueueLen 0]
    set q1 [$qE1C getQueueLen 1]

    puts $QueueLen "$now $q0 $q1"
    $ns at [expr $now + 1] "record_queue_len"
}

# Per-DSCP loss fractions (PktLoss.tr):
#   time  caPL10 boPL10 caPL12 boPL12 caPL14 boPL14 caPL0 boPL0 caPL50 boPL50
# caPL = congestion-avoidance (early / RED) drop ratio (%)
# boPL = buffer-overflow (tail) drop ratio (%)
# Helper: loss-percentage with zero-guard.
# IMPORTANT: on Tcl 8.5 (ns-2.35) `catch {expr X/0}` has a ~50 000x slowdown
# vs Tcl 8.4 (ns-2.29). See docs/HISTORICAL_BUGS.md BUG-7 (Tcl 8.5
# catch+division-by-zero performance regression). Always guard the divisor.
proc loss_pct {q stat_type dscp} {
    set pkts [$q getStat pkts $dscp]
    if {$pkts <= 0} { return 0 }
    return [expr [$q getStat $stat_type $dscp] * 100.0 / $pkts]
}

proc record_pkt_loss {} {
    global qE1C PktLoss
    set ns [Simulator instance]
    set now [$ns now]

    set ca10 [loss_pct $qE1C edrops 10]
    set bo10 [loss_pct $qE1C drops  10]
    set ca12 [loss_pct $qE1C edrops 12]
    set bo12 [loss_pct $qE1C drops  12]
    set ca14 [loss_pct $qE1C edrops 14]
    set bo14 [loss_pct $qE1C drops  14]
    set ca0  [loss_pct $qE1C edrops 0]
    set bo0  [loss_pct $qE1C drops  0]
    set ca50 [loss_pct $qE1C edrops 50]
    set bo50 [loss_pct $qE1C drops  50]

    puts $PktLoss "$now $ca10 $bo10 $ca12 $bo12 $ca14 $bo14 $ca0 $bo0 $ca50 $bo50"
    $ns at [expr $now + 1] "record_pkt_loss"
}

# ---------------------------------------------------------------------------
# Schedule monitoring and run
# ---------------------------------------------------------------------------
puts "Configuration complete. Starting simulation..."

$qE1C printPolicyTable
$qE1C printPolicerTable

$ns at 0.0  "record_service_rate"
$ns at 0.0  "record_queue_len"
$ns at 0.0  "record_pkt_loss"

# Two printStats dumps: midpoint (sanity-check the ramp-up / steady-state
# transition) and final (Table 4.4 reproduction). Both blocks appear in
# run.log; scripts/scenario2-table44.py uses the LAST block (steady-state).
# Percentages are stationary after FTP/Telnet stop at t=50s, so the two
# blocks converge to the same cell values.
$ns at [expr $testTime/2]     "$qE1C printStats"
$ns at [expr $testTime - 0.1] "$qE1C printStats"

proc finish {} {
    global ns ServiceRate QueueLen PktLoss
    $ns flush-trace
    close $ServiceRate
    close $QueueLen
    close $PktLoss
    puts "Simulation complete."
    exit 0
}

$ns at $testTime "finish"
$ns run
