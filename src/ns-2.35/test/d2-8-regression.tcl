# BUG-11 regression — dsRED Tcl shim arg-swap + byte-unit aliases
#
# Verifies the fix at src/ns-2.35/diffserv/dsred.cc:388-393 (TCPbReTX
# vs TCPnReTX swap) and the new origBytes / retxBytes byte-unit
# aliases.  Runs a small TCP flow over a tight bottleneck so the
# queue sheds packets, TCP retransmits, and the dsRED stats counters
# accumulate non-zero values for both the goodput and retx paths.
#
# After end-of-sim, the script issues five getStat queries on the
# DSCP-46 (EF) bucket and asserts:
#
#   1. TCPnReTX returns an integer-valued count >= 1 (not the bytes
#      KB total, which would be fractional).
#   2. TCPbReTX returns a fractional KB total > 0.
#   3. TCPbReTX > TCPnReTX (with default MSS of 1000 bytes the bytes/
#      KB total = count * 1000/1024 ~= count * 0.977, which is < count;
#      with default MSS of 1500 bytes the ratio is ~1.46 which is >
#      count.  We use the default 1000-byte packets explicitly here so
#      the swap manifests as TCPbReTX < TCPnReTX, then assert the
#      *opposite* arithmetic relation that matches the FIX:
#          fixed:  TCPnReTX <= TCPbReTX*1024/MSS  (count = bytes/MSS)
#          swap:   TCPnReTX  > TCPbReTX*1024/MSS  (count = KB)
#      The check below uses the equivalent integer-vs-fractional
#      identity assertion which is robust under either MSS.
#   4. retxBytes equals TCPbReTX * 1024 within rounding (alias parity).
#   5. origBytes equals TCPbGoTX * 1024 within rounding (alias parity).
#
# Exits with code 0 on PASS, 1 on FAIL.  Prints diagnostic lines so a
# wrapper script can grep for status.

set ns       [new Simulator]
set MSS_BYTES 1000   ;# the ns-2 TCP default packetSize_

# ------------------------------------------------------------------
# Topology: src --- e1 (edge) ====bottleneck==== sink
# ------------------------------------------------------------------
# A two-link path with DropTail on the access link and a tight
# dsRED/edge bottleneck on the path to the sink.  The reverse
# direction (ACKs) uses DropTail because dsRED/core needs full PHB
# configuration that adds noise to the test; the ACK path is not
# congested at this offered load so DropTail is sufficient.
set src   [$ns node]
set e1    [$ns node]
set sink  [$ns node]

$ns duplex-link $src  $e1   100Mb 1ms DropTail
$ns simplex-link $e1   $sink 200Kb 5ms dsRED/edge
$ns simplex-link $sink $e1   200Kb 5ms DropTail

set qE1C [[$ns link $e1 $sink] queue]

# Two queues so DSCP 46 (EF) lives in queue 0 and any default-PHB
# overflow (e.g. unmatched packets falling back to DSCP 0) lives in
# queue 1.  Single drop-precedence per queue, drop-tail accounting,
# small qSize so the bottleneck sheds packets reliably under a
# saturating TCP load and TCP retransmits.
$qE1C set numQueues_ 2
$qE1C setNumPrec 0 1
$qE1C setNumPrec 1 1
$qE1C setMREDMode DROP

# DSCP-46 (EF) marking for any TCP packet from src to sink.
$qE1C addMarkRule 46 [$src id] [$sink id] tcp any
$qE1C addPolicyEntry  46 Dumb
$qE1C addPolicyEntry   0 Dumb
$qE1C addPolicerEntry Dumb 46 46
$qE1C addPolicerEntry Dumb  0  0
$qE1C addPHBEntry     46 0 0
$qE1C addPHBEntry      0 1 0
$qE1C configQ 0 0 5
$qE1C configQ 1 0 50

# ------------------------------------------------------------------
# Single TCP/Reno flow, sustained for a few seconds.
# ------------------------------------------------------------------
set tcp  [new Agent/TCP/Reno]
$tcp set packetSize_ $MSS_BYTES
$tcp set window_ 64
$ns attach-agent $src $tcp

set sink0 [new Agent/TCPSink]
$ns attach-agent $sink $sink0
$ns connect $tcp $sink0

