# DiffServ4NS packet-type constants — portable across ns-2.29 and ns-2.35.
#
# Background:
#   ns-2.29 defines packet types as a C++ enum (packet.h ~line 76).
#   ns-2.35 switched to static const integers (packet.h ~line 86).
#   Both versions share the same numeric values for CBR, TELNET, FTP, and
#   HTTP.  The only drift is PT_REALAUDIO: ns-2.35 inserted PT_PBC at
#   position 45, pushing PT_REALAUDIO from 49 to 50.
#
# Version probe:
#   ns-2.29 ships Tcl 8.4.11; ns-2.35 ships Tcl 8.5.10.
#   [info patchlevel] returns "8.4.11" or "8.5.10" respectively, so a
#   string comparison against "8.5" is sufficient.
#
# Usage:
#   source common/apptypes.tcl
#   $agent set_apptype $PT_REALAUDIO
#
# Verified values (see ns-2.29/common/packet.h enum and
# ns-2.35/common/packet.h static const):
#   ns-2.29: CBR=2, TELNET=26, FTP=27, HTTP=31, REALAUDIO=49
#   ns-2.35: CBR=2, TELNET=26, FTP=27, HTTP=31, REALAUDIO=50

if {[string compare [info patchlevel] "8.5"] >= 0} {
    # ns-2.35 (Tcl 8.5.10)
    set PT_CBR        2
    set PT_TELNET     26
    set PT_FTP        27
    set PT_HTTP       31
    set PT_REALAUDIO  50
} else {
    # ns-2.29 (Tcl 8.4.11)
    set PT_CBR        2
    set PT_TELNET     26
    set PT_FTP        27
    set PT_HTTP       31
    set PT_REALAUDIO  49
}
