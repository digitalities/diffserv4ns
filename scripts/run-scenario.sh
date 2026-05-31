#!/usr/bin/env bash
# run-scenario.sh — unified scenario runner for DiffServ4NS
#
# Usage:
#   scripts/run-scenario.sh <scenario> <version> [--sim-time <sec>] [--extra-flags "..."]
#   scripts/run-scenario.sh --all <scenario> [--sim-time <sec>]
#
# <scenario>:
#   example-1            — Scenario 1 (PQ EF/BE, CBR traffic)
#   example-2            — Scenario 2 small-scale (PQ/SCFQ/LLQ, Telnet+FTP+CBR)
#   example-2-fullscale  — Scenario 2 full-scale (469 nodes, WRED sweep)
#   example-3            — Scenario 3 (LLQ+SFQ, Premium/Gold/Silver/Bronze/BE)
#   webtraf-ns235-test   — WebTraf DiffServ classification smoke test
#
# <version>:
#   ns2-29   — ns-2.29 (patched + built, Ubuntu 18.04 Docker)
#   ns2-35   — ns-2.35 (patched + built, Ubuntu 18.04 Docker)
#   ns3      — ns-3 (built in ns3/ns-3-dev)
#
# Options:
#   --sim-time <sec>     Override simulation time (passed to scenario if supported)
#   --extra-flags "..."  Additional flags forwarded to the simulator
#
# Output:
#   output/<version>/<scenario>/   (created if absent, cleaned if present)
#
# Exit code: 0 on success, non-zero on failure.

set -euo pipefail
cd "$(dirname "$0")/.."

# ---------------------------------------------------------------------------
# Parse arguments
# ---------------------------------------------------------------------------
ALL_MODE=false
if [[ "${1:-}" == "--all" ]]; then
    ALL_MODE=true
    shift
