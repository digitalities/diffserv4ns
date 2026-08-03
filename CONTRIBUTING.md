# Contributing to DiffServ4NS

Thanks for considering a contribution. This is a side project run by
the original author of the 2001 ns-2 module; it is licensed under
**GPL-2.0-only** to preserve the lineage of the 2001 release. The goals
of the project are visibility, adoption, and giving back to the network
simulation community — not commercial. Contributions of every size are
welcome on those terms.

> **Looking to contribute to the ns-3 substrate?** *Stratum* — the ns-3
> QoS substrate composing DiffServ, L4S, and CAKE — is developed in a
> separate repository,
> [digitalities/stratum-ns3](https://github.com/digitalities/stratum-ns3).
> Open ns-3 issues and pull requests there. This repository holds the
> ns-2 lineage only.

**What this project welcomes:** reproductions, bug reports, build fixes
for modern toolchains, scenario additions, and documentation
improvements.

**What is coordinated with the author rather than landed via PR:**
anything that changes the behaviour of the 2001 design. The ns-2.29
sources are a historical artefact; the ns-2.35 layer exists to make them
build and run correctly on a modern toolchain, not to evolve the design.
For anything in that territory, please open an issue first.

This file tells you what's worth contributing, how to get started, and
what to expect in return.

## The most useful single contribution

If you only do one thing, **reproduce a scenario and report what
happens.** Open an issue with:

- the scenario you ran (e.g. `example-2-fullscale` on `ns2-35`)
- the host (Linux/macOS, Docker?), and the commit SHA you tested
- what matched the documented result and what did not
- the trace files attached or linked

This kind of report is the highest-signal contribution to an archival
project. A *successful* reproduction is just as valuable as a
discrepancy — it pins the claim across an independent environment.

## What this repository is

The historical DiffServ4NS module (Andreozzi, 2001) for ns-2, in two
variants:

```
src/ns-2.29/   # 2001 original — READ-ONLY (historical reference)
src/ns-2.35/   # 2026 port layer for ns-2.35
               # (fixes catalogued in docs/HISTORICAL_BUGS.md)
examples/      # The 2001 scenarios, plus ns-2.35 regression tests
```

The full layout, build instructions, and reproduction map are in
[`README.md`](README.md) and
[`docs/REPRODUCIBILITY.md`](docs/REPRODUCIBILITY.md). Read those first
if you're new.

## Ways to contribute

Listed roughly from lowest to highest barrier to entry. All are
genuinely welcome.

### 1. Try to reproduce a validation scenario

See *The most useful single contribution* above. No coding required.

### 2. Improve documentation

Typos, unclear paragraphs, missing build steps for your platform,
broken links — open a PR or issue. Documentation lives in `docs/` and
`README.md` at the repo root.

### 3. Fix a build failure on a modern toolchain

ns-2 predates most of the compilers people now have installed. The
Docker path exists precisely because host toolchains drift. If you get
the build working on a platform the scripts don't cover, that fix is
welcome.

### 4. Add a missing scenario or example

If you want to exercise the module in a way the existing examples
don't, add one under `examples/`. The existing scenarios are your
template.

### 5. Fix a bug

Bug reports go in GitHub Issues; bug fixes go in PRs. For the 2001
ns-2.29 source under `src/ns-2.29/`: **do not modify it** — that tree
is preserved as the historical reference. Fixes go into `src/ns-2.35/`
(the port layer) only. The bug catalogue lives in
[`docs/HISTORICAL_BUGS.md`](docs/HISTORICAL_BUGS.md).

## Building and testing

First-time setup:

```bash
./scripts/fetch-ns2-allinone.sh        # ns-allinone-2.29.3
./scripts/patch-ns2-diffserv.sh        # patch DiffServ4NS into the tree
./scripts/build-ns2-allinone-docker.sh # build inside Docker
```

For the ns-2.35 port layer:

```bash
./scripts/fetch-ns2-allinone-235.sh
./scripts/patch-ns2-diffserv-235.sh
./scripts/build-ns2-allinone-235-docker.sh   # ubuntu:18.04 + GCC 7
```

Always validate ns-2.35 changes via Docker. macOS host clang has a
`<version>` header shadowing trap that masks downstream errors.

Run a scenario:

```bash
bash scripts/run-scenario.sh example-1 ns2-35
bash scripts/run-scenario.sh --all example-1     # both versions, summary table
```

If your change makes a documented result move, the change or the
expectation is wrong — not the tolerance. If a tolerance feels wrong,
raise it as a question on the issue or PR rather than widening it
silently.

## Coding conventions

- **Match the surrounding ns-2 code.** This is a 2001 codebase; new
  code in `src/ns-2.35/` should read like its neighbours rather than
  like modern C++.
- **GPL-2.0-only header** preserving Sergio Andreozzi (2001-2026) and
  Nortel Networks (2000).
- **No modifications to `src/ns-2.29/`** — it is frozen.
- Internal-jargon tokens (phase labels, PR labels, bug-catalogue
  identifiers, plan-doc paths) are not used in shipped sources.
  `scripts/lint-jargon.sh` enforces this.

## Pull-request checklist

- [ ] Issue opened first for non-trivial work (so we can discuss
      scope before you spend time)
- [ ] The affected scenarios still run and produce the documented
      results
- [ ] No modifications to `src/ns-2.29/` (frozen)
- [ ] `scripts/lint-jargon.sh` is clean
- [ ] Commit messages use a bracket-prefix style describing the
      kind of change (`[bug]`, `[doc]`, `[test]`, etc.)

## Response time and scope expectations

This is a side project. Realistic response times:

- **Issues**: ~1 week to first triage; longer for complex
  reproduction reports
- **PRs**: small docs/test PRs ~1 week; larger changes negotiated
  on the issue first
- **Releases**: tagged opportunistically, not on a fixed cadence

If you don't hear back in two weeks, please nudge the issue/PR — I
genuinely want to engage and may have just missed the notification.

Out of scope:

- Closed-source forks (incompatible with GPL-2.0-only)
- New features that change the 2001 design (the archive's value is
  that it is what was written)
- Anything targeting ns-3 — that belongs in
  [stratum-ns3](https://github.com/digitalities/stratum-ns3)

## Reporting security issues

This module simulates network behaviour; security-sensitive code
paths are limited (no real packets sent on a real network). If you
find a defect with potential security impact (e.g., a way to crash
the simulator from a crafted trace), please email
<digitalities@gmail.com> rather than opening a public issue, and
allow ~1 week before public disclosure.

## Credit

External contributions are committed under the contributor's name +
email; the project does not require a CLA or DCO sign-off.

If your contribution is substantive enough to warrant it, you'll be
acknowledged in the project's release notes. Feel free to ask if you'd
like such acknowledgement for a specific contribution.

## Code of conduct

Be civil, technically rigorous, and patient with people new to ns-2.
The project follows the spirit of the [Contributor Covenant
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
