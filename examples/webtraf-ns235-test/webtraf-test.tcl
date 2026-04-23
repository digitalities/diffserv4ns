# webtraf-test.tcl
# Tests that PagePool/WebTraf works correctly with DiffServ4NS on ns-2.35.
# On ns-2.29+DS4 this crashed due to hdr_cmn OFFSET macro computing wrong
# header offsets after DS4 added sendtime_/app_type_ fields to hdr_cmn.
# ns-2.35 uses the safer OFFSET form (#define OFFSET(t,f) ((long)&((t*)256)->f - 256)),
# so the crash should be fixed.

set ns [new Simulator]

# Minimal topology: one client (n0) -> router (n1) -> server (n2)
# 10 Mbps, 10 ms link to give TCP room to operate
set n(0) [$ns node]
set n(1) [$ns node]
set n(2) [$ns node]

$ns duplex-link $n(0) $n(1) 10Mb 10ms DropTail
$ns duplex-link $n(1) $n(2) 10Mb 10ms DropTail

# Create PagePool/WebTraf with 1 client, 1 server
set pool [new PagePool/WebTraf]

$pool set-num-client 1
$pool set-num-server 1
$pool set-client 0 $n(0)
$pool set-server 0 $n(2)

# Single session: 5 pages, starts at 0.1s
# Use modest inter-page and object sizes to keep simulation short
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
    global ns
    $ns flush-trace
    puts "WEBTRAF OK"
    exit 0
}

$ns at 25.0 "finish"
$ns run
