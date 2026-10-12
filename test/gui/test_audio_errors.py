#!/usr/bin/env python3
"""Tests what FreeDV does when its audio devices aren't as it expects:
missing, unusable, not set up at all, or changed while it's running.

Each scenario below launches its own copy (or copies) of FreeDV. With no
arguments all of them are run; otherwise, only those named:

    python3 test_audio_errors.py [scenario...]

Run from the build folder (or set FREEDV_BINARY) with a window manager
running. Linux only for now, as it relies on PulseAudio/PipeWire for the audio
devices that FreeDV is set up with.
"""

import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import freedvtest
from freedvtest.audio import Tone, VirtualAudioDevices, move_stream, streams
from test_main_window import (AUDIBLE, MICROPHONE, RADIO_RX, RADIO_TX, RADIO_TX_MONITOR, SPEAKER, SPEAKER_MONITOR,
                              audio_devices, check, check_equal, check_silent, level, quit_and_check_log, settings,
                              start_modem, stop_modem, wait_for_transmit)

# FreeDV's four devices, as its settings name them, with what each is set to.
CARDS = {
    "soundCard1In": VirtualAudioDevices.input_name(RADIO_RX),
    "soundCard1Out": RADIO_TX,
    "soundCard2In": VirtualAudioDevices.input_name(MICROPHONE),
    "soundCard2Out": SPEAKER,
}

OTHER_OUTPUT = "FreeDVGuiTest_Other_Output"
OTHER_INPUT = VirtualAudioDevices.input_name("FreeDVGuiTest_Other_Input")


def card_settings(**changes):
    """Returns settings() with something different for some of the cards:
    each of changes is a card's name and what to set its device to, or
    (as <card>_rate) its sample rate."""
    extra = {}
    for key, value in changes.items():
        if key.endswith("_rate"):
            extra["Audio__%sSampleRate" % key[:-5]] = value
        else:
            extra["Audio__%sDeviceName" % key] = value
    return settings(**extra)


def try_to_start(app):
    """Presses Start and waits for FreeDV to have made its mind up."""
    app.click("main.btnOnOff")
    app.wait_until(lambda: app.get("main.btnOnOff")["enabled"], description="FreeDV to finish starting")
    time.sleep(0.5)


def check_not_started(app, what):
    check_equal(app.state()["running"], False, "Running " + what)
    check_equal(app.get("main.btnOnOff")["value"], False, "Start button pressed " + what)
    for control in ["main.btnPTT", "main.btnTune", "main.btnAnalog", "main.btnVoiceKeyer"]:
        check_equal(app.get(control)["enabled"], False, "%s enabled %s" % (control, what))


def configured_devices(app):
    audio = app.state()["config"]["audio"]
    return {card: audio[card]["deviceName"] for card in CARDS}


# ---------------------------------------------------------------------------
# Scenarios
# ---------------------------------------------------------------------------

def test_missing_device():
    """Starting with a device that isn't there."""
    with audio_devices():
        for card in CARDS:
            missing = "FreeDVGuiTest_Missing" + (".monitor" if card.endswith("In") else "")
            with freedvtest.start(settings=card_settings(**{card: missing})) as app:
                app.wait_for_window("main")
                for attempt in range(2):
                    try_to_start(app)
                    boxes = app.message_boxes(clear=True)
                    check_equal([box["caption"] for box in boxes], ["Sound Device Not Found"],
                                "Messages displayed with %s missing" % card)
                    check(boxes[0]["message"].startswith("Your %s device cannot be found" % missing),
                          "Message displayed with %s missing is %r" % (card, boxes[0]["message"]))
                    check_not_started(app, "with %s missing" % card)
                check(not app.window_exists("settings"), "The Settings window opened with %s missing" % card)
                check_equal(configured_devices(app)[card], missing, "What %s is set to afterwards" % card)
                check_equal(app.quit(), 0, "FreeDV's exit code")

    # A device that goes away between one start and the next, and comes back.
    with VirtualAudioDevices(RADIO_RX, RADIO_TX, MICROPHONE):
        speaker = VirtualAudioDevices(SPEAKER)
        speaker.__enter__()
        try:
            with freedvtest.start(settings=settings()) as app:
                app.wait_for_window("main")
                start_modem(app)
                stop_modem(app)

                speaker.__exit__(None, None, None)
                try_to_start(app)
                boxes = app.message_boxes(clear=True)
                check(len(boxes) == 1 and boxes[0]["message"].startswith("Your %s device cannot be found" % SPEAKER),
                      "Messages displayed with the speaker unplugged are %r" % boxes)
                check_not_started(app, "with the speaker unplugged")

                speaker.__enter__()
                start_modem(app)
                check_equal(configured_devices(app), CARDS, "Devices in use once the speaker is back")
                stop_modem(app)
                quit_and_check_log(app)
        finally:
            speaker.__exit__(None, None, None)


