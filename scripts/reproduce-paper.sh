#!/usr/bin/env bash
# reproduce-paper.sh — one-shot reproduction orchestrator.
#
# Runs every ns-3-side figure / table entry point in sequence, records a
# PASS / FAIL / SKIP verdict for each, and writes a summary table to
# output/paper/reproduction-status.md. Each step is an independent entry
# point that also works standalone; this script just strings them together.
#
# Prerequisite: a built ns-3 tree with the diffserv module. From a clean
# checkout:
#
#   ./scripts/fetch-ns3.sh
#   cd ns3/ns-3-dev && ./ns3 configure --enable-tests --enable-examples
#   ./ns3 build diffserv && cd -
#
# The full cross-simulator equivalence also needs the ns-2.29 / ns-2.35
# Docker backends (see docs/REPRODUCIBILITY.md "One-time setup"); this
# orchestrator runs only the ns-3 side of that comparison. Use
# scripts/run-scenario.sh --all for the full three-way cross-check.
#
# Usage:
#   ./scripts/reproduce-paper.sh            # full ns-3-side reproduction
#   ./scripts/reproduce-paper.sh --smoke    # fast: test suites only
#   ./scripts/reproduce-paper.sh --list     # list the steps and exit
#
# Environment:
#   NS3_DEV   Path to the built ns-3-dev tree.  Default: ns3/ns-3-dev
# ----------------------------------------------------------------------

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
NS3_DEV="${NS3_DEV:-$REPO_ROOT/ns3/ns-3-dev}"
STATUS_DIR="$REPO_ROOT/output/paper"
STATUS_MD="$STATUS_DIR/reproduction-status.md"

MODE="full"
while [ $# -gt 0 ]; do
    case "$1" in
        --smoke)   MODE="smoke"; shift ;;
        --full)    MODE="full";  shift ;;
        --list)    MODE="list";  shift ;;
        --help|-h) grep '^# ' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *)         echo "Unknown arg: $1" >&2; exit 1 ;;
    esac
done

# -- Step registry -----------------------------------------------------
# Each step is "tier|label|command". tier=smoke steps run in both modes;
# tier=full steps run only in full mode.
STEPS=(
    "smoke|Core suites + RFC 2697/2698/2859 meter conformance vectors|cd \"\$NS3_DEV\" && python3 test.py -s diffserv && python3 test.py -s diffserv-per-flow-classifier"
    "smoke|L4S DualPI2 coupled marking + RFC 9331/9332 vectors|cd \"\$NS3_DEV\" && python3 test.py -s diffserv-l4s"
    "smoke|CAKE calibration suite|cd \"\$NS3_DEV\" && python3 test.py -s diffserv-cake-q15"
    "full|Scenario 1/2/3 cross-simulator equivalence — ns-3 side only (ns-2 needs Docker)|bash \"\$REPO_ROOT/scripts/run-scenario.sh\" example-1 ns3 && bash \"\$REPO_ROOT/scripts/run-scenario.sh\" example-2 ns3 && bash \"\$REPO_ROOT/scripts/run-scenario.sh\" example-3 ns3 --sim-time 60"
    "full|Scheduler GPS-convergence (Chang) sweep|bash \"\$REPO_ROOT/scripts/run-q16-chang-sweep.sh\""
    "full|Parekh-Gallager Theorem 1 latency-bound gate|bash \"\$REPO_ROOT/scripts/run-q17-parekh-gate.sh\""
    "full|AQM characterisation envelope (13 AQMs x 9 scenarios)|bash \"\$REPO_ROOT/scripts/aqm-eval/aqm-eval\" reproduce"
    "full|CAKE + L4S composition fairness|bash \"\$REPO_ROOT/scripts/l4s-cake-composition-fairness-sweep.sh\""
)

# Heavier-prerequisite paper evidence that this script intentionally does
# not run, with a pointer to where the full instructions live.
print_coverage_note() {
    echo ""
    echo "Not reproduced by this script (heavier prerequisites — see docs/REPRODUCIBILITY.md):"
    echo "  - Full ns-2 <-> ns-3 cross-simulator equivalence: needs the ns-2.29 / ns-2.35"
    echo "    Docker build (run-scenario.sh --all). This script runs only the ns-3 side, and"
    echo "    Scenario 3 at the quick scale — the full 771-node run and the RealAudio"
    echo "    trace-replay policer validation (gold-policer-replay) are separate."
    echo "  - Stratum vs Linux CAKE cross-validation (host-fairness anchor, trace-replay,"
    echo "    stratum-bridge): needs a Lima VM with sch_cake."
}

