# diffserv test suite

This directory contains the Evaluation-Driven Development (EDD) test
surface for the `diffserv` ns-3 module. Tests pin behaviour against
the three-tier spec suite under `../../../specs/`:

- **I-tier** (Intent — `specs/01-intent.md`): what the module shall
  do. Capability assertions, RFC traceability, design intent.
- **S-tier** (Structural — `specs/02-structural.md`): testable
  per-component assertions linked to one or more I-specs. **One
  S-assertion → one test class** is the convention.
- **Q-tier** (Quality — `specs/03-quality.md`): end-to-end scenario
  tolerances against the original ns-2 outputs and against RFC
  conformance vectors.

## Files

| File | Suite | Scope |
|------|-------|-------|
| `diffserv-test-suite.cc` | `diffserv` | The release suite. Meters (`TokenBucket`, `srTCM`, `trTCM`, `TSW`), schedulers (`PQ`, `RR`, `WRR`, `WIRR`, `SCFQ`, `SFQ`, `WFQ`, `WF2Q+`, `LLQ`), queue discs (`RIO_C`, `RIO_D`, `WRED`), edge/core composition, and Q-tier scenario replications. ~88 test classes. |
| `diffserv-cake-q15-test-suite.cc` | `diffserv-cake-q15` | CAKE Q-tier replication: tin shaping, host isolation, hybrid LLQ + DRR, helper DSCP-map parity with `tc-cake(8)`. |
| `l4s-routing-test.cc` | `diffserv-l4s` | L4S routing structural checks: ECN-codepoint dispatch, slot-byte-identity, multi-slot DSCP routing, DualPI2 controller behaviour. |
| `per-flow-classifier-test.cc` | `diffserv-per-flow-classifier` | `PerFlowPolicyClassifier`: bucket isolation, refill, passthrough, edge dispatch, wildcard rules. |
| `empirical-cdf-loader-test.cc` | `diffserv-empirical-cdf-loader` | `EmpiricalCdfLoader`: file parsing, bounds, sampling determinism. |
| `rfc-test-vectors.h` + `rfc-test-vectors-runner.cc` | (data + helper) | RFC 2697/2698/2859/9331/9332 conformance vectors consumed by the meter tests in `diffserv-test-suite.cc`. |
| `cake-reference-data/` | (data) | Reference traces and expected outputs for the CAKE Q-tier replication. |
| `test-manifest.txt` | (snapshot) | Source-extracted list of `AddTestCase()` invocations across all five suites. Diffing this file at release-tag time flags suite-rename, test-class rename, and test add/remove. Regenerate via `scripts/regen-test-manifest.sh`. |

## Running tests

The build path is `ns3/ns-3-dev/`. From that directory:

```bash
# Configure once (enables the test runner and examples).
./ns3 configure --enable-tests --enable-examples

# Build the module.
./ns3 build diffserv

# Run all five diffserv suites at the default fullness (QUICK).
python3 test.py -s diffserv
python3 test.py -s diffserv-cake-q15
python3 test.py -s diffserv-empirical-cdf-loader
python3 test.py -s diffserv-l4s
python3 test.py -s diffserv-per-flow-classifier

# Run a single test class by name.
python3 test.py -s diffserv -v -r SrTcmIdleAccumulationTest

# Run including the EXTENSIVE Q-tier scenarios (slower, scenario
# replications + statistical convergence checks).
./build/utils/ns3-dev-test-runner-default --suite=diffserv \
    --verbose --fullness=EXTENSIVE
```

The `python3 test.py` entry point is convenient for routine work;
the `ns3-dev-test-runner-default` binary exposes more granular flags
(suite filter, fullness, verbose output, individual test selection).

## Test fullness tiers

ns-3 classifies test cases by a `Duration` enum:

- **QUICK** — fast unit tests; arithmetic checks, single-packet
  scenarios, structural assertions. The default fullness; runs in
  under 10 seconds across the whole `diffserv` suite. CI runs this
  tier on every change.
- **EXTENSIVE** — Q-tier scenario replications (`Example2ThreeClassTest`,
  `S3PerClassRatePreservationTest`, `AfDropPrecedenceQualityTest`,
  `ThreeClassCoexistenceTest`, `PerfRegressionTest`) and statistical
  assertions that need a full simulation run. Several minutes per
  case; runs at release-tag time and on architectural changes.
- **TAKES_FOREVER** — not currently used.

## Naming convention

Tests are named `BriefDescriptionTest` (camel-case, terminating
`Test`). Each test class carries a Doxygen block immediately above
its declaration with two lines only:

```cpp
/// @brief One-sentence statement of the property under test.
/// @see specs/02-structural.md S-X.Y
class SrTcmIdleAccumulationTest : public TestCase
{
  ...
};
```

The `@see` line is the binding from the test class to the spec it
pins. Q-tier tests reference `specs/03-quality.md Q-X.Y`; I-tier
references appear only on tests that span multiple S-assertions.

A handful of pre-existing classes carry the legacy `XxxTestCase`
suffix or the `TestXxx` prefix — these reflect older convention and
are gradually being renamed to the `BriefDescriptionTest` form;
new tests should always use the canonical form.

## Spec ID cross-references

Spec identifiers appear throughout the test source and the public
documentation:

| Token | Source file | Meaning |
|-------|-------------|---------|
| `I-N` | `specs/01-intent.md` | Intent assertion N (capability the module shall provide). |
| `S-X.Y` | `specs/02-structural.md` | Structural assertion Y under topic group X (one observable property of one component). |
| `Q-X.Y` | `specs/03-quality.md` | Quality assertion Y under scenario group X (end-to-end tolerance). |
| `F-A` … `F-D` | catalogue in repo root | Empirical-finding identifier surfaced during validation. |
| `N2-N`, `D2-N`, `N3-N` | `docs/HISTORICAL_BUGS.md` | Bug catalogue: ns-2 core defects, DiffServ4NS-for-ns-2 defects, ns-3 core defects respectively. |

These identifiers are stable contract tokens — they are referenced
from the paper, the handbook, and the public README. Renaming any
identifier requires updating every citing source plus this directory.

## Adding a new test

The EDD workflow is documented in the release-root
`CONTRIBUTING.md` under *Add a meter, scheduler, or queue-disc to
the substrate*; the short version is:

1. Find or write the relevant `S-` or `Q-` spec (the I-tier is
   generally fixed by the 2001 thesis or an RFC).
2. Write the test first, named for the behaviour it pins, with the
   `@brief` + `@see` Doxygen pair.
3. Implement the minimum code to make the test pass.
4. Run the full diffserv test suite to check for regressions.
5. Regenerate `test-manifest.txt` (`bash scripts/regen-test-manifest.sh`).
6. Open the PR.

If the test you would like to add does not have a backing spec, open
an issue first — the spec is the contract, code without a spec
assertion is unverifiable in this project.

## Authority

Behaviour disagreements between sources are resolved in this order
(per the release-root `CLAUDE.md`):

1. The three-tier EDD spec suite (`specs/`).
2. The RFCs (2474, 2475, 2597, 2598, 2697, 2698, 2859, 3246, 9331,
   9332).
3. The 2001 thesis at `provenance/Andreozzi-2001-thesis.pdf`,
   Chapter 3.3.3.
4. The original ns-2 reference at `src/ns-2.29/diffserv/`.
5. ns-3 idioms.

If the implementation disagrees with a spec, the spec is wrong (or
needs tightening with a divergence note) — but never silently widen
a test tolerance to make a failing test pass.
