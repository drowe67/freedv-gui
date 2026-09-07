#!/bin/bash
#
# Begins populating the RADE integration verification report for FreeDV:
#   https://github.com/drowe67/radae/blob/dr-tx-bpf/doc/verification/template.md
#
# The following sections are filled in automatically:
#   * Application Under Test  (software / git / platform information)
#   * Signal Path Declaration (asserted from the loss-test configuration)
#   * Baseline Loss (Step 1)  (rade_tx_wav + rade_rx_wav on all.wav, per the
#                              verification procedure)
#   * Level 1 - Software Loopback (produced by test/test_rade_loss.sh, which is
#                              also how CI exercises RADE loss - see
#                              .github/workflows/cmake-linux.yml and
#                              .github/workflows/cmake-macos.yml)
#
# The hardware sections (Level 2 - OTAC, Level 3 - OTC) are left as blank
# template fields for the tester to complete by hand.
#
# Run this from the FreeDV build directory (build_linux / build_osx), the same
# way CI runs test/test_rade_loss.sh:
#
#   cd build_osx
#   FREEDV_COMPUTER_TO_RADIO_DEVICE="VB-Cable" \
#   FREEDV_RADIO_TO_COMPUTER_DEVICE="VB-Cable" \
#   FREEDV_COMPUTER_TO_SPEAKER_DEVICE="BlackHole1 2ch" \
#   FREEDV_MICROPHONE_TO_COMPUTER_DEVICE="BlackHole2 2ch" \
#   ../test/rade_verification_report.sh -o rade_verification_report.md
#
# Options:
#   -o FILE   write the report to FILE (default: ./rade_verification_report.md)
#   -C DIR    run from DIR instead of the current directory
#   -h        show this help
#
# Environment (same contract as test/test_rade_loss.sh, plus a few extras):
#   FREEDV_BINARY                          FreeDV executable to test
#   FREEDV_RADIO_TO_COMPUTER_DEVICE        virtual audio devices for the
#   FREEDV_COMPUTER_TO_RADIO_DEVICE        Level 1 software loopback
#   FREEDV_MICROPHONE_TO_COMPUTER_DEVICE
#   FREEDV_COMPUTER_TO_SPEAKER_DEVICE
#   PYTHON_BINARY                          python with torch + matplotlib (default python3)
#   RADE_C_TOOLS_DIR                       dir containing rade_tx_wav / rade_rx_wav
#   RADE_BASELINE_LOSS                     use this baseline instead of running the wav tools
#   RADE_TESTER                            tester name / callsign for the report
#   RADE_SKIP_LEVEL1=1                     fill software info + baseline only

set -u

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
REPO_ROOT="$( cd -- "$SCRIPTPATH/.." >/dev/null 2>&1 ; pwd -P )"

OUTPUT="./rade_verification_report.md"
WORKDIR="$(pwd)"

while getopts ":o:C:h" opt; do
    case "$opt" in
        o) OUTPUT="$OPTARG" ;;
        C) WORKDIR="$OPTARG" ;;
        h) sed -n '2,44p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        \?) echo "unknown option: -$OPTARG" >&2; exit 2 ;;
        :) echo "option -$OPTARG requires an argument" >&2; exit 2 ;;
    esac
done

cd "$WORKDIR" || { echo "cannot cd to $WORKDIR" >&2; exit 1; }
WORKDIR="$(pwd)"

