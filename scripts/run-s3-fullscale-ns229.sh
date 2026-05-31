#!/usr/bin/env bash
# run-s3-fullscale-ns229.sh — ns-2.29 Scenario 3 fullscale driver.
#
# Companion to run-s3-fullscale-ns235.sh. Runs scenario-3.tcl against
# the patched ns-2.29 tree (2001 original DS4 semantics) and mirrors
# the resulting traces into `output/ns2-29/example-3-fullscale/` — the
# path used by scripts/compare-three-way.py for the three-way S3
# overlay.
#
# Usage:
#   scripts/run-s3-fullscale-ns229.sh
#   SIM_TIME=200 scripts/run-s3-fullscale-ns229.sh   # short smoke
set -euo pipefail
cd "$(dirname "$0")/.."

SIM_TIME="${SIM_TIME:-}"
EXTRA=()
if [[ -n "$SIM_TIME" ]]; then
    EXTRA+=(--sim-time "$SIM_TIME")
fi

echo "=== run-s3-fullscale-ns229.sh starting : $(date) ==="
scripts/run-scenario.sh example-3 ns2-29 ${EXTRA[@]+"${EXTRA[@]}"}

SRC="output/ns2-29/example-3"
DST="output/ns2-29/example-3-fullscale"
mkdir -p "$DST"
if [[ -d "$SRC" ]]; then
    cp -f "$SRC"/*.tr "$DST/" 2>/dev/null || true
    [[ -f "$SRC/ns2-stdout.log" ]] && cp -f "$SRC/ns2-stdout.log" "$DST/"
    echo "=== Mirrored traces $SRC → $DST ==="
    ls -la "$DST"
fi
echo "=== Done : $(date) ==="
