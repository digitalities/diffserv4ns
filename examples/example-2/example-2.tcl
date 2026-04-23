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


set alg [lindex $argv 0]

set ns [new Simulator]

set EFPacketSize   1300                  ; # EF packet size in bytes

set EFRate         300000b               ; # 300kbit/s rate for EF traffic
set BGRate         100000b               ; # 1Mbit/s   rate for background traffic

set algBW          2000000               ; # Bandwith between interfaces DS enabled

set testTime       100

set rndStartTime [new RNG]
$rndStartTime seed 0 	            ; # seeds the RNG heuristically
set rndSourceNode [new RNG]
$rndSourceNode seed 0               ; # seeds the RNG heuristically

#tracing
set ServiceRate      [open ServiceRate.tr       w]
set ClassRate        [open ClassRate.tr         w]
set PELoss           [open PELoss.tr            w]
set PLLoss           [open PLLoss.tr            w]
set QueueLen         [open QueueLen.tr          w]
set VirtualQueueLen  [open VirQueueLen.tr       w]
set Goodput          [open Goodput.tr           w]
set OWD              [open OWD.tr               w]
set IPDV             [open IPDV.tr              w]

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

#set txTime         [expr $EFPacketSize*8.0/2000]; # transmission time for a EF packet in millisec                      

$ns duplex-link $core $e2 5Mb 3ms DropTail

$ns duplex-link $e2 $dest(0) 100Mb 1ms DropTail
$ns duplex-link $e2 $dest(1) 100Mb 1ms DropTail
$ns duplex-link $e2 $dest(2) 100Mb 1ms DropTail
$ns duplex-link $e2 $dest(3) 100Mb 1ms DropTail
$ns duplex-link $e2 $dest(4) 100Mb 1ms DropTail

set qE1C [[$ns link $e1 $core] queue]
set qCE1 [[$ns link $core $e1] queue]

# Set DS RED parameters from Edge1 to Core:
$qE1C set numQueues_ 3

if {($alg=="PQ")} {
$qE1C setSchedularMode PRI 
}

if {($alg=="SCFQ")} {
$qE1C setSchedularMode SCFQ
$qE1C addQueueWeight 0     3
$qE1C addQueueWeight 1    10
$qE1C addQueueWeight 2     7
}

if {($alg=="LLQ")} {
$qE1C setSchedularMode LLQ SFQ 1700000
$qE1C addQueueWeight 1    10
$qE1C addQueueWeight 2     7
}


# Premium Service

# queue and precedence levels settings
$qE1C setQSize   0  50
$qE1C setNumPrec 0   2            ;# Premium Service            queue 0, two levels of precedence

#classifying and marking
$qE1C addMarkRule 46 [$s(0) id] -1 any any ;# packets coming from s0 are marked for premium service

#metering
$qE1C addPolicyEntry   46 TokenBucket 500000 100000 ;# cir in bit/s, cbs in bytes
$qE1C addPolicerEntry  TokenBucket  46 51           ;# packets out of the bucket are marked with DSCP 51
$qE1C addPHBEntry 46 0 0
$qE1C addPHBEntry 51 0 1


#shaping/dropping
$qE1C setMREDMode DROP 0
$qE1C configQ 0 0  30   ;#max  in-profile packets in queue 0
$qE1C configQ 0 1  -1   ;#drop all out-of-profile packets


# Gold Service
               
# queue and precedence levels settings
$qE1C setQSize   1 150
$qE1C setNumPrec 1   3   ;# Gold Service  queue 1, two levels of precedence, AF11 for telnet,  AF12,AF13 for ftp,
                
#classifying and marking
$qE1C addMarkRule 10 -1 -1 any telnet           ;# telnet packets are marked with DSCP=10
$qE1C addMarkRule 12 -1 -1 any ftp              ;# FTP packets are marked with DSCP=12
               
#metering
$qE1C addPolicyEntry  10 Dumb  ; #no policy for telnet
$qE1C addPolicerEntry    Dumb 10

$qE1C addPolicyEntry  12 TSW2CM [expr $algBW/2/2]  ; #when ftp exceeds 0.5Mbit/s, stronger drop
$qE1C addPolicerEntry    TSW2CM 12 14

$qE1C addPHBEntry 10 1 0
$qE1C addPHBEntry 12 1 1
$qE1C addPHBEntry 14 1 2

