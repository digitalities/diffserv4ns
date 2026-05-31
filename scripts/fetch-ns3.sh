#!/bin/bash
# Clone ns-3-dev as read-only reference for the port.
# Required because the port inherits from ns-3 base classes and needs
# to be compiled against ns-3 mainline.

set -euo pipefail

# REPO_ROOT resolution: env var override wins (useful for running an
# unstaged version of the script from a worktree against the main
# checkout). Otherwise resolves to the parent of this script's directory.
if [ -z "${REPO_ROOT:-}" ]; then
    REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
fi
cd "$REPO_ROOT"

# Arg parsing: --source-only or --worktree <topic>.
#
# --source-only: defines functions but skips side-effects. Used by
# scripts/tests/test-*.sh to unit-test helper logic without cloning
# ns-3-dev or applying patches.
#
# --worktree <topic>: creates a paired ns-3-dev worktree at
# ns3/ns-3-dev-<topic>, applies the local patch set against it, and
# symlinks contrib/diffserv to .worktrees/<topic>/src/ns-3. This gives
# each parallel Claude Code session its own cmake-cache, build/, and
# contrib symlink, removing the contention modes seen with the shared
# ns3/ns-3-dev/ clone. See the "Parallel Claude Code sessions" section
# of CLAUDE.md for the discipline and rationale.
SOURCE_ONLY=0
WORKTREE_TOPIC=""
while [ $# -gt 0 ]; do
    case "$1" in
        --source-only)
            SOURCE_ONLY=1
            shift
            ;;
        --worktree)
            WORKTREE_TOPIC="${2:-}"
            if [ -z "$WORKTREE_TOPIC" ]; then
                echo "ERROR: --worktree requires a topic argument" >&2
                echo "Usage: $0 [--source-only | --worktree <topic>]" >&2
                exit 1
            fi
            shift 2
            ;;
        --worktree=*)
            WORKTREE_TOPIC="${1#--worktree=}"
            if [ -z "$WORKTREE_TOPIC" ]; then
                echo "ERROR: --worktree= requires a topic value" >&2
                exit 1
            fi
            shift
            ;;
        *)
            echo "ERROR: unknown argument: $1" >&2
            echo "Usage: $0 [--source-only | --worktree <topic>]" >&2
            exit 1
            ;;
    esac
done

if [ -n "$WORKTREE_TOPIC" ]; then
    TARGET="ns3/ns-3-dev-${WORKTREE_TOPIC}"
else
    TARGET="ns3/ns-3-dev"
fi

# Pinned ns-3-dev revision: the diffserv module is built and tested
# against this specific commit. See CLAUDE.md ("Pinned version").
NS3_PIN="cc48bf5c15a4918364abc2b2b060b4056dce09a4"

if [ "$SOURCE_ONLY" -eq 0 ]; then
    if [ -n "$WORKTREE_TOPIC" ]; then
        # Peer worktree mode: require primary clone and paired project worktree.
        PRIMARY="ns3/ns-3-dev"
        PROJECT_WORKTREE=".worktrees/${WORKTREE_TOPIC}"
        if [ ! -e "$PRIMARY/.git" ]; then
            echo "ERROR: Primary $PRIMARY does not exist." >&2
            echo "Run scripts/fetch-ns3.sh (without --worktree) first to clone it." >&2
            exit 1
        fi
        if [ ! -d "$PROJECT_WORKTREE" ]; then
            echo "ERROR: Project worktree $PROJECT_WORKTREE does not exist." >&2
            echo "Create it first:" >&2
            echo "  git worktree add $PROJECT_WORKTREE -b $WORKTREE_TOPIC origin/main" >&2
            echo "then EnterWorktree(path=\"$PROJECT_WORKTREE\") to switch the" >&2
            echo "session into it. The harness's bare EnterWorktree(name=...)" >&2
            echo "lands in .claude/worktrees/ and does NOT satisfy this script;" >&2
            echo "see CLAUDE.md \"Parallel Claude Code sessions\"." >&2
            exit 1
        fi
        if [ -d "$TARGET" ]; then
            echo "Peer ns-3-dev worktree already present at $TARGET; verifying patches + symlink."
        else
            echo "Creating ns-3-dev peer worktree at $TARGET (detached at $NS3_PIN)..."
            git -C "$PRIMARY" worktree add --detach "$REPO_ROOT/$TARGET" "$NS3_PIN"
        fi
    else
        # Primary mode: clone if absent.
        if [ -d "$TARGET" ]; then
            echo "ns-3-dev already present at $TARGET"
        else
            echo "Cloning ns-3-dev from GitLab..."
            mkdir -p ns3
            git clone https://gitlab.com/nsnam/ns-3-dev.git "$TARGET"
            (cd "$TARGET" && git checkout "$NS3_PIN")
        fi
    fi
