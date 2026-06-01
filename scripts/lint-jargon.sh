#!/usr/bin/env bash
# scripts/lint-jargon.sh
#
# Quality-check linter for internal-jargon tokens in any source
# file, Doxygen block, or script that ships in the public release
# repository. The release is read by external audiences who do
# not have project-internal context (artefact-evaluation reviewers,
# ns-3 community members, future reusers, students), so internal
# tokens read as opaque jargon and dilute the technical content.
#
# The rule is documented in `docs/ns3-doxygen-style.md` section 10.1
# and section 11 of the same guide.
#
# Exit code:
#   0 — no jargon detected
#   1 — one or more tokens found; offending file:line listed
#
# Scope (SCAN_PATHS_CODE — .cc/.h/.py/.sh only):
#   src/ns-3/, src/ns-2.35/, scripts/, handbook/
# Scope (SCAN_PATHS_MD — .md only):
#   specs/, handbook/
# Scope (SCAN_PATHS_CONFIG — .yml/.yaml/.md):
#   .github/  (issue + PR templates, label definitions)
#
# Note: the `postponed-dir-ref` pattern flags Markdown cross-references to
# `handbook/` and `guide/`, which are deferred from the v1.0 release. Remove
# `guide/` (and, when the handbook ships, `handbook/`) from that pattern once
# those trees are published.
# Out of scope:
#   src/ns-2.29/                  (frozen 2001 original; read-only)
#   docs/adr/                     (decision records, private to the project)
#   docs/superpowers/, paper/     (dev-only; do not ship in release)
#   src/ns-3/CHANGELOG.md         (project-history artefact whose value
#                                  depends on referencing phasing)
#   scripts/lint-ns3-idioms.sh    (author-tooling: encodes rules sourced
#                                  from author-private memory; the
#                                  references to that source layer are
#                                  intentional self-documentation)
#
# Allowlisted token patterns (matched-and-skipped before reporting):
#   I-N, S-N.M, Q-N.M             — public spec identifiers indexed in
#                                   specs/01-intent.md, specs/02-structural.md,
#                                   and specs/03-quality.md
#   F-A..F-D                      — public empirical-findings catalogue
#   N2-N, D2-N, N3-N              — public bug-area-prefix identifiers
#                                   indexed in docs/HISTORICAL_BUGS.md
#                                   (note: D3-N is NOT allowlisted — that
#                                   bucket is the private DS4-for-ns-3
#                                   evolutionary log)
#   DS4-PN                        — public NS2_PATCHES patch tokens

set -euo pipefail

readonly REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

# handbook/ is deferred from the v1.0 release (not mirrored to the public
# repo), so it is no longer release-bound and is out of lint scope. Re-add it
# to both arrays — and drop guide/ from the postponed-dir-ref pattern — when
# the handbook and guide trees are published.
readonly SCAN_PATHS_CODE=(
  "src/ns-3"
  "src/ns-2.35"
  "scripts"
  "patches/ns3"
)

readonly SCAN_PATHS_MD=(
  "specs"
)

# provenance/ ships wholesale in the public release (frozen reference excerpts
# plus the LINEAGE docs), so it is release-bound and must be jargon-clean. It is
# scanned like SCAN_PATHS_MD but with one carve-out applied in the loop below:
# the decision-record-number pattern is skipped, because bare ADR-NNNN is the
# sanctioned citation form in shipped docs and the LINEAGE files cite ADRs by id.
readonly SCAN_PATHS_PROVENANCE=(
  "provenance"
)

# Individual top-level markdown files that ship in the release and form the
# reader's front page. Scanned the same way as SCAN_PATHS_MD but as named
# files (they live at the repo root, not under a scanned directory).
# NOTE: LINEAGE.md / CONTRIBUTING.md are intentionally excluded — they carry
# public catalogue identifiers (e.g. the historical BUG-N list) and a
# reference to the dev style guide that are a separate policy question.
readonly SCAN_FILES_MD=(
  "README.md"
  "README-ns-3.md"
)

