#!/bin/bash
# Patch the DiffServ4NS module into the ns-allinone-2.35 ns-2 source tree.
#
# This script combines two input trees:
#
# 1. The 2001 algorithmic code (shared with the ns-2.29 patch):
#    Copies src/ns-2.29/diffserv/*.{h,cc} into ns-2.35/diffserv/,
#    replacing the stock Nortel DiffServ code with the DiffServ4NS versions.
#
# 2. The 2026 ns-2.35 adaptations (from src/ns-2.35/):
#    These files replace selected base files and apply the nine catalogued
#    bug fixes listed below, so the ns-2.35 port is a functional
#    "improved DS4" reference:
#
#   BUG-1  tools/cbr_traffic.cc    — set_pkttype(PT_CBR) was overwritten;
#                                    fixed to set_apptype(PT_CBR)
#   BUG-2  realaudio/realaudio.cc  — set_pkttype(PT_REALAUDIO) overwritten;
#                                    fixed to set_apptype(PT_REALAUDIO)
#   BUG-3  tcl/ns-source.tcl       — magic number 27 replaced by $PT_FTP
#                                    variable (27==PT_FTP in ns-2.35)
#   BUG-4  diffserv/dsscheduler.cc — SFQ DequeEvent: empty() moved before
#                                    front() to eliminate UB
#   BUG-5  tcl/ns-source.tcl       — FTP set_apptype added to all four
#                                    instprocs (start/send/produce/producemore)
#   UDP hdr apps/udp.cc            — sendmsg() adds 28 bytes (IP 20 + UDP 8)
#                                    so hdr_cmn::size() includes header overhead,
#                                    matching real-world serialisation
#
# Together these inputs cover the 14 base files DS4 modifies in ns-2 plus
# webcache/webtraf.h (BUG-9 fix staging, ns-2.35 only), for 15 base files
# total. Parity with the ns-2.29 patch script (scripts/patch-ns2-diffserv-229.sh)
# is preserved for the 14 shared files; webtraf.h is an additive ns-2.35-only
# patch target.
#
# For a one-line description of every bug (including component scope), see
# the table in LINEAGE.md (section "The 2026 ns-2.35 port layer"). Detailed
# technical notes for the 2026 port extensions live in src/ns-2.35/CHANGELOG.md.
#
# Idempotent: safe to run multiple times.
# Does NOT modify ns-allinone-2.29.3/.
#
# Source of truth:
# - src/ns-2.29/diffserv/    — DiffServ4NS module source (same as ns-2.29 patch)
# - src/ns-2.35/             — ns-2.35-specific replacements (bug fixes)
#
# Target:
# - ns2/ns-allinone-2.35/ns-2.35/   — The ns-2.35 tree to be patched

set -euo pipefail
cd "$(dirname "$0")/.."

DIFFSERV_SRC="src/ns-2.29"
SRC235="src/ns-2.35"
NS235_TARGET="ns2/ns-allinone-2.35/ns-2.35"

# --- Preflight checks ---

if [ ! -d "$DIFFSERV_SRC" ]; then
    echo "ERROR: DiffServ4NS source not found at $DIFFSERV_SRC" >&2
    exit 1
fi

if [ ! -d "$SRC235" ]; then
    echo "ERROR: ns-2.35 patch set not found at $SRC235" >&2
    exit 1
fi

if [ ! -d "$NS235_TARGET" ]; then
    echo "ERROR: ns-2.35 tree not found at $NS235_TARGET" >&2
    echo "Ensure ns2/ns-allinone-2.35/ is present." >&2
    exit 1
fi

echo "=== Patching DiffServ4NS into ns-2.35 ==="
echo ""

# --- 1. Copy DiffServ4NS module source into ns-2 diffserv/ directory ---
# Two input trees:
#   (a) 2001 algorithmic code: src/ns-2.29/diffserv/ (same as ns-2.29 patch)
#   (b) 2026 ns-2.35 adaptations: src/ns-2.35/diffserv/ — these replace
#       selected files from (a) and apply upstream warning-hygiene fixes
#       (dead-variable removals, stray-semicolon fixes) that upstream
#       ns-2.35 applied but that the 2001 base would otherwise discard.

