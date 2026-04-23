# Copyright (C) 2001-2006  Sergio Andreozzi
#
# This file is part of DiffServ4NS, a set of improvements to
# the Network Simulator 2 for DiffServ simulations.
#
# Project page: http://diffserv4ns.sourceforge.net/
#
# DiffServ4NS is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# DiffServ4NS is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with DiffServ4NS; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
# GNU License: http://www.gnu.org/licenses/gpl.txt



set alg            [lindex $argv 0]
set testTime       [lindex $argv 1]
set EFPacketSize   [lindex $argv 2]
set quiet false 
	
set ns [new Simulator]

set cirEF  	   300000			; # parameters for Token Bucket Policer
set cbsEF    	   [expr $EFPacketSize+1]
set EFRate         300000b

#set cbsEF    	   [expr $EFPacketSize]		; #

set BGRate         100000b
set BGPacketSize   64

set rndStartTime [new RNG]
$rndStartTime seed 0 				; # seeds the RNG heuristically
set rndSourceNode [new RNG]
$rndSourceNode seed 0

set txTime         [expr $EFPacketSize*8.0/2000]; # in millisec

set sumEFQueueLen    0
set sumBEQueueLen    0
set samplesNum       0

#tracing
if {($quiet == "false")} {

set ServiceRate  [open ServiceRate.tr w]

set EFQueueLen  [open EFQueueLen.tr  w]
set BEQueueLen  [open BEQueueLen.tr  w]

set OWD  [open OWD.tr w]
set IPDV [open IPDV.tr w]
}

#set f [open test1.out w]
#$ns trace-all $f

#set nf [open test1.nam w]
#$ns namtrace-all $nf

# Set up the network topology shown at the top of this file:
set s(0) [$ns node]
set s(1) [$ns node]
set s(2) [$ns node]
set s(3) [$ns node]
set s(4) [$ns node]


set e1 [$ns node]
set core [$ns node]
set e2 [$ns node]

set dest(0) [$ns node]
set dest(1) [$ns node]
set dest(2) [$ns node]
set dest(3) [$ns node]
set dest(4) [$ns node]

$ns duplex-link $s(0) $e1 100Mb 1ms DropTail
$ns duplex-link $s(1) $e1 100Mb 1ms DropTail
$ns duplex-link $s(2) $e1 100Mb 1ms DropTail
$ns duplex-link $s(3) $e1 100Mb 1ms DropTail
$ns duplex-link $s(4) $e1 100Mb 1ms DropTail

$ns simplex-link $e1 $core 2Mb 5ms dsRED/edge
$ns simplex-link $core $e1 2Mb 5ms dsRED/core

$ns duplex-link $core $e2 5Mb 3ms DropTail

$ns duplex-link $e2 $dest(0) 100Mb 1ms DropTail
$ns duplex-link $e2 $dest(1) 100Mb 1ms DropTail
$ns duplex-link $e2 $dest(2) 100Mb 1ms DropTail
$ns duplex-link $e2 $dest(3) 100Mb 1ms DropTail
$ns duplex-link $e2 $dest(4) 100Mb 1ms DropTail

set qE1C [[$ns link $e1 $core] queue]
set qCE1 [[$ns link $core $e1] queue]

# Set DS RED parameters from Edge1 to Core:
$qE1C set numQueues_ 2
$qE1C setNumPrec 0 2; # queue 0, two levels of precedence
$qE1C setNumPrec 1 1; # queue 1, one level of precedence

if {($alg=="PQ")} {
$qE1C setSchedularMode PRI 
}

if {($alg=="WFQ")} {
$qE1C setSchedularMode WFQ
$qE1C addQueueWeight 0     3
$qE1C addQueueWeight 1     17
}

if {($alg=="SCFQ")} {
$qE1C setSchedularMode SCFQ
$qE1C addQueueWeight 0     3
$qE1C addQueueWeight 1     17
}

if {($alg=="SFQ")} {
$qE1C setSchedularMode SFQ
$qE1C addQueueWeight 0     3
$qE1C addQueueWeight 1    17 
}

if {($alg=="WF2Qp")} {
$qE1C setSchedularMode WF2Qp
$qE1C addQueueWeight 0     3
$qE1C addQueueWeight 1     17
}