fi

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 [--all] <scenario> [<version>] [--sim-time <sec>] [--extra-flags \"...\"] [--print-outdir]" >&2
    exit 1
fi

SCENARIO="$1"; shift
if [[ "$ALL_MODE" == false ]]; then
    if [[ $# -lt 1 ]]; then
        echo "Usage: $0 <scenario> <version> [--sim-time <sec>] [--extra-flags \"...\"] [--print-outdir]" >&2
        exit 1
    fi
    VERSION="$1"; shift
fi

SIM_TIME_OVERRIDE=""
EXTRA_FLAGS=""
PRINT_OUTDIR=false
while [[ $# -gt 0 ]]; do
    case "$1" in
        --sim-time)     SIM_TIME_OVERRIDE="$2"; shift 2 ;;
        --extra-flags)  EXTRA_FLAGS="$2"; shift 2 ;;
        --print-outdir) PRINT_OUTDIR=true; shift ;;
        *) echo "Unknown arg: $1" >&2; exit 1 ;;
    esac
done

# ---------------------------------------------------------------------------
# --all mode: run same scenario against all three versions
# ---------------------------------------------------------------------------
if [[ "$ALL_MODE" == true ]]; then
    echo "=== run-scenario.sh --all $SCENARIO ==="
    echo ""
    printf "%-20s  %-10s  %-12s  %s\n" "version" "exit-code" "wall-clock" "output-dir"
    printf "%-20s  %-10s  %-12s  %s\n" "------" "---------" "----------" "----------"
    for V in ns2-29 ns2-35 ns3; do
        TSTART=$(date +%s)
        ARGS=("$SCENARIO" "$V")
        [[ -n "$SIM_TIME_OVERRIDE" ]] && ARGS+=(--sim-time "$SIM_TIME_OVERRIDE")
        [[ -n "$EXTRA_FLAGS" ]]       && ARGS+=(--extra-flags "$EXTRA_FLAGS")
        RC=0
        "$0" "${ARGS[@]}" 2>&1 | grep -v "^===" | tail -3 || RC=$?
        # Actually capture exit code properly
        set +e
        "$0" "${ARGS[@]}" > /tmp/run-scenario-"${V}".log 2>&1
        RC=$?
        set -e
        TEND=$(date +%s)
        ELAPSED="$((TEND - TSTART))s"
        OUTDIR="output/${V}/${SCENARIO}"
        printf "%-20s  %-10s  %-12s  %s\n" "$V" "$RC" "$ELAPSED" "$OUTDIR"
    done
    echo ""
    echo "Done."
    exit 0
fi

VERSION="${VERSION:-}"

# ---------------------------------------------------------------------------
# Validate inputs
# ---------------------------------------------------------------------------
VALID_SCENARIOS="example-1 example-2 example-2-fullscale example-3 webtraf-ns235-test"
VALID_VERSIONS="ns2-29 ns2-35 ns3"

if ! echo "$VALID_SCENARIOS" | grep -qw "$SCENARIO"; then
    echo "ERROR: Unknown scenario '$SCENARIO'. Valid: $VALID_SCENARIOS" >&2
    exit 1
fi
if ! echo "$VALID_VERSIONS" | grep -qw "$VERSION"; then
    echo "ERROR: Unknown version '$VERSION'. Valid: $VALID_VERSIONS" >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# Output directory
#
# example-2-fullscale ships three TCL variants under
# ns2/diffserv4ns/examples/example-2-fullscale/:
#
#   scenario-2.tcl              — canonical; HTTP_MODEL env switch
#                                 (default bulk-tcp, optional webtraf).
#                                 Output dir is unsuffixed (back-compat).
#   scenario-2-ns235.tcl        — hardcoded WebTraf (ns-2.35 only).
#                                 Output dir gains -webtraf suffix.
#   scenario-2-ns235-srtcm.tcl  — hardcoded WebTraf + srTCM marking.
#                                 Output dir gains -srtcm suffix.
#
# Without per-variant suffixes the helper silently clobbers whichever
# variant ran last into the same dir, breaking apples-to-apples audit
# pairing. The standalone WRED-sweep wrapper
# (scripts/run-ns235-scenario2-bulktcp-sweep.sh) keeps its own dedicated
# `-bulktcp/` dir for the 6-set figure pipeline; the helper is the
# single-set entry point and writes alongside it without collision.
# ---------------------------------------------------------------------------
OUTDIR_SUFFIX=""
if [[ "$SCENARIO" == "example-2-fullscale" ]]; then
    case "${SCENARIO_FILE:-}" in
        ""|"scenario-2.tcl")            OUTDIR_SUFFIX="" ;;
        "scenario-2-webtraf.tcl")       OUTDIR_SUFFIX="-webtraf" ;;
        "scenario-2-ns235.tcl")         OUTDIR_SUFFIX="-webtraf" ;;
        "scenario-2-ns235-srtcm.tcl")   OUTDIR_SUFFIX="-srtcm" ;;
        *)
            echo "WARNING: Unknown SCENARIO_FILE='${SCENARIO_FILE}'; using unsuffixed OUTDIR" >&2
            OUTDIR_SUFFIX=""
            ;;
    esac
fi
OUTDIR="output/${VERSION}/${SCENARIO}${OUTDIR_SUFFIX}"

# --print-outdir: print the computed OUTDIR and exit; no side effects
# (no mkdir, no docker, no ns-3 build). Used by tests and by audit
# pipelines that need to know the output path without running.
if [[ "$PRINT_OUTDIR" == true ]]; then
    echo "$OUTDIR"
    exit 0
fi

mkdir -p "$OUTDIR"
# Clean previous run contents (not the directory itself)
find "$OUTDIR" -mindepth 1 -maxdepth 1 -delete 2>/dev/null || true

echo "=== run-scenario.sh: scenario=$SCENARIO version=$VERSION ==="
echo "    Output: $OUTDIR"
[[ -n "$SIM_TIME_OVERRIDE" ]] && echo "    sim-time override: ${SIM_TIME_OVERRIDE}s"
[[ -n "${SCENARIO_FILE:-}" ]] && echo "    SCENARIO_FILE: ${SCENARIO_FILE}"
echo ""

# ---------------------------------------------------------------------------
# Version-specific dispatch
# ---------------------------------------------------------------------------

