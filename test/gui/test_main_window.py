#!/usr/bin/env python3
"""Tests FreeDV's main window.

Each scenario below launches its own copy (or copies) of FreeDV. With no
arguments all of them are run; otherwise, only those named:

    python3 test_main_window.py [scenario...]

Run from the build folder (or set FREEDV_BINARY) with a window manager
running, as the key used for PTT only works while the window is active.
Linux only for now, as it relies on PulseAudio/PipeWire for the audio devices
that FreeDV is set up with, and for listening to what it does with them.
"""

import glob
import os
import re
import shutil
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import freedvtest
from freedvtest.audio import Loopback, Tone, VirtualAudioDevices, file_level, record_level, write_tone

RADIO_RX = "FreeDVGuiTest_Radio_To_Computer"
RADIO_TX = "FreeDVGuiTest_Computer_To_Radio"
MICROPHONE = "FreeDVGuiTest_Microphone_To_Computer"
SPEAKER = "FreeDVGuiTest_Computer_To_Speaker"

# Where to listen for what FreeDV sends to the radio and to the speaker.
RADIO_TX_MONITOR = VirtualAudioDevices.input_name(RADIO_TX)
SPEAKER_MONITOR = VirtualAudioDevices.input_name(SPEAKER)

TABS = ["Waterfall", "Spectrum", "Frm Radio", "Frm Mic", "Frm Decoder", "SNR"]

# Somewhere for FreeDV to report to that isn't the public FreeDV Reporter
# server (nothing's listening there).
NOWHERE = "127.0.0.1:9"

CALLSIGN = "AA1ZZZ"

# Levels (see freedvtest.audio.record_level()) below and above which a device
# is taken to be silent or to have something playing through it.
SILENT = 50
AUDIBLE = 500

WXK_F5 = 344


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def check_equal(actual, expected, what):
    check(actual == expected, "%s is %r, expected %r" % (what, actual, expected))


def label(app, name):
    return app.get(name)["label"].replace("&", "")


def audio_devices():
    return VirtualAudioDevices(RADIO_RX, RADIO_TX, MICROPHONE, SPEAKER)


def settings(**extra):
    """Returns settings for FreeDV to use the devices from audio_devices(),
    plus whatever else is given (with "/" in names written as "__")."""
    result = {}
    for card, device in [("soundCard1In", VirtualAudioDevices.input_name(RADIO_RX)), ("soundCard1Out", RADIO_TX),
                         ("soundCard2In", VirtualAudioDevices.input_name(MICROPHONE)), ("soundCard2Out", SPEAKER)]:
        result["/Audio/%sDeviceName" % card] = device
        result["/Audio/%sSampleRate" % card] = 48000
    # Leave what goes into the microphone alone: the tones used here are just
    # the kind of steady noise that noise reduction exists to remove.
    result["/Filter/speexpp_enable"] = False
    result["/Filter/agcEnable"] = False
    # Keep FreeDV off the public FreeDV Reporter server.
    result["/Reporting/FreeDV/Hostname"] = NOWHERE
    result["/Reporting/FreeDV/UseTls"] = False
    result["/Reporting/PSKReporter/Enable"] = False
    for name, value in extra.items():
        result["/" + name.replace("__", "/")] = value
    return result


def start_modem(app):
    app.click("main.btnOnOff")
    app.wait_until(lambda: app.get("main.btnOnOff")["enabled"], description="FreeDV to start")
    check_equal(app.message_boxes(), [], "Errors displayed on start")
    check(app.state()["running"], "FreeDV is not running after pressing Start")


def stop_modem(app):
    app.click("main.btnOnOff")
    app.wait_until(lambda: app.get("main.btnOnOff")["enabled"], description="FreeDV to stop")
    check_equal(app.state()["running"], False, "Running after pressing Stop")


def transmitting(app):
    return app.state()["transmitting"]


def wait_for_transmit(app, expected=True, timeout=15):
    try:
        app.wait_until(lambda: transmitting(app) == expected, timeout)
    except freedvtest.FreeDVError:
        check_equal(transmitting(app), expected, "Transmitting")
    if not expected:
        # Going back to receive takes a moment longer to finish.
        app.wait_until(lambda: app.get("main.btnOnOff")["enabled"], timeout, "transmit to finish ending")


def level(device, seconds=0.75):
    return record_level(device, seconds)


def check_silent(device, what):
    measured = level(device, 0.5)
    check(measured < SILENT, "%s (level %d)" % (what, measured))


def check_audible(device, what):
    measured = level(device)
    check(measured > AUDIBLE, "%s (level %d)" % (what, measured))


def quit_and_check_log(app):
    check_equal(app.message_boxes(), [], "Errors displayed")
    check_equal(app.quit(), 0, "FreeDV's exit code")
    errors = [line for line in app.log().splitlines() if " ERROR " in line or " FATAL " in line]
    check_equal(errors, [], "Errors logged by FreeDV")


# ---------------------------------------------------------------------------
# Scenarios
# ---------------------------------------------------------------------------

