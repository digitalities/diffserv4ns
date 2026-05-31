#!/bin/bash
# Fetch pristine ns-2.29 source as read-only reference for diffing.
# DiffServ4NS modifies several ns-2 base files; this script provides
# the unmodified originals for comparison via docs/NS2_PATCHES.md.

set -euo pipefail
cd "$(dirname "$0")/.."

TARGET="ns2/ns-2.29-pristine"

if [ -d "$TARGET" ]; then
    echo "ns-2.29 pristine source already present at $TARGET"
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
    echo "and place the extracted ns-2.29 directory at $TARGET" >&2
    exit 1
fi

echo "Extracting $OUTFILE..."
tar xzf "$OUTFILE"

# The tarball extracts as ns-allinone-2.29/ containing ns-2.29/ and
# a bunch of supporting directories (tcl, tk, otcl, tclcl, nam, xgraph).
# We only need ns-2.29 itself for patch comparison.
if [ -d "ns-allinone-2.29/ns-2.29" ]; then
    mv "ns-allinone-2.29/ns-2.29" ns-2.29-pristine
    rm -rf ns-allinone-2.29
elif [ -d "ns-allinone-2.29.3/ns-2.29" ]; then
    mv "ns-allinone-2.29.3/ns-2.29" ns-2.29-pristine
    rm -rf ns-allinone-2.29.3
elif [ -d "ns-allinone-2.29.3/ns-2.29.3" ]; then
    mv "ns-allinone-2.29.3/ns-2.29.3" ns-2.29-pristine
    rm -rf ns-allinone-2.29.3
else
    echo "ERROR: extracted archive does not have the expected ns-2.29 subdirectory" >&2
    echo "Contents:" >&2
    ls -la >&2
    exit 1
fi

rm -f "$OUTFILE"

echo ""
echo "Done. Pristine ns-2.29 source is at ns2/ns-2.29-pristine/"
echo "Do not modify it. It is the reference for NS2_PATCHES.md."