run_ns2() {
    local ALLINONE_DIR="$1"
    local NS_BIN="$2"
    local OTCL_VER="$3"
    local TCL_VER="$4"
    local TCL_LIB_DIR="$5"

    # Map scenario to tcl file + working directory
    local TCL_FILE=""
    local EXAMPLE_DIR=""
    local EXTRA_ARGS=""

    case "$SCENARIO" in
        example-1)
            EXAMPLE_DIR="ns2/diffserv4ns/examples/example-1"
            TCL_FILE="simulation-1.tcl"
            # simulation-1.tcl takes: alg simTime pktSize [cbsEF]
            #
            # ns-2.35's hdr_cmn::size() includes IP+UDP headers (28 B), so a
            # 300 kbps payload-rate source produces ~316 kbps on the wire and
            # creates a steady 5% token deficit at CIR=300 kbps. A
            # single-packet cbsEF resonates this into 100% policer drops
            # (DSCP 46 → 48 → MRED early-drop), which then trips
            # divide-by-zero in finish() because no EF packets reach the sink.
            # Pass Cisco MQC default Bc = CIR * 125 ms = 4687 B (TF-TANT
            # commercial-router setting per Ferrari, TF-TANT 2000 [ref 48]).
            # ns-2.29 omits IP/UDP from hdr_cmn::size(), so the source rate
            # exactly matches CIR and a single-packet cbsEF is fine — leave
            # cbsArg empty so simulation-1.tcl falls back to EFPacketSize+1.
            local ALG="PQ"
            local PKT_SIZE="512"
            local SIM_T="${SIM_TIME_OVERRIDE:-200.0}"
            local CBS_EF=""
            if [[ "$VERSION" == "ns2-35" ]]; then
                CBS_EF="4687"
            fi
            EXTRA_ARGS="$ALG $SIM_T $PKT_SIZE $CBS_EF"
            ;;
        example-2)
            EXAMPLE_DIR="ns2/diffserv4ns/examples/example-2"
            TCL_FILE="example-2.tcl"
            # example-2.tcl takes: alg  (testTime is hardcoded to 100 s)
            local ALG="PQ"
            EXTRA_ARGS="$ALG"
            if [[ -n "$SIM_TIME_OVERRIDE" ]]; then
                echo "  WARNING: example-2.tcl has hardcoded testTime=100; --sim-time ignored" >&2
            fi
            ;;
        example-2-fullscale)
            EXAMPLE_DIR="ns2/diffserv4ns/examples/example-2-fullscale"
            # SCENARIO_FILE env override: callers can ask for a non-default
            # variant (e.g. scenario-2-webtraf.tcl for the WebTraf-based
            # port-fidelity audit pair). Default is scenario-2.tcl.
            TCL_FILE="${SCENARIO_FILE:-scenario-2.tcl}"
            # scenario-2.tcl (and its variants) take: paramSet [testTime]
            local PARAM_SET="1"
            local SIM_T="${SIM_TIME_OVERRIDE:-5000}"
            EXTRA_ARGS="$PARAM_SET $SIM_T"
            ;;
        example-3)
            EXAMPLE_DIR="ns2/diffserv4ns/examples/example-3"
            TCL_FILE="scenario-3.tcl"
            # scenario-3.tcl: testTime is hardcoded to 5000 but can be overridden
            # via Tcl global 'overrideTestTime' — pass as extra env or set via argv
            # We inject a small wrapper to override testTime when --sim-time is given.
            if [[ -n "$SIM_TIME_OVERRIDE" ]]; then
                # Create a thin wrapper that sets overrideTestTime then sources the real script
                local WRAPPER="/tmp/run-scenario-wrapper-$$.tcl"
                cat > "$WRAPPER" <<TCLEOF
