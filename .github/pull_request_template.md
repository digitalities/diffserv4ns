<!--
Thanks for the PR. Read CONTRIBUTING.md before opening if you have not
already — it covers the contribution scope (reproductions, build fixes,
and bug fixes welcome via PR; anything that changes the 2001 design is
coordinated with the author via a feature-request issue first; use
.github/ISSUE_TEMPLATE/feature.yml).

This repository is the ns-2 archive. Pull requests against the ns-3 QoS
substrate belong in https://github.com/digitalities/stratum-ns3.
-->

## Summary

<!--
1-3 sentences describing what this PR changes and why. Reference an
issue if one exists.
-->

## Scope

<!--
- For bug fixes: what observable symptom does this resolve? Which
  ns-2 version is affected (2.29, 2.35, or both)?
- For build fixes: which platform and toolchain does this unblock?
- For scenario additions: which example, and what does it exercise
  that the existing ones do not?
- For docs: which document (README, docs/installation.md,
  docs/REPRODUCIBILITY.md, ...) does this update?
-->

## Checklist

- [ ] Affected scenarios still run and produce the documented results
      (say which you ran, and on which ns-2 version)
- [ ] ns-2.35 changes validated via the Docker build
      (`./scripts/build-ns2-allinone-235-docker.sh`), not a host build
- [ ] No modifications to `src/ns-2.29/` (frozen historical reference)
- [ ] `bash scripts/lint-jargon.sh` exits 0 (shipped content must be
      free of internal-jargon tokens)
- [ ] Commit messages follow conventional-commit prefixes
      (`feat(scope):`, `fix(scope):`, `docs(scope):`, `test(scope):`,
       `refactor(scope):`)
