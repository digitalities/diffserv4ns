# smoke-bug9-phaseC2.tcl — BUG-9 regression smoke test (numPages=10).
#
# numPages=10, 20 sessions, 300 s — forces ~13 session completions.
# Post-fix this must return rc=0.

set argv [list 1]
set argc 1
set overrideTestTime 300
set overrideNumPages 10
set overrideNumSessions 20

source scenario-2-ns235.tcl
