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


# bandwidth plot setting

set  vFile  [open "bw_x.p" w]
puts $vFile "set title \"Departure rate (EF ps=$EFPacketSize) - sch=$alg\""
puts $vFile "set xlabel \"time (s)\""
puts $vFile "set ylabel \"bandwidth (kbps)\""
puts $vFile "set autoscale"
#puts $vFile "set xrange \[0:1600]"
#puts $vFile "set yrange \[-1:1000]"
puts $vFile "set terminal png"
puts $vFile "set out \"x$alg\_EF$EFPacketSize\_BW.png\""
puts $vFile "plot \"ServiceRate.tr\" using 1:2 title 'EF aggregate' with lines, \"ServiceRate.tr\" using 1:3 title 'BE aggregate' with lines"
close $vFile

# owd plot setting
set pBW  [open "owd_x.p" w]
puts $pBW "set title \"Avg One-Way Delay for EF traffic (EF ps=$EFPacketSize) - sch=$alg\""
puts $pBW "set xlabel \"time (s)\""
puts $pBW "set ylabel \"one-way delay (millisec)\""
puts $pBW "set autoscale"
puts $pBW "set terminal png"
puts $pBW "set out \"x$alg\_EF$EFPacketSize\_OWD.png\""
puts $pBW "plot \"OWD.tr\" using 1:2 title 'One-Way delay' with lines"
close $pBW

# ipdv plot setting
set pBW  [open "ipdv_x.p" w]
puts $pBW "set title \"Avg IPDV for EF traffic (EF ps=$EFPacketSize) - sch=$alg\""
puts $pBW "set xlabel \"time (s)\""
puts $pBW "set ylabel \"ipdv (millisec)\""
puts $pBW "set autoscale"
#puts $pBW "set xrange \[0:30]"
#puts $pBW "set yrange \[-1:30]"
puts $pBW "set terminal png"
puts $pBW "set out \"x$alg\_EF$EFPacketSize\_IPDV.png\""
puts $pBW "plot \"IPDV.tr\" using 1:2 title 'ipdv' with lines"
close $pBW

# queue plot setting
set pBW  [open "queue_x.p" w]
puts $pBW "set title \"Queue Lenght for EF ($EFPacketSize ps) - sch=$alg\""
puts $pBW "set xlabel \"time (s)\""
puts $pBW "set ylabel \"queue lenght (Packets)\""
#puts $pBW "set autoscale"
#puts $pBW "set xrange \[0:50]"
puts $pBW "set yrange \[-1:60]"
puts $pBW "set terminal png"
puts $pBW "set out \"x$alg\_EF$EFPacketSize\_QUEUE.png\""
puts $pBW "plot \"EFQueueLen.tr\" using 1:2 title 'EF queue lenght' with lines, \"BEQueueLen.tr\" using 1:2 title 'BE queue lenght' with lines"
close $pBW

# owd frequency distribution plotting
set  vFile  [open "owdFD_x.p" w]
puts $vFile "set title \"OWD Frequency Distribution for EF ($EFPacketSize ps) - sch=$alg\""
puts $vFile "set xlabel \"time (s)\""
puts $vFile "set ylabel \"packets (%)\""
puts $vFile "set autoscale"
#puts $vFile "set xrange \[0:1600]"
puts $vFile "set yrange \[0:100]"
puts $vFile "set terminal png"
puts $vFile "set out \"x$alg\_EF$EFPacketSize\_owdFD.png\""
puts $vFile "plot \"owd$EFPacketSize\_FD.tr\" using 1:2 title 'owd frequency distribution' with boxes"
close $vFile

# ipdv frequency distribution plotting
set  vFile  [open "ipdvFD_x.p" w]
puts $vFile "set title \"IPDV Frequency Distribution for EF ($EFPacketSize ps) - sch=$alg\""
puts $vFile "set xlabel \"time (s)\""
puts $vFile "set ylabel \"packets (%)\""
puts $vFile "set autoscale"
#puts $vFile "set xrange \[0:1600]"
puts $vFile "set yrange \[0:100]"
puts $vFile "set terminal png"
puts $vFile "set out \"x$alg\_EF$EFPacketSize\_ipdvFD.png\""
puts $vFile "plot \"ipdv$EFPacketSize\_FD.tr\" using 1:2 title 'ipdv frequency distribution' with boxes"
close $vFile