readonly FILE_GLOBS_CODE=(
  "--include=*.cc"
  "--include=*.h"
  "--include=*.py"
  "--include=*.sh"
  "--include=*.patch"
  "--exclude=lint-jargon.sh"
  "--exclude=lint-ns3-idioms.sh"
  "--exclude=mirror-ns3-to-release.sh"
  "--exclude=bug11-shim-regression.sh"
  "--exclude=q6-go-no-go.sh"
  "--exclude-dir=.venv"
  "--exclude-dir=deferred"
  "--exclude-dir=__pycache__"
)

readonly FILE_GLOBS_MD=(
  "--include=*.md"
  "--exclude-dir=.venv"
)

# Repository config that ships in the release: GitHub issue/PR templates and
# label definitions. These are read by external contributors but live outside
# the source/doc trees above, so they are scanned as a separate group covering
# both YAML templates and Markdown.
readonly SCAN_PATHS_CONFIG=(
  ".github"
)

readonly FILE_GLOBS_CONFIG=(
  "--include=*.yml"
  "--include=*.yaml"
  "--include=*.md"
)

# Patterns to detect. Each entry is "label|regex".
# Regex uses POSIX extended; word-boundary tokens use \b.
#
# Note on "shim": this word is intentionally absent from the design-journey
# bucket below. It is an established CS term (in the same category as
# proxy/adapter/wrapper pattern names) and appears as a component-type
# token in class names like DsCakeInputJitterShim. The design-journey
# phrases describe a development trajectory; "shim" describes a thing,
# so it does not belong in that bucket.
readonly PATTERNS=(
  "decision-record-number|ADR-[0-9]{4}"
  "private-repo-url|digitalities/diffserv4ns-dev"
  "phase-label|Phase [0-9]+|phase[0-9]+\b"
  "pr-label|\\bPR[0-9]+[a-z]?\\b"
  "bug-catalogue|\\bBUG-[0-9]+\\b"
  "deprecated-bug-id|\\b[DN][23]-[0-9]+\\b"
  "spec-id-quality|\\bQ-[0-9]+\\.[0-9]+\\b"
  "spec-id-structural|\\bS-[0-9]+\\.[0-9]+\\b"
  "finding-id|\\bF-[A-D]\\b|Finding F-[A-Z]"
  "postponed-dir-ref|(handbook|guide)/[a-z0-9-]+\\.md|handbook chapter [0-9]"
  "internal-plan-path|docs/(superpowers|methodology|prompts|audit|cpp-review-reports)/"
  "internal-adr-path|docs/adr/[0-9]"
  "internal-style-doc-ref|docs/ns3-doxygen-style\\.md"
  "serena-memory-path|reference_ns3_[a-z0-9_]+\\.md|feedback_[a-z0-9_]+\\.md|project_[a-z0-9_]+\\.md"
  "author-private-system|\\bauto-memory\\b|Serena memor"
  "design-journey-phrase|post-refactor|pre-PR[0-9]|composition over inheritance|asymmetric by design|strategy pattern\\b|supersedes|lazy-create"
)

# Tokens that look like jargon under the regex above but actually
# index public artefacts (specs, findings catalogue, bug-area
# prefixes, patch IDs). Matched against the candidate's content and
# the candidate is skipped if at least one allowlist pattern fires.
# Order is irrelevant — the loop short-circuits on the first match.
readonly ALLOWLIST_PATTERNS=(
  '\b[ISQ]-[0-9]+(\.[0-9]+)*\b'   # spec identifiers (I-1, S-2.1, Q-15.6)
  '\bF-[A-D][0-9]?\b'             # empirical-findings catalogue (F-A, F-C)
  '\bA3-DsCake\b'                 # specific multi-character finding ID
  '\bDS4-P[0-9]+\b'               # NS2_PATCHES patch tokens
  '\bN2-[0-9]+\b'                 # ns-2 core defects (HISTORICAL_BUGS public)
  '\bD2-[0-9]+\b'                 # DS4-for-ns-2 defects (HISTORICAL_BUGS public)
  '\bN3-[0-9]+\b'                 # ns-3 core defects (HISTORICAL_BUGS public)
  'Experiment Report, Phase'      # Ferrari 2000 publication title, not a phase label
)

# Returns 0 (true) if any allowlist pattern matches @p line.
is_allowlisted() {
  local line="$1"
  for pat in "${ALLOWLIST_PATTERNS[@]}"; do
    if printf '%s' "$line" | grep -qE "$pat"; then
      return 0
    fi
  done
  return 1
}

