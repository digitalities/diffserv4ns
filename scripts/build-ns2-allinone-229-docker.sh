#!/bin/bash
# Build ns-allinone-2.29.3 inside a Docker container.
#
# Why Docker + Ubuntu 18.04 + GCC 7 + amd64?
#
# ns-2.29 uses pre-C++11 idioms that are errors in
# modern compilers:
#   - implicit int return types and K&R-style declarations
#   - 'register' storage class (removed in C++17, warning in C++14)
#   - narrowing conversions in aggregate initialisers
#   - missing #include <cstring> / <cstdlib> (relied on transitive
#     includes that changed in libstdc++ 8+)
#
# GCC 7 (Ubuntu 18.04's default) is the last major GCC release that
# compiles ns-2.29 out of the box without patching. GCC 8+ and all
# versions of Apple Clang since Xcode 14 reject the code.
#
# Platform: we use the native Docker platform (arm64 on Apple Silicon,
# amd64 on Intel). GCC 7 on Ubuntu 18.04 arm64 compiles ns-2.29 fine
# and avoids Rosetta/QEMU emulation overhead.
#
# Fallback: if the native arm64 build fails (e.g. autoconf scripts
# don't recognise aarch64), add --platform linux/amd64 to the
# docker run command below to force x86 emulation.
#
# Why not ./install?
#
# The stock ./install script builds the full allinone tree including
# Tk, nam, xgraph, cweb, sgb, and gt-itm. Tk and nam require X11
# display access and hang in a headless Docker container. xgraph and
# the graph libraries are unnecessary for running ns-2 simulations.
#
# Instead, this script builds ONLY the four components needed to run
# ns with Tcl scripts: Tcl → OTcl → TclCL → ns-2. This is faster,
# avoids X11 dependencies, and produces the same `ns` binary.
#
# The build artifacts (ns binary, tclsh, libraries) live inside
# ns2/ns-allinone-2.29.3/ which is gitignored.
#
# Prerequisites: Docker installed and running.

set -euo pipefail
cd "$(dirname "$0")/.."

ALLINONE="ns2/ns-allinone-2.29.3"

# Fetch if not present
if [ ! -d "$ALLINONE" ]; then
    echo "ns-allinone-2.29.3 not found. Fetching..."
    ./scripts/fetch-ns2-allinone-229.sh
fi

if [ ! -d "$ALLINONE" ]; then
    echo "ERROR: fetch failed, $ALLINONE still missing." >&2
    exit 1
fi

# Ensure we have the native-platform image (avoids using a cached
# amd64 image on an arm64 host, which triggers QEMU emulation).
echo ">>> Pulling ubuntu:18.04 for native platform..."
docker pull -q ubuntu:18.04

echo "=== Building ns-2.29 in Docker (Ubuntu 18.04) ==="
echo "    Building: Tcl → OTcl → TclCL → ns-2 (skipping Tk/nam/xgraph)"
echo ""

