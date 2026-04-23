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


#  SourceId    	: source agent index
#  SinkId    	: sink   agent index
#  src   	: source node
#  dst   	: destination node
#  flowId       : flow Id used by LossMonitor to compute parameters as OWD and IPDV
#  PacketSize	: UDP packet size  (in bytes)
#  Rate		: 
#  Start	:
#  Stop		:

proc cbr_connection {SourceId SinkId src dst flowId PacketSize Rate start stop} {
	global ns Sink_
	set udp_($SourceId) [new Agent/UDP]
	$udp_($SourceId)    set class_ $flowId
	$ns attach-agent $src $udp_($SourceId)
	set cbr_($SourceId) [new Application/Traffic/CBR]
	$cbr_($SourceId)    attach-agent $udp_($SourceId)
	$cbr_($SourceId)    set packet_size_ $PacketSize
	$udp_($SourceId)    set packetSize_  $PacketSize
	$cbr_($SourceId)    set rate_ $Rate

	if {($flowId == 0)} {
		set Sink_($SinkId) [new Agent/LossMonitor]
		$Sink_($SinkId) set flowid_ $flowId
	} else {
		set Sink_($SinkId) [new Agent/Null]
	}

	$ns attach-agent $dst  $Sink_($SinkId)
	$ns connect $udp_($SourceId) $Sink_($SinkId)
	$ns at $start "$cbr_($SourceId) start"
	$ns at $stop  "$cbr_($SourceId) stop"
}