fi

# Apply local patches carried in patches/ns3/. These address upstream
# ns-3 defects that block our reconstruction. Each patch has a
# corresponding ADR documenting the decision and an upstream artifact
# under docs/upstream/ prepared for contribution back.
#
# apply_patch_robust handles the failure modes seen in practice:
#   1. Plain `git apply` succeeds — pinned upstream context matches.
#   2. Context drift (upstream rebase moved nearby lines) — fall back
#      to `git apply --3way`, which uses blob ancestry to merge.
#   3. Pre-existing untracked files from a prior partial apply (the
#      patch creates new files that already sit on disk untracked).
#      Detect "already exists in working directory" in the apply
#      output, remove the offending untracked files, and retry.
# Post-apply, run `git apply --check --reverse` to verify the patch
# is now logically present (would-be reverse-apply succeeds). This
# catches partial applies that previously slipped through silently.
apply_patch_robust() {
    local patch="$1"
    local name="$(basename "$patch")"

    # Idempotency probe — reverse-apply check succeeds when the patch
    # is fully applied. (May false-negative for new-file patches, in
    # which case we fall through to the apply attempt below.)
    if (cd "$TARGET" && git apply --check --reverse "$patch") 2>/dev/null; then
        echo "Patch $name already applied, skipping."
        return 0
    fi

    echo "Applying patch $name ..."

    # Capture the set of pre-existing tracked files this patch touches.
    # When a tier fails midway and leaves dirty state behind (notably
    # `--3way` writing conflict markers), we restore these files to HEAD
    # so the next tier or next patch starts from a clean baseline.
    local touched_tracked
    touched_tracked=$(grep -E '^diff --git a/' "$patch" | awk '{print $3}' | sed 's|^a/||' \
        | while read f; do
            (cd "$TARGET" && git ls-files --error-unmatch "$f" >/dev/null 2>&1) && echo "$f"
          done)

    _restore_touched() {
        for f in $touched_tracked; do
            (cd "$TARGET" && git checkout HEAD -- "$f") 2>/dev/null
        done
    }

    # Tier 1: plain apply. Capture stdout+stderr; only display on final
    # failure to avoid misleading per-file "Applied patch to X cleanly"
    # output when the patch ultimately rolls back via --3way.
    local err rc
    err=$(cd "$TARGET" && git apply "$patch" 2>&1)
    rc=$?
    if [ $rc -eq 0 ]; then
        echo "  applied (plain)"
        _verify_patch_applied "$patch" "$name"
        return $?
    fi

    # Tier 2: 3-way merge — handles context drift when blob ancestry
    # is recoverable (true for patches generated against a recent
    # upstream commit even after subsequent rebases). On failure,
    # 3-way may leave conflict markers in working files; we scan for
    # those and restore so the next tier starts clean. Capture output
    # and suppress on success — only display if every tier ultimately
    # fails, so operators get one coherent error report.
    local tier2_out tier2_rc
    tier2_out=$(cd "$TARGET" && git apply --3way "$patch" 2>&1)
    tier2_rc=$?
    if [ $tier2_rc -eq 0 ]; then
        echo "  applied via 3-way merge"
        _verify_patch_applied "$patch" "$name"
        return $?
    fi
    if _files_have_conflict_markers $touched_tracked; then
        echo "  3-way merge left conflict markers, restoring touched files"
        _restore_touched
    fi

    # Tier 3: detect "already exists in working directory" from prior
    # partial apply, clean up the untracked leftovers, retry --3way.
    if echo "$err" | grep -q "already exists in working directory"; then
        echo "  pre-existing untracked files from prior partial apply detected"
        local stale_files
        stale_files=$(echo "$err" \
            | grep "already exists in working directory" \
            | awk '{print $2}' | sed 's/://')
        for f in $stale_files; do
            if [ -f "$TARGET/$f" ] \
               && ! (cd "$TARGET" && git ls-files --error-unmatch "$f" >/dev/null 2>&1); then
                echo "  rm $f (untracked, will be recreated by patch)"
                rm "$TARGET/$f"
            fi
        done
        if (cd "$TARGET" && git apply --3way "$patch") 2>/dev/null; then
            echo "  applied via 3-way merge after untracked-cleanup"
            _verify_patch_applied "$patch" "$name"
            return $?
        fi
        if _files_have_conflict_markers $touched_tracked; then
            echo "  3-way merge left conflict markers after cleanup, restoring touched files"
            _restore_touched
        fi
    fi

    # All tiers exhausted. Surface BOTH Tier 1 (plain) and Tier 2 (--3way)
    # outputs so operators have full diagnostic context — Tier 2's failure
    # detail is often the most informative (conflict context, ancestor
    # mismatch reason) and was captured-but-suppressed during the success
    # path.
    echo "ERROR: Patch $name could not be applied." >&2
    echo "Tier 1 (plain) output:" >&2
    echo "$err" | sed 's/^/  /' >&2
    if [ -n "${tier2_out:-}" ]; then
        echo "Tier 2 (--3way) output:" >&2
        echo "$tier2_out" | sed 's/^/  /' >&2
    fi
    return 1
}

