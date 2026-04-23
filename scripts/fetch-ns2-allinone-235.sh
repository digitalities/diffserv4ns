#!/bin/bash
# Fetch the full ns-allinone-2.35 tree for building the ns-2.35 DiffServ4NS
# port.
#
# Sibling of fetch-ns2-allinone-229.sh (which fetches ns-allinone-2.29.3);
# same pattern, different release.
#
# Idempotent: exits 0 if target already exists.

set -euo pipefail
cd "$(dirname "$0")/.."

TARGET="ns2/ns-allinone-2.35"

if [ -d "$TARGET" ]; then
    echo "ns-allinone-2.35 already present at $TARGET"
    exit 0
fi

mkdir -p ns2
cd ns2

# SourceForge download paths. The trailing /download triggers the mirror
# redirect chain; direct-file URLs return HTML error pages.
URLS=(
  "https://sourceforge.net/projects/nsnam/files/allinone/ns-allinone-2.35/ns-allinone-2.35.tar.gz/download"
)

OUTFILE=""
for URL in "${URLS[@]}"; do
    echo "Attempting: $URL"
    CANDIDATE="ns-allinone-2.35-candidate.tar.gz"
    if curl -L --fail --silent --show-error -o "$CANDIDATE" "$URL"; then
        if file "$CANDIDATE" | grep -q 'gzip compressed'; then
            SIZE=$(stat -f%z "$CANDIDATE" 2>/dev/null || stat -c%s "$CANDIDATE")
            if [ "$SIZE" -gt 1000000 ]; then
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
    echo "ERROR: could not fetch ns-allinone-2.35 from any known URL." >&2
    echo "You can download it manually from:" >&2
    echo "  https://sourceforge.net/projects/nsnam/files/allinone/ns-allinone-2.35/" >&2
    echo "and extract it at $TARGET" >&2
    exit 1
fi

echo "Extracting $OUTFILE..."
tar xzf "$OUTFILE"

if [ ! -d "ns-allinone-2.35" ]; then
    echo "ERROR: extracted archive does not have the expected directory" >&2
    echo "Contents:" >&2
    ls -la >&2
    rm -f "$OUTFILE"
    exit 1
fi

rm -f "$OUTFILE"

echo ""
echo "Done. Full ns-allinone-2.35 tree is at ns2/ns-allinone-2.35/"
echo ""
echo "Contents:"
ls ns-allinone-2.35/
echo ""
echo "=== Build instructions ==="
echo ""
echo "On Linux (or in Docker):"
echo "  cd ns2/ns-allinone-2.35"
echo "  ./install"
echo ""
echo "On macOS (recommended: use Docker due to modern toolchain incompatibility):"
echo "  ./scripts/build-ns2-allinone-235-docker.sh"
echo ""
echo "NOTE: This tree does NOT yet include the DiffServ4NS ns-2.35 port."
echo "Apply it with: ./scripts/patch-ns2-diffserv-235.sh"
