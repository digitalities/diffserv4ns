#!/usr/bin/env bash
# d2-8-regression.sh — runs the D2-8 regression scenario under the
# patched ns-2.35 build and asserts PASS.
#
# Prereq: scripts/build-ns2-allinone-235-docker.sh must have been
# invoked at least once after scripts/patch-ns2-diffserv-235.sh, so
# the patched ns-2.35 binary is at ns2/ns-allinone-2.35/ns-2.35/ns.
#
# Exit 0 on PASS, non-zero on FAIL.

set -euo pipefail
cd "$(dirname "$0")/.."

NS_BIN="ns2/ns-allinone-2.35/ns-2.35/ns"
SCENARIO="src/ns-2.35/test/d2-8-regression.tcl"

if [[ ! -x "$NS_BIN" ]]; then
    echo "ERROR: $NS_BIN not found or not executable." >&2
    echo "Run scripts/patch-ns2-diffserv-235.sh + scripts/build-ns2-allinone-235-docker.sh first." >&2
    exit 2
fi

if [[ ! -f "$SCENARIO" ]]; then
    echo "ERROR: $SCENARIO not found." >&2
    exit 2
fi

# Run inside the same Ubuntu 18.04 container the build uses so the
# ns binary's libstdc++ + Tcl library paths line up.  Mirrors the
# smoke-test invocation printed by build-ns2-allinone-235-docker.sh.
echo "=== Running D2-8 regression scenario ==="
docker run --rm \
    -v "$(pwd)/ns2/ns-allinone-2.35:/ns-allinone" \
    -v "$(pwd)/src/ns-2.35/test:/test" \
    -e LD_LIBRARY_PATH=/ns-allinone/otcl-1.14:/ns-allinone/lib \
    -e TCL_LIBRARY=/ns-allinone/tcl8.5.10/library \
    -w /test \
    ubuntu:18.04 \
    /ns-allinone/ns-2.35/ns /test/d2-8-regression.tcl \
    | tee /tmp/d2-8-regression.log

if grep -q "^PASS d2-8-regression$" /tmp/d2-8-regression.log; then
    echo ""
    echo "OK: d2-8-regression PASS"
    exit 0
else
    echo ""
    echo "FAIL: d2-8-regression"
    grep "^FAIL" /tmp/d2-8-regression.log || true
    exit 1
fi
