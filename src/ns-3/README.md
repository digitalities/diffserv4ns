# diffserv

The ns-3 port of DiffServ4NS (Andreozzi, 2001). A composable
DiffServ + L4S + CAKE substrate: full Differentiated Services
pipeline (RFC 2474/2475/2597/2598/2697/2698/2859/3246) with a native
RFC 9332 DualPI2 Coupled AQM, a `tc-cake(8)`-equivalent CAKE
composition (RFC 8290 informed), CAKE host-isolation, 15 named
link-layer overhead presets, 8 named RTT presets, PTM framing,
ingress shaping mode, Linux autorate-ingress closed loop, per-tin
byte-cap eviction, integrated edge/core pipeline, and per-DSCP inner
dispatch enabling hierarchical AQM composition. Includes an
RFC-7928-aligned AQM characterisation harness
(`examples/aqm-eval-runner`) covering 13 queue-disc configurations.

## Installing via the ns-3 App Store

The module is distributed through the ns-3 App Store. To install it
into your own ns-3-dev tree:

1. **Use the pinned ns-3-dev revision.** Built and tested against
   commit `cc48bf5c15a4918364abc2b2b060b4056dce09a4` (`ns-3.47+83`).
   Newer revisions may need API adjustments.

2. **Apply the local ns-3-dev patches.** The module depends on three
   small fixes / extensions to ns-3 mainline that are filed upstream
   but may not yet be merged in your ns-3-dev checkout. See
   `patches/ns3/README.md` in the release repo for the inventory and
   upstream-MR status (currently `!2829` and `!2830` filed).

3. **Drop the module into your `contrib/`.** Either git-clone the
   release repo and symlink `src/ns-3` into `<ns-3-dev>/contrib/diffserv`,
   or copy the contents of `src/ns-3/` directly into a new
   `<ns-3-dev>/contrib/diffserv/` directory.

4. **Build and run.**

   ```bash
   cd <ns-3-dev>
   ./ns3 configure --enable-tests --enable-examples
   ./ns3 build diffserv
   python3 test.py -s diffserv -v
   ```

   Expected: all tests pass.

## Citing

If you use this module in academic work, please cite the release
repository. A `CITATION.cff` is provided at the release root for
GitHub-rendered citation widgets, and the substrate paper provides
the canonical academic reference once published; see the release
README for current DOIs.

## Documentation

- **Reference (this repo):** `doc/diffserv.rst` — Sphinx-format
  module documentation in the ns-3 standard layout.
- **Specifications (release repo):** `specs/` — the tiered evaluation
  suite (intent / structural / quality) that defines and gates module
  behaviour.
- **Reproducing the published results (release repo):**
  `docs/REPRODUCIBILITY.md` — how to regenerate the evidence behind
  the results.

Historically called `diffserv4ns3` in-tree (see ADR-0003); renamed
to `diffserv` on 2026-04-19 (ADR-0036) to match the ns-3 contrib
naming convention and to align with the C++ namespace
`ns3::diffserv`.

## Layout

```
diffserv/
├── CMakeLists.txt              ← ns-3 build integration (LIBNAME diffserv)
├── model/                      ← meters, queue discs, schedulers, edge/core
├── helper/                     ← DiffServHelper, monitor, OnOff helper
├── test/                       ← 130+ test EDD suite (structural + L4S + CAKE + per-flow + CDF)
├── examples/                   ← diffserv-example-1, -2, -3, hierarchical-l4s, etc.
└── doc/diffserv.rst            ← Sphinx model doc
```

## Naming conventions

- **Module name:** `diffserv` (ADR-0036; was `diffserv4ns3` pre-2026-04-19).
- **C++ namespace:** `ns3::diffserv` — all classes nested inside `namespace ns3 { namespace diffserv { ... } }` to prevent collisions with mainline names like `RedQueueDisc`.
- **Class prefix:** `Ds` for queue discs and schedulers (e.g. `DsRedQueueDisc`, `DsL4sQueueDisc`, `DsWfqScheduler`); `DiffServ` where disambiguation is needed (e.g. `DiffServEdgeQueueDisc`, `DiffServPolicyClassifier`).
- **File names:** kebab-case matching the class name (e.g. `ds-red-queue-disc.h` for `DsRedQueueDisc`).

## Authority

Behaviour order of authority (per top-level `CLAUDE.md`):

1. The three-tier EDD spec suite in `../../specs/` (Intent, Structural, Quality).
2. The RFCs (2474, 2475, 2597, 2598, 2697, 2698, 2859, 3246, 9331, 9332).
3. The 2001 thesis at `../../provenance/Andreozzi-2001-thesis.pdf` Chapter 3.3.3.
4. The original ns-2 reference at `../ns-2.29/diffserv/`.
5. ns-3 idioms.

## Build and test (release development checkout)

When working from the release repo (development checkout, with the
companion ns-2 sources, paper, and handbook), the build path is:

```bash
# scripts/fetch-ns3.sh creates the contrib/diffserv symlink and
# applies patches/ns3/ on a fresh ns-3-dev clone.
cd ../../ns3/ns-3-dev
./ns3 configure --enable-tests --enable-examples
./ns3 build diffserv
python3 test.py -s diffserv -v
```

Expected: all tests pass on the pinned ns-3-dev revision
(`cc48bf5c15a4918364abc2b2b060b4056dce09a4`, i.e. `ns-3.47+83`).

App Store users who installed only this module follow the build flow
in the *Installing via the ns-3 App Store* section above.
