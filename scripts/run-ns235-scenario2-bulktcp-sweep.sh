#!/usr/bin/env bash
# run-ns235-scenario2-bulktcp-sweep.sh
# 6-set WRED sweep for scenario-2.tcl (bulk-TCP HTTP approximation) on
# ns-2.35. This is the companion to run-ns235-scenario2-sweep.sh
# (thesis-faithful PagePool/WebTraf) — both sweeps target the same
# topology and WRED sets, but differ in the traffic model used for the
# AF11 HTTP aggregate:
#
#   - bulk-TCP (this script): greedy bidirectional TCP, matches the
#     ns-2.29 and ns-3 reconstructions used for paper §5.6.
#   - WebTraf  (peer script): PagePool/WebTraf session model, matches
#     the thesis §4.2 traffic description.
#
# The two sweeps let Fig 8.2 be drawn as a 4-line figure (ns-2.29,
# ns-2.35-bulkTCP, ns-2.35-WebTraf, ns-3) so the traffic-model impact
# is visible and attributable.
#
# Output: output/ns2-35/example-2-fullscale-bulktcp/set-{1..6}/{ServiceRate,QueueLen,PktLoss}.tr
#
# Usage:
#   scripts/run-ns235-scenario2-bulktcp-sweep.sh                 # 6 sets, 5000 s each
#   SETS="1"       scripts/run-ns235-scenario2-bulktcp-sweep.sh  # subset of sets
#   SIM_TIME=100   scripts/run-ns235-scenario2-bulktcp-sweep.sh  # shortened smoke
set -euo pipefail
cd "$(dirname "$0")/.."

EXAMPLE_DIR="ns2/diffserv4ns/examples/example-2-fullscale"
COMMON_DIR="ns2/diffserv4ns/examples/common"
ALLINONE_DIR="ns2/ns-allinone-2.35"
SIM_TIME="${SIM_TIME:-5000}"
SETS="${SETS:-1 2 3 4 5 6}"

OUT_BASE="output/ns2-35/example-2-fullscale-bulktcp"
mkdir -p "$OUT_BASE"

echo "=== ns-2.35 Scenario 2 bulk-TCP sweep ==="
echo "Sets      : $SETS"
echo "Sim time  : ${SIM_TIME}s per set"
echo "Output    : $OUT_BASE"
echo "Started   : $(date)"
echo ""

# Portable in-place sed.
SED_INPLACE=(-i)
if sed --version >/dev/null 2>&1; then
    :   # GNU sed
else
    SED_INPLACE=(-i '')
fi

for PARAM_SET in $SETS; do
    OUTDIR="$OUT_BASE/set-$PARAM_SET"
    mkdir -p "$OUTDIR"

    WORKDIR=$(mktemp -d)
    trap 'rm -rf "$WORKDIR"' RETURN

    echo "[$(date +%H:%M:%S)] === Set $PARAM_SET starting (workdir=$WORKDIR) ==="

    cp "$EXAMPLE_DIR"/scenario-2.tcl  "$WORKDIR/"
    cp "$EXAMPLE_DIR"/utils.tcl       "$WORKDIR/"
    cp "$COMMON_DIR"/apptypes.tcl     "$WORKDIR/"
    for f in "$WORKDIR"/*.tcl; do
        sed "${SED_INPLACE[@]}" 's|source \.\./common/apptypes\.tcl|source apptypes.tcl|g' "$f"
    done

    # scenario-2.tcl hardcodes `output/ns2/example-2-fullscale/set-$paramSet`
    # as its write path. Rewrite it to land in the bulk-TCP output dir so
    # the trace files pop out under the workdir where we can harvest.
    sed "${SED_INPLACE[@]}" \
        's|output/ns2/example-2-fullscale|output/ns2-35/example-2-fullscale-bulktcp|g' \
        "$WORKDIR/scenario-2.tcl"

    mkdir -p "$WORKDIR/output/ns2-35/example-2-fullscale-bulktcp/set-$PARAM_SET"

    RC=0
    docker run --rm \
        -v "$(pwd)/$ALLINONE_DIR:/ns-allinone" \
        -v "$WORKDIR:/work" \
        -w /work \
        -e LD_LIBRARY_PATH=/ns-allinone/otcl-1.14:/ns-allinone/lib \
        -e TCL_LIBRARY=/ns-allinone/tcl8.5.10/library \
        ubuntu:18.04 \
        /ns-allinone/ns-2.35/ns scenario-2.tcl "$PARAM_SET" "$SIM_TIME" \
        2>&1 | tee "$OUTDIR/run.log" || RC=${PIPESTATUS[0]:-$?}

    TR_FOUND=0
    for TR in ServiceRate.tr QueueLen.tr PktLoss.tr; do
        SRC="$WORKDIR/output/ns2-35/example-2-fullscale-bulktcp/set-$PARAM_SET/$TR"
        if [ -f "$SRC" ]; then
            cp "$SRC" "$OUTDIR/"
            TR_FOUND=$((TR_FOUND + 1))
        fi
    done

    rm -rf "$WORKDIR"
    trap - RETURN

    if [ "$RC" -eq 0 ] && [ "$TR_FOUND" -eq 3 ]; then
        echo "[$(date +%H:%M:%S)] === Set $PARAM_SET PASS (traces=$TR_FOUND) ==="
    else
        echo "[$(date +%H:%M:%S)] === Set $PARAM_SET FAIL (rc=$RC traces=$TR_FOUND) ==="
    fi
    echo ""
done

echo "=== Sweep finished : $(date) ==="