$qE1C setQSize 0 30
$qE1C setQSize 1 50

$qE1C setMREDMode DROP ;# could be also specified for each queue: e.g.  $qE1C setMREDMode DROP 0

$qE1C addMarkRule 46 -1 [$dest(0) id] any any ;# EF   recommend codepoint
$qE1C addMarkRule  0 -1 [$dest(1) id] any any ;# BE   recommend codepoint

$qE1C addPolicyEntry  46 TokenBucket $cirEF $cbsEF 	;# depending on SLS
$qE1C addPolicyEntry  48 Dumb
$qE1C addPolicyEntry   0 Dumb
  
$qE1C addPolicerEntry TokenBucket 46 48
$qE1C addPolicerEntry Dumb  0 0

$qE1C addPHBEntry 46 0 0
$qE1C addPHBEntry 48 0 1
$qE1C addPHBEntry  0 1 0

$qE1C configQ 0 0 30
$qE1C configQ 0 1 -1 
$qE1C configQ 1 0 50 

# Set DS RED parameters from Core to Edge1:
$qCE1 setMREDMode DROP 
$qCE1 set numQueues_ 	1
$qCE1 setNumPrec 	1
$qCE1 addPHBEntry  10 0 0
$qCE1 configQ      0 1 50 

# CBR TRAFFIC ACTIVATION
source traffic-generator.tcl

# EF
set startTime [$rndStartTime uniform 0 5]
set sourceNodeID [$rndSourceNode integer 5]
cbr_connection 0 0 $s($sourceNodeID) $dest(0) 0 $EFPacketSize $EFRate $startTime $testTime
if {($quiet == "false")} {
	puts "EF : s($sourceNodeID)->d(0) - Traffic: CBR - PktSize: $EFPacketSize - Rate: $EFRate - Start $startTime"
}

$Sink_(0) FrequencyDistribution 0.00001 0.00001 $EFPacketSize\_FD.tr

# BE

for {set i 0} {$i < 20} {incr i} {
        set startTime [$rndStartTime uniform 0 5]
        set sourceNodeID [$rndSourceNode integer 5]
	cbr_connection [expr $i+40] 1 $s($sourceNodeID) $dest(1) 1 $BGPacketSize $BGRate $startTime $testTime
	puts "BE: s($sourceNodeID)->d(1) - Traffic: CBR - PktSize: $BGPacketSize - Rate: $BGRate - Start $startTime"
	set BGPacketSize [expr $BGPacketSize+64]
}

proc record_departure_rate {} {
          global qE1C ServiceRate
          #Get an instance of the simulator
          set ns [Simulator instance]

          #Get the current time
          set now [$ns now]

          #Set the time after which the procedure should be called again
          set time 1

	  set EFRate   	    [expr [$qE1C getDepartureRate 0  ]/1000 ]
#	  set AF1Rate       [expr [$qE1C getDepartureRate 1  ]/1000 ]
#	  set AF2Rate       [expr [$qE1C getDepartureRate 2  ]/1000 ]
#	  set AF3Rate       [expr [$qE1C getDepartureRate 3  ]/1000 ]
	  set BERate        [expr [$qE1C getDepartureRate 1  ]/1000 ] 
	  puts $ServiceRate "$now $EFRate $BERate" 

      	  #Re-schedule the procedure
          $ns at [expr $now+$time] "record_departure_rate"
}

proc record_delay {} {
          global Sink_ OWD IPDV txTime
          #Get an instance of the simulator
          set ns [Simulator instance]
          #Set the time after which the procedure should be called again
          set time 0.5

	  set sumOWD  [$Sink_(0) set sumOwd_]
	  set sumIPDV [$Sink_(0) set sumIpdv_]
	  set pkts    [$Sink_(0) set npktsFlowid_]

          #Get the current time
          set now [$ns now]

          if {($pkts<2)} { puts $OWD "$now 0" } else {   
          puts $OWD "$now [expr $sumOWD*1000/$pkts-$txTime]" }
          puts $IPDV "$now [expr $sumIPDV*1000/($pkts-1)]"
          
       	  #Re-schedule the procedure
          $ns at [expr $now+$time] "record_delay"
  }


