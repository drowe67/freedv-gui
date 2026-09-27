#!/bin/bash
# Stand-in for FREEDV_BINARY in test_rade_loss.sh: runs the real FreeDV binary with the
# Time Profiler attached, writing one trace per invocation named after the -ut mode.
#
# Required environment:
#   REAL_FREEDV_BINARY  FreeDV executable to run
#   PROFILE_OUT_DIR     directory for the traces
#
# (xctrace --launch resolves a .app by bundle ID and can pick up an installed
# FreeDV.app instead of the one given, so we attach by PID.)
MODE=unknown
prev=""
for a in "$@"; do
    [ "$prev" == "-ut" ] && MODE="$a"
    prev="$a"
done
TRACE="$PROFILE_OUT_DIR/freedv_${MODE}.trace"
rm -rf "$TRACE"

"$REAL_FREEDV_BINARY" "$@" &
FDV_PID=$!
xctrace record --template "Time Profiler" --output "$TRACE" --attach $FDV_PID > "$PROFILE_OUT_DIR/xctrace_${MODE}.log" 2>&1 &
XCTRACE_PID=$!
wait $FDV_PID
EXIT_CODE=$?
wait $XCTRACE_PID
exit $EXIT_CODE
