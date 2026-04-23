# Installation

DiffServ4NS ships two source variants side-by-side:

- **`src/ns-2.29/`** — the DiffServ4NS module as distributed in the 2006 SourceForge release (DiffServ4NS-0.2). Algorithms designed in 2001 against ns-2.1b8a; 2006 re-skinning for the ns-2.29 API. Preserved read-only.
- **`src/ns-2.35/`** — a 2026 port layer that forward-ports the module to ns-2.35 (the last stable ns-2 release, 2011), fixing nine 2001-era bugs and correcting the UDP header size.

This page provides build instructions for both:

- [**Modern (Docker), ns-2.29**](#building-ns-229-on-modern-systems-docker) — verified on macOS (Apple Silicon) and Linux in 2026
- [**Modern (Docker), ns-2.35**](#building-ns-235-on-modern-systems-docker) — same toolchain, different release
- [**Legacy**](#legacy-instructions-2006) — the original 2006 instructions for ns-2.29, preserved as-is


---

## Building ns-2.29 on modern systems (Docker)

### The problem

ns-2 (and DiffServ4NS) use pre-C++11 idioms that modern compilers reject:

- Implicit `int` return types and K&R-style declarations
- `register` storage class (removed in C++17)
- Narrowing conversions in aggregate initialisers
- Missing `#include <cstring>` / `#include <cstdlib>` (relied on transitive includes that changed in libstdc++ 8+)

**GCC 7** (Ubuntu 18.04's default) is the last major GCC release that compiles ns-2.29 without patching. GCC 8+ and all versions of Apple Clang since Xcode 14 reject the code.

### The solution

Build inside a Docker container running Ubuntu 18.04 with GCC 7. This works on:

- **macOS** (Apple Silicon M-series and Intel) — native arm64, no Rosetta/QEMU needed
- **Linux** (x86_64 and arm64)
- **Windows** (via WSL2 + Docker Desktop)

Build time: ~5 minutes on Apple Silicon.

### Prerequisites

- [Docker](https://www.docker.com/products/docker-desktop/) installed and running
- ~500 MB disk space for the ns-allinone source tree
- Internet access (to download ns-2 source and Ubuntu packages)

### Quick start

```bash
git clone https://github.com/digitalities/diffserv4ns.git
cd diffserv4ns

# 1. Download ns-allinone-2.29.3 source tree
./scripts/fetch-ns2-allinone-229.sh

# 2. Build vanilla ns-2 inside Docker
./scripts/build-ns2-allinone-229-docker.sh

# 3. Patch in DiffServ4NS module
./scripts/patch-ns2-diffserv-229.sh

# 4. Rebuild with DiffServ4NS
./scripts/build-ns2-allinone-229-docker.sh
```

### What the build script does

The Docker build ([`scripts/build-ns2-allinone-229-docker.sh`](https://github.com/digitalities/diffserv4ns/blob/main/scripts/build-ns2-allinone-229-docker.sh)) builds only the four components needed for headless simulation: **Tcl 8.4.11 → OTcl 1.11 → TclCL 1.17 → ns-2.29**. It skips Tk, nam, and xgraph (which require X11 and are unnecessary for running simulations).

Six workarounds are applied automatically:

| Workaround | Why |
|------------|-----|
| Replace `config.guess`/`config.sub` | The vintage versions don't recognise aarch64 (ARM64) |
| Strip `-lX11` flags | Configure adds X11 linker flags even with `--with-tk=no` |
| Add `-fpermissive` to CCOPT | GCC 7 on arm64 is stricter about `const char*` → `char*` conversions |
| Remove `embedded-tk.o` from TclCL | TclCL tries to embed `tk.tcl` even with `--with-tk=no` |
| Clean stale `.o`/`.a` files | Prior builds for a different architecture cause linker errors |
| `DEBIAN_FRONTEND=noninteractive` | Prevents `apt-get` from hanging on timezone prompts |

### What the patch script does

The patch script ([`scripts/patch-ns2-diffserv-229.sh`](https://github.com/digitalities/diffserv4ns/blob/main/scripts/patch-ns2-diffserv-229.sh)) is idempotent (safe to run multiple times) and:

1. Copies `src/ns-2.29/diffserv/*.{h,cc}` into `ns-2.29/diffserv/`, replacing Nortel originals
2. Copies 14 modified ns-2 base files (packet.h, agent, tcp, udp, telnet, loss-monitor, etc.)
3. Adds `dsscheduler.o` to `Makefile.in`

### Verifying the build

After building, confirm DiffServ4NS classes are loaded:

```bash
docker run --rm \
  -v "$(pwd)/ns2/ns-allinone-2.29.3:/ns-allinone" \
  -w /ns-allinone \
  -e LD_LIBRARY_PATH=/ns-allinone/otcl-1.11:/ns-allinone/lib \
  -e TCL_LIBRARY=/ns-allinone/tcl8.4.11/library \
  ubuntu:18.04 \
  bash -c './ns-2.29/ns <<EOF
puts "Queue/dsRED:       [Queue/dsRED info class]"
puts "Queue/dsRED/edge:  [Queue/dsRED/edge info class]"
puts "Queue/dsRED/core:  [Queue/dsRED/core info class]"
exit 0
EOF'
```

Expected output:

```
Queue/dsRED:       Class
Queue/dsRED/edge:  Class
Queue/dsRED/core:  Class
```

### Running simulations

**Run a Tcl script:**

```bash
docker run --rm \
  -v "$(pwd)/ns2/ns-allinone-2.29.3:/ns-allinone" \
  -v "$(pwd)/examples:/examples" \
  -w /examples/example-1 \
  -e LD_LIBRARY_PATH=/ns-allinone/otcl-1.11:/ns-allinone/lib \
  -e TCL_LIBRARY=/ns-allinone/tcl8.4.11/library \
  ubuntu:18.04 \
  /ns-allinone/ns-2.29/ns simulation-1.tcl
```

**Interactive shell** (for debugging):

```bash
docker run --rm -it \
  -v "$(pwd)/ns2/ns-allinone-2.29.3:/ns-allinone" \
  -v "$(pwd):/project" \
  -w /project \
  -e LD_LIBRARY_PATH=/ns-allinone/otcl-1.11:/ns-allinone/lib \
  -e TCL_LIBRARY=/ns-allinone/tcl8.4.11/library \
  -e PATH="/ns-allinone/ns-2.29:/ns-allinone/bin:$PATH" \
  ubuntu:18.04 \
  bash
```

This gives you a shell where `ns` is on PATH and the full project directory is mounted at `/project`.

### Troubleshooting

| Problem | Fix |
|---------|-----|
| `docker: command not found` | Install [Docker Desktop](https://www.docker.com/products/docker-desktop/) |
| Build fails on Apple Silicon | The script uses native arm64 — this should work. If it doesn't, add `--platform linux/amd64` to the `docker run` line in `build-ns2-allinone-229-docker.sh` (slower, uses QEMU) |
| `Cannot connect to the Docker daemon` | Start Docker Desktop |
| Build very slow (>10 minutes) | A native-arm64 build on Apple Silicon typically completes in ~2 minutes; on Intel/amd64 in 3–5 minutes. Builds above ~10 minutes usually indicate QEMU emulation. Check `docker info` — `Architecture` should match your host |
| SourceForge download fails | Download [ns-allinone-2.29.3](https://sourceforge.net/projects/nsnam/files/allinone/ns-allinone-2.29/) manually and extract to `ns2/ns-allinone-2.29.3/` |

---

## Building ns-2.35 on modern systems (Docker)

The ns-2.35 port layer (under [`src/ns-2.35/`](../src/ns-2.35/)) is additive to a stock ns-2.35 tree. It applies the same DiffServ4NS module as the 2001 original plus fixes for nine latent 2001-era bugs (see [`src/ns-2.35/CHANGELOG.md`](../src/ns-2.35/CHANGELOG.md)).

### Quick start

```bash
git clone https://github.com/digitalities/diffserv4ns.git
cd diffserv4ns

# 1. Download ns-allinone-2.35 source tree
./scripts/fetch-ns2-allinone-235.sh

# 2. Build vanilla ns-2.35 inside Docker
./scripts/build-ns2-allinone-235-docker.sh

# 3. Patch in the DiffServ4NS ns-2.35 port layer
./scripts/patch-ns2-diffserv-235.sh

# 4. Rebuild with the port applied
./scripts/build-ns2-allinone-235-docker.sh
```

### What differs from the ns-2.29 path

- **Same toolchain.** Ubuntu 18.04 + GCC 7, same Docker image. ns-2.35 (2011) still uses pre-C++11 idioms that modern compilers reject.
- **No `-lX11` stripping needed.** ns-2.35's configure cooperates with `--with-tk=no`.
- **Shorter file list.** The port layer touches fewer files than the 2001 patch (`src/ns-2.35/` is ~22 files vs. 30 for `src/ns-2.29/`); deprecated components like `dsPolicy.h`'s legacy entry points are not re-patched.
- **UDP header fix.** The port adds the missing 28-byte IP+UDP header accounting that the 2001 code omitted (see [`src/ns-2.35/CHANGELOG.md`](../src/ns-2.35/CHANGELOG.md)).

### Why both variants?

Running scenarios under **both** variants produces a cross-simulator consistency check: if a scenario's outputs match between ns-2.29 (frozen 2001 code) and ns-2.35 (2026 port with bug fixes applied), the port preserved the 2001 semantics.

---

## Legacy instructions (2006)

> These are the installation instructions from the DiffServ4NS-0.2 SourceForge README, targeting ns-2.29. The module was originally developed in 2001 against ns-2.1b8a and retargetted to ns-2.29 for the 2006 release without code changes.

### Installing on a working ns-2.29

1. Download the DiffServ4NS source
2. Extract it:
   ```bash
   tar -zxf DiffServ4NS-0.2.tar.gz
   ```
3. Copy modified source files over your ns-2.29 installation:
   ```bash
   cp -rf diffserv4ns/src/* /path/to/ns-2.29/
   ```
4. Rebuild:
   ```bash
   cd /path/to/ns-2.29/
   make clean
   rm *.o -rf
   ./configure
   make
   ```

### Fresh install (ns-2.29 + DiffServ4NS)

1. Download [ns-allinone-2.29.3](https://sourceforge.net/projects/nsnam/files/)
2. Extract:
   ```bash
   tar -zxf ns-allinone-2.29.3.tar.gz
   ```
3. Download and extract DiffServ4NS
4. Overlay the source files:
   ```bash
   cp -rf diffserv4ns/src/* ns-allinone-2.29/ns-2.29/
   ```
5. Build ns-2:
   ```bash
   cd ns-allinone-2.29
   ./install &> install.out
   ```
6. Set environment variables as reported at the end of `install.out`

---

## Running the examples

### Example 1: Scheduler comparison

Scenario 1 from the thesis (Section 4.1; thesis DOI [10.5281/zenodo.19662899](https://doi.org/10.5281/zenodo.19662899)): a comparison of scheduling algorithms for delay- and jitter-sensitive EF traffic.

```bash
cd examples/example-1
source start          # legacy (direct ns-2 install)
```

### Example 2: AF PHB differentiation

Scenario 2 (thesis Section 4.2; thesis DOI [10.5281/zenodo.19662899](https://doi.org/10.5281/zenodo.19662899)): the level-of-importance differentiation within the AF PHB group using WRED parameter tuning.

```bash
cd examples/example-2
ns example-2.tcl      # legacy (direct ns-2 install)
```

For Docker-based execution, see [Running simulations](#running-simulations) above.

---

## Modified ns-2 files

DiffServ4NS modifies the following ns-2.29 files (all under `src/ns-2.29/`; the ns-2.35 port layer at `src/ns-2.35/` touches the same directories with forward-ported versions):

| Directory | Files | Purpose |
|-----------|-------|---------|
| `diffserv/` | dsCore, dsred, dsredq, dsPolicy, dsEdge, dsscheduler, dsconsts | Core DiffServ module |
| `common/` | agent.h, agent.cc, packet.h | Per-packet metadata (sendtime, app_type) |
| `tcp/` | tcp.h, tcp.cc | TCP state visibility (cwnd, RTT) |
| `apps/` | udp.cc, telnet.cc | Sendtime stamping, app-type tagging |
| `tools/` | loss-monitor.h, loss-monitor.cc, cbr_traffic.cc | OWD/IPDV monitoring framework |
| `webcache/` | webtraf.cc | Web traffic app-type tagging |
| `realaudio/` | realaudio.cc | RealAudio app-type tagging |
| `tcl/lib/` | ns-default.tcl, ns-source.tcl | Default values, FTP app-type |
