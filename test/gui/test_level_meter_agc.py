#!/usr/bin/env python3
"""Tests the level meter in FreeDV's main window and the TX AGC (leveler and
limiter): the meter reading what's heard from the radio and the microphone,
and the AGC bringing quiet and loud microphones to the same level.

Each scenario below launches its own copy of FreeDV. With no arguments all of
them are run; otherwise, only those named:

    python3 test_level_meter_agc.py [scenario...]

Run from the build folder (or set FREEDV_BINARY) with a window manager
running. Linux only for now, as it relies on PulseAudio/PipeWire for the audio
devices that FreeDV is set up with.
"""

import math
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import freedvtest
from freedvtest.audio import Tone, VirtualAudioDevices
from test_main_window import (MICROPHONE, RADIO_RX, RADIO_TX_MONITOR, audio_devices, check, check_equal, level,
                              quit_and_check_log, settings, start_modem, stop_modem, wait_for_transmit)

METER = "main.gaugeLevel"
FULL_SCALE = 32767

# What the AGC aims for unless told otherwise (/Filter/levelerTargetLufs).
TARGET_LUFS = -26.0
# Furthest the leveler's gain goes either way.
GAIN_LIMIT_DB = 12.0
# Below this, what's coming from the microphone counts as a pause and the
# leveler's gain is held (noise reduction off, as settings() has it).
PAUSE_LUFS = -33.0
# Analog mode sends the microphone to the radio at twice its level.
ANALOG_GAIN_DB = 6.0


def amplitude(dbfs):
    """The peak, in 16 bit samples, of a tone that peaks at dbfs."""
    return int(round(FULL_SCALE * 10 ** (dbfs / 20.0)))


def tone_lufs(peak_dbfs):
    """How loud a 1 kHz tone is, by its peak: a full scale one is -3 LUFS."""
    return peak_dbfs - 3.01


def tone_peak_for_lufs(lufs):
    return lufs + 3.01


def rms_dbfs(rms):
    return 20 * math.log10(rms / FULL_SCALE) if rms > 0 else -120.0


def meter(app):
    return app.get(METER)


def gain(app):
    return app.state()["agcAppliedGainDb"]


def check_near(actual, expected, tolerance, what):
    check(abs(actual - expected) <= tolerance, "%s is %.2f, expected %.2f (+/- %.1f)" % (what, actual, expected, tolerance))


def wait_for_meter(app, expected_db, what, tolerance_db=1.0, timeout=10):
    try:
        app.wait_until(lambda: abs(meter(app)["value"] - expected_db) <= tolerance_db, timeout=timeout,
                       description="the level meter to read %.1f dB (%s)" % (expected_db, what))
    except freedvtest.FreeDVError:
        check_near(meter(app)["value"], expected_db, tolerance_db, "The level meter (%s)" % what)


def wait_for_gain(app, expected_db, what, tolerance_db=1.5, timeout=40):
    """Waits for the leveler to settle on a gain, then checks that it stays
    there."""
    try:
        app.wait_until(lambda: abs(gain(app) - expected_db) <= tolerance_db, timeout=timeout,
                       description="the AGC gain to reach %+.1f dB (%s)" % (expected_db, what))
    except freedvtest.FreeDVError:
        check_near(gain(app), expected_db, tolerance_db, "AGC gain (%s)" % what)
    time.sleep(3)
    check_near(gain(app), expected_db, tolerance_db, "AGC gain a few seconds later (%s)" % what)


def output_lufs(analog=True):
    """How loud the tone sent to the radio is, as the microphone would have
    to be to sound like it."""
    measured = rms_dbfs(level(RADIO_TX_MONITOR, 1.5))
    # A tone's RMS is 3 dB below its peak, and so the same as its loudness.
    return measured - (ANALOG_GAIN_DB if analog else 0.0)


def start_analog_transmit(app):
    start_modem(app)
    app.click("main.btnAnalog")
    app.wait_until(lambda: app.state()["analog"], timeout=5, description="analog mode")
    app.click("main.btnPTT")
    wait_for_transmit(app)


def stop_transmit(app):
    app.click("main.btnPTT")
    wait_for_transmit(app, False)
    stop_modem(app)


# ---------------------------------------------------------------------------
# Scenarios
# ---------------------------------------------------------------------------

