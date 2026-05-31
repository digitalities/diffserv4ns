#!/usr/bin/env bash
# scripts/regen-test-manifest.sh
#
# Captures the ns-3 diffserv test surface to src/ns-3/test/test-manifest.txt.
# Run on every release tag (or whenever a test is added or renamed).
#
# Output format: per-suite list of test classes registered via
# AddTestCase(new XxxTest()). Loop-unrolled registrations show up as
# a single entry — the runtime case count is therefore an upper bound
# on the lines per suite. Diffing the manifest at release-tag time
# detects: suite added/removed, test class added/removed, test class
# renamed.
#
# Source-extracted (no build required) so the manifest stays
# regeneratable from a fresh clone.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

readonly OUT="src/ns-3/test/test-manifest.txt"
readonly TEST_DIR="src/ns-3/test"

# suite-name|source-file  — keep entries one-per-line for diff sanity.
readonly SUITES=(
  "diffserv|diffserv-test-suite.cc"
  "diffserv-cake-q15|diffserv-cake-q15-test-suite.cc"
  "diffserv-empirical-cdf-loader|empirical-cdf-loader-test.cc"
  "diffserv-l4s|l4s-routing-test.cc"
  "diffserv-per-flow-classifier|per-flow-classifier-test.cc"
  "diffserv-q16-chang-convergence|diffserv-q16-chang-convergence-test.cc"
  "diffserv-q17-parekh-theorem1|diffserv-q17-parekh-theorem1-test.cc"
  "diffserv-wf2qp-regression|diffserv-wf2qp-regression-test.cc"
)

date_iso="$(date -u +%Y-%m-%d)"
ns3_pin="unknown"
if [ -d "ns3/ns-3-dev/.git" ]; then
  ns3_pin="$(cd ns3/ns-3-dev && git rev-parse --short=10 HEAD 2>/dev/null || echo unknown)"
fi

{
  printf '# diffserv ns-3 test manifest\n'
  printf '#\n'
  printf '# Snapshot of the test surface registered via AddTestCase() across\n'
  printf '# %s. A diff against this file at release-tag time flags any\n' "$TEST_DIR"
  printf '# suite-rename, test-class rename, or test-class add/remove that\n'
  printf '# changed the surface.\n'
  printf '#\n'
  printf '# Regenerate via: bash scripts/regen-test-manifest.sh\n'
  printf '#\n'
  printf '# Loop-unrolled registrations (e.g. AddTestCase(new XxxTestCase(i))\n'
  printf '# inside a for-loop) appear as a single line; the actual runtime\n'
  printf '# case count is therefore >= the lines listed per suite. To audit\n'
  printf '# runtime case counts, run\n'
  printf '#   ./build/utils/ns3-dev-test-runner-default --suite=<suite> --verbose --fullness=EXTENSIVE\n'
  printf '# from inside ns3/ns-3-dev/.\n'
  printf '#\n'
  printf '# Generated: %s\n' "$date_iso"
  printf '# ns-3-dev pin: %s\n' "$ns3_pin"
  printf '\n'
} > "$OUT"

total_invocations=0

for entry in "${SUITES[@]}"; do
  suite="${entry%%|*}"
  file="${entry#*|}"
  full_path="$TEST_DIR/$file"

  if [ ! -f "$full_path" ]; then
    printf 'WARN: %s missing; skipping suite %s\n' "$full_path" "$suite" >&2
    continue
  fi

  printf '## %s (%s)\n\n' "$suite" "$file" >> "$OUT"

  # AddTestCase(new XxxTestCase()  — extract the class name token.
  # Sed is preferred over awk for portable POSIX BRE matching.
  classes=$(
    grep -E 'AddTestCase\(new [A-Za-z][A-Za-z0-9_]*' "$full_path" \
      | sed -E 's|.*AddTestCase\(new ([A-Za-z][A-Za-z0-9_]*).*|\1|'
  )

  if [ -z "$classes" ]; then
    printf '(no AddTestCase invocations found)\n\n' >> "$OUT"
    continue
  fi

  printf '%s\n\n' "$classes" >> "$OUT"

  count=$(printf '%s\n' "$classes" | wc -l | tr -d ' ')
  total_invocations=$((total_invocations + count))
done

{
  printf '## Summary\n\n'
  printf 'Suites:                 %d\n' "${#SUITES[@]}"
  printf 'AddTestCase invocations: %d\n' "$total_invocations"
  printf '(loop-unrolled registrations counted once)\n'
} >> "$OUT"

printf 'wrote %s — %d AddTestCase invocations across %d suites\n' \
       "$OUT" "$total_invocations" "${#SUITES[@]}"
