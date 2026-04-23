# Copyright (C) 2001-2026  Sergio Andreozzi
#
# smoke-bug9-phaseA.tcl — BUG-9 regression smoke test.
#
# numPages=3, 20 sessions, 200 s — forces first-session completion at
# t ≈ 45 s so the (former) WebTraf session-completion crash would fire
# well inside the sim budget.
#
# Before the BUG-9 fix this scenario SEGVed at t ≈ 11 s (first session
# death → shared RV deleted → next session UAF on rvInterPage_->value()).
# After the fix (src/ns-2.35/webcache/webtraf.cc destructor) this must
# exit 0 and all 20 sessions complete cleanly.
#
# Usage (from inside ns-2.35 dir):
#   ns /scenario-path/smoke-bug9-phaseA.tcl; echo "rc=$?"
# ---------------------------------------------------------------------------

set argv [list 1]            ;# WRED parameter set 1
set argc 1
set overrideTestTime 200
set overrideNumPages 3
set overrideNumSessions 20

source scenario-2-ns235.tcl