# Resolve OUTPUT to an absolute path now that we've changed directory.
case "$OUTPUT" in
    /*) : ;;
    *)  OUTPUT="$WORKDIR/$OUTPUT" ;;
esac

PYTHON_BINARY="${PYTHON_BINARY:-python3}"
RADE_TESTER="${RADE_TESTER:-}"
RADE_BASELINE_LOSS="${RADE_BASELINE_LOSS:-}"
RADE_SKIP_LEVEL1="${RADE_SKIP_LEVEL1:-0}"
OPERATING_SYSTEM="$(uname -s)"

echo "RADE verification report -> $OUTPUT"
echo "Working directory: $WORKDIR"

# ---------------------------------------------------------------------------
# Locate rade_tx_wav / rade_rx_wav and the rade_c source (same discovery as
# test/test_rade_loss.sh).
# ---------------------------------------------------------------------------
if [ -z "${RADE_C_TOOLS_DIR:-}" ]; then
    RADE_C_TOOLS_DIR=""
    for candidate in \
        "$WORKDIR/_deps/freedv_backend-build/rade_build/src" \
        "$WORKDIR"/build*/_deps/freedv_backend-build/rade_build/src \
        "$REPO_ROOT"/build*/_deps/freedv_backend-build/rade_build/src; do
        if [ -x "$candidate/rade_tx_wav" ] && [ -x "$candidate/rade_rx_wav" ]; then
            RADE_C_TOOLS_DIR="$candidate"
            break
        fi
    done
fi
if [ -z "$RADE_C_TOOLS_DIR" ] || [ ! -x "$RADE_C_TOOLS_DIR/rade_tx_wav" ]; then
    found=$(find "$WORKDIR" "$REPO_ROOT" -name rade_tx_wav -type f 2>/dev/null | head -1)
    [ -n "$found" ] && RADE_C_TOOLS_DIR="$(dirname "$found")"
fi

RADE_C_SRC_DIR=""
if [ -n "$RADE_C_TOOLS_DIR" ]; then
    guess="$( cd -- "$RADE_C_TOOLS_DIR/../.." >/dev/null 2>&1 && pwd -P )/rade_src"
    [ -d "$guess" ] && RADE_C_SRC_DIR="$guess"
fi

# ---------------------------------------------------------------------------
# radae repo (all.wav + loss.py). Cloned the same way test/test_rade_loss.sh
# clones it.
# ---------------------------------------------------------------------------
if [ ! -d "$WORKDIR/rade_src" ]; then
    echo "Cloning radae repo into $WORKDIR/rade_src ..."
    git clone -b main https://github.com/drowe67/radae.git "$WORKDIR/rade_src" || {
        echo "failed to clone radae repo" >&2; exit 1; }
fi

# ---------------------------------------------------------------------------
# Software / platform information.
# ---------------------------------------------------------------------------
APP_NAME="FreeDV"

PROJECT_VERSION=$(sed -n 's/^set(PROJECT_VERSION \([0-9.]*\)).*/\1/p' "$REPO_ROOT/CMakeLists.txt" 2>/dev/null)
FREEDV_GIT=$(git -C "$REPO_ROOT" describe --tags --always --dirty 2>/dev/null || echo "unknown")
if [ -n "$PROJECT_VERSION" ]; then
    APP_VERSION="$PROJECT_VERSION-dev (git $FREEDV_GIT)"
else
    APP_VERSION="git $FREEDV_GIT"
fi

case "$OPERATING_SYSTEM" in
    Darwin)
        PLATFORM="macOS $(sw_vers -productVersion 2>/dev/null) ($(sw_vers -buildVersion 2>/dev/null)) $(uname -m)"
        ;;
    Linux)
        if [ -r /etc/os-release ]; then
            . /etc/os-release
            PLATFORM="${PRETTY_NAME:-Linux}, kernel $(uname -r), $(uname -m)"
        else
            PLATFORM="$(uname -sr) $(uname -m)"
        fi
        ;;
    *)
        PLATFORM="$(uname -sr) $(uname -m)"
        ;;
esac

REPORT_DATE="$(date -u '+%Y-%m-%d')"
RADAE_COMMIT="unknown"
[ -d "$WORKDIR/rade_src/.git" ] && RADAE_COMMIT=$(git -C "$WORKDIR/rade_src" rev-parse HEAD 2>/dev/null || echo "unknown")
RADE_C_COMMIT="unknown"
[ -n "$RADE_C_SRC_DIR" ] && [ -d "$RADE_C_SRC_DIR/.git" ] && \
    RADE_C_COMMIT=$(git -C "$RADE_C_SRC_DIR" rev-parse HEAD 2>/dev/null || echo "unknown")

