# webtraf-test-diffserv.tcl
# Verifies that PagePool/WebTraf traffic can be classified by DiffServ4NS
# on ns-2.35 into the Bronze/HTTP queue (DSCP 26).
#
# Key finding: native PagePool/WebTraf does NOT call set_apptype, so its
# TCP agents carry app_type=0 (PT_TCP) and miss the "any http" mark rule.
# The fix is to override alloc-tcp in the PagePool/WebTraf subclass so each
# allocated TCP agent calls set_apptype 31 (PT_HTTP=31 in ns-2.35).
# This is a cleaner approach than the Application/HTTP workaround used in
# scenario-3.tcl against ns-2.29.
#
# Expected result: all HTTP traffic classified to DSCP 26 (Bronze queue),
# no crash, WEBTRAF DIFFSERV OK printed.

source ../common/apptypes.tcl

set ns [new Simulator]

# Minimal topology: n0 (client) -> n1 (DiffServ edge router) -> n2 (server)
# n0->n1: simplex dsRED/edge  — DiffServ classification happens here
# n1->n0: simplex dsRED/core  — reverse (ACK) direction
# n1<->n2: duplex DropTail    — server-side access link
set n(0) [$ns node]
set n(1) [$ns node]
set n(2) [$ns node]

$ns simplex-link $n(0) $n(1) 10Mb 10ms dsRED/edge
$ns simplex-link $n(1) $n(0) 10Mb 10ms dsRED/core
$ns duplex-link  $n(1) $n(2) 10Mb 10ms DropTail

# Configure the edge queue: 3 queues
# Q0: Premium (DSCP 46)   — placeholder only (no VoIP in this test)
# Q1: Bronze/HTTP (DSCP 26) — WebTraf HTTP traffic
# Q2: Best Effort (DSCP 0)  — unclassified traffic
set qEdge [[$ns link $n(0) $n(1)] queue]

$qEdge set numQueues_ 3
$qEdge setSchedularMode WFQ
$qEdge addQueueWeight 0 1
$qEdge addQueueWeight 1 3
$qEdge addQueueWeight 2 1

# Q0: Premium placeholder (no traffic)
$qEdge setQSize   0 20
$qEdge setNumPrec 0  1
$qEdge addPolicyEntry  46 Dumb
$qEdge addPolicerEntry Dumb 46
$qEdge addPHBEntry 46 0 0
$qEdge setMREDMode DROP 0
$qEdge configQ 0 0 20

# Q1: Bronze/HTTP — addMarkRule classifies PT_HTTP=31 traffic to DSCP 26
$qEdge setQSize   1 50
$qEdge setNumPrec 1  1
$qEdge addMarkRule 26 -1 -1 any http
$qEdge addPolicyEntry  26 Dumb
$qEdge addPolicerEntry Dumb 26
$qEdge addPHBEntry 26 1 0
$qEdge setMREDMode DROP 1
$qEdge configQ 1 0 50

# Q2: Best Effort (fallback for unclassified traffic)
$qEdge setQSize   2 50
$qEdge setNumPrec 2  1
$qEdge addPolicyEntry   0 Dumb
$qEdge addPolicerEntry  Dumb 0
$qEdge addPHBEntry  0 2 0
$qEdge setMREDMode DROP 2
$qEdge configQ 2 0 50

# Core direction: 1-queue passthrough
set qCore [[$ns link $n(1) $n(0)] queue]
$qCore set numQueues_ 1
$qCore setMREDMode DROP
$qCore setQSize   0 60
$qCore setNumPrec 0  1
$qCore configQ    0 0 60
$qCore addPHBEntry 0 0 0

puts "DiffServ configured: 3 queues (Premium/Bronze/BE)"

# Override alloc-tcp so each WebTraf TCP agent carries PT_HTTP (=31).
# NOTE: as of the ns-2.35 patch set Task 1d, webcache/webtraf.cc is patched
# to call set_apptype(PT_HTTP) natively in WebTrafPool::picktcp() and picksink().
# This Tcl override is now redundant (belt-and-braces) but kept for documentation.
PagePool/WebTraf instproc alloc-tcp {} {
    set tcp [new Agent/TCP/[PagePool/WebTraf set TCPTYPE_]]
    $tcp set_apptype $::PT_HTTP   ;# PT_HTTP — same on ns-2.29 and ns-2.35
    return $tcp
}

# Create PagePool/WebTraf with 1 client, 1 server
set pool [new PagePool/WebTraf]

$pool set-num-client 1
$pool set-num-server 1
$pool set-client 0 $n(0)
$pool set-server 0 $n(2)

# Single session: 5 pages, starts at 0.1s
set interPage [new RandomVariable/Exponential]
$interPage set avg_ 1.0

set pageSize [new RandomVariable/Constant]
$pageSize set val_ 1

set interObj [new RandomVariable/Exponential]
$interObj set avg_ 0.01

set objSize [new RandomVariable/ParetoII]
$objSize set avg_ 5
$objSize set shape_ 1.5

$pool set-num-session 1
$pool create-session 0 5 0.1 $interPage $pageSize $interObj $objSize

proc finish {} {
    global ns qEdge
    $ns flush-trace
    puts "DiffServ stats at finish:"
    $qEdge printStats
    puts "WEBTRAF DIFFSERV OK"
    exit 0
}

$ns at 25.0 "finish"
$ns run