# Native platform (arm64 on Apple Silicon, amd64 on Intel).
# If native arm64 fails, add --platform linux/amd64 here.
docker run --rm \
    -v "$(pwd)/$ALLINONE:/ns-allinone" \
    -w /ns-allinone \
    ubuntu:18.04 \
    bash -c '
        set -euo pipefail
        export DEBIAN_FRONTEND=noninteractive

        echo ">>> Installing build dependencies (timeout 600s)..."
        timeout 600 apt-get update -qq
        timeout 600 apt-get install -y -qq --no-install-recommends \
            build-essential \
            autoconf \
            autotools-dev \
            > /dev/null 2>&1
        echo ">>> Dependencies installed."
        echo ""

        CUR_PATH=$(pwd)
        TCLVER=8.4.11
        OTCLVER=1.11
        TCLCLVER=1.17
        NSVER=2.29

        # ---- 0a. Update config.guess/config.sub for aarch64 support ----
        # The 2005-vintage config.guess shipped with ns-2.29 does not
        # recognise aarch64. Replace all copies with the modern version
        # from autotools-dev.
        echo ">>> Updating config.guess/config.sub for aarch64..."
        # Find the system config.guess (autotools-dev installs it)
        SYS_GUESS=$(find /usr -name config.guess -type f 2>/dev/null | head -1)
        SYS_SUB=$(find /usr -name config.sub -type f 2>/dev/null | head -1)
        if [ -z "$SYS_GUESS" ] || [ -z "$SYS_SUB" ]; then
            echo "WARNING: could not find system config.guess/config.sub" >&2
            echo "         aarch64 builds may fail" >&2
        else
            echo "  System config.guess: $SYS_GUESS"
            echo "  System config.sub:   $SYS_SUB"
            # Replace every config.guess/config.sub in the source tree
            find . -name config.guess -type f -exec cp "$SYS_GUESS" {} \;
            find . -name config.sub   -type f -exec cp "$SYS_SUB"   {} \;
            echo "  Replaced $(find . -name config.guess -type f | wc -l) config.guess files"
        fi
        echo ">>> config.guess/config.sub updated."
        echo ""

        # ---- 0b. Clean stale build artifacts from prior runs ----
        # If a previous build ran on a different architecture (e.g. amd64
        # via QEMU), the .o files and .a libraries are incompatible.
        # Remove them so configure + make start fresh.
        echo "============================================================"
        echo "* Cleaning stale build artifacts"
        echo "============================================================"
        for d in tcl$TCLVER/unix otcl-$OTCLVER tclcl-$TCLCLVER ns-$NSVER; do
            if [ -f "$d/Makefile" ]; then
                echo "  Cleaning $d..."
                (cd "$d" && make distclean 2>/dev/null || true)
            fi
        done
        # Also remove any leftover .o files in Tcl generic dir
        find . -name "*.o" -delete 2>/dev/null || true
        find . -name "*.a" -not -path "./gt-itm/*" -delete 2>/dev/null || true
        echo ">>> Clean complete."
        echo ""

        # ---- 1. Build Tcl (no Tk — headless) ----
        echo "============================================================"
        echo "* Build Tcl $TCLVER"
        echo "============================================================"
        cd ./tcl$TCLVER/unix
        ./configure --enable-gcc --disable-shared --prefix=$CUR_PATH \
            || { echo "Tcl configure failed"; exit 1; }
        make || { echo "Tcl make failed"; exit 1; }
        make install
        cp ../generic/*.h ../../include/
        echo ">>> Tcl $TCLVER installed."
        cd $CUR_PATH

        # Put tclsh in PATH for subsequent builds
        PATH=$CUR_PATH/tcl$TCLVER/unix:$PATH
        export PATH
        LD_LIBRARY_PATH=$CUR_PATH/tcl$TCLVER/unix:${LD_LIBRARY_PATH:-}
        export LD_LIBRARY_PATH

        # ---- 2. Build OTcl (without X/Tk) ----
        echo "============================================================"
        echo "* Build OTcl $OTCLVER"
        echo "============================================================"
        cd ./otcl-$OTCLVER
        ./configure --with-tcl=$CUR_PATH --with-tk=no \
            --x-includes=no --x-libraries=no \
            || { echo "OTcl configure failed"; exit 1; }
        # Strip -lX11 and X11 include/lib paths from Makefile — the
        # 2005 configure unconditionally adds them even with --with-tk=no.
        sed -i "s|-lX11||g; s|-I[^ ]*/X11[^ ]*||g; s|-L[^ ]*/X11[^ ]*||g" Makefile
        # Build only libotcl.a and otclsh — skip owish (needs tk.h)
        make libotcl.a otclsh || { echo "OTcl make failed"; exit 1; }
        echo ">>> OTcl $OTCLVER installed."
        cd $CUR_PATH

        # ---- 3. Build TclCL (without X/Tk) ----
        echo "============================================================"
        echo "* Build TclCL $TCLCLVER"
        echo "============================================================"
        cd ./tclcl-$TCLCLVER
        ./configure --with-tcl=$CUR_PATH --with-tk=no \
            --x-includes=no --x-libraries=no \
            || { echo "TclCL configure failed"; exit 1; }
        sed -i "s|-lX11||g; s|-I[^ ]*/X11[^ ]*||g; s|-L[^ ]*/X11[^ ]*||g" Makefile
        # TclCL has const-correctness bugs that GCC 7 on arm64 treats as
        # errors. Add -fpermissive to downgrade to warnings.
        sed -i -E "s|^(CCOPT[[:space:]]*=)|\1 -fpermissive|" Makefile
        # With --with-tk=no, V_LIBRARY_TK=FAIL, so the Makefile tries to
        # build embedded-tk.cc from FAIL/tk.tcl. Remove Tk-dependent objects.
        sed -i "s|embedded-tk.o||g; s|embedded-tklobj.o||g" Makefile
        make || { echo "TclCL make failed"; exit 1; }
        echo ">>> TclCL $TCLCLVER installed."
        cd $CUR_PATH

        # Remove .so to force static linking (John'\''s hack from ./install)
        test -f ./otcl-$OTCLVER/libotcl.a && rm -f ./otcl-$OTCLVER/libotcl.so

        # ---- 4. Build ns-2 ----
        echo "============================================================"
        echo "* Build ns-$NSVER"
        echo "============================================================"
        cd ./ns-$NSVER
        # Remove old Makefile directly — make distclean fails if
        # Makefile.in is newer than Makefile (our patch triggers this).
        rm -f Makefile config.cache
        ./configure --with-tk=no --x-includes=no --x-libraries=no \
            || { echo "ns-2 configure failed"; exit 1; }
        sed -i "s|-lX11||g; s|-I[^ ]*/X11[^ ]*||g; s|-L[^ ]*/X11[^ ]*||g" Makefile
        # ns-2.29 has const-correctness issues with GCC 7 on arm64
        sed -i -E "s|^(CCOPT[[:space:]]*=)|\1 -fpermissive|" Makefile
        make || { echo "ns-2 make failed"; exit 1; }
        echo ">>> ns-$NSVER installed."
        cd $CUR_PATH

        # ---- 5. Create bin/ symlinks ----
        mkdir -p bin
        cd bin
        ln -sf $CUR_PATH/ns-$NSVER/ns ns
        cd $CUR_PATH

        echo ""
        echo ">>> Build complete."
        echo ">>> ns binary: /ns-allinone/ns-$NSVER/ns"
        echo ">>> tclsh:     /ns-allinone/tcl$TCLVER/unix/tclsh$TCLVER"
    '