echo "  Application : $APP_NAME $APP_VERSION"
echo "  Platform    : $PLATFORM"
echo "  radae commit: $RADAE_COMMIT"
echo "  rade_c commit: $RADE_C_COMMIT"

# ---------------------------------------------------------------------------
# Baseline Loss (Step 1): all.wav through the rade_c software-only TX/RX path.
# ---------------------------------------------------------------------------
BASELINE_CMD="rade_tx_wav --v2 -f baseline_txfeatures.f32 all.wav baseline_tx.wav
rade_rx_wav --v2 -f baseline_rxfeatures.f32 baseline_tx.wav baseline_decoded.wav
python3 loss.py baseline_txfeatures.f32 baseline_rxfeatures.f32 --clip_start 100 --clip_end 300"

compute_baseline_loss () (
    all_wav="$WORKDIR/rade_src/wav/all.wav"
    tx_wav="$RADE_C_TOOLS_DIR/rade_tx_wav"
    rx_wav="$RADE_C_TOOLS_DIR/rade_rx_wav"

    [ -f "$all_wav" ] || { echo "baseline: $all_wav not found" >&2; return 1; }
    if [ ! -x "$tx_wav" ] || [ ! -x "$rx_wav" ]; then
        echo "baseline: rade_tx_wav / rade_rx_wav not found (set RADE_C_TOOLS_DIR)" >&2
        return 1
    fi

    # rade_tx_wav requires 16 kHz mono 16-bit PCM (all.wav already is).
    sox "$all_wav" -r 16000 -c 1 -b 16 -e signed-integer "$WORKDIR/baseline_in.wav" || return 1

    export LD_LIBRARY_PATH="$RADE_C_TOOLS_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    export DYLD_LIBRARY_PATH="$RADE_C_TOOLS_DIR${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"

    # --v2: FreeDV opens RADE with RADE_MODE_V2 unconditionally on this branch
    # (src/freedv_interface.cpp), so the baseline must use V2 to be comparable.
    "$tx_wav" --v2 -f "$WORKDIR/baseline_txfeatures.f32" "$WORKDIR/baseline_in.wav" "$WORKDIR/baseline_tx.wav" >/dev/null 2>&1 || return 1
    "$rx_wav" --v2 -f "$WORKDIR/baseline_rxfeatures.f32" "$WORKDIR/baseline_tx.wav" "$WORKDIR/baseline_decoded.wav" >/dev/null 2>&1 || return 1

    "$PYTHON_BINARY" "$WORKDIR/rade_src/loss.py" \
        "$WORKDIR/baseline_txfeatures.f32" "$WORKDIR/baseline_rxfeatures.f32" \
        --clip_start 100 --clip_end 300 2>&1
)

BASELINE_LOSS=""
BASELINE_NOTE=""
if [ -n "$RADE_BASELINE_LOSS" ]; then
    BASELINE_LOSS="$RADE_BASELINE_LOSS"
    BASELINE_NOTE="Supplied via RADE_BASELINE_LOSS."
    echo "  Baseline loss: $BASELINE_LOSS (supplied)"
else
    echo "Computing baseline loss with rade_tx_wav / rade_rx_wav ..."
    BASELINE_OUTPUT=$(compute_baseline_loss)
    BASELINE_LOSS=$(printf '%s\n' "$BASELINE_OUTPUT" | sed -n 's/.*loss: *\([0-9][0-9.]*\).*/\1/p' | head -1)
    if [ -n "$BASELINE_LOSS" ]; then
        echo "  Baseline loss: $BASELINE_LOSS"
        echo "  (rade_c tools: $RADE_C_TOOLS_DIR)"
    else
        BASELINE_NOTE="Could not run rade_tx_wav / rade_rx_wav automatically. Build the rade_c wav tools (they are excluded from Windows builds) and re-run, or pass RADE_BASELINE_LOSS / fill this in by hand."
        echo "  Baseline loss: (unavailable)"
        printf '%s\n' "$BASELINE_OUTPUT" | sed 's/^/    /' >&2
    fi
fi

BASELINE_NOTE_BLOCK=""
if [ -n "$BASELINE_NOTE" ]; then
    BASELINE_NOTE_BLOCK="
_${BASELINE_NOTE}_"
fi

BASELINE_FIELD="_not determined_"
TOLERANCE_FIELD="baseline ± 10%"
THRESHOLD=""
if [ -n "$BASELINE_LOSS" ]; then
    TOL=$(awk -v b="$BASELINE_LOSS" 'BEGIN { printf "%.4f", b * 0.10 }')
    LO=$(awk  -v b="$BASELINE_LOSS" 'BEGIN { printf "%.4f", b * 0.90 }')
    HI=$(awk  -v b="$BASELINE_LOSS" 'BEGIN { printf "%.4f", b * 1.10 }')
    THRESHOLD="$HI"
    BASELINE_FIELD="$BASELINE_LOSS"
    TOLERANCE_FIELD="baseline ± $TOL  ($LO – $HI)"
fi

# ---------------------------------------------------------------------------
# Level 1 - Software Loopback: run test/test_rade_loss.sh and harvest its loss.
# ---------------------------------------------------------------------------
LEVEL1_LOSS_FIELD="_not run_"
LEVEL1_CHECK="- [ ] Pass (loss within ±10% of baseline)"
LEVEL1_SUMMARY="PASS / FAIL / N/A"
LEVEL1_REPRO="Not run by rade_verification_report.sh (RADE_SKIP_LEVEL1 was set)."
LEVEL1_LOG="$WORKDIR/rade_verification_level1.log"

if [ "$RADE_SKIP_LEVEL1" != "1" ]; then
    echo "Running Level 1 software loopback via test/test_rade_loss.sh ..."
    if [ -n "$THRESHOLD" ]; then
        RADE_LOSS_THRESHOLD="$THRESHOLD" PYTHON_BINARY="$PYTHON_BINARY" \
            bash "$SCRIPTPATH/test_rade_loss.sh" >"$LEVEL1_LOG" 2>&1
    else
        PYTHON_BINARY="$PYTHON_BINARY" \
            bash "$SCRIPTPATH/test_rade_loss.sh" >"$LEVEL1_LOG" 2>&1
    fi
    L1_RC=$?

    L1_LOSS=$(sed -n 's/.*loss: *\([0-9][0-9.]*\).*/\1/p' "$LEVEL1_LOG" | tail -1)
    if grep -qx 'PASS' "$LEVEL1_LOG"; then
        L1_RESULT="PASS"
    elif grep -qx 'FAIL' "$LEVEL1_LOG"; then
        L1_RESULT="FAIL"
    else
        L1_RESULT="ERROR"
    fi

    echo "  Level 1 result: $L1_RESULT (loss ${L1_LOSS:-unknown}, exit $L1_RC)"
    echo "  Full log: $LEVEL1_LOG"

    if [ -n "$L1_LOSS" ]; then
        LEVEL1_LOSS_FIELD="$L1_LOSS  ($L1_RESULT)"
    else
        LEVEL1_LOSS_FIELD="$L1_RESULT (see log)"
    fi
    case "$L1_RESULT" in
        PASS) LEVEL1_CHECK="- [x] Pass (loss within ±10% of baseline)"; LEVEL1_SUMMARY="PASS" ;;
        FAIL) LEVEL1_CHECK="- [ ] Pass (loss within ±10% of baseline)  <!-- FAIL -->"; LEVEL1_SUMMARY="FAIL" ;;
        *)    LEVEL1_SUMMARY="FAIL" ;;
    esac

    LEVEL1_REPRO="# From the FreeDV build directory (see .github/workflows/cmake-linux.yml /
