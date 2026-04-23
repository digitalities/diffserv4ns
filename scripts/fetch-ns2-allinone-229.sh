#!/bin/bash
# Fetch the full ns-allinone-2.29.3 tree for building ns-2.
#
# Unlike fetch-ns2.sh (which extracts only the ns-2.29 subdirectory for
# diffing against DiffServ4NS modifications), this script keeps the FULL
# allinone tree: tcl, tk, otcl, tclcl, nam, xgraph, and all build
# infrastructure needed to compile ns-2.
#
# Idempotent: exits 0 if target already exists.

set -euo pipefail
cd "$(dirname "$0")/.."

TARGET="ns2/ns-allinone-2.29.3"

if [ -d "$TARGET" ]; then
    echo "ns-allinone-2.29.3 already present at $TARGET"
    exit 0
fi

mkdir -p ns2
cd ns2

# Try each candidate URL in turn until one gives us a real tarball.
# SourceForge paths for old releases require the trailing /download
# to trigger the mirror redirect chain.
URLS=(
  "https://sourceforge.net/projects/nsnam/files/allinone/ns-allinone-2.29/ns-allinone-2.29.tar.gz/download"
  "https://sourceforge.net/projects/nsnam/files/allinone/ns-allinone-2.29/ns-allinone-2.29.3.tar.gz/download"
)

OUTFILE=""
for URL in "${URLS[@]}"; do
    echo "Attempting: $URL"
    CANDIDATE="ns-allinone-2.29-candidate.tar.gz"
    if curl -L --fail --silent --show-error -o "$CANDIDATE" "$URL"; then
        # Verify we actually got a gzip, not an HTML error page
        if file "$CANDIDATE" | grep -q 'gzip compressed'; then
            SIZE=$(stat -f%z "$CANDIDATE" 2>/dev/null || stat -c%s "$CANDIDATE")
            if [ "$SIZE" -gt 1000000 ]; then  # must be at least 1 MB
                echo "  → valid gzip, $SIZE bytes"
                OUTFILE="$CANDIDATE"
                break
            else
                echo "  → too small ($SIZE bytes), skipping"
                rm -f "$CANDIDATE"
            fi
        else
            echo "  → not a gzip (got $(file -b "$CANDIDATE")), skipping"
            rm -f "$CANDIDATE"
        fi
    else
        echo "  → download failed"
        rm -f "$CANDIDATE"
    fi
done

if [ -z "$OUTFILE" ]; then
    echo ""
    echo "ERROR: could not fetch ns-allinone-2.29 from any known URL." >&2
    echo "You can download it manually from:" >&2
    echo "  https://sourceforge.net/projects/nsnam/files/allinone/ns-allinone-2.29/" >&2
    echo "and extract it at $TARGET" >&2
    exit 1
fi

echo "Extracting $OUTFILE (keeping full allinone tree)..."
tar xzf "$OUTFILE"

# Normalize directory name to ns-allinone-2.29.3 regardless of which
# tarball we got (2.29 vs 2.29.3).
if [ -d "ns-allinone-2.29" ] && [ ! -d "ns-allinone-2.29.3" ]; then
    mv "ns-allinone-2.29" "ns-allinone-2.29.3"
fi

if [ ! -d "ns-allinone-2.29.3" ]; then
    echo "ERROR: extracted archive does not have the expected directory" >&2
    echo "Contents:" >&2
    ls -la >&2
    rm -f "$OUTFILE"
    exit 1
fi

rm -f "$OUTFILE"

echo ""
echo "Done. Full ns-allinone-2.29.3 tree is at ns2/ns-allinone-2.29.3/"
echo ""
echo "Contents:"
ls ns-allinone-2.29.3/
echo ""
echo "=== Build instructions ==="
echo ""
echo "On Linux (or in Docker):"
echo "  cd ns2/ns-allinone-2.29.3"
echo "  ./install"
echo ""
echo "On macOS (recommended: use Docker due to modern toolchain incompatibility):"
echo "  ./scripts/build-ns2-allinone-229-docker.sh"
echo ""
echo "NOTE: This tree does NOT yet include the DiffServ4NS module."
echo "Patching it in is a separate step (see scripts/patch-ns2-diffserv-229.sh)."