RC=$?
if [ $RC -eq 0 ]; then
    echo ""
    echo "=== SUCCESS ==="
    echo "ns-2 binary:  $ALLINONE/ns-2.29/ns"
    echo "Tcl shell:    $ALLINONE/bin/tclsh8.4"
    echo ""
    echo "To run an ns-2 simulation:"
    echo "  docker run --rm \\"
    echo "    -v \"\$(pwd)/$ALLINONE:/ns-allinone\" \\"
    echo "    -v \"\$(pwd)/ns2/diffserv4ns/examples:/examples\" \\"
    echo "    -w /examples \\"
    echo "    ubuntu:18.04 \\"
    echo "    bash -c 'export LD_LIBRARY_PATH=/ns-allinone/otcl-1.11:/ns-allinone/lib; \\"
    echo "             export TCL_LIBRARY=/ns-allinone/tcl8.4.11/library; \\"
    echo "             /ns-allinone/ns-2.29/ns simulation-1.tcl'"
    echo ""
    echo "To patch in DiffServ4NS (if not already done):"
    echo "  ./scripts/patch-ns2-diffserv-229.sh"
    echo "  ./scripts/build-ns2-allinone-229-docker.sh   # rebuild"
else
    echo ""
    echo "=== BUILD FAILED (exit code $RC) ===" >&2
    echo "Check the output above for errors." >&2
    echo "Common issues:" >&2
    echo "  - Docker not running" >&2
    echo "  - Network issues fetching apt packages" >&2
    echo "  - On Apple Silicon: Rosetta/QEMU may be slow (10-20 min is normal)" >&2
    exit $RC
fi