def test_transmit():
    """The XMIT button, and what else changes while transmitting."""
    with audio_devices():
        with freedvtest.start(settings=settings()) as app:
            app.wait_for_window("main")
            check_equal(app.get("main.notebook")["pages"], TABS, "Tabs")

            # Nothing to do with transmitting can be used until started.
            for button in ["main.btnPTT", "main.btnTune", "main.btnVoiceKeyer", "main.btnAnalog"]:
                check_equal(app.get(button)["enabled"], False, button + " enabled before starting")

            start_modem(app)
            for button in ["main.btnPTT", "main.btnTune", "main.btnVoiceKeyer", "main.btnAnalog"]:
                check_equal(app.get(button)["enabled"], True, button + " enabled once started")
            check_equal(label(app, "main.btnPTT"), "XMIT", "PTT button's label")
            check_silent(RADIO_TX_MONITOR, "Something is being sent to the radio while receiving")

            # Each of the tabs can be looked at.
            for tab in TABS:
                app.set_value("main.notebook", tab)
                check_equal(app.get("main.notebook")["value"], TABS.index(tab), "Tab displayed")
            app.set_value("main.notebook", "Spectrum")

            app.click("main.btnPTT")
            wait_for_transmit(app)
            check_equal(app.get("main.btnPTT")["value"], True, "PTT button pressed")
            check_audible(RADIO_TX_MONITOR, "Nothing is being sent to the radio while transmitting")

            # While transmitting, FreeDV can't be stopped, tuned or retuned,
            # and what the microphone's picking up is displayed.
            for control in ["main.btnOnOff", "main.btnTune", "main.cboReportFrequency"]:
                check_equal(app.get(control)["enabled"], False, control + " enabled while transmitting")
            check_equal(app.get("main.notebook")["value"], TABS.index("Frm Mic"), "Tab displayed while transmitting")

            app.click("main.btnPTT")
            wait_for_transmit(app, False)
            check_equal(app.get("main.btnPTT")["value"], False, "PTT button pressed")
            check_equal(label(app, "main.btnPTT"), "XMIT", "PTT button's label")
            for control in ["main.btnOnOff", "main.btnTune", "main.cboReportFrequency"]:
                check_equal(app.get(control)["enabled"], True, control + " enabled after transmitting")
            check_equal(app.get("main.notebook")["value"], TABS.index("Spectrum"), "Tab displayed after transmitting")
            check_silent(RADIO_TX_MONITOR, "Something is still being sent to the radio")

            # The PTT button's menu.
            items = app.context_menu("main.btnPTT")
            check_equal([(item["label"], item["enabled"], item["checked"]) for item in items],
                        [("Monitor transmitted audio", True, False), ("Adjust Monitor Volume...", False, False)],
                        "PTT button's menu")
            app.context_menu("main.btnPTT", choose="Monitor transmitted audio")
            items = app.context_menu("main.btnPTT")
            check_equal([(item["label"], item["enabled"], item["checked"]) for item in items],
                        [("Monitor transmitted audio", True, True), ("Adjust Monitor Volume...", True, False)],
                        "PTT button's menu when monitoring")

            # With monitoring on, what's said into the microphone is heard.
            with Tone(MICROPHONE):
                app.click("main.btnPTT")
                wait_for_transmit(app)
                check_audible(SPEAKER_MONITOR, "Transmitted audio isn't being monitored")
                app.click("main.btnPTT")
                wait_for_transmit(app, False)
            app.context_menu("main.btnPTT", choose="Monitor transmitted audio")

            # Closing FreeDV while it's transmitting stops everything first.
            app.click("main.btnPTT")
            wait_for_transmit(app)
            check_equal(app.message_boxes(), [], "Errors displayed")
            check_equal(app.quit(), 0, "FreeDV's exit code")
            check_silent(RADIO_TX_MONITOR, "Something is still being sent to the radio after closing")


def test_ptt_key():
    """Transmitting with a key on the keyboard."""
    with audio_devices():
        # By default, the space bar switches between transmit and receive.
        with freedvtest.start(settings=settings()) as app:
            app.wait_for_window("main")
            app.activate("main")
            app.key("main.notebook", "Space")
            time.sleep(0.5)
            check_equal(app.state()["running"], False, "Started by the PTT key")

            start_modem(app)
            app.activate("main")
            app.key("main.notebook", "Space")
            wait_for_transmit(app)
            check_equal(app.get("main.btnPTT")["value"], True, "PTT button pressed")
            # Any other key is just a key.
            app.key("main.notebook", "F5")
            time.sleep(0.5)
            check_equal(transmitting(app), True, "Transmitting after pressing another key")
            app.key("main.notebook", "Space")
            wait_for_transmit(app, False)
            check_equal(app.get("main.btnPTT")["value"], False, "PTT button pressed")

            stop_modem(app)
            quit_and_check_log(app)

        # Another key, which has to be held down.
        with freedvtest.start(settings=settings(Rig__PttKeyCode=WXK_F5, Rig__PttMomentaryMode=True)) as app:
            app.wait_for_window("main")
            start_modem(app)
            app.activate("main")
            app.key("main.notebook", "Space")
            time.sleep(0.5)
            check_equal(transmitting(app), False, "Transmitting after pressing a key that isn't the PTT key")

            app.key("main.notebook", "F5", hold=True)
            wait_for_transmit(app)
            time.sleep(1)
            check_equal(transmitting(app), True, "Transmitting while the PTT key is held")
            app.key("main.notebook", "F5", release=True)
            wait_for_transmit(app, False)

            stop_modem(app)
            quit_and_check_log(app)

        # No key at all.
        with freedvtest.start(settings=settings(Rig__EnableSpacebarForPTT=False)) as app:
            app.wait_for_window("main")
            start_modem(app)
            app.activate("main")
            app.key("main.notebook", "Space")
            time.sleep(1)
            check_equal(transmitting(app), False, "Transmitting with the PTT key turned off")
            stop_modem(app)
            quit_and_check_log(app)


