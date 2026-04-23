#!/bin/bash
# Build ns-allinone-2.35 inside a Docker container.
#
# Why Docker + Ubuntu 18.04 + GCC 7?
#
# ns-2.35 (released 2011) still uses some pre-C++11 idioms that are
# errors or warnings in modern compilers. GCC 7 (Ubuntu 18.04 default)
# compiles it cleanly with -fpermissive where needed.
#
# Platform: native Docker platform (arm64 on Apple Silicon, amd64 on Intel).
# GCC 7 on Ubuntu 18.04 arm64 compiles ns-2.35 fine.
#
# Why not ./install?
#
# The stock ./install script builds the full allinone tree including
# Tk, nam, xgraph. Tk and nam require X11 display access and hang in
# a headless Docker container. This script builds ONLY the four
# components needed to run ns: Tcl → OTcl → TclCL → ns-2.
#
# ns-2.35 ships with:
#   Tcl 8.5.10, OTcl 1.14, TclCL 1.20
#
# The build artifacts live inside ns2/ns-allinone-2.35/ (gitignored).
#
# Prerequisites: Docker installed and running.

set -euo pipefail
cd "$(dirname "$0")/.."

ALLINONE="ns2/ns-allinone-2.35"

if [ ! -d "$ALLINONE" ]; then
    echo "ERROR: $ALLINONE not found." >&2
    echo "Ensure the ns-allinone-2.35 source tree is present." >&2
    exit 1
fi

# Ensure we have the native-platform image
echo ">>> Pulling ubuntu:18.04 for native platform..."
docker pull -q ubuntu:18.04

echo "=== Building ns-2.35 in Docker (Ubuntu 18.04) ==="
echo "    Building: Tcl 8.5.10 → OTcl 1.14 → TclCL 1.20 → ns-2.35"
echo "    (skipping Tk/nam/xgraph)"
echo ""