# .github/workflows/cmake-macos.yml for the per-platform virtual audio setup):
FREEDV_BINARY=<freedv binary> \\
FREEDV_RADIO_TO_COMPUTER_DEVICE=<device> \\
FREEDV_COMPUTER_TO_RADIO_DEVICE=<device> \\
FREEDV_MICROPHONE_TO_COMPUTER_DEVICE=<device> \\
FREEDV_COMPUTER_TO_SPEAKER_DEVICE=<device> \\
${THRESHOLD:+RADE_LOSS_THRESHOLD=$THRESHOLD }test/test_rade_loss.sh

# loss.py invocation performed inside test_rade_loss.sh:
python3 rade_src/loss.py txfeatures.f32 rxfeatures.f32 \\
    --loss_test <baseline x 1.10> --clip_start 100 --clip_end 300"
fi

# ---------------------------------------------------------------------------
# Emit the report.
# ---------------------------------------------------------------------------
cat > "$OUTPUT" <<EOF
# RADE Integration Verification Report

## Application Under Test

| Field | Value |
|---|---|
| Application name | $APP_NAME |
| Application version / git hash | $APP_VERSION |
| Platform (OS + version) | $PLATFORM |
| Tester name / callsign | ${RADE_TESTER:-} |
| Date | $REPORT_DATE |
| radae repo commit hash | $RADAE_COMMIT |
| rade_c repo commit hash | $RADE_C_COMMIT |