def test_meter():
    """The level meter's scale, and its readings of the radio (receiving) and
    the microphone (transmitting)."""
    with audio_devices():
        with freedvtest.start(settings=settings()) as app:
            app.wait_for_window("main")
            described = meter(app)
            check_equal((described["minDb"], described["maxDb"]), (-30.0, 0.0), "The level meter's range")
            check_equal((described["amberStartDb"], described["redStartDb"]), (-9.0, -3.0),
                        "Where the level meter turns amber and red")
            check_equal(described["value"], -30.0, "The level meter before starting")

            start_modem(app)

            # Receiving, it shows what comes from the radio, peak reading.
            # Louder each time, as it takes a while to fall back.
            for peak in [-24.0, -12.0, -6.0]:
                with Tone(RADIO_RX, amplitude=amplitude(peak)):
                    wait_for_meter(app, peak, "radio at %.0f dBFS" % peak)
            check_equal(meter(app)["zoneColours"], False, "The level meter showing colour zones while receiving")
            app.wait_until(lambda: meter(app)["value"] < -24.0, timeout=10,
                           description="the level meter to fall once the radio is quiet")

            # Transmitting, it shows what comes from the microphone, before
            # anything is done to it (here, the AGC, which would otherwise
            # make every tone the same level).
            app.click("main.btnPTT")
            wait_for_transmit(app)
            for peak in [-24.0, -12.0, -6.0]:
                with Tone(MICROPHONE, amplitude=amplitude(peak)):
                    wait_for_meter(app, peak, "microphone at %.0f dBFS" % peak)
            check_equal(meter(app)["zoneColours"], True, "The level meter showing colour zones while transmitting")
            app.wait_until(lambda: meter(app)["value"] < -24.0, timeout=10,
                           description="the level meter to fall once the microphone is quiet")
            app.click("main.btnPTT")
            wait_for_transmit(app, False)

            stop_modem(app)
            quit_and_check_log(app)


def test_agc():
    """With the AGC on, quiet and loud microphones are sent at the target
    level, and the gain holds while nothing is said."""
    with audio_devices():
        with freedvtest.start(settings=settings(Filter__agcEnable=True)) as app:
            app.wait_for_window("main")
            check_equal(app.state()["config"]["filter"]["levelerTargetLufs"], TARGET_LUFS, "The AGC's target")
            start_analog_transmit(app)

            for name, lufs in [("quiet", -30.0), ("loud", -16.0)]:
                expected_gain = max(-GAIN_LIMIT_DB, min(GAIN_LIMIT_DB, TARGET_LUFS - lufs))
                with Tone(MICROPHONE, amplitude=amplitude(tone_peak_for_lufs(lufs)), max_seconds=60):
                    wait_for_gain(app, expected_gain, "%s microphone at %.0f LUFS" % (name, lufs))
                    check_near(output_lufs(), TARGET_LUFS, 1.5, "Loudness sent to the radio (%s microphone)" % name)
                    settled = gain(app)

            # Too quiet to be speech: the gain stays where the loud
            # microphone left it, rather than climbing on the background.
            with Tone(MICROPHONE, amplitude=amplitude(tone_peak_for_lufs(PAUSE_LUFS - 9)), max_seconds=30):
                time.sleep(8)
                check_near(gain(app), settled, 0.5, "AGC gain during a pause")

            stop_transmit(app)
            quit_and_check_log(app)


def test_agc_off():
    """With the AGC off, the gain stays at 0 dB and what's sent to the radio
    follows the microphone."""
    with audio_devices():
        with freedvtest.start(settings=settings()) as app:
            app.wait_for_window("main")
            check_equal(app.state()["config"]["filter"]["agcEnabled"], False, "AGC enabled")
            start_analog_transmit(app)

            sent = []
            for lufs in [-30.0, -18.0]:
                with Tone(MICROPHONE, amplitude=amplitude(tone_peak_for_lufs(lufs))):
                    time.sleep(3)
                    check_near(gain(app), 0.0, 0.1, "AGC gain with the AGC off")
                    sent.append(output_lufs())
                    check_near(sent[-1], lufs, 1.5, "Loudness sent to the radio with the AGC off")
            check_near(sent[1] - sent[0], 12.0, 1.0, "Change in loudness sent to the radio")

            stop_transmit(app)
            quit_and_check_log(app)


def test_saved_gain():
    """The AGC carries on from the gain it had last time, unless the
    microphone has changed since."""
    microphone = VirtualAudioDevices.input_name(MICROPHONE)
    saved = 8.0
    for saved_with, expected in [(microphone, saved), ("Some other microphone", 0.0), (None, saved)]:
        what = "saved with %s" % (saved_with or "no microphone recorded")
        extra = dict(Filter__agcEnable=True, Filter__levelerGainDb=saved)
        if saved_with is not None:
            extra["Filter__levelerMicDeviceName"] = saved_with
        with audio_devices():
            with freedvtest.start(settings=settings(**extra)) as app:
                app.wait_for_window("main")
                start_analog_transmit(app)
                # Nothing's being said, so the gain stays where it starts.
                time.sleep(2)
                check_near(gain(app), expected, 0.3, "AGC gain at the start (%s)" % what)
                stop_transmit(app)
                quit_and_check_log(app)

                config = freedvtest.read_config(app.config_path)
                check_equal(config.get("/Filter/levelerMicDeviceName"), microphone,
                            "Microphone saved with the AGC's gain (%s)" % what)
                check_near(float(config["/Filter/levelerGainDb"]), expected, 0.3, "AGC gain saved (%s)" % what)


SCENARIOS = {
    "meter": test_meter,
    "agc": test_agc,
    "agc_off": test_agc_off,
    "saved_gain": test_saved_gain,
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