docker run --rm \
    -v "$(pwd)/$ALLINONE:/ns-allinone" \
    -w /ns-allinone \
    ubuntu:18.04 \
    bash -c '
        set -euo pipefail
        export DEBIAN_FRONTEND=noninteractive

        echo ">>> Installing build dependencies..."
        timeout 600 apt-get update -qq
        timeout 600 apt-get install -y -qq --no-install-recommends \
            build-essential \
            autoconf \
            autotools-dev \
            > /dev/null 2>&1
        echo ">>> Dependencies installed."
        echo ""

        CUR_PATH=$(pwd)
        TCLVER=8.5.10
        OTCLVER=1.14
        TCLCLVER=1.20
        NSVER=2.35

        # ---- 0a. Update config.guess/config.sub for aarch64 support ----
        echo ">>> Updating config.guess/config.sub for aarch64..."
        SYS_GUESS=$(find /usr -name config.guess -type f 2>/dev/null | head -1)
        SYS_SUB=$(find /usr -name config.sub -type f 2>/dev/null | head -1)
        if [ -z "$SYS_GUESS" ] || [ -z "$SYS_SUB" ]; then
            echo "WARNING: could not find system config.guess/config.sub" >&2
        else
            find . -name config.guess -type f -exec cp "$SYS_GUESS" {} \;
            find . -name config.sub   -type f -exec cp "$SYS_SUB"   {} \;
            echo "  Replaced $(find . -name config.guess -type f | wc -l) config.guess files"
        fi
        echo ""

        # ---- 0b. Clean stale build artifacts from prior runs ----
        echo ">>> Cleaning stale build artifacts..."
        for d in tcl$TCLVER/unix otcl-$OTCLVER tclcl-$TCLCLVER ns-$NSVER; do
            if [ -f "$d/Makefile" ]; then
                echo "  Cleaning $d..."
                (cd "$d" && make distclean 2>/dev/null || true)
            fi
        done
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
        cp ../unix/*.h    ../../include/
        echo ">>> Tcl $TCLVER installed."
        cd $CUR_PATH

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
        # Strip -lX11 and X11 paths — configure unconditionally adds them
        sed -i "s|-lX11||g; s|-I[^ ]*/X11[^ ]*||g; s|-L[^ ]*/X11[^ ]*||g" Makefile
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
        # const-correctness issues in TclCL: downgrade errors to warnings
        sed -i -E "s|^(CCOPT[[:space:]]*=)|\1 -fpermissive|" Makefile
        # Remove Tk-dependent objects (V_LIBRARY_TK=FAIL without --with-tk)
        sed -i "s|embedded-tk.o||g; s|embedded-tklobj.o||g" Makefile
        make || { echo "TclCL make failed"; exit 1; }
        echo ">>> TclCL $TCLCLVER installed."
        cd $CUR_PATH

        # Remove .so to force static linking
        test -f ./otcl-$OTCLVER/libotcl.a && rm -f ./otcl-$OTCLVER/libotcl.so

        # ---- 4. Build ns-2.35 ----
        echo "============================================================"
        echo "* Build ns-$NSVER"
        echo "============================================================"
        cd ./ns-$NSVER
        rm -f Makefile config.cache
        ./configure --with-tk=no --x-includes=no --x-libraries=no \
            || { echo "ns-2 configure failed"; exit 1; }
        sed -i "s|-lX11||g; s|-I[^ ]*/X11[^ ]*||g; s|-L[^ ]*/X11[^ ]*||g" Makefile
        # ns-2.35 has const-correctness issues with GCC 7
        sed -i -E "s|^(CCOPT[[:space:]]*=)|\1 -fpermissive|" Makefile
        # mdart/mdart_adp.cc has ambiguous hash() vs std::hash in GCC 7+.
        # The inline hash(nsaddr_t) defined in mdart_function.h is in global
        # namespace; qualify calls with :: to bypass std::hash lookup.
        sed -i \
            "s/= hash(reqId)/= ::hash(reqId)/g; \
             s/= hash(mdart_->id_)/= ::hash(mdart_->id_)/g" \
            mdart/mdart_adp.cc
        # tkAppInit.cc requires Tk headers (tk.h) which are absent.
        # We only need the headless "ns" binary; exclude nstk and tkAppInit.o.
        sed -i "s|common/tkAppInit.o||g" Makefile
        make ns || { echo "ns-2 make failed"; exit 1; }
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
    echo "ns-2.35 binary:  $ALLINONE/ns-2.35/ns"
    echo "Tcl shell:       $ALLINONE/bin/tclsh8.5"
    echo ""
    echo "Smoke test:"
    echo "  docker run --rm \\"
    echo "    -v \"\$(pwd)/$ALLINONE:/ns-allinone\" \\"
    echo "    -e LD_LIBRARY_PATH=/ns-allinone/otcl-1.14:/ns-allinone/lib \\"
    echo "    -e TCL_LIBRARY=/ns-allinone/tcl8.5.10/library \\"
    echo "    ubuntu:18.04 \\"
    echo "    /ns-allinone/ns-2.35/ns -c 'puts [Queue/dsRED info class]; exit 0'"
    echo ""
    echo "To run a DiffServ simulation:"
    echo "  docker run --rm \\"
    echo "    -v \"\$(pwd)/$ALLINONE:/ns-allinone\" \\"
    echo "    -v \"\$(pwd)/ns2/diffserv4ns/examples:/examples\" \\"
    echo "    -w /examples \\"
    echo "    ubuntu:18.04 \\"
    echo "    bash -c 'export LD_LIBRARY_PATH=/ns-allinone/otcl-1.14:/ns-allinone/lib; \\"
    echo "             export TCL_LIBRARY=/ns-allinone/tcl8.5.10/library; \\"
    echo "             /ns-allinone/ns-2.35/ns simulation-1.tcl'"
else
    echo ""
    echo "=== BUILD FAILED (exit code $RC) ===" >&2
    echo "Check the output above for errors." >&2
    exit $RC
fi