def test_tx_level():
    """TX attenuation and Tune."""
    def tx_level(app):
        return label(app, "main.textTxLevel")

    with audio_devices():
        with freedvtest.start(settings=settings(Reporting__Frequency=14236000)) as app:
            app.wait_for_window("main")
            check_equal(label(app, "main.boxTxLevel"), "TX Attenuation", "Level box's title")
            check_equal(tx_level(app), "0.0dB", "TX attenuation")

            # Small and large steps down and up, stopping at either end.
            for button, expected in [("Incr", "0.0dB"), ("Decr", "-0.2dB"), ("DecrBig", "-1.2dB"),
                                     ("Incr", "-1.0dB"), ("IncrBig", "0.0dB"), ("IncrBig", "0.0dB")]:
                app.click("main.btnTxLevel" + button)
                check_equal(tx_level(app), expected, "TX attenuation after %s" % button)
            for _ in range(31):
                app.click("main.btnTxLevelDecrBig")
            check_equal(tx_level(app), "-30.0dB", "TX attenuation at its lowest")
            for _ in range(24):
                app.click("main.btnTxLevelIncrBig")
            check_equal(tx_level(app), "-6.0dB", "TX attenuation")

            # Tune sends a tone to the radio at a level of its own.
            start_modem(app)
            app.click("main.btnTune")
            check_equal(label(app, "main.boxTxLevel"), "Tune Attenuation", "Level box's title while tuning")
            check_equal(tx_level(app), "0.0dB", "Tune attenuation")
            for control in ["main.btnPTT", "main.btnVoiceKeyer", "main.cboReportFrequency"]:
                check_equal(app.get(control)["enabled"], False, control + " enabled while tuning")
            full_level = level(RADIO_TX_MONITOR)
            check(full_level > AUDIBLE, "No tone is being sent to the radio (level %d)" % full_level)

            app.click("main.btnTxLevelDecrBig")
            app.click("main.btnTxLevelDecrBig")
            check_equal(tx_level(app), "-2.0dB", "Tune attenuation")

            # Its menu can take it straight to the bottom.
            items = app.context_menu("main.btnTune", choose="Set tune output to minimum (-30 dB)")
            check_equal([item["label"] for item in items],
                        ["Set tune output to minimum (-30 dB)", "Enable auto-save of tune atten for 20m",
                         "Restore tune atten level for 20m"], "Tune button's menu")
            check_equal(tx_level(app), "-30.0dB", "Tune attenuation")
            time.sleep(0.5)
            low_level = level(RADIO_TX_MONITOR)
            check(low_level < full_level / 10, "The tone isn't much quieter at -30 dB (level %d, was %d)"
                  % (low_level, full_level))

            app.click("main.btnTune")
            check_equal(label(app, "main.boxTxLevel"), "TX Attenuation", "Level box's title")
            check_equal(tx_level(app), "-6.0dB", "TX attenuation after tuning")
            for control in ["main.btnPTT", "main.btnVoiceKeyer", "main.cboReportFrequency"]:
                check_equal(app.get(control)["enabled"], True, control + " enabled after tuning")
            time.sleep(1)  # for the last of the tone to be sent
            check_silent(RADIO_TX_MONITOR, "Something is still being sent to the radio")

            # Attenuation can be kept separately for each band, with a way
            # back to what it was when the band was last switched to.
            items = app.context_menu("main.textTxLevel", choose="Enable auto-save of TX atten for 20m")
            check_equal([(item["label"], item["enabled"]) for item in items],
                        [("Enable auto-save of TX atten for 20m", True), ("Restore TX atten level for 20m", False)],
                        "TX attenuation's menu")
            app.click("main.btnTxLevelDecrBig")
            check_equal(tx_level(app), "-7.0dB", "TX attenuation")
            items = app.context_menu("main.textTxLevel", choose="Restore TX atten level for 20m")
            check_equal([(item["label"], item["enabled"]) for item in items],
                        [("Disable auto-save of TX atten for 20m", True), ("Restore TX atten level for 20m", True)],
                        "TX attenuation's menu")
            check_equal(tx_level(app), "-6.0dB", "TX attenuation after restoring")
            app.context_menu("main.textTxLevel", choose="Disable auto-save of TX atten for 20m")
            items = app.context_menu("main.textTxLevel", choose="Enable auto-save of TX atten for 20m")
            check_equal([(item["label"], item["enabled"]) for item in items],
                        [("Enable auto-save of TX atten for 20m", True), ("Restore TX atten level for 20m", False)],
                        "TX attenuation's menu after turning auto-save off")

            stop_modem(app)
            quit_and_check_log(app)
            saved = freedvtest.read_config(app.config_path)
            check_equal(saved.get("/Audio/transmitLevelByBand/20m"), "-60", "Saved TX attenuation for 20m")
            check_equal((saved.get("/Audio/transmitLevel"), saved.get("/Audio/tuneLevel")), ("-60", "-300"),
                        "Saved TX and tune attenuation")


def test_receive():
    """What's displayed while receiving a signal. FreeDV receives its own
    transmission here: full duplex, with what it sends to the radio fed back
    into what it hears from it."""
    reporting = dict(Reporting__Enable=True, Reporting__Callsign=CALLSIGN, Reporting__GridSquare="DM12")
    with audio_devices(), Loopback(RADIO_TX, RADIO_RX):
        with freedvtest.start(settings=settings(Rig__HalfDuplex=False, Debug__showDecodeStats=True,
                                                **reporting)) as app:
            app.wait_for_window("main")
            check_equal(label(app, "main.textSNR"), "--", "SNR before starting")
            check_equal(label(app, "main.textCurrentDecodeMode"), "Mode: unk", "Mode before starting")
            check_equal(app.get("main.cboLastReportedCallsigns")["value"], "", "Last callsign received")

            start_modem(app)
            check_equal(app.state()["sync"], 0, "Sync with nothing being received")

            app.click("main.btnPTT")
            wait_for_transmit(app)
            app.wait_until(lambda: app.state()["sync"] == 1, timeout=20, description="FreeDV to sync")
            def snr():
                text = label(app, "main.textSNR")
                return int(text[:-2]) if re.match(r"^-?\d+dB$", text) else None

            try:
                app.wait_until(lambda: snr() is not None and snr() > 10, timeout=15)
            except freedvtest.FreeDVError:
                check(False, "SNR of a clean signal is " + label(app, "main.textSNR"))
            check(app.get("main.gaugeSNR")["value"] > 0, "The SNR gauge isn't showing anything")
            check_equal(label(app, "main.textCurrentDecodeMode"), "Mode: RADEV1", "Mode while receiving")
            time.sleep(3)
            check_equal(app.state()["sync"], 1, "Sync a few seconds on")

            # Controls that go with receiving.
            app.set_value("main.ckboxSNR", True)
            app.click("main.btnCenterRx")
            app.click("main.btnBerReset")

            # The callsign arrives as the transmission ends.
            app.click("main.btnPTT")
            wait_for_transmit(app, False)
            app.wait_until(lambda: app.get("main.cboLastReportedCallsigns")["value"] == CALLSIGN, timeout=15,
                           description="the callsign to be received")
            app.wait_until(lambda: app.state()["sync"] == 0, timeout=15, description="sync to be lost")

            app.click("main.btnCallSignReset")
            check_equal(app.get("main.cboLastReportedCallsigns")["value"], "", "Last callsign after clearing")

            stop_modem(app)
            quit_and_check_log(app)
            check_equal(freedvtest.read_config(app.config_path).get("/Audio/snrSlow"), "1", "Saved slow SNR setting")


