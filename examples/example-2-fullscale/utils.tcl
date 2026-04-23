# Copyright (C) 2001-2026  Sergio Andreozzi
#
# DISCLAIMER: This is a 2026 reconstruction of a Tcl helper library for
# thesis Scenario 2 (Section 4.2). The original 2001/2006 DiffServ4NS
# release did not ship a Tcl script for the 469-node full-scale scenario.
# This file is derived from the working Scenario 3 helpers
# (ns2/diffserv4ns/examples/example-3/scenario-3.tcl) with the
# VoIP/RealAudio helpers removed, since Scenario 2 does not use them.
#
# Contains:
#   Application/HTTP Tcl subclass      — overrides start{} to set PT_HTTP=31
#   http_session                       — stagger-friendly HTTP bulk xfer
#   cbr_connection                     — CBR, with explicit PT_CBR=2
#   telnet_connection                  — Telnet (tcplib model)
#   ftp_connection                     — FTP bulk transfer
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Application/HTTP Tcl subclass
# ---------------------------------------------------------------------------
# Application/FTP::start() (ns-lib source: tcl/lib/ns-source.tcl:50) hardcodes
# [$self agent] set_apptype 27   ;# PT_FTP
# which overrides any manual set_apptype call made from outside start{}.
# To get HTTP-classified bulk traffic we subclass Application/FTP and override
# start{} so it stamps PT_HTTP (31) into hdr_cmn::app_type_ before kicking
# the TCP source.
#
# See `reference_paper_snippet_apptype` in project memory and
# `project_ns2_apptype_finding` for background.
Class Application/HTTP -superclass Application/FTP
Application/HTTP instproc start {} {
    [$self agent] set_apptype $::PT_HTTP   ;# PT_HTTP — same value on ns-2.29 and ns-2.35
    [$self agent] send -1
}

# ---------------------------------------------------------------------------
# HTTP session (bulk TCP approximation of a web page download)
#
# Scenario 2 thesis §4.2 originally described HTTP sessions driven by the
# PagePool/WebTraf empirical model. That module crashes under DiffServ4NS
# because the DiffServ4NS-patched hdr_cmn struct is 12 B larger than the
# pristine ns-2.29 one and WebTraf's internal TCP agent construction
# reads past the new tail. See `project_ns2_apptype_finding` memory for
# the detailed crash analysis.
#
# The deterministic approximation used here is an independent TCP bulk
# download per "session", with a 0.25 s stagger between session starts.
# This gives comparable aggregate Bronze-queue load and is consistent
# with the approach taken in the working Scenario 3 reconstruction.
# ---------------------------------------------------------------------------
proc http_session {id src dst start stop} {
    global ns
    set tcp [new Agent/TCP]
    $tcp set class_ 4
    $ns attach-agent $src $tcp

    set http [new Application/HTTP]     ;# uses PT_HTTP=31 via our subclass
    $http attach-agent $tcp

    set sink [new Agent/TCPSink]
    $ns attach-agent $dst $sink

    $ns connect $tcp $sink

    $ns at $start "$http start"
    if { $stop > $start } {
        $ns at $stop  "$http stop"
    }
}

# ---------------------------------------------------------------------------
# http_bursty_session — WebTraf-style page-browsing session (approximation)
#
# Thesis §4.2 originally drove HTTP via PagePool/WebTraf with these params:
#   - 400 sessions total (per traffic loop in scenario-2.tcl)
#   - 250 pages per session (constant)
#   - inter-page idle time ~Exp(15s)
#   - page size = 1 object per page (pageSize=1 in WebTraf API)
#   - object size ~ParetoII(avg=12KB, shape=1.2)
#
# PagePool/WebTraf crashes under DiffServ4NS (hdr_cmn struct mismatch, see
# the http_session header above), so we simulate the same behaviour with a
# persistent TCP connection whose 'send N' events are scheduled at Pareto-
# drawn byte counts with exponentially-distributed idle gaps between them.
#
# Each session holds its TCP connection open for the duration of the
# simulation. When the session's 250 scheduled page-sends are done, the
# connection sits idle but stays alive (no tear-down cost).
#
# Required globals (set once in scenario-2.tcl before traffic creation):
#   interPageRV  - RandomVariable/Exponential with avg_=15
#   objSizeRV    - RandomVariable/ParetoII    with avg_=12 and shape_=1.2
# ---------------------------------------------------------------------------
proc http_bursty_session {id src dst startTime numPages endTime} {
    global ns interPageRV objSizeRV
    set tcp [new Agent/TCP]
    $tcp set class_ 4
    $tcp set_apptype $::PT_HTTP   ;# PT_HTTP — same value on ns-2.29 and ns-2.35
    $ns attach-agent $src $tcp

    set sink [new Agent/TCPSink]
    $ns attach-agent $dst $sink
    $ns connect $tcp $sink

    set t $startTime
    for {set p 0} {$p < $numPages} {incr p} {
        if {$t >= $endTime} break
        set objKB [$objSizeRV value]
        # Clamp Pareto tail so a single outlier doesn't stall the whole session.
        if {$objKB < 1}    {set objKB 1}
        if {$objKB > 1000} {set objKB 1000}
        set bytes [expr int($objKB * 1024)]
        $ns at $t "$tcp send $bytes"
        set gap [$interPageRV value]
        set t [expr $t + $gap]
    }
}

