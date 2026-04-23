# Changelog — `src/ns-2.35/` (ns-2.35 port layer)

All notable changes to the 2026 ns-2.35 port layer live here. The port
layer is additive to a stock `ns-allinone-2.35/ns-2.35/` tree: the
files shipped here selectively replace the stock ns-2 files during the
patching step (`scripts/patch-ns2-diffserv-235.sh`). The 2001
DiffServ4NS algorithms themselves are preserved verbatim under
`src/ns-2.29/`.

## Bug fixes carried by the port layer

Nine latent bugs from the 2001 code are fixed in this layer:
**BUG-1, BUG-2, BUG-3, BUG-4, BUG-5, BUG-7, BUG-8, BUG-9, BUG-10**.
For a one-line description of each (including the component affected),
see the table in [`LINEAGE.md`](../../LINEAGE.md#the-2026-ns-235-port-layer).

## 2026-04-18 — Per-flow `PolicyClassifier` extension

Extends `PolicyClassifier` (in `diffserv/dsPolicy.{h,cc}`) and
`edgeQueue::command` (in `diffserv/dsEdge.cc`) with a per-flow variant
of `addPolicyEntry`. The change completes the per-flow lookup path that
has been declared but dormant in the 2001 code:
`policyTableEntry::sourceNode` and `destNode` have always existed
alongside `ANY_HOST = -1` in `dsPolicy.h`, but the original code path
parsed only `<codePt> <policyType> [params]` and `getPolicyTableEntry`
linear-searched by codePt alone. This change finally wires those
fields up.

New public API on `PolicyClassifier`:

- `void addFlowPolicyEntry(int argc, const char*const* argv)` — Tcl
  signature

      $edge addFlowPolicyEntry <srcNodeId> <dstNodeId>
                               <arrivalDSCP>
                               <greenCP> <yellowCP> <redCP>
                               <policyType>
                               <params...>

  Delegates the policyType/params parsing to the existing
  `addPolicyEntry` (via a shifted argv) and back-fills
  `(sourceNode, destNode, perFlow, greenCP, yellowCP, redCP)` on the
  newly-appended entry. The per-flow (green, yellow, red) triple
  overrides the shared DSCP-keyed `policerTable` so that two per-flow
  rules sharing an arrival DSCP can still carry independent 3-color
  outputs (e.g. Telnet 10/10/12 and HTTP 10/12/14 both with an arrival
  DSCP of 10 are impossible through the legacy policerTable alone).

New protected overload:

- `policyTableEntry* getPolicyTableEntry(nsaddr_t src, nsaddr_t dst,
  int codePt)` — two-pass lookup (exact `(src, dst, codePt)` first, then
  `(ANY_HOST, ANY_HOST, codePt)` fallback). The legacy single-arg
  `getPolicyTableEntry(int codePt)` is preserved for back-compat.

Behaviour change in `mark(Packet *pkt)`:

- Replaces `getPolicyTableEntry(iph->prio())` with
  `getPolicyTableEntry(iph->saddr(), iph->daddr(), iph->prio())`. The
  lookup falls back to the DSCP-only entry (the existing
  `addPolicyEntry` path now initialises `sourceNode=destNode=ANY_HOST`,
  `perFlow=false`), so scenarios that use `addPolicyEntry` exclusively
  see no behaviour change.
- When the resolved entry has `perFlow=true`, a `policerTableEntry` is
  synthesized on the stack from the entry's `(greenCP, yellowCP, redCP)`
  triple instead of searching the shared policer table.
- If no entry matches (not even a wildcard fallback), `mark()` returns
  the incoming DSCP unchanged rather than dereferencing a `NULL`
  policy pointer (which segfaulted the 2001 code).

Policy and policer table bug fixes surfaced while exercising the new
paths (all in `dsPolicy.cc`):

- `addPolicerEntry`: `if (argc == 5)` / `if (argc == 6)` were
  exclusive, leaving `downgrade1` uninitialised for 3-color policers
  (argc=6). Changed to `>=` and explicit zero-init. Latent since 2001
  — every shipping scenario used `Dumb`, which takes 2 args.
- `getPolicyTableEntry(int codePt)`: loop bound `i <= policyTableSize`
  read one past the last entry on every miss; corrected to `<`.
- Removed the `printPolicyTable()` diagnostic from the
  `getPolicyTableEntry` miss path (hundreds of per-flow entries made
  the legacy dump spam the run log).

Edge dispatch:

- `edgeQueue::command` gains an `addFlowPolicyEntry` branch next to the
  existing `addPolicyEntry` dispatch.

Policy table capacity:

- `MAX_POLICIES` raised from 20 → 1024. Per-flow registration can
  produce hundreds of entries per simulation (the reference srTCM
  scenario registers 500), whereas the 2001 value of 20 was sized for
  one entry per DSCP codepoint. `policerTableEntry` sizing
  (`MAX_CP = 64`) is unchanged — the policer table still keys on DSCP.

Consumers:

- `examples/example-2-fullscale/scenario-2-ns235-srtcm.tcl` — per-flow
  srTCM configuration. Registers one per-flow policy entry per
  Telnet / FTP / HTTP connection (50 Telnet + 50 FTP + 400 HTTP) with
  the following CIR / CBS parameters:

  | Traffic | Green/Yellow/Red DSCP | CIR (bps) | CBS (bytes) |
  |---------|-----------------------|-----------|-------------|
  | Telnet  | 10 / 10 / 12          |    50 000 |       6 250 |
  | FTP     | 12 / 12 / 14          |   160 000 |      20 000 |
  | HTTP    | 10 / 12 / 14          |     6 375 |         797 |

No existing scenario is affected. Scenarios that never call
`addFlowPolicyEntry` see the same behaviour as before (single-arg
lookup still resolves, now through the `ANY_HOST` fallback path).