def test_analog():
    """Analog mode, and the speaker level."""
    with audio_devices():
        with freedvtest.start(settings=settings()) as app:
            app.wait_for_window("main")
            check_equal(app.get("main.sliderMicSpkrLevel")["enabled"], False, "Speaker level enabled before starting")
            start_modem(app)
            check_equal(label(app, "main.btnAnalog"), "Switch to Analog", "Analog button's label")
            check_equal(app.state()["analog"], False, "Analog mode")
            check_equal(app.get("main.sliderMicSpkrLevel")["enabled"], True, "Speaker level enabled")

            with Tone(RADIO_RX):
                # A tone isn't a FreeDV signal, so there's nothing to hear...
                check_silent(SPEAKER_MONITOR, "Something that isn't FreeDV is being played through the speaker")

                # ...until it's passed straight through.
                app.click("main.btnAnalog")
                check_equal(label(app, "main.btnAnalog"), "Switch to Digital", "Analog button's label")
                check_equal(app.state()["analog"], True, "Analog mode")
                loud = level(SPEAKER_MONITOR)
                check(loud > AUDIBLE, "The radio isn't being played through the speaker (level %d)" % loud)

                # Speaker level
                check_equal(label(app, "main.textMicSpkrLevel"), "0.0dB", "Speaker level")
                app.set_value("main.sliderMicSpkrLevel", -200)
                check_equal(label(app, "main.textMicSpkrLevel"), "-20.0dB", "Speaker level")
                time.sleep(0.5)
                quiet = level(SPEAKER_MONITOR)
                check(SILENT < quiet < loud / 5, "Turning the speaker down 20 dB took it from %d to %d" % (loud, quiet))
                app.set_value("main.sliderMicSpkrLevel", 0)
                check_equal(label(app, "main.textMicSpkrLevel"), "0.0dB", "Speaker level")

            # Transmitting in analog mode sends the microphone to the radio.
            app.click("main.btnPTT")
            wait_for_transmit(app)
            check_silent(RADIO_TX_MONITOR, "Something is being sent to the radio with nothing said")
            with Tone(MICROPHONE):
                check_audible(RADIO_TX_MONITOR, "The microphone isn't being sent to the radio")
            app.click("main.btnPTT")
            wait_for_transmit(app, False)

            app.click("main.btnAnalog")
            check_equal(label(app, "main.btnAnalog"), "Switch to Analog", "Analog button's label")
            check_equal(app.state()["analog"], False, "Analog mode")
            with Tone(RADIO_RX):
                check_silent(SPEAKER_MONITOR, "The radio is still being played through the speaker")

            stop_modem(app)
            quit_and_check_log(app)


def test_play_file():
    """Tools > Play File - From Radio, which stands a file in for the radio."""
    start_item = "Tools/Start Play File - From Radio..."
    stop_item = "Tools/Stop Play File - From Radio..."
    folder = tempfile.mkdtemp(prefix="freedvtest-files-")
    try:
        tone = os.path.join(folder, "tone.wav")
        stereo = os.path.join(folder, "stereo.wav")
        garbage = os.path.join(folder, "garbage.wav")
        write_tone(tone, seconds=3, rate=8000)
        write_tone(stereo, seconds=1, rate=8000, channels=2)
        with open(garbage, "w") as f:
            f.write("This is not a sound file.\n")

        with audio_devices():
            with freedvtest.start(settings=settings()) as app:
                app.wait_for_window("main")
                start_modem(app)
                # Analog mode, so that what's in the file can be heard.
                app.click("main.btnAnalog")
                check_silent(SPEAKER_MONITOR, "Something is being played through the speaker")

                # Changing one's mind
                app.menu(start_item)
                check_equal(app.path_dialogs(clear=True), ["Play File - From Radio"], "File choosers displayed")
                check_equal(app.get("main")["status"], "", "Status after cancelling")

                # Files that can't be played
                for path, caption in [(garbage, "Couldn't open sound file"), (stereo, "Too Many Channels")]:
                    app.choose_next_path(path)
                    app.menu(start_item)
                    check_equal([box["caption"] for box in app.message_boxes(clear=True)], [caption],
                                "Message displayed for %s" % os.path.basename(path))
                    check_equal(app.get("main")["status"], "", "Status after a file couldn't be played")

                # One that can
                app.choose_next_path(tone)
                app.menu(start_item)
                check_equal(app.get("main")["status"], "Playing file %s as radio input" % tone, "Status while playing")
                check_audible(SPEAKER_MONITOR, "The file isn't being played")

                # ...which stops by itself when it runs out,
                app.wait_until(lambda: app.get("main")["status"] == "", timeout=15, description="the file to finish")
                time.sleep(1.5)  # for the last of it to be played
                check_silent(SPEAKER_MONITOR, "Something is still being played through the speaker")

                # or when it's told to.
                app.choose_next_path(tone)
                app.menu(start_item)
                check_audible(SPEAKER_MONITOR, "The file isn't being played")
                app.menu(stop_item)
                check_equal(app.get("main")["status"], "", "Status after stopping")
                time.sleep(1.5)
                check_silent(SPEAKER_MONITOR, "Something is still being played through the speaker")
                app.menu(start_item)  # back to its original name; cancelled this time

                stop_modem(app)
                quit_and_check_log(app)
    finally:
        shutil.rmtree(folder, ignore_errors=True)


def count_transmissions(app, seconds):
    """Returns how many times FreeDV starts transmitting in the given time."""
    count = 0
    was_transmitting = False
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        now_transmitting = transmitting(app)
        if now_transmitting and not was_transmitting:
            count += 1
        was_transmitting = now_transmitting
        time.sleep(0.1)
    return count