def test_low_sample_rate():
    """Starting with a device set to a sample rate that's too low to use."""
    with audio_devices():
        for card, minimum in [("soundCard1In", 8000), ("soundCard1Out", 8000), ("soundCard2In", 16000),
                              ("soundCard2Out", 16000)]:
            rate = minimum // 2
            with freedvtest.start(settings=card_settings(**{card + "_rate": rate})) as app:
                app.wait_for_window("main")
                try_to_start(app)
                check_equal([(box["caption"], box["message"]) for box in app.message_boxes(clear=True)],
                            [("Sample Rate Too Low",
                              "Your %s device is set to use a sample rate of %d, which is less than the minimum of "
                              "%d. Please go to Tools->Settings to check your settings." % (CARDS[card], rate, minimum))],
                            "Messages displayed with %s at %d" % (card, rate))
                check_not_started(app, "with %s at %d" % (card, rate))
                check_equal(app.quit(), 0, "FreeDV's exit code")

        # The lowest rates that are allowed.
        lowest = dict(soundCard1In_rate=8000, soundCard1Out_rate=8000, soundCard2In_rate=16000,
                      soundCard2Out_rate=16000)
        with freedvtest.start(settings=card_settings(**lowest)) as app:
            app.wait_for_window("main")
            start_modem(app)
            stop_modem(app)
            quit_and_check_log(app)

        # Receiving only, what goes to the radio otherwise goes to the
        # speaker, which needs more.
        receive_only = dict(soundCard1Out=SPEAKER, soundCard2In="none", soundCard2Out="none")
        with freedvtest.start(settings=card_settings(soundCard1Out_rate=8000, **receive_only)) as app:
            app.wait_for_window("main")
            try_to_start(app)
            boxes = app.message_boxes(clear=True)
            check(len(boxes) == 1 and "sample rate of 8000, which is less than the minimum of 16000" in
                  boxes[0]["message"], "Messages displayed are %r" % boxes)
            check_not_started(app, "with the speaker at 8000")
            check_equal(app.quit(), 0, "FreeDV's exit code")


def test_no_devices():
    """Starting with no devices chosen, which gets the Settings window
    rather than a complaint."""
    nothing = {card: "none" for card in CARDS}
    with audio_devices():
        with freedvtest.start(settings=card_settings(**nothing)) as app:
            app.wait_for_window("main")
            app.click("main.btnOnOff")
            app.wait_for_window("settings")
            check_equal(app.message_boxes(), [], "Messages displayed")
            app.click("settings.sdbSizer5Cancel")
            app.wait_for_window_closed("settings")
            app.wait_until(lambda: app.get("main.btnOnOff")["enabled"], description="FreeDV to give up starting")
            check_not_started(app, "with no devices chosen")

            # Only half of the first pair is as good as none.
            check_equal(app.quit(), 0, "FreeDV's exit code")

        with freedvtest.start(settings=card_settings(soundCard1Out="none", soundCard2In="none",
                                                     soundCard2Out="none")) as app:
            app.wait_for_window("main")
            app.click("main.btnOnOff")
            app.wait_for_window("settings")
            app.click("settings.sdbSizer5Cancel")
            app.wait_for_window_closed("settings")
            app.wait_until(lambda: app.get("main.btnOnOff")["enabled"], description="FreeDV to give up starting")
            check_not_started(app, "with only the radio's input chosen")
            check_equal(app.quit(), 0, "FreeDV's exit code")


def test_receive_only():
    """Starting with only the first pair of devices, which is enough to
    receive with."""
    with audio_devices():
        for second_pair in [dict(soundCard2In="none", soundCard2Out="none"),
                            # Half a second pair is no pair at all.
                            dict(soundCard2Out="none")]:
            what = "with %r" % second_pair
            with freedvtest.start(settings=card_settings(soundCard1Out=SPEAKER, **second_pair)) as app:
                app.wait_for_window("main")
                start_modem(app)
                check_equal(app.state()["numSoundCards"], 1, "Pairs of devices in use " + what)
                for control in ["main.btnPTT", "main.btnTune", "main.btnVoiceKeyer"]:
                    check_equal(app.get(control)["enabled"], False, "%s enabled %s" % (control, what))
                check_equal(app.get("main.btnAnalog")["enabled"], True, "Analog enabled " + what)

                # What's received is heard through the one output there is.
                app.click("main.btnAnalog")
                with Tone(RADIO_RX):
                    time.sleep(0.5)
                    heard = level(SPEAKER_MONITOR)
                    check(heard > AUDIBLE, "The radio isn't being played through the speaker %s (level %d)"
                          % (what, heard))
                    check_silent(RADIO_TX_MONITOR, "Something is being sent to the radio " + what)

                stop_modem(app)
                quit_and_check_log(app)


