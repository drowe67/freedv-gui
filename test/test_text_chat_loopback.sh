#!/bin/bash
#
# Two FreeDV instances wired to each other through PulseAudio null sinks, so
# the text chat transmit and receive paths can be exercised end to end with no
# radio and no RF. This is the bench the design notes ask for before trusting
# the transmit path on the air: it exercises the transmitter borrowed from the
# mic pipeline, the PTT handoff, fragmentation, acknowledgements and retries.
#
#   test/test_text_chat_loopback.sh up       start the cables and both stations
#   test/test_text_chat_loopback.sh down     stop both stations, remove cables
#   test/test_text_chat_loopback.sh status   what is currently running
#
# Linux/PulseAudio (or pipewire-pulse) only.

set -uo pipefail

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
REPO_ROOT="$( cd -- "$SCRIPTPATH/.." >/dev/null 2>&1 ; pwd -P )"

WORKDIR="${FREEDV_TEXT_CHAT_WORKDIR:-$(pwd)/text_chat_loopback}"

# RADE by default, because that is what the fork is actually run in. Two RADE
# instances are heavy: on a small machine set FREEDV_TEST_MODE=4 (700D) so a
# CPU starved dropout does not get mistaken for a protocol bug.
FREEDV_TEST_MODE="${FREEDV_TEST_MODE:-257}"

# Deliberately not anybody's callsign: this never reaches the air, and a real
# one has no business being a default in a public repository. The portable
# suffix is on purpose, since it exercises the base 40 packing that carries "/"
# while keeping the two stations distinct. Override for your own station.
STATION_A_CALLSIGN="${STATION_A_CALLSIGN:-TEST1/P}"
STATION_B_CALLSIGN="${STATION_B_CALLSIGN:-TEST2/P}"

# A's transmit audio lands in the sink that B listens to, and the other way
# round. The monitor of a null sink already carries whatever was played to it,
# so no module-loopback is needed.
CABLE_A_TO_B=TextChat_A_To_B
CABLE_B_TO_A=TextChat_B_To_A
# Speakers are discarded, and the mic sinks are never played to, so their
# monitors are a source of silence -- which is what a chat only station wants
# going into its transmit pipeline.
SPEAKER_A=TextChat_A_Speaker
SPEAKER_B=TextChat_B_Speaker
MIC_A=TextChat_A_Mic
MIC_B=TextChat_B_Mic

ALL_CABLES="$CABLE_A_TO_B $CABLE_B_TO_A $SPEAKER_A $SPEAKER_B $MIC_A $MIC_B"

findBinary () {
    if [ -n "${FREEDV_BINARY:-}" ]; then echo "$FREEDV_BINARY"; return; fi
    for candidate in "$(pwd)/src/freedv" "$REPO_ROOT/build/src/freedv"; do
        if [ -x "$candidate" ]; then echo "$candidate"; return; fi
    done
    echo ""
}

createVirtualAudioCable () {
    local name=$1
    pactl load-module module-null-sink \
        sink_name="$name" \
        sink_properties=device.description="$name" \
        format=s16le rate=48000 channels=2
}

waitForCableUp () {
    local name=$1
    local tries=0
    until pactl list short sinks | grep -qE "^[0-9]+\s+${name}\s+"; do
        tries=$((tries + 1))
        if [ $tries -gt 20 ]; then
            echo "ERROR: $name never appeared" >&2
            return 1
        fi
        sleep 0.5
    done
}

# Each station gets its own HOME. FreeDV takes the chat database path from the
# user data directory rather than from -f, so without this both instances open
# the same text_messaging.db and each would show the other's sent messages as
# its own history.
writeStationConfig () {
    local dir=$1 callsign=$2 rxin=$3 txout=$4 micin=$5 spkout=$6
    mkdir -p "$dir"
    sed -e "s|@FREEDV_RADIO_TO_COMPUTER_DEVICE@|$rxin|g" \
        -e "s|@FREEDV_COMPUTER_TO_RADIO_DEVICE@|$txout|g" \
        -e "s|@FREEDV_MICROPHONE_TO_COMPUTER_DEVICE@|$micin|g" \
        -e "s|@FREEDV_COMPUTER_TO_SPEAKER_DEVICE@|$spkout|g" \
        -e "s|@FREEDV_CALLSIGN@|$callsign|g" \
        -e "s|@FREEDV_TEST_MODE@|$FREEDV_TEST_MODE|g" \
        "$SCRIPTPATH/freedv-text-chat-station.conf.tmpl" > "$dir/freedv.conf"
}