proc record_queue_len {} {
          global EFQueueLen BEQueueLen qE1C samplesNum sumEFQueueLen  sumBEQueueLen quiet

          #Get an instance of the simulator
          set ns [Simulator instance]
          #Set the time after which the procedure should be called again
          set time 0.5

	  set queue0Len [$qE1C getQueueLen 0]
	  set queue1Len [$qE1C getQueueLen 1]

	  set sumEFQueueLen  [expr $sumEFQueueLen +$queue0Len ]
	  set sumBEQueueLen  [expr $sumBEQueueLen +$queue1Len ]
	  set samplesNum     [expr $samplesNum+1]

          #Get the current time
          set now [$ns now]

	  if {($quiet == "false")} {
          	puts $EFQueueLen  "$now $queue0Len" 
          	puts $BEQueueLen  "$now $queue1Len" 
          }

       	  #Re-schedule the procedure
          $ns at [expr $now+$time] "record_queue_len"
}




proc finish {} {
    global ns   Sink_ OWD IPDV ServiceRate quiet EFPacketSize  alg EFQueueLen BEQueueLen sumEFQueueLen samplesNum qE1C

    $ns flush-trace
    $Sink_(0) flushFD

    if {($quiet == "false")} {	

    	close $OWD 
        close $IPDV 
        close $ServiceRate       
	close $EFQueueLen
	close $BEQueueLen
	
	source gnuplot-x.tcl
	catch { exec gnuplot bw_x.p }
	catch { exec gnuplot owd_x.p }
	catch { exec gnuplot ipdv_x.p }
	catch { exec gnuplot queue_x.p }
	catch { exec gnuplot owdFD_x.p }
	catch { exec gnuplot ipdvFD_x.p }
    }

    set gIPDV      [open "$alg\_IPDV.tr" a]
    set gOWD       [open "$alg\_OWD.tr"  a]
    set gQueueLen  [open "$alg\_QueueLen.tr"  a]
    set gPktLoss   [open "$alg\_PktLoss.tr"  a]
    set gEFPktLoss [open "$alg\_EFPktLoss.tr"  a]
    set gEF_MBS    [open "$alg\_EF_MBS.tr"  a]

    set sumOWD  [$Sink_(0) set sumOwd_]
    set sumIPDV [$Sink_(0) set sumIpdv_]
    set npkts   [$Sink_(0) set npktsFlowid_]
    puts $gOWD      "$EFPacketSize [expr $sumOWD*1000/$npkts]"
    puts $gIPDV     "$EFPacketSize [expr $sumIPDV*1000/($npkts-1)]"
    puts $gQueueLen "$EFPacketSize [expr $sumEFQueueLen/$samplesNum]"

    set  EF_MBS    [$qE1C set MBS0_ ]
    puts $gEF_MBS  "$EFPacketSize $EF_MBS"

    set  Pkt	   [$qE1C getStat pkts   ] 
    set  dropPkt   [$qE1C getStat drops  ] 	
    set  edropPkt  [$qE1C getStat edrops ]
    puts $gPktLoss  "$EFPacketSize [expr ($Pkt-$dropPkt-$edropPkt)*100.0/$Pkt]  [expr $dropPkt*100.0/$Pkt] [expr $edropPkt*100.0/$Pkt]"

    set  Pkt	  [$qE1C getStat pkts   46 ]
    set  dropPkt  [$qE1C getStat drops  46 ] 	
    set  edropPkt [$qE1C getStat edrops 46 ]
    puts $gEFPktLoss  "$EFPacketSize [expr ($Pkt-$dropPkt-$edropPkt)*100.0/$Pkt]  [expr $dropPkt*100.0/$Pkt] [expr $edropPkt*100.0/$Pkt]"

    close $gOWD
    close $gIPDV
    close $gQueueLen
    close $gPktLoss
    close $gEFPktLoss
    close $gEF_MBS
 
   exit 0
}



	puts "EF packet size: $EFPacketSize"
	$qE1C printPolicyTable
	$qE1C printPolicerTable
	$ns at 0.0 "record_departure_rate"
 	$ns at 6   "record_delay"
	$ns at [expr $testTime/2]     "$qE1C printStats"
	$ns at [expr $testTime - 0.1] "$qE1C printStats"


$ns at 6 "record_queue_len"
$ns at $testTime "finish"

$ns run
