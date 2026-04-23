# Copyright (C) 2001-2026  Sergio Andreozzi
#
# smoke-test.tcl — Scenario 2 sanity check (WRED parameter set 1, 100 s)
#
# Runs scenario-2.tcl with a 100 s simulated time so a smoke run completes
# in a few seconds. The scenario-2.tcl script honours `overrideTestTime`
# when it is set in the calling context, bypassing both the argv[1] path
# and the 5000 s default.
#
# Usage:
#   ns smoke-test.tcl
# ---------------------------------------------------------------------------

set argv [list 1]            ;# WRED parameter set 1
set argc 1
set overrideTestTime 100

source scenario-2.tcl