def test_voice_keyer():
    """The voice keyer: transmitting a recording, with pauses to listen."""
    folder = tempfile.mkdtemp(prefix="freedvtest-files-")
    try:
        write_tone(os.path.join(folder, "cq.wav"), seconds=3, rate=16000)
        keyer = dict(VoiceKeyer__WaveFilePath=folder, VoiceKeyer__WaveFile="cq.wav", VoiceKeyer__Repeats=1,
                     VoiceKeyer__RxPause=2)

        with audio_devices():
            with freedvtest.start(settings=settings(**keyer)) as app:
                app.wait_for_window("main")
                check_equal(label(app, "main.btnVoiceKeyer"), "Start Voice Keyer\ncq", "Voice keyer button's label")
                start_modem(app)
                # Analog mode, so that what's sent to the radio is what's in the file.
                app.click("main.btnAnalog")

                def check_idle():
                    check_equal(transmitting(app), False, "Transmitting")
                    check_equal(app.get("main.btnVoiceKeyer")["value"], False, "Voice keyer button pressed")
                    check_equal(label(app, "main.btnVoiceKeyer"), "Start Voice Keyer\ncq", "Voice keyer button's label")
                    check_equal(app.get("main")["status"], "", "Status")

                # The file is transmitted, then again after a pause.
                app.click("main.btnVoiceKeyer")
                wait_for_transmit(app)
                check_equal(app.get("main.btnVoiceKeyer")["value"], True, "Voice keyer button pressed")
                check_equal(label(app, "main.btnVoiceKeyer"), "Stop Voice Keyer\ncq", "Voice keyer button's label")
                check_equal(app.get("main.btnPTT")["value"], True, "PTT button pressed")
                check_equal(app.get("main")["status"],
                            "Voice Keyer: Playing file %s to mic input" % os.path.join(folder, "cq.wav"), "Status")
                check_audible(RADIO_TX_MONITOR, "The file isn't being sent to the radio")
                wait_for_transmit(app, False)
                check_equal(app.get("main.btnVoiceKeyer")["value"], True, "Voice keyer button pressed during the pause")
                check_equal(count_transmissions(app, 5), 1, "Transmissions after the first")
                app.wait_until(lambda: not transmitting(app) and not app.get("main.btnVoiceKeyer")["value"],
                               timeout=15, description="the voice keyer to finish")
                app.wait_until(lambda: app.get("main.btnOnOff")["enabled"], description="transmit to finish ending")
                check_idle()
                check_equal(count_transmissions(app, 4), 0, "Transmissions after the voice keyer finished")

                # It can be stopped part of the way through with its own
                # button, the XMIT button or the PTT key.
                for stop in [lambda: app.click("main.btnVoiceKeyer"),
                             lambda: app.click("main.btnPTT"),
                             lambda: app.key("main.notebook", "Space")]:
                    app.activate("main")
                    app.click("main.btnVoiceKeyer")
                    wait_for_transmit(app)
                    time.sleep(0.5)
                    stop()
                    wait_for_transmit(app, False)
                    check_idle()
                    check_equal(count_transmissions(app, 4), 0, "Transmissions after the voice keyer was stopped")

                stop_modem(app)
                quit_and_check_log(app)
    finally:
        shutil.rmtree(folder, ignore_errors=True)


def test_voice_keyer_files():
    """Choosing and recording the voice keyer's file."""
    folder = tempfile.mkdtemp(prefix="freedvtest-files-")
    try:
        good = os.path.join(folder, "good.wav")
        low_rate = os.path.join(folder, "low_rate.wav")
        stereo = os.path.join(folder, "stereo.wav")
        recorded = os.path.join(folder, "recorded.wav")
        write_tone(good, seconds=2, rate=16000)
        write_tone(low_rate, seconds=1, rate=4000)
        write_tone(stereo, seconds=1, rate=16000, channels=2)

        with audio_devices():
            # No file's been chosen yet (the default one doesn't exist).
            with freedvtest.start(settings=settings()) as app:
                app.wait_for_window("main")
                start_modem(app)

                # So the voice keyer asks for one, and does nothing without.
                app.click("main.btnVoiceKeyer")
                check_equal(app.path_dialogs(clear=True), ["Select Voice Keyer File"], "File choosers displayed")
                check_equal(transmitting(app), False, "Transmitting without a file")
                check_equal(app.get("main.btnVoiceKeyer")["value"], False, "Voice keyer button pressed")

                # Asked again, it's given a file that it can't use. That's
                # still the file chosen, so it isn't asked for again.
                app.choose_next_path(low_rate)
                for attempt in range(2):
                    app.click("main.btnVoiceKeyer")
                    check_equal([box["caption"] for box in app.message_boxes(clear=True)], ["Sample Rate Too Low"],
                                "Message displayed for low_rate.wav")
                    check_equal(transmitting(app), False, "Transmitting a file that can't be used")
                    check_equal(app.get("main.btnVoiceKeyer")["value"], False, "Voice keyer button pressed")
                check_equal(app.path_dialogs(clear=True), ["Select Voice Keyer File"], "File choosers displayed")
                check_equal(label(app, "main.btnVoiceKeyer"), "Start Voice Keyer\nlow_rate",
                            "Voice keyer button's label")

                # Another that it can't use, chosen from its menu this time.
                app.choose_next_path(stereo)
                app.context_menu("main.btnVoiceKeyer", choose="Use another voice keyer file...")
                app.click("main.btnVoiceKeyer")
                check_equal([box["caption"] for box in app.message_boxes(clear=True)], ["Too Many Channels"],
                            "Message displayed for stereo.wav")
                check_equal(transmitting(app), False, "Transmitting a file that can't be used")
                app.path_dialogs(clear=True)

                # Its menu
                items = app.context_menu("main.btnVoiceKeyer")
                check_equal([(item["label"], item["enabled"], item["checked"]) for item in items],
                            [("Use another voice keyer file...", True, False),
                             ("Record new voice keyer file...", True, False),
                             ("Monitor transmitted audio", True, False), ("Adjust Monitor Volume...", False, False)],
                            "Voice keyer button's menu")

                # Using another file
                app.choose_next_path(good)
                app.context_menu("main.btnVoiceKeyer", choose="Use another voice keyer file...")
                check_equal(label(app, "main.btnVoiceKeyer"), "Start Voice Keyer\ngood", "Voice keyer button's label")
                app.click("main.btnVoiceKeyer")
                wait_for_transmit(app)
                app.click("main.btnVoiceKeyer")
                wait_for_transmit(app, False)

                # Recording a new one from the microphone, which carries on
                # until the button's pressed.
                app.choose_next_path(recorded)
                with Tone(MICROPHONE):
                    app.context_menu("main.btnVoiceKeyer", choose="Record new voice keyer file...")
                    check_equal(app.get("main")["status"], "Recording file %s from microphone" % recorded, "Status")
                    check_equal(app.get("main.btnVoiceKeyer")["value"], True, "Voice keyer button pressed")
                    check_equal(label(app, "main.btnVoiceKeyer"), "Start Voice Keyer\nrecorded",
                                "Voice keyer button's label")
                    check_equal(app.get("main.btnAnalog")["enabled"], False, "Analog button enabled while recording")
                    check_equal(app.get("main.notebook")["value"], TABS.index("Frm Mic"), "Tab displayed")
                    check_equal(transmitting(app), False, "Transmitting while recording")
                    time.sleep(2)
                    app.click("main.btnVoiceKeyer")
                check_equal(app.get("main")["status"], "", "Status after recording")
                check_equal(app.get("main.btnVoiceKeyer")["value"], False, "Voice keyer button pressed")
                check_equal(app.get("main.btnAnalog")["enabled"], True, "Analog button enabled after recording")
                check_equal(transmitting(app), False, "Transmitting after recording")
                recorded_level, recorded_seconds = file_level(recorded)
                check(recorded_level > AUDIBLE and recorded_seconds > 1,
                      "The recording is %.1f s at level %d" % (recorded_seconds, recorded_level))

                # ...and is then what's transmitted.
                app.click("main.btnAnalog")
                app.click("main.btnVoiceKeyer")
                wait_for_transmit(app)
                check_audible(RADIO_TX_MONITOR, "The recording isn't being sent to the radio")
                app.click("main.btnVoiceKeyer")
                wait_for_transmit(app, False)

                stop_modem(app)
                quit_and_check_log(app)
                saved = freedvtest.read_config(app.config_path)
                check_equal((saved.get("/VoiceKeyer/WaveFilePath"), saved.get("/VoiceKeyer/WaveFile")),
                            (folder, "recorded.wav"), "Saved voice keyer file")
    finally:
        shutil.rmtree(folder, ignore_errors=True)