echo ">>> Copying DiffServ4NS module files (2001 algorithmic code)..."
cp "$DIFFSERV_SRC"/diffserv/*.h  "$NS235_TARGET/diffserv/"
cp "$DIFFSERV_SRC"/diffserv/*.cc "$NS235_TARGET/diffserv/"
echo "  Copied: $(ls "$DIFFSERV_SRC"/diffserv/*.{h,cc} | wc -l | tr -d ' ') files to $NS235_TARGET/diffserv/"

# 2026 ns-2.35 adaptations replace the 2001 base files where needed.
if [ -d "$SRC235/diffserv" ]; then
    echo ">>> Applying ns-2.35 adaptations (2026 port layer)..."
    for f in "$SRC235"/diffserv/*; do
        [ -e "$f" ] || continue
        cp "$f" "$NS235_TARGET/diffserv/"
        echo "  Replaced: diffserv/$(basename "$f") (ns-2.35 adaptation)"
    done
fi

# --- 2. Copy modified ns-2.35 base files ---
# These are ns-2.35 files modified by DiffServ4NS to add per-packet
# metadata (sendtime_, app_type_), TCP state visibility (cwnd_, t_rtt_),
# application-type tagging.
# Note: packet_t is typedef unsigned int in ns-2.35 (was enum in ns-2.29).

echo ">>> Copying modified ns-2.35 base files..."

cp "$SRC235/common/packet.h"      "$NS235_TARGET/common/packet.h"
echo "  Patched: common/packet.h"

cp "$SRC235/common/agent.h"       "$NS235_TARGET/common/agent.h"
echo "  Patched: common/agent.h"

cp "$SRC235/common/agent.cc"      "$NS235_TARGET/common/agent.cc"
echo "  Patched: common/agent.cc"

cp "$SRC235/apps/udp.cc"          "$NS235_TARGET/apps/udp.cc"
echo "  Patched: apps/udp.cc"

cp "$SRC235/tcp/tcp.h"            "$NS235_TARGET/tcp/tcp.h"
echo "  Patched: tcp/tcp.h"

cp "$SRC235/apps/telnet.cc"         "$NS235_TARGET/apps/telnet.cc"
echo "  Patched: apps/telnet.cc"

cp "$SRC235/tcl/ns-source.tcl"      "$NS235_TARGET/tcl/lib/ns-source.tcl"
echo "  Patched: tcl/lib/ns-source.tcl (BUG-3: PT_FTP symbolic var; BUG-5: set_apptype in all FTP instprocs)"

cp "$SRC235/tools/cbr_traffic.cc"   "$NS235_TARGET/tools/cbr_traffic.cc"
echo "  Patched: tools/cbr_traffic.cc (BUG-1: set_apptype(PT_CBR))"

cp "$SRC235/tools/loss-monitor.h"   "$NS235_TARGET/tools/loss-monitor.h"
cp "$SRC235/tools/loss-monitor.cc"  "$NS235_TARGET/tools/loss-monitor.cc"
echo "  Patched: tools/loss-monitor.{h,cc} (OWD/IPDV monitoring + FrequencyDistribution)"

cp "$SRC235/realaudio/realaudio.cc" "$NS235_TARGET/realaudio/realaudio.cc"
echo "  Patched: realaudio/realaudio.cc (BUG-2: set_apptype(PT_REALAUDIO))"

cp "$SRC235/tcp/tcp.cc"                 "$NS235_TARGET/tcp/tcp.cc"
echo "  Patched: tcp/tcp.cc (DS4: stamp cwnd_/t_rtt_ on outgoing TCP packets)"

cp "$SRC235/webcache/webtraf.h"         "$NS235_TARGET/webcache/webtraf.h"
echo "  Patched: webcache/webtraf.h (DS4 working copy; pristine upstream + BUG-9 fix staging)"

cp "$SRC235/webcache/webtraf.cc"        "$NS235_TARGET/webcache/webtraf.cc"
echo "  Patched: webcache/webtraf.cc (DS4: tag WebTraf TCP agents with PT_HTTP)"

cp "$SRC235/tcl/lib/ns-default.tcl"    "$NS235_TARGET/tcl/lib/ns-default.tcl"
echo "  Patched: tcl/lib/ns-default.tcl (DS4: Queue/dsRED defaults + Agent/LossMonitor monitoring defaults)"

# --- 3. Add dsscheduler.o to Makefile.in ---
# The stock Makefile.in already includes the Nortel DiffServ objects.
# DiffServ4NS adds dsscheduler.o to this list.

echo ">>> Updating Makefile.in..."
if grep -q "dsscheduler.o" "$NS235_TARGET/Makefile.in"; then
    echo "  dsscheduler.o already in Makefile.in (idempotent, skipping)"
else
    # Insert after the line containing dewp.o (the last Nortel diffserv object)
    sed -i.bak '/diffserv\/dewp\.o/a\
	diffserv/dsscheduler.o \\' "$NS235_TARGET/Makefile.in"
    rm -f "$NS235_TARGET/Makefile.in.bak"
    echo "  Added diffserv/dsscheduler.o to Makefile.in"
fi

echo ""
echo "=== Patch complete ==="
echo ""
echo "Files patched in $NS235_TARGET:"
echo "  DiffServ4NS module (2001 algorithmic code): diffserv/*.{h,cc} replaced"
echo "  ns-2.35 adaptations (2026 port layer; bug fixes applied, 15 base files: the 14 DS4-patched files plus webtraf.h for BUG-9 staging):"
echo "    dsred.cc, dsEdge.cc           — upstream warning-hygiene fixes"
echo "    dsscheduler.cc                — BUG-4: SFQ empty() before front()"
echo "    tools/cbr_traffic.cc          — BUG-1: set_apptype(PT_CBR)"
echo "    tools/loss-monitor.{h,cc}     — OWD/IPDV monitoring + FrequencyDistribution"
echo "    realaudio/realaudio.cc        — BUG-2: set_apptype(PT_REALAUDIO)"
echo "    tcl/lib/ns-source.tcl         — BUG-3: PT_FTP symbolic var"
echo "                                    BUG-5: set_apptype in all FTP instprocs"
echo "    apps/udp.cc                   — UDP header: +28 bytes (IP 20 + UDP 8)"
echo "    common/packet.h, agent.h, agent.cc, tcp/tcp.h, apps/telnet.cc"
echo "    tcp/tcp.cc                    — DS4: stamp cwnd_/t_rtt_ on outgoing TCP"
echo "    webcache/webtraf.cc           — DS4: tag WebTraf TCP agents with PT_HTTP"
echo "    tcl/lib/ns-default.tcl        — DS4: Queue/dsRED + LossMonitor defaults"
echo "  Makefile.in: dsscheduler.o added"
echo ""
echo "  ns-2.35 port now covers 15 base files: parity with the ns-2.29 patch script's 14, plus webtraf.h (ns-2.35-only BUG-9 staging)."
echo ""
echo "Next steps:"
echo "  1. Rebuild ns-2.35: ./scripts/build-ns2-allinone-235-docker.sh"
echo "  2. Test: docker run --rm \\"
echo "       -v \"\$(pwd)/ns2/ns-allinone-2.35:/ns-allinone\" \\"
echo "       -e LD_LIBRARY_PATH=/ns-allinone/otcl-1.14:/ns-allinone/lib \\"
echo "       -e TCL_LIBRARY=/ns-allinone/tcl8.5.10/library \\"
echo "       ubuntu:18.04 \\"
echo "       /ns-allinone/ns-2.35/ns -c 'puts [Queue/dsRED info class]; exit 0'"