# FREEDV_TEXT_CHAT_UI_LOG makes the chat window report what it is showing, so
# a refresh bug can be found in the log rather than over someone's shoulder.
startStation () {
    local name=$1 dir=$2
    ( HOME="$dir" FREEDV_TEXT_CHAT_UI_LOG=1 \
      exec "$BINARY" -f "$dir/freedv.conf" > "$dir/freedv.log" 2>&1 ) &
    echo $! > "$dir/freedv.pid"
    echo "  station $name: pid $(cat "$dir/freedv.pid"), log $dir/freedv.log"
}

doUp () {
    BINARY="$(findBinary)"
    if [ -z "$BINARY" ]; then
        echo "ERROR: no freedv binary. Build first, or set FREEDV_BINARY." >&2
        exit 1
    fi
    if ! pactl info >/dev/null 2>&1; then
        echo "ERROR: no PulseAudio server reachable via pactl." >&2
        exit 1
    fi

    echo "Using $BINARY (FreeDV mode $FREEDV_TEST_MODE)"
    mkdir -p "$WORKDIR"
    : > "$WORKDIR/modules"

    echo "Creating virtual audio cables..."
    for cable in $ALL_CABLES; do
        if pactl list short sinks | grep -qE "^[0-9]+\s+${cable}\s+"; then
            echo "  $cable already exists, leaving it alone"
            continue
        fi
        createVirtualAudioCable "$cable" >> "$WORKDIR/modules" || exit 1
        waitForCableUp "$cable" || exit 1
        echo "  $cable"
    done

    writeStationConfig "$WORKDIR/stationA" "$STATION_A_CALLSIGN" \
        "$CABLE_B_TO_A.monitor" "$CABLE_A_TO_B" "$MIC_A.monitor" "$SPEAKER_A"
    writeStationConfig "$WORKDIR/stationB" "$STATION_B_CALLSIGN" \
        "$CABLE_A_TO_B.monitor" "$CABLE_B_TO_A" "$MIC_B.monitor" "$SPEAKER_B"

    echo "Starting stations..."
    startStation "A ($STATION_A_CALLSIGN)" "$WORKDIR/stationA"
    startStation "B ($STATION_B_CALLSIGN)" "$WORKDIR/stationB"

    cat <<EOF

Both windows are on your desktop. To run the test:

  1. Press Start in BOTH windows. Text chat cannot transmit until FreeDV is
     running, because it borrows the transmit thread from the mic pipeline.
  2. Open Tools -> Text Chat... in both.
  3. From A, Send as Broadcast. B should list A under Heard Stations.
  4. Select the other station in Heard Stations and Ping it, then Send an
     addressed message and watch the delivery chip go SENDING -> OK.
  5. Send something over $((39 * 2)) characters to exercise fragmentation, and
     close B mid-message to watch A go RETRY 1..3 and then FAILED.

The reporter hostname is pointed at 127.0.0.1 in both configs. FreeDV builds
its reporter client regardless of the Reporting/Enable setting, so disabling
reporting alone is not enough to keep these test callsigns off the live
FreeDV Reporter list -- the hostname is what actually stops it.

  tail -f $WORKDIR/station{A,B}/freedv.log
  $0 down

EOF
}

doDown () {
    for station in stationA stationB; do
        local pidfile="$WORKDIR/$station/freedv.pid"
        if [ -f "$pidfile" ]; then
            local pid
            pid=$(cat "$pidfile")
            if kill -0 "$pid" 2>/dev/null; then
                echo "Stopping $station (pid $pid)"
                kill "$pid" 2>/dev/null
            fi
            rm -f "$pidfile"
        fi
    done

    if [ -f "$WORKDIR/modules" ]; then
        while read -r module; do
            [ -n "$module" ] && pactl unload-module "$module" 2>/dev/null
        done < "$WORKDIR/modules"
        rm -f "$WORKDIR/modules"
        echo "Virtual audio cables removed."
    fi

    echo "Logs and chat databases are kept under $WORKDIR."
}

doStatus () {
    echo "Cables:"
    for cable in $ALL_CABLES; do
        if pactl list short sinks | grep -qE "^[0-9]+\s+${cable}\s+"; then
            echo "  $cable up"
        else
            echo "  $cable down"
        fi
    done

    echo "Stations:"
    for station in stationA stationB; do
        local pidfile="$WORKDIR/$station/freedv.pid"
        if [ -f "$pidfile" ] && kill -0 "$(cat "$pidfile")" 2>/dev/null; then
            echo "  $station running (pid $(cat "$pidfile"))"
        else
            echo "  $station not running"
        fi
    done
}

case "${1:-up}" in
    up)     doUp ;;
    down)   doDown ;;
    status) doStatus ;;
    *)      echo "Usage: $0 [up|down|status]" >&2; exit 1 ;;
esac