## Signal Path Declaration

- [x] No additional signal processing (AGC, noise gate, resampler, EQ,
      compression) between WAV file input and RADE encoder input during
      this verification test
      <!-- Asserted automatically: test/freedv-ctest-loss.conf.tmpl disables
           AGC, the mic/speaker EQ and Speex noise suppression, and the RADE
           path runs at a fixed 8/16 kHz with no resampling. -->

## Baseline Loss (Step 1)

Re-run with the current repository version before filling in this section.

| Field | Value |
|---|---|
| Baseline loss | $BASELINE_FIELD |
| 10% tolerance window | $TOLERANCE_FIELD |

Command used:
\`\`\`
$BASELINE_CMD
\`\`\`
$BASELINE_NOTE_BLOCK

## Level 1 — Software Loopback (mandatory)

$LEVEL1_CHECK

| Field | Value |
|---|---|
| Loss result | $LEVEL1_LOSS_FIELD |

Command used / reproduction notes:
\`\`\`
$LEVEL1_REPRO
\`\`\`

## Level 2 — OTAC: Over The Audio Cable (mandatory for hardware integrations)

- [ ] Pass (loss within ±10% of baseline)
- [ ] N/A (software-only integration)

| Field | Value |
|---|---|
| Loss result | |
| Sound card (Tx) | |
| Sound card (Rx) | |
| Cable description | |

Photo of test setup:
_(attach or link)_

Reproduction notes:
\`\`\`
(paste notes here)
\`\`\`

## Level 3 — OTC: Over The Coax (optional)

- [ ] Pass (loss within ±10% of baseline)
- [ ] Not performed

| Field | Value |
|---|---|
| Loss result | |
| Tx radio | |
| Rx radio | |
| Attenuator(s) | |

Photo of test setup:
_(attach or link)_

Reproduction notes:
\`\`\`
(paste notes here)
\`\`\`

## Summary

| Level | Result |
|---|---|
| Level 1 — Software loopback | $LEVEL1_SUMMARY |
| Level 2 — OTAC | PASS / FAIL / N/A |
| Level 3 — OTC | PASS / FAIL / N/A |

Additional notes:
EOF

echo "Wrote $OUTPUT"
