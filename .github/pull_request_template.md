<!--
Thanks for the PR. Read CONTRIBUTING.md before opening if you have not
already — it covers the contribution scope (reproductions and bug
fixes welcome via PR; new features or architectural changes coordinated
with the author via a feature-request issue first; use
.github/ISSUE_TEMPLATE/feature.yml).
-->

## Summary

<!--
1-3 sentences describing what this PR changes and why. Reference an
issue if one exists.
-->

## Scope

<!--
- For new behaviour: which substrate client does this touch
  (Classical DiffServ / L4S / CAKE / Wireless / Registry)? Which RFC
  or external paper is the behaviour grounded in?
- For bug fixes: what observable symptom does this resolve? Is there
  a test that pins the regression?
- For docs / recipe additions: which document (README, `specs/`,
  `doc/diffserv.rst`, or an example under `src/ns-3/examples/`) does
  this update?
-->

## Checklist

- [ ] All `diffserv*` test suites pass at EXTENSIVE level:
      `cd ns3/ns-3-dev && python3 test.py --list | grep ^diffserv |
       xargs -I{} python3 test.py -s {} -f EXTENSIVE`
- [ ] New tests for new behaviour (Evaluation-Driven Development)
- [ ] Doxygen `@brief` on new public APIs
- [ ] No modifications to `src/ns-2.29/` (frozen historical reference)
- [ ] No modifications to `ns3/ns-3-dev/` (use `patches/ns3/` workflow)
- [ ] `bash scripts/lint-jargon.sh` exits 0 (public-umbrella content
      must be free of internal-jargon tokens)
- [ ] Commit messages follow conventional-commit prefixes
      (`feat(scope):`, `fix(scope):`, `docs(scope):`, `test(scope):`,
       `refactor(scope):`)
