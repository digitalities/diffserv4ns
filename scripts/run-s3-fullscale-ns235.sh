#!/usr/bin/env bash
# run-s3-fullscale-ns235.sh — ns-2.35 Scenario 3 fullscale driver.
#
# Thin wrapper over scripts/run-scenario.sh example-3 ns2-35 that
# additionally mirrors the produced traces into the plan's canonical
# path `output/ns2-35/example-3-fullscale/` used by
# scripts/compare-three-way.py and the three-way comparison
# handbook section.
#
# scenario-3.tcl is the sole S3 reconstruction (771 nodes, 5000 s,
# LLQ+SFQ 3:3:3:1); there is no separate "fullscale" Tcl variant.
# The `-fullscale` suffix in the output tree is used only for naming
# consistency with ns-3 (diffserv-example-3 --scale=full).
#
# Usage:
#   scripts/run-s3-fullscale-ns235.sh
#   SIM_TIME=200 scripts/run-s3-fullscale-ns235.sh   # short smoke
set -euo pipefail
cd "$(dirname "$0")/.."

SIM_TIME="${SIM_TIME:-}"
EXTRA=()
if [[ -n "$SIM_TIME" ]]; then
    EXTRA+=(--sim-time "$SIM_TIME")
fi

echo "=== run-s3-fullscale-ns235.sh starting : $(date) ==="
scripts/run-scenario.sh example-3 ns2-35 ${EXTRA[@]+"${EXTRA[@]}"}

SRC="output/ns2-35/example-3"
DST="output/ns2-35/example-3-fullscale"
mkdir -p "$DST"
if [[ -d "$SRC" ]]; then
    cp -f "$SRC"/*.tr "$DST/" 2>/dev/null || true
    [[ -f "$SRC/ns2-stdout.log" ]] && cp -f "$SRC/ns2-stdout.log" "$DST/"
    echo "=== Mirrored traces $SRC → $DST ==="
    ls -la "$DST"
fi
echo "=== Done : $(date) ==="