#shaping/dropping
$qE1C setMREDMode RIO-C 1
$qE1C meanPktSize 1300               ;# needed by the setQueueBW  
$qE1C setQueueBW  1 1000000          ;# the alg is related to the bw assigned to the service
$qE1C configQ     1 0 60 110 0.02
$qE1C configQ     1 1 30  60  0.6
$qE1C configQ     1 2  5  10  0.8


# Best Effort Service
           
# queue and precedence levels settings
$qE1C setQSize   2 100  
$qE1C setNumPrec 2   2   ;# Best Effort Service     queue 2, two levels of precedence
                
#classifying and marking
#no rules applyed, all packets that do not match any rules are marked with default codepoint DSCP=0

#metering
$qE1C addPolicyEntry   0 TokenBucket 700000 100000 ;# cir in bit/s, cbs in bytes
$qE1C addPolicerEntry TokenBucket  0 50
$qE1C addPHBEntry  0 2 0  
$qE1C addPHBEntry 50 2 1
                
#shaping/dropping
$qE1C setMREDMode DROP 2
$qE1C configQ 2 0  100   ;#max  in-profile packets in queue 0
$qE1C configQ 2 1  -1    ;#drop all out-of-profile packets


# Set DS RED parameters from Core to Edge1:
$qCE1 setMREDMode DROP 
$qCE1 set numQueues_ 	1
$qCE1 setQSize          0 60  
$qCE1 setNumPrec 	1 
$qCE1 configQ           0  0 50 
$qCE1 addPHBEntry      10  0  0
$qCE1 addPHBEntry       0  0  0

# CBR TRAFFIC ACTIVATION
source utils.tcl

# EF - CBR traffic
set startTime [$rndStartTime uniform 1 3]
cbr_connection 0 0 $s(0) $dest(0) 1 $EFPacketSize $EFRate $startTime $testTime
puts "EF: cbr connection starting at time $startTime"


# TELNET TRAFFIC
for {set i 0} {$i < 12} {incr i} {
    telnet_connection [expr $i+1000] $s([expr $i/4+1]) $dest([expr $i/4+1]) 0 $i
}
        
# FTP TRAFFIC
for {set i 0} {$i < 12} {incr i} {
    ftp_connection [expr $i+2000] $s([expr $i/4+1]) $dest([expr $i/4+1]) $i
}

# creating background traffic with different flows
set BGPacketSize 64
set i 0

while {$i<23} {
	set startTime   [$rndStartTime uniform 0 2]
        set sourceNode  [expr [$rndSourceNode integer 4]+1]
        set destNode    [expr [$rndSourceNode integer 4]+1]
	set flowBW    100000
	cbr_connection [expr $i+10] 1 $s($sourceNode) $dest($destNode) 0 $BGPacketSize $flowBW $startTime $testTime
        puts "flow $i: cbr connection ($sourceNode -> $destNode) starting at time $startTime, rate $flowBW Kbps, packet size 
$BGPacketSize"
	set i [expr $i+1]
	set BGPacketSize [expr $BGPacketSize+64]
}


proc record_goodput {} {
  global qE1C Goodput
  
  #Get an instance of the simulator
  set ns [Simulator instance]
   
  #Get the current time
  set now [$ns now]
          
  #Set the time after which the procedure should be called again
  set time 1
  
  set   r10	 [$qE1C getStat TCPbReTX 10]
  set   r12	 [$qE1C getStat TCPbReTX 12] 
  set   r14	 [$qE1C getStat TCPbReTX 14]
  set   g10	 [$qE1C getStat TCPbGoTX 10]
  set   g12	 [$qE1C getStat TCPbGoTX 12]
  set   g14	 [$qE1C getStat TCPbGoTX 14]

  puts  $Goodput  "$now [expr $g10/($g10+$r10)] [expr $g12/($g12+$r12)] [expr $g14/($g14+$r14)]"

  #Re-schedule the procedure
  $ns at [expr $now+$time] "record_goodput"
}

proc record_departure_rate {} {
          global qE1C ServiceRate ClassRate
          #Get an instance of the simulator
          set ns [Simulator instance]

          #Get the current time
          set now [$ns now]

          #Set the time after which the procedure should be called again
          set time 1

          set EFRate        [expr [$qE1C getDepartureRate  0 0]/1000 ]
          set TelnetRate    [expr [$qE1C getDepartureRate  1 0]/1000 ]
          set FtpRate       [expr ([$qE1C getDepartureRate 1 1]+[$qE1C getDepartureRate 1 2])/1000 ]   
          set BERate        [expr [$qE1C getDepartureRate  2  ]/1000 ]
          puts $ClassRate   "$now $EFRate $TelnetRate $FtpRate $BERate"
          
          set PremiumRate   [expr [$qE1C getDepartureRate 0  ]/1000 ]
          set GoldRate      [expr [$qE1C getDepartureRate 1  ]/1000 ]
          set BERate        [expr [$qE1C getDepartureRate 2  ]/1000 ]
          puts $ServiceRate "$now $PremiumRate $GoldRate $BERate"

      	  #Re-schedule the procedure
          $ns at [expr $now+$time] "record_departure_rate"
}