if [ "$MODE" = "list" ]; then
    echo "Steps (tier shown in brackets):"
    for s in "${STEPS[@]}"; do
        IFS='|' read -r tier label _ <<< "$s"
        echo "  [$tier] $label"
    done
    echo ""
    echo "smoke = run with --smoke; full mode (default) runs both tiers."
    print_coverage_note
    exit 0
fi

# -- Pre-flight --------------------------------------------------------
if [ ! -f "$NS3_DEV/test.py" ]; then
    echo "ERROR: no built ns-3 tree at $NS3_DEV" >&2
    echo "Run ./scripts/fetch-ns3.sh and build the diffserv module first" >&2
    echo "(see the header of this script, or docs/REPRODUCIBILITY.md)." >&2
    exit 1
fi

# The diffserv module requires the local ns-3 patches under patches/ns3/
# (FqCobaltQueueDisc::HostIsolationMode and others). Without them the build
# fails with a cryptic "no member named ..." error. Check the sentinel
# symbol up front and point at the setup script that applies and verifies
# the patches, so a skipped fetch-ns3.sh step is a clear message, not a
# confusing compiler failure.
fq_cobalt="$NS3_DEV/src/traffic-control/model/fq-cobalt-queue-disc.h"
if [ ! -f "$fq_cobalt" ] || ! grep -q 'HostIsolationMode' "$fq_cobalt"; then
    echo "ERROR: the ns-3 tree at $NS3_DEV is missing the local patches/ns3/ changes" >&2
    echo "(FqCobaltQueueDisc::HostIsolationMode not found); the diffserv module will" >&2
    echo "not compile. Run ./scripts/fetch-ns3.sh to fetch ns-3-dev at the pinned" >&2
    echo "revision and apply the patches, then rebuild before re-running." >&2
    exit 1
fi
mkdir -p "$STATUS_DIR"

# -- Run ---------------------------------------------------------------
declare -a RESULT_LABEL RESULT_VERDICT
fail_count=0

run_step() {
    local label="$1" cmd="$2" verdict
    echo ""
    echo "=== $label ==="
    if eval "$cmd"; then
        verdict="PASS"
    else
        verdict="FAIL"
        fail_count=$((fail_count + 1))
    fi
    echo "--- $label: $verdict ---"
    RESULT_LABEL+=("$label")
    RESULT_VERDICT+=("$verdict")
}

for s in "${STEPS[@]}"; do
    IFS='|' read -r tier label cmd <<< "$s"
    if [ "$MODE" = "smoke" ] && [ "$tier" != "smoke" ]; then
        RESULT_LABEL+=("$label"); RESULT_VERDICT+=("SKIP")
        continue
    fi
    run_step "$label" "$cmd"
done

# -- Summary -----------------------------------------------------------
{
    echo "# Reproduction status"
    echo ""
    echo "Mode: \`$MODE\` — generated by \`scripts/reproduce-paper.sh\`."
    echo ""
    echo "| Step | Verdict |"
    echo "|------|---------|"
    for i in "${!RESULT_LABEL[@]}"; do
        echo "| ${RESULT_LABEL[$i]} | ${RESULT_VERDICT[$i]} |"
    done
} > "$STATUS_MD"

echo ""
echo "============================================================"
echo "  Reproduction summary  (mode: $MODE)"
echo "============================================================"
for i in "${!RESULT_LABEL[@]}"; do
    printf "  %-6s %s\n" "${RESULT_VERDICT[$i]}" "${RESULT_LABEL[$i]}"
done
echo "------------------------------------------------------------"
echo "  Summary written to ${STATUS_MD#"$REPO_ROOT"/}"
echo "============================================================"
print_coverage_note

[ "$fail_count" -eq 0 ] || { echo "$fail_count step(s) FAILED." >&2; exit 1; }
echo "All executed steps PASSED."