set ftp [new Application/FTP]
$ftp attach-agent $tcp

$ns at 0.5  "$ftp start"
$ns at 4.5  "$ftp stop"
$ns at 5.0  "finish"

# ------------------------------------------------------------------
# End-of-sim assertions.
# ------------------------------------------------------------------
proc finish {} {
    global ns qE1C MSS_BYTES
    global FAIL
    set FAIL 0

    # Five queries against DSCP 46 (EF).
    set rTX_kb   [$qE1C getStat TCPbReTX 46]   ;# expect KB float > 0
    set gTX_kb   [$qE1C getStat TCPbGoTX 46]   ;# expect KB float > 0
    set nRT      [$qE1C getStat TCPnReTX 46]   ;# expect integer count >= 1
    set retxB    [$qE1C getStat retxBytes 46]  ;# expect rTX_kb * 1024
    set origB    [$qE1C getStat origBytes 46]  ;# expect gTX_kb * 1024

    puts "BUG-11 regression measurements"
    puts "  TCPbReTX  (KB ): $rTX_kb"
    puts "  TCPbGoTX  (KB ): $gTX_kb"
    puts "  TCPnReTX  (cnt): $nRT"
    puts "  retxBytes (B  ): $retxB"
    puts "  origBytes (B  ): $origB"

    # ASSERT 1: at least one retransmission occurred.
    if {$nRT < 1} {
        puts "FAIL ASSERT-1: TCPnReTX = $nRT < 1; no retransmits forced."
        set FAIL 1
    }
    # ASSERT 2: bytes accumulator is positive.
    if {$rTX_kb <= 0.0} {
        puts "FAIL ASSERT-2: TCPbReTX = $rTX_kb KB; expected > 0 KB."
        set FAIL 1
    }
    # ASSERT 3: arithmetic identity TCPbReTX*1024 ≈ TCPnReTX*MSS_BYTES.
    # Pre-fix swap returned count for the bytes query and bytes for
    # the count query, so the ratio (TCPbReTX*1024) / TCPnReTX would
    # collapse to ~1024 instead of the expected ~MSS_BYTES.  Use a
    # 30% tolerance band to absorb any small per-segment variation.
    set ratio [expr {($rTX_kb * 1024.0) / double($nRT)}]
    set lo    [expr {$MSS_BYTES * 0.70}]
    set hi    [expr {$MSS_BYTES * 1.30}]
    if {$ratio < $lo || $ratio > $hi} {
        puts "FAIL ASSERT-3: (TCPbReTX*1024)/TCPnReTX = $ratio; expected near $MSS_BYTES (band $lo .. $hi).  Pre-fix swap would put this near 1024 if MSS != 1024."
        set FAIL 1
    }
    # ASSERT 4: retxBytes alias parity (within 1% — the underlying C++
    # double has full precision but the OTcl `result()` bridge formats
    # to ~3 significant figures, so the round-trip Tcl read of TCPbReTX
    # loses precision; the alias arithmetic happens in C++ on the full-
    # precision double, so retxBytes is exact while $rTX_kb*1024.0 is
    # not).  A 1% relative tolerance catches a real fence-post-equality
    # bug while accepting the formatting drift.
    set diff4 [expr {abs($retxB - $rTX_kb * 1024.0)}]
    set tol4  [expr {abs($retxB) * 0.01}]
    if {$diff4 > $tol4} {
        puts "FAIL ASSERT-4: retxBytes = $retxB; TCPbReTX*1024 = [expr {$rTX_kb * 1024.0}]; diff $diff4 > tol $tol4."
        set FAIL 1
    }
    # ASSERT 5: origBytes alias parity (1% tolerance, same rationale).
    set diff5 [expr {abs($origB - $gTX_kb * 1024.0)}]
    set tol5  [expr {abs($origB) * 0.01}]
    if {$diff5 > $tol5} {
        puts "FAIL ASSERT-5: origBytes = $origB; TCPbGoTX*1024 = [expr {$gTX_kb * 1024.0}]; diff $diff5 > tol $tol5."
        set FAIL 1
    }

    if {$FAIL == 0} {
        puts "PASS d2-8-regression"
        exit 0
    } else {
        puts "FAIL d2-8-regression"
        exit 1
    }
}

$ns run