# ---------------------------------------------------------------------------
# CBR connection (background UDP)
#
# DiffServ4NS CBR classification requires the UDP agent to be stamped with
# PT_CBR=2 manually because the stock Application/Traffic/CBR does not call
# set_apptype (unlike FTP/Telnet which have app_type hardcoded in their
# start{} methods). Without this line the packets fall through to the
# default class even if a DSCP-50 rule is set.
# ---------------------------------------------------------------------------
proc cbr_connection {id class src dst start packetSize rate streamOffset stop} {
    global ns
    set udp [new Agent/UDP]
    $udp set class_ $class
    $udp set_apptype $::PT_CBR           ;# PT_CBR — same value on ns-2.29 and ns-2.35
    $ns attach-agent $src $udp

    set cbr [new Application/Traffic/CBR]
    $cbr attach-agent $udp
    $cbr set packet_size_ $packetSize
    $udp set packetSize_  $packetSize
    $cbr set rate_        $rate
    if { $streamOffset > 0 } {
        $cbr set random_ 1
    }

    set sink [new Agent/Null]
    $ns attach-agent $dst $sink
    $ns connect $udp $sink

    $ns at $start "$cbr start"
    if { $stop > $start } {
        $ns at $stop  "$cbr stop"
    }
}

# ---------------------------------------------------------------------------
# Telnet connection (tcplib-telnet inter-arrival model)
#
# Thesis §4.2: "Both FTP and Telnet traffics are activated during the first
# 50 seconds of simulation." We interpret this literally — traffic active
# only during [0, stop], then silent. This matches Table 4.4's DP0 caPL=0%
# (Telnet generates negligible aggregate load if only active for 50s).
# ---------------------------------------------------------------------------
proc telnet_connection {id src dst start interarrival {stop 0}} {
    global ns
    set tcp [new Agent/TCP]
    $tcp set class_ 2
    $ns attach-agent $src $tcp

    set sink [new Agent/TCPSink]
    $ns attach-agent $dst $sink

    set telnet [new Application/Telnet]
    $telnet attach-agent $tcp
    $telnet set interval_ $interarrival

    $ns connect $tcp $sink
    $ns at $start "$telnet start"
    if { $stop > $start } {
        $ns at $stop "$telnet stop"
    }
}

# ---------------------------------------------------------------------------
# FTP connection (configurable: bulk or finite transfer)
#
# Thesis §4.2: "Both FTP and Telnet traffics are activated during the first
# 50 seconds of simulation." Under the literal reading FTP is active only
# during [0, stop]. The thesis is silent on whether each FTP connection is
# a single finite-size file transfer or a persistent bulk stream.
#
# Table 4.4's DP1 caPL=0.01-5% across the six sets is inconsistent with
# 50 bulk streams saturating DP1 for the full 50-second window (which
# yields 6-11% caPL in practice). Passing an explicit bytes argument
# models each connection as a single file download ending when the bytes
# are delivered — closer to what real FTP exchanges look like. bytes=0
# (default) preserves the legacy bulk-until-stop behaviour.
# ---------------------------------------------------------------------------
proc ftp_connection {id src dst start {stop 0} {bytes 0}} {
    global ns
    set tcp [new Agent/TCP]
    $tcp set class_ 2
    # Stamp PT_FTP=27 at agent creation. Application/FTP::start{} in
    # ns-source.tcl:50 does this too, but only on the `start` path;
    # `$ftp send N` skips it, which silently routes packets to the
    # Default DSCP (0) instead of DSCP 12 (AF PHB DP1). Setting it here
    # makes both code paths behave consistently.
    $tcp set_apptype $::PT_FTP           ;# PT_FTP — same value on ns-2.29 and ns-2.35
    $ns attach-agent $src $tcp

    set ftp [new Application/FTP]
    $ftp attach-agent $tcp

    set sink [new Agent/TCPSink]
    $ns attach-agent $dst $sink

    $ns connect $tcp $sink
    if { $bytes > 0 } {
        $ns at $start "$ftp send $bytes"
    } else {
        $ns at $start "$ftp start"
    }
    if { $stop > $start } {
        $ns at $stop "$ftp stop"
    }
}