def test_recording():
    """The Record button."""
    folder = tempfile.mkdtemp(prefix="freedvtest-files-")
    try:
        raw_folder = os.path.join(folder, "raw")
        decoded_folder = os.path.join(folder, "decoded")
        os.mkdir(raw_folder)
        os.mkdir(decoded_folder)

        def recordings():
            return (sorted(os.listdir(raw_folder)), sorted(os.listdir(decoded_folder)))

        paths = dict(QuickRecord__SavePath=raw_folder, QuickRecord__SaveDecodedPath=decoded_folder)
        with audio_devices():
            with freedvtest.start(settings=settings(**paths)) as app:
                app.wait_for_window("main")
                start_modem(app)

                # Changing one's mind
                app.click("main.btnRecord")
                dialog = app.wait_for_window("beginRecording")
                check_equal(dialog["label"], "Start Recording", "Window title")
                check_equal(app.get("beginRecording.rawRecording")["value"], True, "Off Air chosen")
                check_equal(app.get("beginRecording.formatWav")["value"], True, "WAV chosen")
                check_equal(app.get("beginRecording.formatMp3")["enabled"], False, "MP3 available off air")
                check_equal(app.get("beginRecording.recordingSuffix")["value"], "", "Recording suffix")
                app.click("beginRecording.btnCancel")
                app.wait_for_window_closed("beginRecording")
                check_equal(app.get("main.btnRecord")["value"], False, "Record button pressed after cancelling")
                check_equal(recordings(), ([], []), "Recordings after cancelling")

                # What's coming from the radio
                with Tone(RADIO_RX):
                    app.click("main.btnRecord")
                    app.wait_for_window("beginRecording")
                    app.set_value("beginRecording.recordingSuffix", "K1ABC")
                    app.click("beginRecording.btnStart")
                    app.wait_for_window_closed("beginRecording")
                    check_equal(app.get("main.btnRecord")["value"], True, "Record button pressed")
                    raw, decoded = recordings()
                    check(len(raw) == 1 and re.match(r"^FDV_FromRadio_\d{8}-\d{6}_K1ABC\.wav$", raw[0]),
                          "Off air recordings are %r" % raw)
                    check_equal(decoded, [], "Decoded recordings")
                    check_equal(app.get("main")["status"],
                                "Recording file %s from radio" % os.path.join(raw_folder, raw[0]), "Status")
                    time.sleep(3)
                    app.click("main.btnRecord")
                check_equal(app.get("main.btnRecord")["value"], False, "Record button pressed after stopping")
                check_equal(app.get("main")["status"], "", "Status after stopping")
                recorded_level, recorded_seconds = file_level(os.path.join(raw_folder, raw[0]))
                check(recorded_level > AUDIBLE and recorded_seconds > 2,
                      "The off air recording is %.1f s at level %d" % (recorded_seconds, recorded_level))

                # Both that and what's decoded from it (which here is nothing).
                time.sleep(1.1)  # for a different time in the file's name
                app.click("main.btnRecord")
                app.wait_for_window("beginRecording")
                app.click("beginRecording.bothRecording")
                check_equal(app.get("beginRecording.formatMp3")["enabled"], False, "MP3 available for both")
                app.click("beginRecording.btnStart")
                app.wait_for_window_closed("beginRecording")
                time.sleep(2)
                app.click("main.btnRecord")
                raw, decoded = recordings()
                check(len(raw) == 2 and len(decoded) == 1 and
                      re.match(r"^FDV_FromDecoder_\d{8}-\d{6}\.wav$", decoded[0]),
                      "Recordings are %r and %r" % (raw, decoded))
                check_equal(app.get("main.btnRecord")["value"], False, "Record button pressed after stopping")

                # Recording stops with FreeDV.
                app.click("main.btnRecord")
                app.wait_for_window("beginRecording")
                app.click("beginRecording.btnStart")
                app.wait_for_window_closed("beginRecording")
                check_equal(app.get("main.btnRecord")["value"], True, "Record button pressed")

                stop_modem(app)
                quit_and_check_log(app)
                for path in glob.glob(os.path.join(raw_folder, "*.wav")):
                    file_level(path)  # all of them are sound files
    finally:
        shutil.rmtree(folder, ignore_errors=True)