_files_have_conflict_markers() {
    for f in "$@"; do
        if [ -f "$TARGET/$f" ] && grep -qE '^(<<<<<<<|>>>>>>>)' "$TARGET/$f"; then
            return 0
        fi
    done
    return 1
}

# Verify a patch is logically present by checking that a reverse-apply
# would now succeed. Loud failure here means the apply only partially
# took effect (e.g. some hunks landed but new files were skipped).
_verify_patch_applied() {
    local patch="$1"
    local name="$2"
    if (cd "$TARGET" && git apply --check --reverse "$patch") 2>/dev/null; then
        echo "  verified applied"
        return 0
    fi
    echo "ERROR: Patch $name applied but post-verify failed (partial apply?)" >&2
    return 1
}

# Preflight: validate the patch series cumulatively before the real apply
# loop. Patches may anchor on context introduced by earlier patches in
# the series (e.g. a later patch adds a sibling field below one declared
# by an earlier patch), so each patch must be checked against the state
# left behind by its predecessors, not against the unpatched baseline.
#
# Strategy: actually apply each patch sequentially. On any failure, stop
# and report. At the end, roll the working tree back to its pre-preflight
# state — both tracked file modifications (`git reset --hard`) and any
# untracked new files added by patches (diff pre/post untracked listings
# and remove the new ones). The apply phase that follows starts from a
# clean slate.
#
# Returns 0 if every patch would apply cleanly in order, nonzero otherwise.
_preflight_patches() {
    local dir="$1"
    local fail=0
    local pass=0
    local saved_head
    saved_head=$(cd "$TARGET" && git rev-parse HEAD)
    local pre_untracked
    pre_untracked=$(cd "$TARGET" && git ls-files --others --exclude-standard | sort)

    for patch in "$dir"/*.patch; do
        [ -e "$patch" ] || continue
        local name="$(basename "$patch")"
        # Idempotency: if a patch is already applied in the current state,
        # report and move on without re-applying.
        if (cd "$TARGET" && git apply --check --reverse "$patch") 2>/dev/null; then
            pass=$((pass + 1))
            echo "PREFLIGHT OK:   $name (already applied)"
            continue
        fi
        if (cd "$TARGET" && git apply "$patch") 2>/dev/null; then
            pass=$((pass + 1))
            echo "PREFLIGHT OK:   $name"
        elif (cd "$TARGET" && git apply --3way "$patch") 2>/dev/null; then
            pass=$((pass + 1))
            echo "PREFLIGHT OK:   $name (via 3-way)"
        else
            fail=$((fail + 1))
            echo "PREFLIGHT FAIL: $name (neither plain nor --3way passes apply)"
            # Bail early — subsequent patches likely depend on this one
            # and would produce cascading misleading failure reports.
            break
        fi
    done

    # Roll back tracked modifications and any new untracked files added
    # during preflight, leaving the working tree in its pre-preflight
    # state for the real apply loop.
    (cd "$TARGET" && git reset --hard "$saved_head" >/dev/null 2>&1)
    local post_untracked
    post_untracked=$(cd "$TARGET" && git ls-files --others --exclude-standard | sort)
    comm -13 <(echo "$pre_untracked") <(echo "$post_untracked") | while read -r f; do
        [ -n "$f" ] && rm -f "$TARGET/$f"
    done

    echo "PREFLIGHT SUMMARY: $pass OK, $fail FAIL"
    [ "$fail" -eq 0 ]
}

PATCH_DIR="$REPO_ROOT/patches/ns3"
if [ "$SOURCE_ONLY" -eq 0 ] && [ -d "$PATCH_DIR" ]; then
    # Preflight: structured per-patch --check report BEFORE any disk-touching
    # apply. Catches cross-patch drift before the apply loop emits misleading
    # per-file probing output.
    if ! _preflight_patches "$PATCH_DIR"; then
        echo "" >&2
        echo "FATAL: preflight failed for one or more patches." >&2
        echo "Investigate cross-patch drift before retrying. Common causes:" >&2
        echo "  - Upstream rebase moved context lines beyond --3way recovery" >&2
        echo "  - Earlier patch in the loop shifted lines that a later patch targets" >&2
        echo "  - Untracked leftovers from a prior partial apply" >&2
        exit 1
    fi
    PATCH_FAILURES=0
    for patch in "$PATCH_DIR"/*.patch; do
        [ -e "$patch" ] || continue
        if ! apply_patch_robust "$patch"; then
            PATCH_FAILURES=$((PATCH_FAILURES + 1))
        fi
    done
    if [ "$PATCH_FAILURES" -gt 0 ]; then
        echo "" >&2
        echo "FATAL: $PATCH_FAILURES patch(es) failed to apply despite preflight pass." >&2
        echo "This indicates an apply-tier bug; capture full output and investigate." >&2
        exit 1
    fi
fi

if [ "$SOURCE_ONLY" -eq 0 ]; then
    # Create symlink so ns-3 can find the diffserv contrib module.
    # This is the integration mechanism between our repo and ns-3's
    # build system: ns-3's build system discovers contrib modules under
    # $NS3/contrib/, and a symlink lets us keep the source tree under
    # the project's src/ns-3/ (per-session in --worktree mode) without
    # copying or moving files.
    SYMLINK="$REPO_ROOT/$TARGET/contrib/diffserv"
    LEGACY_SYMLINK="$REPO_ROOT/$TARGET/contrib/diffserv4ns3"
    # Retire the legacy symlink if a previous run created it; idempotent.
    if [ -L "$LEGACY_SYMLINK" ]; then
        echo "Removing legacy symlink: contrib/diffserv4ns3"
        rm "$LEGACY_SYMLINK"
    fi
    if [ -n "$WORKTREE_TOPIC" ]; then
        SYM_TARGET="$REPO_ROOT/.worktrees/${WORKTREE_TOPIC}/src/ns-3"
    else
        SYM_TARGET="$REPO_ROOT/src/ns-3"
    fi
    # Re-point if an existing symlink targets a different path. Idempotent
    # across topic-name corrections and primary <-> peer mode switches on
    # the same ns-3-dev directory (rare but possible during migration).
    if [ -L "$SYMLINK" ]; then
        CURRENT_TARGET="$(readlink "$SYMLINK")"
        if [ "$CURRENT_TARGET" != "$SYM_TARGET" ]; then
            echo "Re-pointing symlink: $CURRENT_TARGET -> $SYM_TARGET"
            rm "$SYMLINK"
        fi
    fi
    if [ ! -e "$SYMLINK" ]; then
        ln -s "$SYM_TARGET" "$SYMLINK"
        echo "Created symlink: contrib/diffserv -> $SYM_TARGET"
    fi

    if [ -n "$WORKTREE_TOPIC" ]; then
        echo "Done. Peer ns-3-dev worktree at $TARGET/ (detached at $NS3_PIN)."
        echo "Paired with project worktree at .worktrees/${WORKTREE_TOPIC}/."
        echo "Build from $TARGET/ (not from the primary ns3/ns-3-dev/)."
    else
        echo "Done. ns-3-dev source is at $TARGET/ (pinned at $NS3_PIN)."
    fi
    echo "The checkout carries local patches from patches/ns3/."
    echo "The diffserv module is symlinked into contrib/ for building."
fi
