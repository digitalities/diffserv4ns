#!/usr/bin/env bash
# run-ns235-scenario2-srtcm-sweep.sh
# 6-set WRED sweep for scenario-2-ns235-srtcm.tcl, the per-flow
# srTCM classifier mirror of ns-3
# diffserv-example-2 --scale=full --classifier=srtcm.
#
# Each set's stdout lands in
# output/ns2-35/example-2-fullscale-srtcm/set-$i/run.log and the three
# trace files (ServiceRate.tr, QueueLen.tr, PktLoss.tr) beside it.
#
# Usage:
#   scripts/run-ns235-scenario2-srtcm-sweep.sh                  # 6 sets, 5000 s each
#   SETS="1"       scripts/run-ns235-scenario2-srtcm-sweep.sh   # subset of sets
#   SIM_TIME=100   scripts/run-ns235-scenario2-srtcm-sweep.sh   # shortened smoke
#
# The simulator binary is the patched ns-2.35 at ns2/ns-allinone-2.35/ns-2.35/ns.
set -euo pipefail
cd "$(dirname "$0")/.."

EXAMPLE_DIR="ns2/diffserv4ns/examples/example-2-fullscale"
COMMON_DIR="ns2/diffserv4ns/examples/common"
ALLINONE_DIR="ns2/ns-allinone-2.35"
SIM_TIME="${SIM_TIME:-5000}"
SETS="${SETS:-1 2 3 4 5 6}"

OUT_BASE="output/ns2-35/example-2-fullscale-srtcm"
mkdir -p "$OUT_BASE"

echo "=== ns-2.35 Scenario 2 srTCM (per-flow) sweep ==="
echo "Sets      : $SETS"
echo "Sim time  : ${SIM_TIME}s per set"
echo "Output    : $OUT_BASE"
echo "Started   : $(date)"
echo ""

# Portable in-place sed (BSD on macOS needs `-i ''`, GNU does not).
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

    # Stage Tcl files and rewrite the common/apptypes source path.
    cp "$EXAMPLE_DIR"/scenario-2-ns235-srtcm.tcl  "$WORKDIR/"
    cp "$EXAMPLE_DIR"/utils.tcl                   "$WORKDIR/"
    cp "$COMMON_DIR"/apptypes.tcl                 "$WORKDIR/"
    for f in "$WORKDIR"/*.tcl; do
        sed "${SED_INPLACE[@]}" 's|source \.\./common/apptypes\.tcl|source apptypes.tcl|g' "$f"
    done

    # Scenario writes traces to
    # output/ns2-35/example-2-fullscale-srtcm/set-$N/, resolved relative
    # to the working directory. Pre-create the tree in the workdir so
    # `file mkdir` inside the scenario lands there.
    mkdir -p "$WORKDIR/output/ns2-35/example-2-fullscale-srtcm/set-$PARAM_SET"

    RC=0
    docker run --rm \
        -v "$(pwd)/$ALLINONE_DIR:/ns-allinone" \
        -v "$WORKDIR:/work" \
        -w /work \
        -e LD_LIBRARY_PATH=/ns-allinone/otcl-1.14:/ns-allinone/lib \
        -e TCL_LIBRARY=/ns-allinone/tcl8.5.10/library \
        ubuntu:18.04 \
        /ns-allinone/ns-2.35/ns scenario-2-ns235-srtcm.tcl "$PARAM_SET" "$SIM_TIME" \
        2>&1 | tee "$OUTDIR/run.log" || RC=${PIPESTATUS[0]:-$?}

    # Harvest trace files.
    TR_FOUND=0
    for TR in ServiceRate.tr QueueLen.tr PktLoss.tr; do
        SRC="$WORKDIR/output/ns2-35/example-2-fullscale-srtcm/set-$PARAM_SET/$TR"
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