def test_recording_options():
    """The rest of what can be chosen when starting a recording."""
    folder = tempfile.mkdtemp(prefix="freedvtest-files-")
    try:
        raw_folder = os.path.join(folder, "raw")
        decoded_folder = os.path.join(folder, "decoded")
        os.mkdir(raw_folder)
        os.mkdir(decoded_folder)

        def recordings():
            return (sorted(os.listdir(raw_folder)), sorted(os.listdir(decoded_folder)))

        # Someone that's been heard before, for FreeDV to list.
        csv_log = os.path.join(folder, "heard.csv")
        with open(csv_log, "w") as f:
            f.write("date,time,callsign,mode,frequency_hz,snr_db\n")
            f.write("2026-03-04,05:06:07,K1ABC/P,RADEV1,14236000,7\n")

        extra = dict(QuickRecord__SavePath=raw_folder, QuickRecord__SaveDecodedPath=decoded_folder,
                     Reporting__Enable=True, Reporting__Callsign=CALLSIGN, Reporting__GridSquare="DM12",
                     Reporting__CSV__LogFilePath=csv_log)
        with audio_devices():
            with freedvtest.start(settings=settings(**extra)) as app:
                app.wait_for_window("main")
                start_modem(app)

                # With someone chosen, the recording is named after them, as
                # far as a file's name allows.
                app.combo_popup("main.cboLastReportedCallsigns", open=True)
                app.wait_until(lambda: app.get("main.listLastReportedCallsigns")["shown"])
                app.click("main.listLastReportedCallsigns", item=0)
                app.wait_until(lambda: not app.get("main.listLastReportedCallsigns")["shown"])
                check_equal(app.get("main.cboLastReportedCallsigns")["value"], "K1ABC/P", "Callsign chosen")

                app.click("main.btnRecord")
                app.wait_for_window("beginRecording")
                check_equal(app.get("beginRecording.recordingSuffix")["value"], "K1ABC_P", "Recording suffix")

                # MP3 is only for what's decoded.
                mp3 = app.get("beginRecording.formatMp3")["shown"]
                app.click("beginRecording.decodedRecording")
                if mp3:
                    check_equal(app.get("beginRecording.formatMp3")["enabled"], True, "MP3 available for decoded")
                    app.click("beginRecording.formatMp3")
                    app.click("beginRecording.rawRecording")
                    check_equal((app.get("beginRecording.formatMp3")["enabled"],
                                 app.get("beginRecording.formatWav")["value"]), (False, True),
                                "MP3 available and WAV chosen off air")
                    app.click("beginRecording.decodedRecording")
                    app.click("beginRecording.formatMp3")
                extension = "mp3" if mp3 else "wav"

                app.click("beginRecording.btnStart")
                app.wait_for_window_closed("beginRecording")
                check_equal(app.message_boxes(), [], "Errors displayed")
                raw, decoded = recordings()
                check(raw == [] and len(decoded) == 1 and
                      re.match(r"^FDV_FromDecoder_\d{8}-\d{6}_K1ABC_P\.%s$" % extension, decoded[0]),
                      "Recordings are %r and %r" % (raw, decoded))
                check_equal(app.get("main")["status"],
                            "Recording file %s from decoder" % os.path.join(decoded_folder, decoded[0]), "Status")
                time.sleep(2)
                app.click("main.btnRecord")
                check_equal(app.get("main.btnRecord")["value"], False, "Record button pressed after stopping")
                check_equal(app.get("main")["status"], "", "Status after stopping")
                check(os.path.getsize(os.path.join(decoded_folder, decoded[0])) >= 0, "The recording has gone")

                # Whatever ends up in the suffix, the recording stays where
                # recordings go.
                time.sleep(1.1)  # for a different time in the file's name
                app.click("main.btnRecord")
                app.wait_for_window("beginRecording")
                app.set_value("beginRecording.recordingSuffix", "../up one/K1ABC")
                app.click("beginRecording.btnStart")
                app.wait_for_window_closed("beginRecording")
                check_equal(app.message_boxes(), [], "Errors displayed")
                check_equal(app.get("main.btnRecord")["value"], True, "Record button pressed")
                raw, decoded = recordings()
                check(len(raw) == 1 and re.match(r"^FDV_FromRadio_\d{8}-\d{6}____up_one_K1ABC\.wav$", raw[0]),
                      "Off air recordings are %r" % raw)
                check_equal(sorted(os.listdir(folder)), ["decoded", "heard.csv", "raw"], "What's been created")
                app.click("main.btnRecord")

                stop_modem(app)
                quit_and_check_log(app)
    finally:
        shutil.rmtree(folder, ignore_errors=True)


def test_time_out_timer():
    """The time-out timer, which stops a transmission that's gone on too long."""
    with audio_devices():
        # A warning appears when there's 15 seconds left.
        with freedvtest.start(settings=settings(Rig__TotTimerSecs=18)) as app:
            app.wait_for_window("main")
            start_modem(app)

            app.click("main.btnPTT")
            wait_for_transmit(app)
            check(not app.window_exists("totWarning"), "The time-out warning appeared straight away")
            warning = app.wait_for_window("totWarning", timeout=10)
            check_equal(warning["label"], "Time-Out Timer Warning", "Warning's title")
            check(re.match(r"^1[0-5] seconds remaining$", label(app, "totWarning.countdownText")),
                  "Countdown is %r" % label(app, "totWarning.countdownText"))
            first = int(label(app, "totWarning.countdownText").split()[0])
            time.sleep(2.5)
            second = int(label(app, "totWarning.countdownText").split()[0])
            check(second < first, "The countdown went from %d to %d" % (first, second))

            # It can be put off...
            app.click("totWarning.btnExtend")
            app.wait_for_window_closed("totWarning")
            check_equal(transmitting(app), True, "Transmitting after resetting the timer")

            # ...in which case it comes around again, and then does stop
            # the transmission.
            app.wait_for_window("totWarning", timeout=10)
            wait_for_transmit(app, False, timeout=25)
            check(not app.window_exists("totWarning"), "The time-out warning is still there")
            check_equal(app.get("main.btnPTT")["value"], False, "PTT button pressed after timing out")

            # Ending the transmission oneself gets rid of the warning too.
            app.click("main.btnPTT")
            wait_for_transmit(app)
            app.wait_for_window("totWarning", timeout=10)
            app.click("main.btnPTT")
            wait_for_transmit(app, False)
            check(not app.window_exists("totWarning"), "The time-out warning is still there")

            stop_modem(app)
            quit_and_check_log(app)

        # It can be turned off.
        with freedvtest.start(settings=settings(Rig__TotTimerSecs=5, Rig__TotTimerEnabled=False)) as app:
            app.wait_for_window("main")
            start_modem(app)
            app.click("main.btnPTT")
            wait_for_transmit(app)
            time.sleep(8)
            check_equal(transmitting(app), True, "Transmitting past a time-out that's turned off")
            check(not app.window_exists("totWarning"), "The time-out warning appeared with the timer turned off")
            app.click("main.btnPTT")
            wait_for_transmit(app, False)
            stop_modem(app)
            quit_and_check_log(app)