def test_device_moved():
    """Streams being moved to other devices while FreeDV is running, as the
    system's sound settings can do. FreeDV carries on with, and remembers,
    where each ended up."""
    with audio_devices(), VirtualAudioDevices(OTHER_OUTPUT, "FreeDVGuiTest_Other_Input"):
        with freedvtest.start(settings=settings()) as app:
            app.wait_for_window("main")
            start_modem(app)
            check_equal(configured_devices(app), CARDS, "Devices to begin with")
            def stream_on(device):
                found = [stream for stream in streams(app.pid) if stream["device"] == device]
                check_equal(len(found), 1, "Number of FreeDV's streams on %s (it has %r)" % (device, streams(app.pid)))
                return found[0]

            def move(device, to):
                # The sound server doesn't always do as it's asked the first
                # time, so make sure of it before expecting anything of FreeDV.
                for attempt in range(5):
                    time.sleep(1)
                    move_stream(stream_on(device), to)
                    deadline = time.time() + 3
                    while time.time() < deadline:
                        if any(stream["device"] == to for stream in streams(app.pid)):
                            return
                        time.sleep(0.25)
                check(False, "FreeDV's stream on %s couldn't be moved to %s" % (device, to))

            def wait_for_devices(expected, what):
                try:
                    app.wait_until(lambda: configured_devices(app) == expected, timeout=10)
                except freedvtest.FreeDVError:
                    told = [line.split("PulseAudioDevice.cpp")[-1] for line in app.log().splitlines()
                            if "has been moved" in line]
                    check_equal(configured_devices(app), expected,
                                "%s (streams: %r; FreeDV was told: %r)" % (what, streams(app.pid), told))

            # Each of the four in turn, and back again.
            for card, device in CARDS.items():
                other = OTHER_INPUT if card.endswith("In") else OTHER_OUTPUT
                move(device, other)
                wait_for_devices(dict(CARDS, **{card: other}), "Devices after moving " + card)
                check_equal(freedvtest.read_config(app.config_path).get("/Audio/%sDeviceName" % card), other,
                            "Saved device for " + card)
                check_equal(app.message_boxes(), [], "Messages displayed after moving " + card)
                check_equal(app.state()["running"], True, "Running after moving " + card)

                move(other, device)
                wait_for_devices(CARDS, "Devices after moving %s back" % card)

            # It's the new device that's then used: the speaker's stream,
            # with what's coming from the radio played through it.
            app.click("main.btnAnalog")
            move(SPEAKER, OTHER_OUTPUT)
            wait_for_devices(dict(CARDS, soundCard2Out=OTHER_OUTPUT), "Devices after moving the speaker")
            with Tone(RADIO_RX):
                time.sleep(0.75)
                heard = level(VirtualAudioDevices.input_name(OTHER_OUTPUT))
                check(heard > AUDIBLE, "The radio isn't being played through the other output (level %d)" % heard)
                check_silent(SPEAKER_MONITOR, "Something is still being played through the old speaker")

            # Likewise for what's sent to the radio.
            move(RADIO_TX, SPEAKER)
            wait_for_devices(dict(CARDS, soundCard1Out=SPEAKER, soundCard2Out=OTHER_OUTPUT),
                             "Devices after moving the radio's output")
            app.click("main.btnPTT")
            wait_for_transmit(app)
            with Tone(MICROPHONE):
                time.sleep(0.75)
                sent = level(SPEAKER_MONITOR)
                check(sent > AUDIBLE, "The microphone isn't being sent to where the radio's output went (level %d)"
                      % sent)
            app.click("main.btnPTT")
            wait_for_transmit(app, False)

            # And where it'll start from the next time.
            stop_modem(app)
            start_modem(app)
            check_equal(configured_devices(app), dict(CARDS, soundCard1Out=SPEAKER, soundCard2Out=OTHER_OUTPUT),
                        "Devices after starting again")
            check_equal(sorted(stream["device"] for stream in streams(app.pid)),
                        sorted([CARDS["soundCard1In"], SPEAKER, CARDS["soundCard2In"], OTHER_OUTPUT]),
                        "What FreeDV's streams are connected to after starting again")
            stop_modem(app)
            quit_and_check_log(app)


SCENARIOS = {
    "missing_device": test_missing_device,
    "low_sample_rate": test_low_sample_rate,
    "no_devices": test_no_devices,
    "receive_only": test_receive_only,
    "device_moved": test_device_moved,
}


def main():
    names = sys.argv[1:] or list(SCENARIOS)
    for scenario in names:
        if scenario not in SCENARIOS:
            sys.exit("Unknown scenario '%s' (available: %s)" % (scenario, ", ".join(SCENARIOS)))
    for scenario in names:
        print("=== %s ===" % scenario, flush=True)
        SCENARIOS[scenario]()
        print("PASS: %s" % scenario, flush=True)


if __name__ == "__main__":
    main()
