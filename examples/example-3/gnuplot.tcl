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


set alg     [lindex $argv 0]
set EF1ps   [lindex $argv 1]
set EF2ps   [lindex $argv 2]

 
# owd plot setting
set pOWD  [open "owd.p" w]
puts $pOWD "set title \"Average OWD for EF traffic using $alg\""
puts $pOWD "set xlabel \"EF packet size (byte)\""
puts $pOWD "set ylabel \"owd (millisec)\""
puts $pOWD "set autoscale"
puts $pOWD "set out \"sc1_$alg\_OWD.png\""
puts $pOWD "set terminal png"
puts $pOWD "plot \"$alg\_OWD.tr\" using 1:2 title 'owd using $alg' with linespoints"
close $pOWD


# ipdv plot setting

set pIPDV  [open "ipdv.p" w]
puts $pIPDV "set title \"Average IPDV for EF traffic using $alg\""
puts $pIPDV "set xlabel \"EF packet size (byte)\""
puts $pIPDV "set ylabel \"ipdv (millisec)\""
puts $pIPDV "set autoscale"
puts $pIPDV "set terminal png"
puts $pIPDV "set out \"sc1_$alg\_IPDV.png\""
puts $pIPDV "plot \"$alg\_IPDV.tr\" using 1:2 title 'IPDV using $alg' with linespoints"
close $pIPDV



# queue plot setting

set pQUEUE  [open "queue.p" w]
puts $pQUEUE "set title \"Average Queue Lenght for EF traffic using $alg\""
puts $pQUEUE "set xlabel \"EF packet size (byte)\""
puts $pQUEUE "set ylabel \"queue lenght (packet)\""
#puts $pQUEUE "set autoscale"
puts $pQUEUE "set xrange \[0:1500]"
puts $pQUEUE "set yrange \[-1:52]"
puts $pQUEUE "set terminal png"
puts $pQUEUE "set out \"sc1_$alg\_QueueLen.png\""
puts $pQUEUE "plot \"$alg\_QueueLen.tr\" using 1:2 title 'queue lenght using $alg' with lines"
close $pQUEUE

# packet loss plot setting (pSize= $EF1ps)
set pQUEUE  [open "pktLoss1.p" w]
puts $pQUEUE "set title \"Packet loss for all trasmitted packets using $alg\""
puts $pQUEUE "set xlabel \"EF packet size (byte)\""
puts $pQUEUE "set ylabel \"Percentage (%)\""
#puts $pQUEUE "set autoscale"
puts $pQUEUE "set xrange \[0:1500]"
puts $pQUEUE "set yrange \[-1:101]"
puts $pQUEUE "set terminal png"
puts $pQUEUE "set out \"sc1_$alg\_PktLoss_$EF1ps.png\""
puts $pQUEUE "plot \"$alg\_PktLoss.tr\" using 1:2 title 'Transmitted packets' with lines, \"$alg\_PktLoss.tr\" using 1:3 title 'Packet Loss due to buffer overflow' with lines, \"$alg\_PktLoss.tr\" using 1:4 title 'Packet Loss due to congestion avoidance' with lines"
close $pQUEUE

# maximum burstiness of EF aggregate 
set pQUEUE  [open "EF_MBS.p" w]
puts $pQUEUE "set title \"Maximum Burstiness for EF aggregate using $alg\" "
puts $pQUEUE "set xlabel \"EF packet size (byte)\""
puts $pQUEUE "set ylabel \"Packets (number)\""
puts $pQUEUE "set autoscale"
puts $pQUEUE "set xrange \[0:1500]"
puts $pQUEUE "set yrange \[-1:31]"
puts $pQUEUE "set terminal png"
puts $pQUEUE "set out \"sc1_$alg\_EF_MBS.png\""
puts $pQUEUE "plot \"$alg\_EF_MBS.tr\" using 1:2 title 'MBS using $alg' with lines"
close $pQUEUE