proc record_delay {} {
          global Sink_ OWD IPDV 

          #Get an instance of the simulator
          set ns [Simulator instance]
          #Set the time after which the procedure should be called again
          set time 0.5

          #Get the current time
          set now [$ns now]

          set EF_OWD     [expr 1000*[$Sink_(0) set owd_]]
          puts $OWD      "$now $EF_OWD" 

          set EF_IPDV     [expr 1000*[$Sink_(0) set ipdv_]]
          puts $IPDV      "$now $EF_IPDV" 
          
       	  #Re-schedule the procedure
          $ns at [expr $now+$time] "record_delay"
  }


proc record_queue_len {} {
          global QueueLen VirtualQueueLen qE1C

          #Get an instance of the simulator
          set ns [Simulator instance]
          #Set the time after which the procedure should be called again
          set time 0.5

          set TelnetQueue    [$qE1C getVirtQueueLen 1 0]
          set FTPinQueue     [$qE1C getVirtQueueLen 1 1]
          set FTPoutQueue    [$qE1C getVirtQueueLen 1 2]

	  set PremiumQueue   [$qE1C getQueueLen     0]
	  set GoldQueue      [$qE1C getQueueLen     1]
          set BEQueue        [$qE1C getQueueLen     2]

          #Get the current time
          set now [$ns now]

          puts $QueueLen "$now $PremiumQueue $GoldQueue $BEQueue" 
          puts $VirtualQueueLen  "$now $TelnetQueue $FTPinQueue $FTPoutQueue" 

       	  #Re-schedule the procedure
          $ns at [expr $now+$time] "record_queue_len"
}



proc record_packet_loss {} {
          global PELoss PLLoss qE1C
          
          #Get an instance of the simulator
          set ns [Simulator instance]
          #Set the time after which the procedure should be called again
          set time 2
          
          #Get the current time
          set now [$ns now]
  set edropsTelnet [expr [$qE1C getStat edrops 10]*100.0/[$qE1C getStat pkts 10]]
  set edropsFTP    [expr ([$qE1C getStat edrops 12]+[$qE1C getStat edrops 14])*100.0/([$qE1C getStat pkts 12]+[$qE1C getStat pkts 14])]
          puts $PELoss "$now  $edropsTelnet $edropsFTP"

  set dropsTelnet [expr [$qE1C getStat drops 10]*100.0/[$qE1C getStat pkts 10]]  
  set dropsFTP    [expr ([$qE1C getStat drops 12]+[$qE1C getStat drops 14])*100.0/([$qE1C getStat pkts 12]+[$qE1C getStat pkts 14])]                
          puts $PLLoss "$now  $dropsTelnet $dropsFTP"

          #Re-schedule the procedure
          $ns at [expr $now+$time] "record_packet_loss"
}

proc finish {} {
global ns Sink_ OWD IPDV EFPacketSize  alg QueueLen VirtualQueueLen qE1C PELoss PLLoss ServiceRate ClassRate

    $ns flush-trace

    close $OWD 
    close $IPDV 
    close $PELoss       
    close $PLLoss       
    close $ServiceRate       
    close $ClassRate
    close $QueueLen
    close $VirtualQueueLen

    catch { exec gnuplot ServiceRate.p }
    catch { exec gnuplot ClassRate.p }
    catch { exec gnuplot owd.p }
    catch { exec gnuplot ipdv.p }
    catch { exec gnuplot queue.p }
    catch { exec gnuplot virqueue.p }
    catch { exec gnuplot pktLoss.p }
    catch { exec gnuplot goodput.p }

    exit 0
}


puts "EF packet size: $EFPacketSize"
$qE1C printPolicyTable
$qE1C printPolicerTable
$ns at 0.0 "record_departure_rate"
$ns at 5 "record_packet_loss"
$ns at 5 "record_goodput"
$ns at 6   "record_delay"
$ns at 6 "record_queue_len"
$ns at [expr $testTime/2]     "$qE1C printStats"
$ns at [expr $testTime - 0.1] "$qE1C printStats"
$ns at $testTime "finish"

$ns run