def test_menus():
    """The Settings and Help menus, other than what opens other windows.
    (File > Keep On Top isn't offered on Linux.)"""
    folder = tempfile.mkdtemp(prefix="freedvtest-files-")
    try:
        exported = os.path.join(folder, "exported.conf")
        other = os.path.join(folder, "other.conf")
        empty = os.path.join(folder, "empty.conf")
        freedvtest.write_config(other, {"/Reporting/Callsign": "K1ABC", "/Reporting/GridSquare": "FN42"})
        open(empty, "w").close()

        start = {"/Reporting/Callsign": CALLSIGN, "/Reporting/GridSquare": "DM12",
                 "/Reporting/FreeDV/Hostname": NOWHERE, "/Reporting/FreeDV/UseTls": False}
        with freedvtest.start(settings=start) as app:
            app.wait_for_window("main")
            title = app.get("main")["label"]
            check(title.startswith("FreeDV ") and "(freedv.conf)" in title, "Window title is %r" % title)

            def callsign():
                return app.state()["config"]["reporting"]["reportingCallsign"]

            # Help > About
            app.menu("Help/About FreeDV")
            boxes = app.message_boxes(clear=True)
            check_equal([box["caption"] for box in boxes], ["About"], "Message displayed")
            check(boxes[0]["message"].startswith("FreeDV GUI ") and "freedv-gui git hash:" in boxes[0]["message"],
                  "About says %r" % boxes[0]["message"])

            # Settings > Export Configuration
            app.menu("Settings/Export Configuration...")
            check_equal(app.path_dialogs(clear=True), ["Export FreeDV Configuration"], "File choosers displayed")
            check(not os.path.exists(exported), "Something was exported after cancelling")
            app.choose_next_path(exported)
            app.menu("Settings/Export Configuration...")
            app.wait_until(lambda: os.path.exists(exported), timeout=5, description="the configuration to be exported")
            check_equal(freedvtest.read_config(exported).get("/Reporting/Callsign"), CALLSIGN, "Exported callsign")

            # Settings > Use Configuration
            app.menu("Settings/Use Configuration...")
            check_equal(app.path_dialogs(clear=True), ["Export FreeDV Configuration", "Import FreeDV Configuration"],
                        "File choosers displayed")
            check_equal(callsign(), CALLSIGN, "Callsign after cancelling")

            for path, message in [(os.path.join(folder, "missing.conf"), "The selected file does not exist."),
                                  (empty, "The selected file could not be parsed as a FreeDV configuration.")]:
                app.choose_next_path(path)
                app.menu("Settings/Use Configuration...")
                check_equal([box["message"] for box in app.message_boxes(clear=True)], [message],
                            "Message displayed for %s" % os.path.basename(path))
                check_equal(callsign(), CALLSIGN, "Callsign after a file that couldn't be used")
                check_equal(app.get("main")["label"], title, "Window title")

            app.choose_next_path(other)
            app.menu("Settings/Use Configuration...")
            check_equal(app.message_boxes(), [], "Messages displayed")
            check_equal(callsign(), "K1ABC", "Callsign after using another configuration")
            check("(other.conf)" in app.get("main")["label"], "Window title is %r" % app.get("main")["label"])

            # Settings > Load Default Configuration, which asks first.
            app.answer_next_message_box("no")
            app.menu("Settings/Load Default Configuration")
            check_equal([box["message"] for box in app.message_boxes(clear=True)],
                        ["This will load the default FreeDV configuration. Are you sure?"], "Message displayed")
            check_equal(callsign(), "K1ABC", "Callsign after declining")

            app.answer_next_message_box("yes")
            app.menu("Settings/Load Default Configuration")
            app.message_boxes(clear=True)
            check_equal(callsign(), "", "Callsign after loading the default configuration")
            check(".conf)" not in app.get("main")["label"], "Window title is %r" % app.get("main")["label"])

            check_equal(app.quit(), 0, "FreeDV's exit code")

        # What was in use before is left as it was.
        check_equal(freedvtest.read_config(other).get("/Reporting/Callsign"), "K1ABC", "Callsign in other.conf")
    finally:
        shutil.rmtree(folder, ignore_errors=True)


SCENARIOS = {
    "transmit": test_transmit,
    "ptt_key": test_ptt_key,
    "tx_level": test_tx_level,
    "receive": test_receive,
    "analog": test_analog,
    "play_file": test_play_file,
    "voice_keyer": test_voice_keyer,
    "voice_keyer_files": test_voice_keyer_files,
    "recording": test_recording,
    "recording_options": test_recording_options,
    "time_out_timer": test_time_out_timer,
    "menus": test_menus,
}


def main():
    names = sys.argv[1:] or list(SCENARIOS)
    for scenario in names:
        if scenario not in SCENARIOS:
            sys.exit("Unknown scenario '%s'. Scenarios: %s" % (scenario, ", ".join(SCENARIOS)))
    for scenario in names:
        print("Running %s..." % scenario, flush=True)
        SCENARIOS[scenario]()
    print("PASS")


if __name__ == "__main__":
    main()
