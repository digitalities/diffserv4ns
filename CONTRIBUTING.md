# Contributing to DiffServ4NS

Thanks for considering a contribution. This is a side project run by
the original author of the 2001 ns-2 module; it is licensed under
**GPL-2.0-only** to preserve the lineage of the 2001 release. The goals
of the project are visibility, adoption, and giving back to the ns-3
and bufferbloat communities — not commercial. Contributions of every
size are welcome on those terms.

**What this project welcomes:** reproductions, bug reports, scenario
additions, missing-feature implementations against existing specs,
documentation improvements, and ns-3 mainline fixes surfaced by the
reconstruction work.

**What is coordinated with the author rather than landed via PR:**
architectural direction (new substrate primitives, refactors of the
edge/core composition, classifier-mode additions, scheduler-strategy
contracts). For those, please open an issue first to discuss scope
and design before writing code — this is a single-maintainer
reconstruction project and architectural changes need to fit the
ongoing design narrative.

This file tells you what's worth contributing, how to get started, and
what to expect in return.

## The most useful single contribution

If you only do one thing, **reproduce a scenario and report what
happens.** Open an issue with:

- the scenario you ran (e.g. `diffserv-example-2 --scale=full`)
- the host (Linux/macOS, Docker?), commit SHA you tested
- what matched the paper tolerance and what did not
- the trace files (CSV) attached or linked

This kind of report is the highest-signal contribution to a
reconstruction project. A *successful* reproduction is just as
valuable as a discrepancy — it pins the claim across an independent
environment.

## What this repository is

A 25-year reconstruction of the original DiffServ4NS module
(Andreozzi, 2001) into modern ns-3, alongside its historical ns-2
lineage:

```
src/ns-2.29/   # 2001 original — READ-ONLY (historical reference)
src/ns-2.35/   # 2026 modernisation layer
               # (N2-/D2-/N3- fixes per docs/HISTORICAL_BUGS.md)
src/ns-3/      # The new ns-3 contrib module
specs/         # Evaluation-Driven Development spec suite
patches/ns3/   # Local patches to ns-3 mainline (filed upstream)
```

The full layout, build instructions, and reproduction map are in
[`README.md`](README.md) and [`docs/REPRODUCIBILITY.md`](docs/REPRODUCIBILITY.md).
Read those first if you're new.

## Ways to contribute

Listed roughly from lowest to highest barrier to entry. All are
genuinely welcome.

### 1. Try to reproduce a paper claim or validation scenario

See *The most useful single contribution* above. No coding required.

### 2. Improve documentation

Typos, unclear paragraphs, missing build steps for your platform,
broken links — open a PR or issue. Documentation lives in
`specs/`, `docs/`, `src/ns-3/doc/`, and `README.md` at the repo root.

### 3. Add a missing scenario or example

If you want to exercise the substrate in a way the existing examples
don't (a new RFC, an L4S corner case, a CAKE composition variant),
add an example under `src/ns-3/examples/`. Existing examples are
your template.

### 4. Add a meter, scheduler, or queue-disc to the substrate

This project follows **Evaluation-Driven Development (EDD)**: the
spec suite under `specs/` is the contract, written before the code.
The path is:

1. Find or write the relevant `S-` (Structural) and `Q-` (Quality)
   tier specs — the I-tier (Intent) is generally fixed by the 2001
   thesis or RFC.
2. Write the test first, named for the behaviour it pins
   (e.g. `SrTcmGreenRatioMatchesCirRatioTest`), with a Doxygen
   `@brief` and a `@see specs/02-structural.md S-X.Y` line above
   the class declaration.
3. Implement the minimum code to make the test pass.
4. Run the full test suite to check for regressions.
5. Open the PR.

Read `CLAUDE.md` for the full project conventions before starting a
substantive code contribution.

### 5. Fix a bug

Bug reports go in GitHub Issues; bug fixes go in PRs. For the 2001
ns-2.29 source under `src/ns-2.29/`: **do not modify it** — that
tree is preserved as the historical reference. Fixes go into
`src/ns-2.35/` (the modernisation layer) only. The bug catalogue
lives in `docs/HISTORICAL_BUGS.md`.

### 6. Upstream an ns-3 mainline fix surfaced by this work

If the reconstruction surfaces a defect in ns-3 mainline (we have
two so far: `patches/ns3/0001-tcp-persist-empty-buffer.patch` and
`patches/ns3/0002-tcp-retransmit-tag.patch`), the workflow is:

1. Open an issue here describing the upstream defect and your
   proposed fix.
2. Prepare an upstream-quality artifact (issue text, proposed patch,
   regression test).
3. File against `gitlab.com/nsnam/ns-3-dev`.
4. Commit the patch as a `.patch` file under `patches/ns3/`.

`scripts/fetch-ns3.sh` auto-applies anything in `patches/ns3/`.

## Building and testing

First-time setup:

