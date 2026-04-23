#!/bin/bash
# Patch the DiffServ4NS module into the ns-allinone-2.29.3 ns-2 source tree.
#
# This script:
# 1. Copies DiffServ4NS source files (diffserv/*.{h,cc}) into ns-2.29/diffserv/,
#    replacing the stock Nortel DiffServ code with the DiffServ4NS versions
# 2. Copies modified ns-2 base files (packet.h, agent.{h,cc}, tcp.{h,cc}, etc.)
#    from the DiffServ4NS tree, replacing the stock versions
# 3. Adds dsscheduler.o to the ns-2 Makefile.in
# 4. The ns-2 binary must be rebuilt after patching (use build-ns2-allinone-229-docker.sh)
#
# Idempotent: safe to run multiple times.
#
# Source of truth:
# - src/ns-2.29/diffserv/    — DiffServ4NS module source (read-only)
# - src/ns-2.29/             — Modified ns-2 base files (read-only)
#
# Target:
# - ns2/ns-allinone-2.29.3/ns-2.29/  — The ns-2 tree to be patched

set -euo pipefail
cd "$(dirname "$0")/.."

DIFFSERV_SRC="src/ns-2.29"
NS2_TARGET="ns2/ns-allinone-2.29.3/ns-2.29"

# --- Preflight checks ---

if [ ! -d "$DIFFSERV_SRC" ]; then
    echo "ERROR: DiffServ4NS source not found at $DIFFSERV_SRC" >&2
    exit 1
fi

if [ ! -d "$NS2_TARGET" ]; then
    echo "ERROR: ns-2.29 tree not found at $NS2_TARGET" >&2
    echo "Run ./scripts/fetch-ns2-allinone-229.sh first." >&2
    exit 1
fi

echo "=== Patching DiffServ4NS into ns-2.29 ==="
echo ""

# --- 1. Copy DiffServ4NS module source into ns-2 diffserv/ directory ---
# This replaces the stock Nortel DiffServ files with the DiffServ4NS versions
# and adds dsscheduler.{h,cc} and dsconsts.h which are new in DiffServ4NS.

echo ">>> Copying DiffServ4NS module files..."
cp "$DIFFSERV_SRC"/diffserv/*.h  "$NS2_TARGET/diffserv/"
cp "$DIFFSERV_SRC"/diffserv/*.cc "$NS2_TARGET/diffserv/"
echo "  Copied: $(ls "$DIFFSERV_SRC"/diffserv/*.{h,cc} | wc -l | tr -d ' ') files to $NS2_TARGET/diffserv/"

# --- 2. Copy modified ns-2 base files ---
# These are stock ns-2 files modified by DiffServ4NS to add per-packet
# metadata (sendtime_, app_type_), TCP state visibility (cwnd_, t_rtt_),
# application-type tagging, and monitoring extensions.

echo ">>> Copying modified ns-2 base files..."

PATCHED_FILES=(
    "common/packet.h"       # sendtime_, app_type_ fields + accessors
    "common/agent.h"        # app_type_ member, set_apptype(), 2-arg constructor
    "common/agent.cc"       # app_type_ init, set_apptype cmd, allocpkt() stamp
    "tcp/tcp.h"             # cwnd_, t_rtt_ fields + accessors
    "tcp/tcp.cc"            # copies cwnd_/t_rtt_ into outgoing segments
    "apps/udp.cc"           # stamps sendtime_ on UDP packets
    "apps/telnet.cc"        # sets app_type to PT_TELNET
    "tools/cbr_traffic.cc"  # sets pkttype (note: has a known bug; the ns-2.35 port layer fixes it as BUG-1)
    "tools/loss-monitor.h"  # OWD/IPDV monitoring framework
    "tools/loss-monitor.cc" # OWD/IPDV computation + frequency distributions
    "realaudio/realaudio.cc"  # sets pkttype
    "webcache/webtraf.cc"     # sets app_type to PT_HTTP
    "tcl/lib/ns-default.tcl"  # default values for DiffServ/monitoring variables
    "tcl/lib/ns-source.tcl"   # FTP set_apptype
)

for f in "${PATCHED_FILES[@]}"; do
    if [ -f "$DIFFSERV_SRC/$f" ]; then
        cp "$DIFFSERV_SRC/$f" "$NS2_TARGET/$f"
        echo "  Patched: $f"
    else
        echo "  WARNING: $DIFFSERV_SRC/$f not found, skipping" >&2
    fi
done

# --- 3. Add dsscheduler.o to Makefile.in ---
# The stock Makefile.in already includes the Nortel DiffServ objects
# (dsred.o, dsredq.o, dsEdge.o, dsCore.o, dsPolicy.o, ew.o, dewp.o).
# DiffServ4NS adds dsscheduler.o to this list.

echo ">>> Updating Makefile.in..."
if grep -q "dsscheduler.o" "$NS2_TARGET/Makefile.in"; then
    echo "  dsscheduler.o already in Makefile.in (idempotent, skipping)"
else
    # Insert after the line containing dewp.o (the last Nortel diffserv object)
    sed -i.bak '/diffserv\/dewp\.o/a\
	diffserv/dsscheduler.o \\' "$NS2_TARGET/Makefile.in"
    rm -f "$NS2_TARGET/Makefile.in.bak"
    echo "  Added diffserv/dsscheduler.o to Makefile.in"
fi

echo ""
echo "=== Patch complete ==="
echo ""
echo "Files patched in $NS2_TARGET:"
echo "  - diffserv/ directory: replaced with DiffServ4NS versions + dsscheduler"
echo "  - ${#PATCHED_FILES[@]} modified ns-2 base files"
echo "  - Makefile.in: dsscheduler.o added"
echo ""
echo "Next steps:"
echo "  1. Rebuild ns-2: ./scripts/build-ns2-allinone-229-docker.sh"
echo "  2. Test: docker run --rm \\"
echo "       -v \"\$(pwd)/ns2/ns-allinone-2.29.3:/ns-allinone\" \\"
echo "       -e LD_LIBRARY_PATH=/ns-allinone/otcl-1.11:/ns-allinone/lib \\"
echo "       -e TCL_LIBRARY=/ns-allinone/tcl8.4.11/library \\"
echo "       ubuntu:18.04 \\"
echo "       /ns-allinone/ns-2.29/ns -c 'puts [Queue/dsRED info class]; exit 0'"