violations=0
> /tmp/lint-jargon.out

for path in "${SCAN_PATHS_CODE[@]}"; do
  if [ ! -d "$path" ]; then
    continue
  fi
  for entry in "${PATTERNS[@]}"; do
    label="${entry%%|*}"
    regex="${entry#*|}"
    while IFS=: read -r file line content; do
      [ -z "$file" ] && continue
      if is_allowlisted "$content"; then
        continue
      fi
      printf '%s:%s [%s] %s\n' "$file" "$line" "$label" "$content" >> /tmp/lint-jargon.out
      violations=$((violations + 1))
    done < <(grep -rnE "${FILE_GLOBS_CODE[@]}" "$regex" "$path" 2>/dev/null || true)
  done
done

for path in "${SCAN_PATHS_MD[@]}"; do
  if [ ! -d "$path" ]; then
    continue
  fi
  for entry in "${PATTERNS[@]}"; do
    label="${entry%%|*}"
    regex="${entry#*|}"
    while IFS=: read -r file line content; do
      [ -z "$file" ] && continue
      if is_allowlisted "$content"; then
        continue
      fi
      printf '%s:%s [%s] %s\n' "$file" "$line" "$label" "$content" >> /tmp/lint-jargon.out
      violations=$((violations + 1))
    done < <(grep -rnE "${FILE_GLOBS_MD[@]}" "$regex" "$path" 2>/dev/null || true)
  done
done

for path in "${SCAN_PATHS_CONFIG[@]}"; do
  if [ ! -d "$path" ]; then
    continue
  fi
  for entry in "${PATTERNS[@]}"; do
    label="${entry%%|*}"
    regex="${entry#*|}"
    while IFS=: read -r file line content; do
      [ -z "$file" ] && continue
      if is_allowlisted "$content"; then
        continue
      fi
      printf '%s:%s [%s] %s\n' "$file" "$line" "$label" "$content" >> /tmp/lint-jargon.out
      violations=$((violations + 1))
    done < <(grep -rnE "${FILE_GLOBS_CONFIG[@]}" "$regex" "$path" 2>/dev/null || true)
  done
done

for md_file in "${SCAN_FILES_MD[@]}"; do
  if [ ! -f "$md_file" ]; then
    continue
  fi
  for entry in "${PATTERNS[@]}"; do
    label="${entry%%|*}"
    regex="${entry#*|}"
    while IFS=: read -r file line content; do
      [ -z "$file" ] && continue
      if is_allowlisted "$content"; then
        continue
      fi
      printf '%s:%s [%s] %s\n' "$file" "$line" "$label" "$content" >> /tmp/lint-jargon.out
      violations=$((violations + 1))
    done < <(grep -HnE "$regex" "$md_file" 2>/dev/null || true)
  done
done

for path in "${SCAN_PATHS_PROVENANCE[@]}"; do
  if [ ! -d "$path" ]; then
    continue
  fi
  for entry in "${PATTERNS[@]}"; do
    label="${entry%%|*}"
    # Bare ADR-NNNN is the sanctioned citation form in shipped docs/provenance,
    # so the decision-record-number pattern is skipped for this tree only.
    if [ "$label" = "decision-record-number" ]; then
      continue
    fi
    regex="${entry#*|}"
    while IFS=: read -r file line content; do
      [ -z "$file" ] && continue
      if is_allowlisted "$content"; then
        continue
      fi
      printf '%s:%s [%s] %s\n' "$file" "$line" "$label" "$content" >> /tmp/lint-jargon.out
      violations=$((violations + 1))
    done < <(grep -rnE "${FILE_GLOBS_MD[@]}" "$regex" "$path" 2>/dev/null || true)
  done
done

if [ "$violations" -eq 0 ]; then
  printf 'lint-jargon: clean — no internal-jargon tokens found in release-bound sources.\n'
  exit 0
fi

printf 'lint-jargon: %d violation(s) across release-bound sources:\n\n' "$violations"
sort -u /tmp/lint-jargon.out
printf '\nSee docs/ns3-doxygen-style.md section 10.1 for the rule and acceptable substitutes.\n'
exit 1