set overrideTestTime $SIM_TIME_OVERRIDE
source scenario-3.tcl
TCLEOF
                TCL_FILE="$(basename $WRAPPER)"
                # We'll copy it into workdir below
            fi
            ;;
        webtraf-ns235-test)
            EXAMPLE_DIR="ns2/diffserv4ns/examples/webtraf-ns235-test"
            TCL_FILE="webtraf-test-diffserv.tcl"
            if [[ -n "$SIM_TIME_OVERRIDE" ]]; then
                echo "  WARNING: webtraf-test-diffserv.tcl has hardcoded sim time; --sim-time ignored" >&2
            fi
            ;;
    esac

    # Build work directory
    local WORKDIR
    WORKDIR=$(mktemp -d)
    trap "rm -rf $WORKDIR" EXIT

    # Copy example directory contents into workdir (follow symlinks so CDF
    # files in example-3 are copied as regular files)
    cp -rL "$EXAMPLE_DIR/." "$WORKDIR/"

    # apptypes.tcl is sourced as "../common/apptypes.tcl" relative to the
    # example dir, but Docker only mounts the flat workdir.  Copy it into
    # workdir and rewrite the source path in all Tcl files.
    cp "ns2/diffserv4ns/examples/common/apptypes.tcl" "$WORKDIR/"
    # Replace '../common/apptypes.tcl' with 'apptypes.tcl' in all copied Tcl files
    for f in "$WORKDIR"/*.tcl; do
        sed -i.bak 's|source \.\./common/apptypes\.tcl|source apptypes.tcl|g' "$f" 2>/dev/null || true
    done
    rm -f "$WORKDIR"/*.bak 2>/dev/null || true

    # Handle testTime override for scenario-3 (testTime is hardcoded to 5000
    # inside scenario-3.tcl but can be overridden via the global variable)
    if [[ "$SCENARIO" == "example-3" && -n "$SIM_TIME_OVERRIDE" ]]; then
        local WRAPPER="$WORKDIR/run-wrapper-$$.tcl"
        cat > "$WRAPPER" <<TCLEOF
set overrideTestTime $SIM_TIME_OVERRIDE
source scenario-3.tcl
TCLEOF
        TCL_FILE="$(basename "$WRAPPER")"
    fi

    # Remove gnuplot exec calls to avoid failure in Docker (no X11)
    for f in "$WORKDIR"/*.tcl; do
        sed -i.bak '/exec.*gnuplot/d; /source gnuplot/d' "$f" 2>/dev/null || true
    done
    rm -f "$WORKDIR"/*.bak 2>/dev/null || true

    echo "  Running ns-2 ($VERSION) inside Docker..."
    echo "  Tcl: $TCL_FILE $EXTRA_ARGS"
    echo ""

    local RC=0
    docker run --rm \
        -v "$(pwd)/${ALLINONE_DIR}:/ns-allinone" \
        -v "$WORKDIR:/work" \
        -w /work \
        -e LD_LIBRARY_PATH=/ns-allinone/${OTCL_VER}:/ns-allinone/lib \
        -e TCL_LIBRARY=/ns-allinone/${TCL_LIB_DIR}/library \
        ubuntu:18.04 \
        /ns-allinone/${NS_BIN} $TCL_FILE $EXTRA_ARGS $EXTRA_FLAGS \
        2>&1 | tee "$OUTDIR/ns2-stdout.log" || RC=${PIPESTATUS[0]:-$?}

    # Capture any trace files produced.
    # Two layouts must be supported:
    #   (a) Flat — TCL writes *.tr / *.out / *.p / *.png at WORKDIR root
    #       (e.g. scenario-1.tcl). Caught by the top-level globs.
    #   (b) Nested set-N/ — scenario-2.tcl writes traces into a hardcoded
    #       relative path "output/ns2/example-2-fullscale/set-$paramSet/"
    #       inside its cwd, which inside docker is /work (= WORKDIR). The
    #       legacy `$WORKDIR/set-*` glob missed this 4-level nest, silently
    #       losing every fresh S2 ns-2 run since the nested-path TCL change.
    #       Recursive find catches both layouts without enumeration.
    local TRACE_FILES_FOUND=0
    for tr in "$WORKDIR"/*.tr "$WORKDIR"/*.out "$WORKDIR"/*.p "$WORKDIR"/*.png "$WORKDIR"/set-*; do
        if [ -e "$tr" ]; then
            cp -r "$tr" "$OUTDIR/"
            TRACE_FILES_FOUND=$((TRACE_FILES_FOUND + 1))
        fi
    done
    # Sweep nested set-N/ produced via scenario-2.tcl's hardcoded subpath
    # (or any future TCL that writes into a relative output/ tree).
    while IFS= read -r set_dir; do
        [ -d "$set_dir" ] || continue
        cp -r "$set_dir" "$OUTDIR/"
        TRACE_FILES_FOUND=$((TRACE_FILES_FOUND + 1))
    done < <(find "$WORKDIR" -mindepth 2 -type d -name 'set-*' 2>/dev/null)

    # Summary
    local PKT_COUNT=0
    if grep -q "printStats\|pkts\|DSCP\|Packets" "$OUTDIR/ns2-stdout.log" 2>/dev/null; then
        PKT_COUNT=$(grep -oP '\d+(?= pkts| packets)' "$OUTDIR/ns2-stdout.log" 2>/dev/null | head -1 || echo "0")
    fi

    local STDOUT_LINES
    STDOUT_LINES=$(wc -l < "$OUTDIR/ns2-stdout.log" 2>/dev/null || echo 0)

    echo ""
    if [[ $RC -eq 0 ]]; then
        echo "PASS: scenario=$SCENARIO version=$VERSION (stdout $STDOUT_LINES lines, $TRACE_FILES_FOUND trace files)"
    else
        echo "FAIL: scenario=$SCENARIO version=$VERSION (exit code $RC)"
    fi

    return $RC
}

run_ns3() {
    # Map scenario to ns-3 example name
    local NS3_EXAMPLE=""
    local NS3_ARGS="--outputDir=$(pwd)/$OUTDIR"

    case "$SCENARIO" in
        example-1)
            NS3_EXAMPLE="diffserv-example-1"
            [[ -n "$SIM_TIME_OVERRIDE" ]] && NS3_ARGS="$NS3_ARGS --simTime=$SIM_TIME_OVERRIDE"
            ;;
        example-2)
            NS3_EXAMPLE="diffserv-example-2"
            [[ -n "$SIM_TIME_OVERRIDE" ]] && NS3_ARGS="$NS3_ARGS --simTime=$SIM_TIME_OVERRIDE"
            ;;
        example-2-fullscale)
            NS3_EXAMPLE="diffserv-example-2"
            NS3_ARGS="$NS3_ARGS --scale=full"
            [[ -n "$SIM_TIME_OVERRIDE" ]] && NS3_ARGS="$NS3_ARGS --simTime=$SIM_TIME_OVERRIDE"
            ;;
        example-3)
            NS3_EXAMPLE="diffserv-example-3"
            [[ -n "$SIM_TIME_OVERRIDE" ]] && NS3_ARGS="$NS3_ARGS --simTime=$SIM_TIME_OVERRIDE"
            ;;
        example-3-fullscale)
            NS3_EXAMPLE="diffserv-example-3"
            NS3_ARGS="$NS3_ARGS --scale=full"
            [[ -n "$SIM_TIME_OVERRIDE" ]] && NS3_ARGS="$NS3_ARGS --simTime=$SIM_TIME_OVERRIDE"
            ;;
        webtraf-ns235-test)
            echo "ERROR: webtraf-ns235-test is an ns-2.35-only smoke test; no ns-3 equivalent." >&2
            exit 1
            ;;
    esac

    [[ -n "$EXTRA_FLAGS" ]] && NS3_ARGS="$NS3_ARGS $EXTRA_FLAGS"

    if [[ ! -d "ns3/ns-3-dev" ]]; then
        echo "ERROR: ns3/ns-3-dev not found. Run: ./scripts/fetch-ns3.sh" >&2
        exit 1
    fi

    echo "  Running ns-3 example: $NS3_EXAMPLE"
    echo "  Args: $NS3_ARGS"
    echo ""

    local RC=0
    (cd ns3/ns-3-dev && ./ns3 run "$NS3_EXAMPLE $NS3_ARGS" 2>&1) \
        | tee "$OUTDIR/ns3-stdout.log" || RC=${PIPESTATUS[0]:-$?}

    local STDOUT_LINES
    STDOUT_LINES=$(wc -l < "$OUTDIR/ns3-stdout.log" 2>/dev/null || echo 0)

    echo ""
    if [[ $RC -eq 0 ]]; then
        echo "PASS: scenario=$SCENARIO version=$VERSION (stdout $STDOUT_LINES lines)"
    else
        echo "FAIL: scenario=$SCENARIO version=$VERSION (exit code $RC)"
    fi

    return $RC
}

# ---------------------------------------------------------------------------
# Dispatch
# ---------------------------------------------------------------------------
case "$VERSION" in
    ns2-29)
        run_ns2 "ns2/ns-allinone-2.29.3" "ns-2.29/ns" "otcl-1.11" "tcl8.4.11" "tcl8.4.11"
        ;;
    ns2-35)
        run_ns2 "ns2/ns-allinone-2.35" "ns-2.35/ns" "otcl-1.14" "tcl8.5.10" "tcl8.5.10"
        ;;
    ns3)
        run_ns3
        ;;
esac