```bash
./scripts/fetch-ns3.sh    # ns-3.47 pinned + patches/ns3/ auto-applied
./scripts/fetch-ns2.sh    # ns-2.29 frozen + ns-2.35 modernised
cd ns3/ns-3-dev
./ns3 configure --enable-tests --enable-examples
./ns3 build diffserv
```

Run the test suite:

```bash
python3 test.py -s diffserv                         # core diffserv suite
python3 test.py -s diffserv-l4s                     # L4S routing
python3 test.py -s diffserv-per-flow-classifier     # per-flow classifier
python3 test.py -s diffserv-cake-q15                # CAKE Q-tier replication
```

A passing PR is one where these four suites all stay green. If your
change makes a test fail, the test or the implementation is wrong —
not the tolerance. If a tolerance feels wrong, raise it as a question
on the issue/PR rather than widening it silently.

For ns-2.35 contributions, build inside Docker:

```bash
./scripts/build-ns2-allinone-235-docker.sh   # ubuntu:18.04 + GCC 7
```

(macOS host clang has a `<version>` header shadowing trap that masks
downstream errors. Always validate ns-2.35 changes via Docker.)

## Coding conventions

- **ns-3 style** (CamelCase, `m_` member prefix, `k_` constants).
- **Headers** include `NS3_DIFFSERV_<FILE>_H` guards.
- **Namespace** `ns3::diffserv` (double namespace prevents clashes
  with mainline classes like `RedQueueDisc`).
- **GPL-2.0-only header** preserving Sergio Andreozzi (2001–2026),
  Nortel Networks (2000), and Imputato/Avallone for traffic-control
  patterns where adapted.
- **No silent edits to `ns3/ns-3-dev/`** — go through `patches/ns3/`.
- **Doxygen + Comments**: follow ns-3 mainline Doxygen conventions.
  Rule of thumb: shipped source comments describe what the code does
  in present tense; internal-jargon tokens (phase labels, PR labels,
  bug-catalogue identifiers, plan-doc paths) are not used.
  `scripts/lint-jargon.sh` enforces this.

## Pull-request checklist

- [ ] Issue opened first for non-trivial work (so we can discuss
      scope before you spend time)
- [ ] Tests pass: all four diffserv test suites at EXTENSIVE level
- [ ] New tests for new behaviour (EDD discipline)
- [ ] Doxygen comments on new public APIs
- [ ] No modifications to `src/ns-2.29/` (frozen)
- [ ] No modifications to `ns3/ns-3-dev/` (use `patches/ns3/`
      workflow if you need to)
- [ ] Commit messages use a bracket-prefix style describing the
      kind of change (`[bug]`, `[doc]`, `[test]`, `[paper]`, etc.);
      reference a spec ID where it applies

## Response time and scope expectations

This is a side project. Realistic response times:

- **Issues**: ~1 week to first triage; longer for complex
  reproduction reports
- **PRs**: small docs/test PRs ~1 week; larger features negotiated
  on the issue first
- **Releases**: tagged opportunistically, not on a fixed cadence

If you don't hear back in two weeks, please nudge the issue/PR — I
genuinely want to engage and may have just missed the notification.

Out of scope:

- Closed-source forks (incompatible with GPL-2.0-only)
- Performance benchmarks of ns-3 itself (this is a *module*; ns-3
  performance work belongs upstream)
- Inter-domain DiffServ control plane (RSVP/NSIS), IPv6, MPLS shim
  handling — listed as future work in the paper, not on the v1 path

## Reporting security issues

This module simulates network behaviour; security-sensitive code
paths are limited (no real packets sent on a real network). If you
find a defect with potential security impact (e.g., a way to crash
ns-3 from a crafted packet trace), please email
<digitalities@gmail.com> rather than opening a public issue, and
allow ~1 week before public disclosure.

## Credit

External contributions are committed under the contributor's name +
email; the project does not require a CLA or DCO sign-off.

If your contribution is substantive enough to warrant it, you'll be
acknowledged in the project's decision records or — for paper-
relevant work — directly in the paper acknowledgements. Feel free
to ask if you'd like such acknowledgement for a specific
contribution.

## Code of conduct

Be civil, technically rigorous, and patient with people new to
ns-3. The project follows the spirit of the [Contributor Covenant
v3.0](https://www.contributor-covenant.org/version/3/0/code_of_conduct/);
formal adoption is on the to-do list.

## Author

Sergio Andreozzi —
[@digitalities](https://github.com/digitalities) on GitHub —
[ORCID 0000-0001-5567-4000](https://orcid.org/0000-0001-5567-4000).
Independent researcher (Amsterdam, Netherlands). Original DiffServ4NS
author (2001 ns-2 implementation; M.Sc. thesis, University of Pisa).

For non-security correspondence, please open a GitHub issue or
discussion rather than emailing directly — this keeps the project
conversation public and findable for future contributors.
