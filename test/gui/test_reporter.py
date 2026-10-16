#!/usr/bin/env python3
"""Tests FreeDV's FreeDV Reporter window.

Each scenario below launches its own copy of FreeDV. With no arguments all
of them are run; otherwise, only those named:

    python3 test_reporter.py [scenario...]

All but "live_view" use a FreeDV Reporter server run on this machine (see
freedvtest/reporter.py for what that needs), so that what's in the window is
known and nothing is reported publicly. "live_view" only looks at the public
server, and is skipped (exit code 77) if that can't be reached.

Run from the build folder (or set FREEDV_BINARY) with a window manager
running, as the list in the window only reacts to the mouse when the window
is active. Linux or macOS: see freedvtest/audio.py for the audio devices it
needs.
"""

import os
import random
import shutil
import socket
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import freedvtest
import freedvtest.audio
from freedvtest.audio import VirtualAudioDevices, device
from freedvtest.reporter import ReporterServer

LIST = "reporter.listSpots"

# The list's columns, by ID.
(CALLSIGN, LOCATOR, DISTANCE, HEADING, VERSION, FREQUENCY, TX_MODE, STATUS, MESSAGE, LAST_TX, RX_CALLSIGN, SNR,
 LAST_UPDATE) = range(13)

# Who FreeDV reports as. (Not the callsign used by FreeDV's other tests, as
# the server keeps that one to itself.)
CALLSIGN_OF_FREEDV = "AA1ZZZ"
GRID_SQUARE_OF_FREEDV = "DM12"
FREQUENCY_OF_FREEDV = 14236000

# Default colors of rows that have something going on.
TX_COLOR = "#FC4500"
RX_COLOR = "#379BAF"
MESSAGE_COLOR = "#E58BE5"

RADIO_RX = device("FreeDVGuiTest_Radio_To_Computer")
RADIO_TX = device("FreeDVGuiTest_Computer_To_Radio")
MICROPHONE = device("FreeDVGuiTest_Microphone_To_Computer")
SPEAKER = device("FreeDVGuiTest_Computer_To_Speaker")

SKIPPED = 77


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def check_equal(actual, expected, what):
    check(actual == expected, "%s is %r, expected %r" % (what, actual, expected))


def label(app, name):
    return app.get(name)["label"].replace("&", "")


def reporter_settings(hostname, **settings):
    """Returns the settings for FreeDV to report to the given server."""
    result = {
        "/Reporting/Enable": True,
        "/Reporting/Callsign": CALLSIGN_OF_FREEDV,
        "/Reporting/GridSquare": GRID_SQUARE_OF_FREEDV,
        "/Reporting/Frequency": FREQUENCY_OF_FREEDV,
        "/Reporting/ManualFrequencyReporting": True,
        "/Reporting/PSKReporter/Enable": False,
        "/Reporting/FreeDV/Enable": True,
        "/Reporting/FreeDV/Hostname": hostname,
        "/Reporting/FreeDV/UseTls": False,
    }
    if freedvtest.audio.MACOS or freedvtest.audio.WINDOWS:
        # Room for every column: lists there aren't scrolled sideways to
        # show a cell that's out of view, as they are on Linux. (On Windows,
        # the window's otherwise made only tall enough for a row or so.)
        result.update({"/Windows/FreeDVReporter/width": 1500, "/Windows/FreeDVReporter/height": 500})
    result.update(settings)
    return result


def audio_settings():
    """Returns the settings for FreeDV to use the devices created by
    audio_devices(), which it needs to be started."""
    settings = {}
    for card, device in [("soundCard1In", VirtualAudioDevices.input_name(RADIO_RX)), ("soundCard1Out", RADIO_TX),
                         ("soundCard2In", VirtualAudioDevices.input_name(MICROPHONE)), ("soundCard2Out", SPEAKER)]:
        settings["/Audio/%sDeviceName" % card] = device
        settings["/Audio/%sSampleRate" % card] = 48000
    return settings


def audio_devices():
    return VirtualAudioDevices(RADIO_RX, RADIO_TX, MICROPHONE, SPEAKER)


def open_reporter(app):
    """Opens the FreeDV Reporter window and makes it the active one."""
    app.wait_for_window("main")
    if not app.window_exists("reporter"):
        app.menu("Tools/FreeDV Reporter")
    app.wait_for_window("reporter")
    app.activate("reporter")


def start_modem(app):
    app.click("main.btnOnOff")
    app.wait_until(lambda: app.get("main.btnOnOff")["enabled"], description="FreeDV to start")
    check_equal(app.message_boxes(), [], "Errors displayed on start")
    check(app.state()["running"], "FreeDV is not running after pressing Start")
    app.activate("reporter")


def stop_modem(app):
    app.click("main.btnOnOff")
    app.wait_until(lambda: app.get("main.btnOnOff")["enabled"], description="FreeDV to stop")
    check_equal(app.state()["running"], False, "Running after pressing Stop")


def quit_and_check_log(app):
    check_equal(app.message_boxes(), [], "Errors displayed")
    check_equal(app.quit(), 0, "FreeDV's exit code")
    errors = [line for line in app.log().splitlines() if " ERROR " in line or " FATAL " in line]
    check_equal(errors, [], "Errors logged by FreeDV")


def number(text):
    """Returns the number in a cell. Thousands are grouped as the user's
    region has them on macOS (where LC_ALL doesn't apply), e.g. "4,153" in
    the US."""
    return float(text.replace(",", ""))


def callsigns(app):
    """Returns the callsigns in the list, from top to bottom."""
    return [row[CALLSIGN] for row in app.rows(LIST)]


def wait_for_callsigns(app, expected, timeout=15):
    """Waits for the list to consist of exactly the given callsigns, in the
    given order."""
    try:
        app.wait_until(lambda: callsigns(app) == expected, timeout)
    except freedvtest.FreeDVError:
        check_equal(callsigns(app), expected, "Callsigns listed")


def row_of(app, callsign):
    """Returns the position of a callsign's row in the list."""
    listed = callsigns(app)
    check(callsign in listed, "%s is not listed (%s are)" % (callsign, ", ".join(listed) or "none"))
    return listed.index(callsign)


def row_for(app, callsign):
    """Returns a callsign's row in the list."""
    return app.rows(LIST)[row_of(app, callsign)]


def wait_for_cell(app, callsign, column, expected, timeout=15):
    """Waits for a callsign's row to have the given text in a column (or for
    expected(text) to be true, if it's a function)."""
    matches = expected if callable(expected) else (lambda text: text == expected)

    def cell():
        rows = [row for row in app.rows(LIST) if row[CALLSIGN] == callsign]
        return rows[0][column] if rows else None

    try:
        app.wait_until(lambda: cell() is not None and matches(cell()), timeout)
    except freedvtest.FreeDVError:
        check(False, "Column %d for %s is %r%s" % (
            column, callsign, cell(), "" if callable(expected) else ", expected %r" % expected))


def wait_for_color(app, callsign, expected, timeout=15):
    def color():
        return (row_for(app, callsign)["background"] or "").upper()

    try:
        app.wait_until(lambda: (color() == expected) if isinstance(expected, str) else expected(color()), timeout)
    except freedvtest.FreeDVError:
        check(False, "%s's row is colored %s" % (callsign, color()))


def column_titles(app, shown_only=True):
    return [column["title"] for column in app.columns(LIST)
            if column["id"] <= LAST_UPDATE and (column["shown"] or not shown_only)]


def sort_order(app):
    """Returns the ID of the column that the list is sorted by and whether
    that's ascending."""
    for column in app.columns(LIST):
        if column["sort"] != "none":
            return (column["id"], column["sort"] == "ascending")
    return None


# ---------------------------------------------------------------------------
# Scenarios
# ---------------------------------------------------------------------------

def test_live_view():
    """The window can show what's on the public FreeDV Reporter server.
    Nothing is reported to it: FreeDV isn't given a callsign."""
    try:
        socket.create_connection(("qso.freedv.org", 443), timeout=10).close()
    except OSError as e:
        print("Skipping: the public FreeDV Reporter server can't be reached (%s)" % e)
        sys.exit(SKIPPED)

    public_server = {"/Reporting/FreeDV/Hostname": "qso.freedv.org", "/Reporting/FreeDV/UseTls": True}
    with freedvtest.start(settings=public_server) as app:
        open_reporter(app)
        check_equal(app.get("reporter")["label"], "FreeDV Reporter (freedv.conf)", "Window title")
        check_equal(column_titles(app), ["Callsign", "Locator", "km", "Hdg", "Version", "MHz", "Mode", "Status",
                                         "Msg", "Last TX", "RX Call", "SNR", "Last Update"], "Columns")
        check_equal(sort_order(app), (FREQUENCY, True), "Sort order")
        check_equal(app.get("reporter.cbBandFilter")["value"], "All", "Band filter")
        check_equal(label(app, "reporter.stFilterStatus"), "Idle Off", "Idle filter status")

        # Who's on is anyone's guess, but there's rarely nobody at all. Give
        # the list a while to fill, then make sure what's there is sound.
        try:
            app.wait_until(lambda: app.get(LIST)["rowCount"] > 0, timeout=20)
        except freedvtest.FreeDVError:
            print("Note: nobody is on the public FreeDV Reporter server")
        time.sleep(2)
        rows = app.rows(LIST)
        print("%d station(s) listed" % len(rows))
        for row in rows:
            check(row[CALLSIGN] != "", "A row has no callsign: %r" % row)
            check(row[LAST_UPDATE] != "", "%s has no last update time" % row[CALLSIGN])
            # Without a grid square of our own, there are no distances.
            check_equal(row[DISTANCE], "", "%s's distance" % row[CALLSIGN])
        # Lowest frequency first. (Not everyone on there has said what frequency
        # they're on, and the list is changing all the time, so it gets a
        # few goes at being caught in order.)
        for attempt in range(5):
            frequencies = [number(row[FREQUENCY]) for row in app.rows(LIST)
                           if row[FREQUENCY].strip() != ""]
            if frequencies == sorted(frequencies):
                break
            time.sleep(1)
        check_equal(frequencies, sorted(frequencies), "Frequencies listed")

        # With nothing to report as, there's nothing to send.
        if rows:
            app.select_row(LIST, (CALLSIGN, rows[0][CALLSIGN]))
            check_equal(app.get("reporter.btnSendQSY")["enabled"], False, "Request QSY enabled")

        app.click("reporter.btnClose")
        app.wait_for_window_closed("reporter")
        quit_and_check_log(app)


def test_stations():
    """Stations appear in the list, are updated as they do things and
    disappear again."""
    with ReporterServer() as server:
        k1abc = server.station("K1ABC", "FN42")
        k1abc.set_frequency(14236000)
        vk5dgr = server.station("VK5DGR", "PF95", rx_only=True)
        vk5dgr.set_frequency(7177000)

        with freedvtest.start(settings=reporter_settings(server.hostname)) as app:
            open_reporter(app)

            # Those already on, lowest frequency first. (FreeDV itself isn't
            # listed until it's started.)
            wait_for_callsigns(app, ["VK5DGR", "K1ABC"])
            row = row_for(app, "K1ABC")
            check_equal(row[LOCATOR], "FN42", "K1ABC's locator")
            check(4000 < number(row[DISTANCE]) < 4300, "K1ABC is %s km from DM12" % row[DISTANCE])
            check(50 < number(row[HEADING]) < 75, "K1ABC is at %s degrees from DM12" % row[HEADING])
            check_equal(row[VERSION], "freedvtest", "K1ABC's version")
            check_equal(row[FREQUENCY], "14.2360", "K1ABC's frequency")
            check_equal(row[STATUS], "", "K1ABC's status")
            check_equal(row[MESSAGE], "", "K1ABC's message")
            check(row[LAST_UPDATE] != "", "K1ABC has no last update time")
            row = row_for(app, "VK5DGR")
            check_equal(row[STATUS], "RX Only", "VK5DGR's status")
            check(12000 < number(row[DISTANCE]) < 14000, "VK5DGR is %s km from DM12" % row[DISTANCE])

            # Transmitting...
            k1abc.transmit("RADEV1", True)
            wait_for_cell(app, "K1ABC", STATUS, "TX")
            wait_for_cell(app, "K1ABC", TX_MODE, "RADEV1")
            wait_for_color(app, "K1ABC", TX_COLOR)
            wait_for_cell(app, "K1ABC", LAST_TX, lambda text: text != "")

            # ...and being received by someone, which highlights the receiver.
            vk5dgr.receive("K1ABC", "RADEV1", 7)
            wait_for_cell(app, "VK5DGR", RX_CALLSIGN, "K1ABC")
            wait_for_cell(app, "VK5DGR", SNR, lambda text: text.strip() in ("7", "7.0"))
            wait_for_color(app, "VK5DGR", RX_COLOR)

            k1abc.transmit("RADEV1", False)
            wait_for_cell(app, "K1ABC", STATUS, "RX")
            wait_for_color(app, "K1ABC", lambda color: color != TX_COLOR)

            # A status message is highlighted for a few seconds.
            k1abc.set_message("CQ CQ de K1ABC")
            wait_for_cell(app, "K1ABC", MESSAGE, "CQ CQ de K1ABC")
            wait_for_color(app, "K1ABC", MESSAGE_COLOR)
            wait_for_color(app, "K1ABC", lambda color: color != MESSAGE_COLOR, timeout=20)

            # Changing frequency moves a station to where it now belongs.
            k1abc.set_frequency(3625000)
            wait_for_cell(app, "K1ABC", FREQUENCY, "3.6250")
            wait_for_callsigns(app, ["K1ABC", "VK5DGR"])

            # Someone new, with a longer locator than is displayed in full.
            g0abc = server.station("G0ABC", "IO91wm")
            g0abc.set_frequency(14236000)
            wait_for_callsigns(app, ["K1ABC", "VK5DGR", "G0ABC"])
            check_equal(row_for(app, "G0ABC")[LOCATOR], "IO91wm", "G0ABC's locator")

            # Stations can drop out of sight without disconnecting.
            k1abc.hide()
            wait_for_callsigns(app, ["VK5DGR", "G0ABC"])
            k1abc.show()
            wait_for_callsigns(app, ["K1ABC", "VK5DGR", "G0ABC"])
            check_equal(row_for(app, "K1ABC")[MESSAGE], "CQ CQ de K1ABC", "K1ABC's message after reappearing")

            vk5dgr.disconnect()
            wait_for_callsigns(app, ["K1ABC", "G0ABC"])
            g0abc.disconnect()
            k1abc.disconnect()
            wait_for_callsigns(app, [])

            quit_and_check_log(app)


def test_sorting():
    """Clicking on a column's heading sorts the list by it."""
    with ReporterServer() as server:
        # Chosen so that no two columns put them in the same order, and so
        # that sorting numbers as text would get it wrong.
        for callsign, grid_square, frequency in [("K1ABC", "FN42", 14236000), ("VK5DGR", "PF95", 7177000),
                                                 ("W6XYZ", "DM13", 28330000)]:
            server.station(callsign, grid_square).set_frequency(frequency)

        with freedvtest.start(settings=reporter_settings(server.hostname)) as app:
            open_reporter(app)

            check_equal(sort_order(app), (FREQUENCY, True), "Initial sort order")
            wait_for_callsigns(app, ["VK5DGR", "K1ABC", "W6XYZ"])

            app.click_column(LIST, FREQUENCY)
            check_equal(sort_order(app), (FREQUENCY, False), "Sort order")
            wait_for_callsigns(app, ["W6XYZ", "K1ABC", "VK5DGR"])

            # Someone a long way up the dial (here, on a satellite's uplink)
            # still comes after everyone below them.
            with server.station("DL1ABC", "JO62") as satellite:
                satellite.set_frequency(2400050000)
                wait_for_callsigns(app, ["DL1ABC", "W6XYZ", "K1ABC", "VK5DGR"])
                app.click_column(LIST, FREQUENCY)
                wait_for_callsigns(app, ["VK5DGR", "K1ABC", "W6XYZ", "DL1ABC"])
                app.click_column(LIST, FREQUENCY)
            wait_for_callsigns(app, ["W6XYZ", "K1ABC", "VK5DGR"])

            app.click_column(LIST, CALLSIGN)
            check_equal(sort_order(app), (CALLSIGN, True), "Sort order")
            wait_for_callsigns(app, ["K1ABC", "VK5DGR", "W6XYZ"])
            app.click_column(LIST, CALLSIGN)
            check_equal(sort_order(app), (CALLSIGN, False), "Sort order")
            wait_for_callsigns(app, ["W6XYZ", "VK5DGR", "K1ABC"])

            # W6XYZ is about 100 km away, K1ABC 4,000 and VK5DGR 13,000.
            app.click_column(LIST, DISTANCE)
            check_equal(sort_order(app), (DISTANCE, True), "Sort order")
            wait_for_callsigns(app, ["W6XYZ", "K1ABC", "VK5DGR"])
            app.click_column(LIST, DISTANCE)
            wait_for_callsigns(app, ["VK5DGR", "K1ABC", "W6XYZ"])

            # The list stays sorted as stations come and go...
            server.station("JA1XYZ", "PM95").set_frequency(21313000)
            wait_for_callsigns(app, ["VK5DGR", "JA1XYZ", "K1ABC", "W6XYZ"])

            # ...and when the window's closed and opened again.
            app.click("reporter.btnClose")
            app.wait_for_window_closed("reporter")
            open_reporter(app)
            check_equal(sort_order(app), (DISTANCE, False), "Sort order after reopening")
            wait_for_callsigns(app, ["VK5DGR", "JA1XYZ", "K1ABC", "W6XYZ"])

            quit_and_check_log(app)


def test_band_filter():
    """The list can be limited to one band, or to wherever FreeDV is."""
    with ReporterServer() as server:
        for callsign, frequency in [("K1ABC", 14236000), ("W2DEF", 14240000), ("VK5DGR", 7177000),
                                    ("W6XYZ", 28330000), ("N7GHI", 50160000), ("G0ABC", 5403500)]:
            server.station(callsign, "FN42").set_frequency(frequency)
        everyone = ["G0ABC", "VK5DGR", "K1ABC", "W2DEF", "W6XYZ", "N7GHI"]

        with freedvtest.start(settings=reporter_settings(server.hostname)) as app:
            open_reporter(app)
            check_equal(app.get("reporter.cbBandFilter")["value"], "All", "Band filter")
            wait_for_callsigns(app, everyone)

            for band, expected in [("20 m", ["K1ABC", "W2DEF"]), ("40 m", ["VK5DGR"]), ("60 m", ["G0ABC"]),
                                   ("10 m", ["W6XYZ"]), (">= 6 m", ["N7GHI"]), ("80 m", []), ("All", everyone)]:
                app.set_value("reporter.cbBandFilter", band)
                wait_for_callsigns(app, expected)

            # A station that changes band moves in or out of view.
            app.set_value("reporter.cbBandFilter", "40 m")
            wait_for_callsigns(app, ["VK5DGR"])
            w6xyz = server.station("W6XYZ", "DM13")
            w6xyz.set_frequency(7200000)
            wait_for_callsigns(app, ["VK5DGR", "W6XYZ"])
            w6xyz.set_frequency(18118000)
            wait_for_callsigns(app, ["VK5DGR"])
            w6xyz.disconnect()

            # Tracking: follow the band that FreeDV is on (20 m here)...
            check_equal(app.get("reporter.ckTrackFrequency")["value"], False, "Tracking")
            for name in ["reporter.rbTrackFreqBand", "reporter.rbTrackExactFreq"]:
                check_equal(app.get(name)["enabled"], False, name + " enabled")
            app.set_value("reporter.ckTrackFrequency", True)
            check_equal(app.get("reporter.cbBandFilter")["enabled"], False, "Band filter enabled while tracking")
            check_equal(app.get("reporter.rbTrackFreqBand")["value"], True, "Tracking by band")
            for name in ["reporter.rbTrackFreqBand", "reporter.rbTrackExactFreq"]:
                check_equal(app.get(name)["enabled"], True, name + " enabled")
            wait_for_callsigns(app, ["K1ABC", "W2DEF"])

            # ...or only its exact frequency.
            app.click("reporter.rbTrackExactFreq")
            wait_for_callsigns(app, ["K1ABC"])
            app.click("reporter.rbTrackFreqBand")
            wait_for_callsigns(app, ["K1ABC", "W2DEF"])

            # Changing FreeDV's frequency changes what's being tracked.
            app.set_value("main.cboReportFrequency", "7.1770")
            wait_for_callsigns(app, ["VK5DGR"])
            app.set_value("main.cboReportFrequency", "14.2400")
            wait_for_callsigns(app, ["K1ABC", "W2DEF"])
            app.activate("reporter")
            app.click("reporter.rbTrackExactFreq")
            wait_for_callsigns(app, ["W2DEF"])

            # Without tracking, it's back to the band that was chosen.
            app.set_value("reporter.ckTrackFrequency", False)
            check_equal(app.get("reporter.cbBandFilter")["enabled"], True, "Band filter enabled")
            wait_for_callsigns(app, ["VK5DGR"])

            quit_and_check_log(app)


def test_column_filters():
    """Right-clicking a column's heading filters the list by that column."""
    with ReporterServer() as server:
        for callsign, grid_square, frequency in [("K1ABC", "FN42", 14236000), ("VK5DGR", "PF95", 7177000),
                                                 ("W6XYZ", "DM13", 28330000)]:
            server.station(callsign, grid_square).set_frequency(frequency)
        everyone = ["VK5DGR", "K1ABC", "W6XYZ"]

        def set_filter(app, column, operator, value, existing=""):
            app.column_context_menu(LIST, column, choose=operator)
            app.wait_for_window("@Set Column Filter")
            check_equal(app.get("@Set Column Filter/wxTextCtrl")["value"], existing, "Existing filter value")
            app.set_value("@Set Column Filter/wxTextCtrl", value)
            app.click("@Set Column Filter/OK")
            app.wait_for_window_closed("@Set Column Filter")
            app.activate("reporter")

        with freedvtest.start(settings=reporter_settings(server.hostname)) as app:
            open_reporter(app)
            wait_for_callsigns(app, everyone)

            # Nothing's filtered to begin with.
            items = app.column_context_menu(LIST, DISTANCE)
            check_equal([item["label"] for item in items],
                        [">= (Greater or equal to)", ">  (Greater than)", "=  (Equal to)", "!= (Not equal to)",
                         "<  (Less than)", "<= (Less or equal to)", "Clear Column Filter"], "Filter menu")
            check_equal([item["label"] for item in items if item["checked"]], [], "Filters in use")
            check_equal(items[-1]["enabled"], False, "Clear Column Filter enabled")

            # A column of numbers.
            set_filter(app, DISTANCE, ">= (Greater or equal to)", "4000")
            wait_for_callsigns(app, ["VK5DGR", "K1ABC"])
            check("km *" in column_titles(app), "The filtered column isn't marked: %r" % column_titles(app))

            items = app.column_context_menu(LIST, DISTANCE)
            check_equal([item["label"] for item in items if item["checked"]], [">= (Greater or equal to)"],
                        "Filters in use")
            check_equal(items[-1]["enabled"], True, "Clear Column Filter enabled")

            set_filter(app, DISTANCE, "<  (Less than)", "10000", existing="4000")
            wait_for_callsigns(app, ["K1ABC", "W6XYZ"])

            # A column of text, at the same time.
            set_filter(app, CALLSIGN, "!= (Not equal to)", "W6XYZ")
            wait_for_callsigns(app, ["K1ABC"])
            check("Callsign *" in column_titles(app), "The filtered column isn't marked: %r" % column_titles(app))

            # Cancelling leaves things as they were.
            app.column_context_menu(LIST, CALLSIGN, choose="=  (Equal to)")
            app.wait_for_window("@Set Column Filter")
            app.set_value("@Set Column Filter/wxTextCtrl", "VK5DGR")
            app.click("@Set Column Filter/Cancel")
            app.wait_for_window_closed("@Set Column Filter")
            app.activate("reporter")
            wait_for_callsigns(app, ["K1ABC"])

            # Filters survive the window being closed and reopened...
            app.click("reporter.btnClose")
            app.wait_for_window_closed("reporter")
            open_reporter(app)
            wait_for_callsigns(app, ["K1ABC"])

            # ...until they're cleared.
            app.column_context_menu(LIST, DISTANCE, choose="Clear Column Filter")
            wait_for_callsigns(app, ["VK5DGR", "K1ABC"])
            app.column_context_menu(LIST, CALLSIGN, choose="Clear Column Filter")
            wait_for_callsigns(app, everyone)
            check("km" in column_titles(app) and "Callsign" in column_titles(app),
                  "Columns are still marked as filtered: %r" % column_titles(app))

            quit_and_check_log(app)


def test_idle_filter():
    """The Filter menu hides stations that have been idle for a while."""
    idle_menu = "Filter/Idle more than (minutes).../"

    def check_idle_setting(app, checked_item, status):
        check_equal(label(app, "reporter.stFilterStatus"), status, "Idle filter status")
        for item in ["Disabled", "30", "60", "90", "120", "Custom..."]:
            check_equal(app.menu_checked(idle_menu + item, window="reporter"), item == checked_item,
                        "Idle filter '%s' checked" % item)

    with ReporterServer() as server:
        server.station("K1ABC", "FN42").set_frequency(14236000)

        with freedvtest.start(settings=reporter_settings(server.hostname)) as app:
            open_reporter(app)
            wait_for_callsigns(app, ["K1ABC"])
            check_idle_setting(app, "Disabled", "Idle Off")

            # Nobody here has been on for half an hour, so nobody's hidden.
            app.menu(idle_menu + "30", window="reporter")
            check_idle_setting(app, "30", "Idle 30")
            wait_for_callsigns(app, ["K1ABC"])

            app.menu(idle_menu + "90", window="reporter")
            check_idle_setting(app, "90", "Idle 90")

            # Any other number of minutes can be asked for.
            app.menu(idle_menu + "Custom...", window="reporter")
            app.wait_for_window("@Max Idle Time")
            check_equal(app.get("@Max Idle Time/wxSpinCtrl")["value"], 90, "Suggested idle time")
            app.set_value("@Max Idle Time/wxSpinCtrl", 45)
            app.click("@Max Idle Time/OK")
            app.wait_for_window_closed("@Max Idle Time")
            check_idle_setting(app, "Custom...", "Idle 45")

            # Asking for one of the standard ones that way selects it.
            app.menu(idle_menu + "Custom...", window="reporter")
            app.wait_for_window("@Max Idle Time")
            app.set_value("@Max Idle Time/wxSpinCtrl", 60)
            app.click("@Max Idle Time/OK")
            app.wait_for_window_closed("@Max Idle Time")
            check_idle_setting(app, "60", "Idle 60")

            # Cancelling changes nothing.
            app.menu(idle_menu + "Custom...", window="reporter")
            app.wait_for_window("@Max Idle Time")
            app.set_value("@Max Idle Time/wxSpinCtrl", 15)
            app.click("@Max Idle Time/Cancel")
            app.wait_for_window_closed("@Max Idle Time")
            check_idle_setting(app, "60", "Idle 60")

            app.menu(idle_menu + "Disabled", window="reporter")
            check_idle_setting(app, "Disabled", "Idle Off")
            wait_for_callsigns(app, ["K1ABC"])

            # Nor does cancelling while it's disabled.
            app.menu(idle_menu + "Custom...", window="reporter")
            app.wait_for_window("@Max Idle Time")
            app.click("@Max Idle Time/Cancel")
            app.wait_for_window_closed("@Max Idle Time")
            check_idle_setting(app, "Disabled", "Idle Off")

            quit_and_check_log(app)


def test_columns():
    """The Show menu chooses which columns are displayed."""
    with ReporterServer() as server:
        server.station("K1ABC", "FN42").set_frequency(14236000)

        with freedvtest.start(settings=reporter_settings(server.hostname)) as app:
            open_reporter(app)
            wait_for_callsigns(app, ["K1ABC"])

            all_columns = ["Callsign", "Locator", "km", "Hdg", "Version", "MHz", "Mode", "Status", "Msg", "Last TX",
                           "RX Call", "SNR", "Last Update"]
            menu_items = {"Version": "Version", "Hdg": "Heading", "Msg": "User Message", "Last TX": "Last TX Date",
                          "Locator": "Locator"}
            check_equal(column_titles(app), all_columns, "Columns displayed")

            hidden = []
            for title, item in menu_items.items():
                check_equal(app.menu_checked("Show/" + item, window="reporter"), True, "Show/%s checked" % item)
                app.menu("Show/" + item, window="reporter")
                hidden.append(title)
                check_equal(app.menu_checked("Show/" + item, window="reporter"), False, "Show/%s checked" % item)
                check_equal(column_titles(app), [column for column in all_columns if column not in hidden],
                            "Columns displayed")

            # What's in the other columns is unaffected.
            row = row_for(app, "K1ABC")
            check_equal((row[CALLSIGN], row[FREQUENCY]), ("K1ABC", "14.2360"), "K1ABC's row")

            for title, item in menu_items.items():
                app.menu("Show/" + item, window="reporter")
                hidden.remove(title)
                check_equal(app.menu_checked("Show/" + item, window="reporter"), True, "Show/%s checked" % item)
                check_equal(column_titles(app), [column for column in all_columns if column not in hidden],
                            "Columns displayed")

            quit_and_check_log(app)


def test_status_message():
    """FreeDV's own status message, and the list of saved ones."""
    message_box = "reporter.cbStatusMessage"
    saved_list = "reporter.lbSavedMessages"

    def saved(app):
        return [item["label"] for item in app.get(saved_list)["items"]]

    def popup(app, button):
        return {item["label"]: item["enabled"] for item in app.context_menu(button)}

    with audio_devices(), ReporterServer() as server:
        viewer = server.viewer()

        def reported_message():
            updates = viewer.events("message_update", CALLSIGN_OF_FREEDV)
            return updates[-1]["message"] if updates else None

        def wait_for_reported_message(message):
            try:
                viewer.wait_for_event("message_update", CALLSIGN_OF_FREEDV, lambda data: data["message"] == message)
            except Exception:
                check_equal(reported_message(), message, "Message reported by FreeDV")

        settings = reporter_settings(server.hostname)
        settings.update(audio_settings())
        with freedvtest.start(settings=settings) as app:
            open_reporter(app)
            start_modem(app)
            wait_for_callsigns(app, [CALLSIGN_OF_FREEDV])
            check_equal(app.get(message_box)["value"], "", "Status message")
            check_equal(saved(app), [], "Saved messages")

            # With nothing typed in, there's nothing to save.
            check_equal(popup(app, "reporter.btnSend"),
                        {"Send and save message": False, "Only save message": False}, "Send button's menu")

            # Send
            app.set_value(message_box, "Testing 1 2 3")
            app.click("reporter.btnSend")
            wait_for_reported_message("Testing 1 2 3")
            wait_for_cell(app, CALLSIGN_OF_FREEDV, MESSAGE, "Testing 1 2 3")
            check_equal(saved(app), [], "Saved messages after sending")

            # Send and save
            app.set_value(message_box, "QRV 20 m")
            check_equal(popup(app, "reporter.btnSend"),
                        {"Send and save message": True, "Only save message": True}, "Send button's menu")
            app.context_menu("reporter.btnSend", choose="Send and save message")
            wait_for_reported_message("QRV 20 m")
            check_equal(saved(app), ["QRV 20 m"], "Saved messages")

            # What's already saved can't be saved again.
            check_equal(popup(app, "reporter.btnSend"),
                        {"Send and save message": False, "Only save message": False}, "Send button's menu")

            # Only save
            app.set_value(message_box, "Back in 5 minutes")
            app.context_menu("reporter.btnSend", choose="Only save message")
            check_equal(saved(app), ["Back in 5 minutes", "QRV 20 m"], "Saved messages")
            time.sleep(1)
            check_equal(reported_message(), "QRV 20 m", "Message reported by FreeDV after only saving another")

            # Clear
            app.click("reporter.btnClear")
            wait_for_reported_message("")
            check_equal(app.get(message_box)["value"], "", "Status message after clearing")
            wait_for_cell(app, CALLSIGN_OF_FREEDV, MESSAGE, "")
            check_equal(saved(app), ["Back in 5 minutes", "QRV 20 m"], "Saved messages after clearing")

            # Picking a saved message from the list puts it in the box, ready
            # to be sent.
            app.combo_popup(message_box, True)
            app.wait_until(lambda: app.get(saved_list)["shown"], description="the saved messages to be listed")
            app.click(saved_list, item=1)
            app.wait_until(lambda: app.get(message_box)["value"] == "QRV 20 m", description="the message to be chosen")
            time.sleep(1)
            check_equal(reported_message(), "", "Message reported by FreeDV after choosing a saved one")
            app.activate("reporter")
            app.click("reporter.btnSend")
            wait_for_reported_message("QRV 20 m")
            check_equal(saved(app), ["Back in 5 minutes", "QRV 20 m"], "Saved messages after sending one of them")

            # Right-clicking a saved message offers to edit or delete it, or
            # to add another.
            def saved_message_menu(item, choose):
                app.combo_popup(message_box, True)
                app.wait_until(lambda: app.get(saved_list)["shown"], description="the saved messages to be listed")
                items = app.context_menu(saved_list, item=item, choose=choose)
                check_equal([entry["label"] for entry in items], ["Edit message", "Delete message", "Add new message"],
                            "Saved message's menu")

            saved_message_menu(0, "Add new message")
            app.wait_for_window("@Add Message")
            app.set_value("@Add Message/wxTextCtrl", "73")
            app.click("@Add Message/OK")
            app.wait_for_window_closed("@Add Message")
            app.activate("reporter")
            check_equal(saved(app), ["73", "Back in 5 minutes", "QRV 20 m"], "Saved messages after adding one")

            saved_message_menu(1, "Edit message")
            app.wait_for_window("@Edit Message")
            check_equal(app.get("@Edit Message/wxTextCtrl")["value"], "Back in 5 minutes", "Message being edited")
            app.set_value("@Edit Message/wxTextCtrl", "Back in 10 minutes")
            app.click("@Edit Message/OK")
            app.wait_for_window_closed("@Edit Message")
            app.activate("reporter")
            check_equal(saved(app), ["73", "Back in 10 minutes", "QRV 20 m"], "Saved messages after editing one")

            # Editing the message that's in the box changes it there too.
            saved_message_menu(2, "Edit message")
            app.wait_for_window("@Edit Message")
            app.set_value("@Edit Message/wxTextCtrl", "QRV 40 m")
            app.click("@Edit Message/OK")
            app.wait_for_window_closed("@Edit Message")
            app.activate("reporter")
            check_equal(saved(app), ["73", "Back in 10 minutes", "QRV 40 m"], "Saved messages after editing one")
            check_equal(app.get(message_box)["value"], "QRV 40 m", "Status message after editing it")

            saved_message_menu(0, "Delete message")
            check_equal(saved(app), ["Back in 10 minutes", "QRV 40 m"], "Saved messages after deleting one")

            # Changing one's mind, changing nothing or leaving nothing leaves
            # the list as it was, as does adding what's already there.
            def answer(title, text, button):
                app.wait_for_window("@" + title)
                if text is not None:
                    app.set_value("@%s/wxTextCtrl" % title, text)
                app.click("@%s/%s" % (title, button))
                app.wait_for_window_closed("@" + title)
                app.activate("reporter")

            for text, button in [("Something else", "Cancel"), (None, "OK"), ("", "OK"), ("   ", "OK")]:
                saved_message_menu(0, "Edit message")
                answer("Edit Message", text, button)
                check_equal(saved(app), ["Back in 10 minutes", "QRV 40 m"],
                            "Saved messages after editing one to %r and pressing %s" % (text, button))
            for text, button in [("Something else", "Cancel"), ("", "OK"), ("QRV 40 m", "OK"), ("qrv 40 m ", "OK")]:
                saved_message_menu(0, "Add new message")
                answer("Add Message", text, button)
                expected = ["Back in 10 minutes", "QRV 40 m"] + (["qrv 40 m"] if text == "qrv 40 m " else [])
                check_equal(sorted(saved(app)), sorted(expected),
                            "Saved messages after adding %r and pressing %s" % (text, button))
            saved_message_menu(saved(app).index("qrv 40 m"), "Delete message")
            check_equal(saved(app), ["Back in 10 minutes", "QRV 40 m"], "Saved messages after deleting one")

            # The Clear button's menu removes saved messages: the one in the
            # box, or all of them.
            app.activate("reporter")
            app.combo_popup(message_box, True)
            app.wait_until(lambda: app.get(saved_list)["shown"], description="the saved messages to be listed")
            app.click(saved_list, item=1)
            app.wait_until(lambda: app.get(message_box)["value"] == "QRV 40 m", description="the message to be chosen")
            app.activate("reporter")
            check_equal(popup(app, "reporter.btnClear"),
                        {"Remove selected from list": True, "Clear all messages from list": True},
                        "Clear button's menu")
            app.context_menu("reporter.btnClear", choose="Remove selected from list")
            check_equal(saved(app), ["Back in 10 minutes"], "Saved messages after removing the current one")
            app.set_value(message_box, "One more")
            app.context_menu("reporter.btnSend", choose="Only save message")
            check_equal(saved(app), ["Back in 10 minutes", "One more"], "Saved messages")
            app.context_menu("reporter.btnClear", choose="Clear all messages from list")
            check_equal(saved(app), [], "Saved messages after clearing them all")

            stop_modem(app)
            quit_and_check_log(app)


def test_qsy():
    """Asking another station to change frequency, and being asked to."""
    with audio_devices(), ReporterServer() as server:
        k1abc = server.station("K1ABC", "FN42")
        k1abc.set_frequency(14240000)
        w2def = server.station("W2DEF", "FN30")
        w2def.set_frequency(FREQUENCY_OF_FREEDV)

        settings = reporter_settings(server.hostname)
        settings.update(audio_settings())
        with freedvtest.start(settings=settings) as app:
            open_reporter(app)
            wait_for_callsigns(app, ["W2DEF", "K1ABC"])

            def selected():
                return [row[CALLSIGN] for row in app.rows(LIST) if row["selected"]]

            def select(callsign):
                app.select_row(LIST, (CALLSIGN, callsign))
                check_equal(selected(), [callsign], "Selected station")
                return app.get("reporter.btnSendQSY")["enabled"]

            # Until FreeDV is started, it isn't on any frequency to ask
            # anyone to change to.
            check_equal(app.get("reporter.btnSendQSY")["enabled"], False, "Request QSY enabled with nothing selected")
            check_equal(select("K1ABC"), False, "Request QSY enabled before starting")

            start_modem(app)
            app.wait_until(lambda: CALLSIGN_OF_FREEDV in callsigns(app), description="FreeDV to be listed")

            # Only stations on another frequency can be asked.
            check_equal(select("W2DEF"), False, "Request QSY enabled for a station on the same frequency")
            check_equal(select(CALLSIGN_OF_FREEDV), False, "Request QSY enabled for FreeDV itself")
            check_equal(select("K1ABC"), True, "Request QSY enabled for a station on another frequency")

            app.click("reporter.btnSendQSY")
            request = k1abc.wait_for_event("qsy_request")
            check_equal((request["callsign"], request["frequency"]), (CALLSIGN_OF_FREEDV, FREQUENCY_OF_FREEDV),
                        "QSY request received by K1ABC")
            check_equal(w2def.events("qsy_request"), [], "QSY requests received by W2DEF")
            check_equal([box["message"] for box in app.message_boxes(clear=True)], ["QSY request sent to K1ABC"],
                        "Messages displayed")
            check_equal(selected(), [], "Selected station after requesting QSY")
            check_equal(app.get("reporter.btnSendQSY")["enabled"], False, "Request QSY enabled with nothing selected")

            # Being asked. There's no radio for FreeDV to retune, so it can
            # only say so.
            k1abc.request_qsy(CALLSIGN_OF_FREEDV, 14240000)
            app.wait_until(lambda: app.message_boxes(), description="the QSY request to be displayed")
            check_equal([box["message"] for box in app.message_boxes(clear=True)],
                        ["K1ABC has requested that you QSY to 14.2400 MHz."], "Messages displayed")
            check_equal(app.get("main.cboReportFrequency")["value"], "14.2360", "FreeDV's frequency")

            stop_modem(app)
            quit_and_check_log(app)


def test_row_actions():
    """What can be done to a station in the list with the mouse, other than
    selecting it: looking it up, copying its message and tuning to it."""
    with audio_devices(), ReporterServer() as server:
        k1abc = server.station("K1ABC", "FN42")
        k1abc.set_frequency(7177000)
        k1abc.set_message("Calling CQ on 40m")
        vk5dgr = server.station("VK5DGR", "PF95", rx_only=True)
        vk5dgr.set_frequency(14240000)

        # With a radio that FreeDV can tune: Hamlib's "Dummy" rig.
        settings = reporter_settings(server.hostname)
        settings.update(audio_settings())
        settings.update({"/Reporting/ManualFrequencyReporting": False, "/Hamlib/UseForPTT": True,
                         "/Hamlib/RigNameStr": "Hamlib Dummy"})
        with freedvtest.start(settings=settings) as app:
            open_reporter(app)
            wait_for_callsigns(app, ["K1ABC", "VK5DGR"])
            wait_for_cell(app, "K1ABC", MESSAGE, "Calling CQ on 40m")
            vk5dgr.receive("K1ABC", "RADEV1", 5)
            wait_for_cell(app, "VK5DGR", RX_CALLSIGN, "K1ABC")

            lookups = ["Lookup via QRZ", "Lookup via HamQTH", "Lookup via HamCall"]

            def menu(callsign, column, choose=None):
                app.activate("reporter")
                return [item["label"] for item in app.row_context_menu(LIST, (CALLSIGN, callsign), column, choose)]

            # Right-clicking a callsign offers places to look it up...
            check_equal(menu("K1ABC", CALLSIGN), lookups, "Menu for K1ABC's callsign")
            check_equal(app.browser_urls(), [], "Web pages opened by just looking at the menu")
            for item, url in zip(lookups, ["https://www.qrz.com/db/K1ABC", "https://www.hamqth.com/K1ABC",
                                           "https://hamcall.net/call?callsign=K1ABC"]):
                menu("K1ABC", CALLSIGN, choose=item)
                check_equal(app.browser_urls(clear=True), [url], "Web pages opened by " + item)

            # ...including the callsign of whoever a station is hearing.
            check_equal(menu("VK5DGR", RX_CALLSIGN, choose="Lookup via QRZ"), lookups, "Menu for who VK5DGR hears")
            check_equal(app.browser_urls(clear=True), ["https://www.qrz.com/db/K1ABC"], "Web pages opened")
            menu("VK5DGR", CALLSIGN, choose="Lookup via HamQTH")
            check_equal(app.browser_urls(clear=True), ["https://www.hamqth.com/VK5DGR"], "Web pages opened")

            # Right-clicking a message offers to copy it.
            check_equal(menu("K1ABC", MESSAGE, choose="Copy message"), ["Copy message"], "Menu for K1ABC's message")
            check_equal(app.clipboard(), "Calling CQ on 40m", "What's on the clipboard")
            check_equal([row[CALLSIGN] for row in app.rows(LIST) if row["selected"]], [],
                        "Selected station after right-clicking")

            # Anywhere else, there's no menu.
            for callsign, column in [("K1ABC", FREQUENCY), ("VK5DGR", MESSAGE), ("K1ABC", RX_CALLSIGN)]:
                try:
                    items = menu(callsign, column)
                    check(False, "Right-clicking column %d for %s gave a menu: %r" % (column, callsign, items))
                except freedvtest.CommandError as e:
                    check("no popup menu appeared" in str(e), "Right-clicking column %d failed: %s" % (column, e))

            # The website button opens the server's own page.
            app.click("reporter.btnWebsite")
            check_equal(app.browser_urls(clear=True), ["https://%s/" % server.hostname], "Web pages opened")
            check_equal([row[CALLSIGN] for row in app.rows(LIST) if row["selected"]], [],
                        "Selected station after opening the website")

            # Double-clicking a station does nothing without a radio to tune...
            app.activate_row(LIST, (CALLSIGN, "K1ABC"))
            time.sleep(0.5)
            check_equal(app.message_boxes(), [], "Messages displayed")

            # ...and with one, takes it to where that station is.
            start_modem(app)
            app.wait_until(lambda: CALLSIGN_OF_FREEDV in callsigns(app), description="FreeDV to be listed")
            # Once FreeDV has the radio, that is: on connecting to it, FreeDV
            # tunes it to the frequency chosen in the main window, which would
            # undo a double-click from before then.
            app.wait_until(lambda: app.get("main.textModeStatus")["label"] != "unk",
                           description="FreeDV to connect to the radio")
            app.select_row(LIST, (CALLSIGN, "K1ABC"))
            app.activate_row(LIST, (CALLSIGN, "K1ABC"))
            app.wait_until(lambda: app.get("main.cboReportFrequency")["value"] == "7.1770", timeout=15,
                           description="FreeDV's radio to be tuned to K1ABC's frequency")
            wait_for_cell(app, CALLSIGN_OF_FREEDV, FREQUENCY, "7.1770", timeout=45)
            check_equal([row[CALLSIGN] for row in app.rows(LIST) if row["selected"]], [],
                        "Selected station after double-clicking")
            app.activate_row(LIST, (CALLSIGN, "VK5DGR"))
            app.wait_until(lambda: app.get("main.cboReportFrequency")["value"] == "14.2400", timeout=15,
                           description="FreeDV's radio to be tuned to VK5DGR's frequency")

            stop_modem(app)
            quit_and_check_log(app)


def test_turn_off():
    """The main window's button for going off FreeDV Reporter for a while."""
    with audio_devices(), ReporterServer() as server:
        watcher = server.viewer()
        settings = reporter_settings(server.hostname)
        settings.update(audio_settings())
        with freedvtest.start(settings=settings) as app:
            app.wait_for_window("main")
            button = "main.btnReporterHidden"
            check_equal((label(app, button), app.get(button)["value"]), ("Turn Off", False), "Button to begin with")

            def listed():
                return CALLSIGN_OF_FREEDV in watcher.callsigns()

            # Pressed before starting, it's only a note of what's wanted.
            app.click(button)
            check_equal((label(app, button), app.get(button)["value"]), ("Turn On", True), "Button while stopped")
            app.click(button)
            check_equal((label(app, button), app.get(button)["value"]), ("Turn Off", False), "Button while stopped")

            start_modem_only(app)
            app.wait_until(listed, timeout=15, description="FreeDV to be listed on the server")
            check_equal(app.get(button)["enabled"], True, "Button enabled while reporting")

            app.click(button)
            check_equal((label(app, button), app.get(button)["value"]), ("Turn On", True), "Button once turned off")
            app.wait_until(lambda: not listed(), timeout=15, description="FreeDV to leave the server's list")
            check_equal(app.state()["config"]["reporting"]["freedvReporterForcedOff"], True, "Turned off")

            app.click(button)
            check_equal((label(app, button), app.get(button)["value"]), ("Turn Off", False), "Button once back on")
            app.wait_until(listed, timeout=15, description="FreeDV to be listed on the server again")

            # Left off, it stays off through a stop and start...
            app.click(button)
            app.wait_until(lambda: not listed(), timeout=15, description="FreeDV to leave the server's list")
            stop_modem(app)
            check_equal(label(app, button), "Turn On", "Button's label after stopping")
            start_modem_only(app)
            time.sleep(3)
            check_equal(listed(), False, "Listed on the server after starting while turned off")

            # ...until it's turned back on.
            app.click(button)
            app.wait_until(listed, timeout=15, description="FreeDV to be listed on the server once turned on")
            stop_modem(app)
            quit_and_check_log(app)


def test_off_at_start():
    """Left off FreeDV Reporter the last time, FreeDV starts out that way."""
    with audio_devices(), ReporterServer() as server:
        watcher = server.viewer()
        settings = reporter_settings(server.hostname, **{"/Reporting/FreeDV/ForcedOff": True})
        settings.update(audio_settings())
        with freedvtest.start(settings=settings) as app:
            app.wait_for_window("main")
            button = "main.btnReporterHidden"
            check_equal((label(app, button), app.get(button)["value"]), ("Turn On", True), "Button to begin with")

            start_modem_only(app)
            time.sleep(3)
            check_equal(CALLSIGN_OF_FREEDV in watcher.callsigns(), False, "Listed on the server while turned off")

            app.click(button)
            check_equal((label(app, button), app.get(button)["value"]), ("Turn Off", False), "Button once turned on")
            app.wait_until(lambda: CALLSIGN_OF_FREEDV in watcher.callsigns(), timeout=15,
                           description="FreeDV to be listed on the server once turned on")
            stop_modem(app)
            quit_and_check_log(app)
            check_equal(freedvtest.read_config(app.config_path).get("/Reporting/FreeDV/ForcedOff"), "0",
                        "Saved as turned off")


def test_qsy_with_radio():
    """Being asked to change frequency with a radio that FreeDV can retune:
    it offers to."""
    for as_khz, asked, declined, accepted in [(False, "14.2400 MHz", "14.2360", "14.2400"),
                                              (True, "14240.0 kHz", "14236.0", "14240.0")]:
        with audio_devices(), ReporterServer() as server:
            k1abc = server.station("K1ABC", "FN42")
            k1abc.set_frequency(14240000)

            settings = reporter_settings(server.hostname)
            settings.update(audio_settings())
            settings.update({"/Reporting/ManualFrequencyReporting": False, "/Hamlib/UseForPTT": True,
                             "/Hamlib/RigNameStr": "Hamlib Dummy", "/Reporting/FrequencyAsKHz": as_khz})
            with freedvtest.start(settings=settings) as app:
                open_reporter(app)
                start_modem(app)
                app.wait_until(lambda: CALLSIGN_OF_FREEDV in callsigns(app), description="FreeDV to be listed")
                app.wait_until(lambda: app.get("main.textModeStatus")["label"] != "unk",
                               description="FreeDV to connect to the radio")

                def frequency():
                    return app.get("main.cboReportFrequency")["value"].replace(",", "")

                question = "K1ABC has requested that you QSY to %s. Would you like to change to that frequency now?" \
                    % asked

                # Declined
                app.answer_next_message_box("no")
                k1abc.request_qsy(CALLSIGN_OF_FREEDV, 14240000)
                app.wait_until(lambda: app.message_boxes(), description="the QSY request to be displayed")
                check_equal([box["message"].replace(",", "") for box in app.message_boxes(clear=True)], [question],
                            "Messages displayed")
                time.sleep(1)
                check_equal(frequency(), declined, "FreeDV's frequency after declining")

                # Accepted
                app.answer_next_message_box("yes")
                k1abc.request_qsy(CALLSIGN_OF_FREEDV, 14240000)
                app.wait_until(lambda: app.message_boxes(), description="the QSY request to be displayed")
                check_equal([box["message"].replace(",", "") for box in app.message_boxes(clear=True)], [question],
                            "Messages displayed")
                app.wait_until(lambda: frequency() == accepted, timeout=15,
                               description="FreeDV to change frequency (it's on %s)" % frequency())
                wait_for_cell(app, CALLSIGN_OF_FREEDV, FREQUENCY, lambda text: text.replace(",", "") == accepted,
                              timeout=45)

                stop_modem(app)
                quit_and_check_log(app)


def test_new_callsign():
    """Changing who FreeDV reports as, which can only be done while it's
    stopped: it's listed under the new callsign from then on."""
    with audio_devices(), ReporterServer() as server:
        watcher = server.viewer()
        settings = reporter_settings(server.hostname)
        settings.update(audio_settings())
        with freedvtest.start(settings=settings) as app:
            open_reporter(app)
            start_modem(app)
            app.wait_until(lambda: watcher.callsigns() == [CALLSIGN_OF_FREEDV], timeout=15,
                           description="FreeDV to be listed on the server")

            def open_settings():
                app.menu("Settings/Edit Settings...")
                app.wait_for_window("settings")
                app.set_value("settings.notebook", "Reporting")

            def close_settings():
                app.click("settings.sdbSizer5OK")
                app.wait_for_window_closed("settings")
                check_equal(app.message_boxes(), [], "Messages displayed")

            open_settings()
            for control in ["txt_callsign", "txt_grid_square", "freedvReporterHostname"]:
                check_equal(app.get("settings." + control)["enabled"], False, control + " enabled while running")
            close_settings()
            check_equal(watcher.callsigns(), [CALLSIGN_OF_FREEDV], "Listed on the server after closing Settings")

            stop_modem(app)
            app.wait_until(lambda: watcher.callsigns() == [], timeout=20,
                           description="FreeDV to leave the server's list")
            open_settings()
            app.set_value("settings.txt_callsign", "ZZ9ZZZ")
            close_settings()

            start_modem(app)
            app.wait_until(lambda: watcher.callsigns() == ["ZZ9ZZZ"], timeout=20,
                           description="only ZZ9ZZZ to be listed on the server (%r is)" % watcher.callsigns())
            wait_for_callsigns(app, ["ZZ9ZZZ"])
            wait_for_cell(app, "ZZ9ZZZ", FREQUENCY, "14.2360")

            stop_modem(app)
            quit_and_check_log(app)


def start_modem_only(app):
    """start_modem() for when the FreeDV Reporter window isn't open."""
    app.click("main.btnOnOff")
    app.wait_until(lambda: app.get("main.btnOnOff")["enabled"], description="FreeDV to start")
    check_equal(app.message_boxes(), [], "Errors displayed on start")
    check(app.state()["running"], "FreeDV is not running after pressing Start")


def test_sort_every_column():
    """Each column can be sorted by, in both directions."""
    with ReporterServer() as server:
        # Three stations that differ in every column.
        k1abc = server.station("K1ABC", "FN42", version="alpha 3")
        k1abc.set_frequency(14236000)
        k1abc.set_message("Third")
        vk5dgr = server.station("VK5DGR", "PF95", version="alpha 1", rx_only=True)
        vk5dgr.set_frequency(7177000)
        vk5dgr.set_message("First")
        w6xyz = server.station("W6XYZ", "DM13", version="alpha 2")
        w6xyz.set_frequency(28330000)
        w6xyz.set_message("Second")

        with freedvtest.start(settings=reporter_settings(server.hostname)) as app:
            open_reporter(app)
            wait_for_callsigns(app, ["VK5DGR", "K1ABC", "W6XYZ"])
            k1abc.transmit("RADEV1", True)
            wait_for_cell(app, "K1ABC", STATUS, "TX")
            k1abc.transmit("RADEV1", False)
            wait_for_cell(app, "K1ABC", STATUS, "RX")
            # (W6XYZ a moment later, so that when each last transmitted differs.)
            time.sleep(1.1)
            w6xyz.transmit("RADEV1", True)
            wait_for_cell(app, "W6XYZ", STATUS, "TX")
            w6xyz.transmit("RADEV1", False)
            wait_for_cell(app, "W6XYZ", STATUS, "RX")
            vk5dgr.receive("K1ABC", "RADEV1", 12)
            wait_for_cell(app, "VK5DGR", RX_CALLSIGN, "K1ABC")
            w6xyz.receive("AB1CD", "RADEV1", 3)
            wait_for_cell(app, "W6XYZ", RX_CALLSIGN, "AB1CD")

            # Where sorting by a column is known to put them.
            expected = {
                CALLSIGN: ["K1ABC", "VK5DGR", "W6XYZ"],
                LOCATOR: ["W6XYZ", "K1ABC", "VK5DGR"],        # DM13, FN42, PF95
                DISTANCE: ["W6XYZ", "K1ABC", "VK5DGR"],
                VERSION: ["VK5DGR", "W6XYZ", "K1ABC"],
                FREQUENCY: ["VK5DGR", "K1ABC", "W6XYZ"],
                MESSAGE: ["VK5DGR", "W6XYZ", "K1ABC"],        # First, Second, Third
                LAST_TX: ["VK5DGR", "K1ABC", "W6XYZ"],        # never, then the order they did
            }
            for column in app.columns(LIST):
                if column["title"].strip() == "":
                    continue  # what fills the space to the right of the last column
                column_id = column["id"]
                app.click_column(LIST, column_id)
                check_equal(sort_order(app), (column_id, True), "Sort order after clicking %r" % column["title"])
                ascending = callsigns(app)
                check_equal(sorted(ascending), ["K1ABC", "VK5DGR", "W6XYZ"], "Callsigns listed")
                if column_id in expected:
                    wait_for_callsigns(app, expected[column_id])

                app.click_column(LIST, column_id)
                check_equal(sort_order(app), (column_id, False), "Sort order after clicking %r again"
                            % column["title"])
                descending = callsigns(app)
                check_equal(sorted(descending), ["K1ABC", "VK5DGR", "W6XYZ"], "Callsigns listed")
                if column_id in expected:
                    wait_for_callsigns(app, list(reversed(expected[column_id])))

            quit_and_check_log(app)


def test_display_options():
    """Distances in miles, directions as compass points and times in UTC,
    and what's shown when where someone is can't be worked out."""
    with ReporterServer() as server:
        server.station("K1ABC", "FN42").set_frequency(14236000)
        server.station("W6XYZ", "DM12").set_frequency(14236000)    # the same square as FreeDV
        server.station("N0WHR", "!!").set_frequency(14236000)      # nowhere there is
        everyone = ["K1ABC", "N0WHR", "W6XYZ"]

        def listed(app):
            open_reporter(app)
            app.click_column(LIST, CALLSIGN)
            app.wait_until(lambda: callsigns(app) == everyone, timeout=15, description="everyone to be listed")
            return {row[CALLSIGN]: row for row in app.rows(LIST)}

        # As it comes: kilometres and degrees.
        with freedvtest.start(settings=reporter_settings(server.hostname)) as app:
            rows = listed(app)
            check(4000 < number(rows["K1ABC"][DISTANCE]) < 4300, "K1ABC is %s km away" % rows["K1ABC"][DISTANCE])
            check(50 < number(rows["K1ABC"][HEADING]) < 75, "K1ABC is at %s degrees" % rows["K1ABC"][HEADING])
            check_equal((rows["W6XYZ"][DISTANCE].strip(), rows["W6XYZ"][HEADING].strip()), ("0", ""),
                        "Distance and direction to the same square")
            check_equal((rows["N0WHR"][DISTANCE].strip(), rows["N0WHR"][HEADING].strip()), ("", ""),
                        "Distance and direction to nowhere")

            # Nor once the list has been redone (as changing the band filter
            # does), which used to give a direction of 0.
            app.set_value("reporter.cbBandFilter", "20 m")
            app.set_value("reporter.cbBandFilter", "All")
            rows = {row[CALLSIGN]: row for row in app.rows(LIST)}
            check_equal((rows["N0WHR"][DISTANCE].strip(), rows["N0WHR"][HEADING].strip()), ("", ""),
                        "Distance and direction to nowhere after the list was redone")
            local_time = rows["K1ABC"][LAST_UPDATE]
            quit_and_check_log(app)

        # Miles, compass points and UTC.
        options = {"/Reporting/FreeDV/UseMetricDistances": False, "/Reporting/DirectionAsCardinal": True,
                   "/CallsignList/UseUTCTime": True}
        with freedvtest.start(settings=reporter_settings(server.hostname, **options)) as app:
            rows = listed(app)
            check(2500 < number(rows["K1ABC"][DISTANCE]) < 2700, "K1ABC is %s miles away" % rows["K1ABC"][DISTANCE])
            check_equal(rows["K1ABC"][HEADING].strip(), "ENE", "Direction to K1ABC")
            check_equal((rows["W6XYZ"][DISTANCE].strip(), rows["W6XYZ"][HEADING].strip()), ("0", ""),
                        "Distance and direction to the same square")
            check(rows["K1ABC"][LAST_UPDATE] != "", "K1ABC has no last update time")
            check("UTC" in rows["K1ABC"][LAST_UPDATE] or rows["K1ABC"][LAST_UPDATE] != local_time or time.timezone == 0,
                  "K1ABC's last update is %r in UTC and %r in local time" % (rows["K1ABC"][LAST_UPDATE], local_time))
            quit_and_check_log(app)

        # Without knowing where FreeDV is, nobody's distance is known.
        with freedvtest.start(settings=reporter_settings(server.hostname, **{"/Reporting/GridSquare": "??"})) as app:
            rows = listed(app)
            for callsign in everyone:
                check_equal((rows[callsign][DISTANCE].strip(), rows[callsign][HEADING].strip()), ("", ""),
                            "Distance and direction to %s" % callsign)
            quit_and_check_log(app)


def test_sorted_updates():
    """The list re-sorts itself as what it's sorted by changes, for each of
    the columns that change while a station's connected."""
    with ReporterServer() as server:
        k1abc = server.station("K1ABC", "FN42")
        k1abc.set_frequency(14236000)
        vk5dgr = server.station("VK5DGR", "PF95")
        vk5dgr.set_frequency(14236000)

        with freedvtest.start(settings=reporter_settings(server.hostname)) as app:
            open_reporter(app)
            app.wait_until(lambda: sorted(callsigns(app)) == ["K1ABC", "VK5DGR"], timeout=15,
                           description="both stations to be listed")

            def sort_by(column):
                app.click_column(LIST, column)
                check_equal(sort_order(app), (column, True), "Sort order")

            def wait_for_order(first, second, what):
                try:
                    app.wait_until(lambda: callsigns(app) == [first, second], timeout=15)
                except freedvtest.FreeDVError:
                    check_equal(callsigns(app), [first, second], "Callsigns listed after %s" % what)

            def transmit(station, mode):
                station.transmit(mode, True)
                wait_for_cell(app, station.callsign, STATUS, "TX")
                station.transmit(mode, False)
                wait_for_cell(app, station.callsign, STATUS, "RX")

            # Who's transmitting
            sort_by(STATUS)
            k1abc.transmit("RADEV1", True)
            wait_for_cell(app, "K1ABC", STATUS, "TX")
            wait_for_order("VK5DGR", "K1ABC", "K1ABC began transmitting")       # RX, TX
            vk5dgr.transmit("RADEV1", True)
            wait_for_cell(app, "VK5DGR", STATUS, "TX")
            k1abc.transmit("RADEV1", False)
            wait_for_cell(app, "K1ABC", STATUS, "RX")
            wait_for_order("K1ABC", "VK5DGR", "they swapped over")
            vk5dgr.transmit("RADEV1", False)
            wait_for_cell(app, "VK5DGR", STATUS, "RX")

            # The mode last transmitted in
            sort_by(TX_MODE)
            transmit(k1abc, "RADEV1")
            transmit(vk5dgr, "700D")
            wait_for_order("VK5DGR", "K1ABC", "transmitting in 700D and RADEV1")
            transmit(vk5dgr, "RADEV2")
            wait_for_cell(app, "VK5DGR", TX_MODE, "RADEV2")
            wait_for_order("K1ABC", "VK5DGR", "VK5DGR changed to RADEV2")
            transmit(vk5dgr, "RADEV2")  # nothing that's sorted by changes
            check_equal(callsigns(app), ["K1ABC", "VK5DGR"], "Callsigns listed after transmitting again")

            # When they last transmitted
            sort_by(LAST_TX)
            wait_for_order("K1ABC", "VK5DGR", "sorting by last transmission")
            time.sleep(1.1)
            transmit(k1abc, "RADEV1")
            wait_for_order("VK5DGR", "K1ABC", "K1ABC transmitted again")

            # Who they last received, and how well
            sort_by(RX_CALLSIGN)
            k1abc.receive("ZL1ZZ", "RADEV1", 20)
            wait_for_cell(app, "K1ABC", RX_CALLSIGN, "ZL1ZZ")
            vk5dgr.receive("AA1AA", "RADEV1", -3)
            wait_for_cell(app, "VK5DGR", RX_CALLSIGN, "AA1AA")
            wait_for_order("VK5DGR", "K1ABC", "receiving AA1AA and ZL1ZZ")
            vk5dgr.receive("ZZ9ZZ", "RADEV1", -3)
            wait_for_cell(app, "VK5DGR", RX_CALLSIGN, "ZZ9ZZ")
            wait_for_order("K1ABC", "VK5DGR", "VK5DGR received ZZ9ZZ")
            vk5dgr.receive("ZZ9ZZ", "RADEV1", -3)  # the same again

            sort_by(SNR)
            wait_for_order("VK5DGR", "K1ABC", "sorting by SNR")                 # -3, 20
            vk5dgr.receive("ZZ9ZZ", "RADEV1", 25)
            wait_for_cell(app, "VK5DGR", SNR, lambda text: text.strip() in ("25", "25.0"))
            wait_for_order("K1ABC", "VK5DGR", "VK5DGR's SNR rose to 25")
            vk5dgr.receive("ZZ9ZZ", "RADEV1", 25)

            # Receiving nothing in particular (as after changing frequency)
            # clears who and how well, sorted by either.
            k1abc.receive("", "", 0)
            wait_for_cell(app, "K1ABC", RX_CALLSIGN, lambda text: text.strip() != "ZL1ZZ")
            check(row_for(app, "K1ABC")[SNR].strip() not in ("20", "20.0"),
                  "K1ABC's SNR is still %r" % row_for(app, "K1ABC")[SNR])
            k1abc.receive("", "", 0)  # with nothing left to clear
            sort_by(RX_CALLSIGN)
            vk5dgr.receive("", "", 0)
            wait_for_cell(app, "VK5DGR", RX_CALLSIGN, lambda text: text.strip() != "ZZ9ZZ")
            check_equal(sorted(callsigns(app)), ["K1ABC", "VK5DGR"], "Callsigns listed")

            # Their messages
            sort_by(MESSAGE)
            k1abc.set_message("zebra")
            wait_for_cell(app, "K1ABC", MESSAGE, "zebra")
            vk5dgr.set_message("aardvark")
            wait_for_cell(app, "VK5DGR", MESSAGE, "aardvark")
            wait_for_order("VK5DGR", "K1ABC", "messages of aardvark and zebra")
            k1abc.set_message("Aardvark 1")
            wait_for_cell(app, "K1ABC", MESSAGE, "Aardvark 1")
            vk5dgr.set_message("zulu")
            wait_for_cell(app, "VK5DGR", MESSAGE, "zulu")
            wait_for_order("K1ABC", "VK5DGR", "messages of Aardvark 1 and zulu")
            vk5dgr.set_message("zulu")
            vk5dgr.set_message("")
            wait_for_cell(app, "VK5DGR", MESSAGE, "")
            vk5dgr.set_message("")
            check_equal(sorted(callsigns(app)), ["K1ABC", "VK5DGR"], "Callsigns listed")

            quit_and_check_log(app)


def test_numeric_filters():
    """Filtering by the columns that hold numbers, with each comparison."""
    with ReporterServer() as server:
        k1abc = server.station("K1ABC", "FN42")
        k1abc.set_frequency(14236000)
        vk5dgr = server.station("VK5DGR", "PF95")
        vk5dgr.set_frequency(7177000)
        w6xyz = server.station("W6XYZ", "DM13")
        w6xyz.set_frequency(28330000)
        everyone = ["VK5DGR", "K1ABC", "W6XYZ"]

        def set_filter(app, column, operator, value):
            app.column_context_menu(LIST, column, choose=operator)
            app.wait_for_window("@Set Column Filter")
            app.set_value("@Set Column Filter/wxTextCtrl", value)
            app.click("@Set Column Filter/OK")
            app.wait_for_window_closed("@Set Column Filter")
            app.activate("reporter")

        def clear_filter(app, column):
            app.column_context_menu(LIST, column, choose="Clear Column Filter")
            wait_for_callsigns(app, everyone)

        for as_khz in [False, True]:
            settings = reporter_settings(server.hostname, **{"/Reporting/FrequencyAsKHz": as_khz})
            with freedvtest.start(settings=settings) as app:
                open_reporter(app)
                wait_for_callsigns(app, everyone)
                k1abc.receive("W6XYZ", "RADEV1", 12)
                vk5dgr.receive("W6XYZ", "RADEV1", -2)
                w6xyz.receive("K1ABC", "RADEV1", 5)
                wait_for_cell(app, "W6XYZ", RX_CALLSIGN, "K1ABC")
                wait_for_cell(app, "VK5DGR", RX_CALLSIGN, "W6XYZ")

                # Frequency, in the units that it's displayed in.
                twenty = "14236" if as_khz else "14.236"
                for operator, listed in [(">= (Greater or equal to)", ["K1ABC", "W6XYZ"]),
                                         (">  (Greater than)", ["W6XYZ"]),
                                         ("=  (Equal to)", ["K1ABC"]),
                                         ("!= (Not equal to)", ["VK5DGR", "W6XYZ"]),
                                         ("<  (Less than)", ["VK5DGR"]),
                                         ("<= (Less or equal to)", ["VK5DGR", "K1ABC"])]:
                    set_filter(app, FREQUENCY, operator, twenty)
                    wait_for_callsigns(app, listed)
                clear_filter(app, FREQUENCY)

                # Signal report
                set_filter(app, SNR, ">  (Greater than)", "4")
                wait_for_callsigns(app, ["K1ABC", "W6XYZ"])
                set_filter(app, SNR, "<  (Less than)", "0")
                wait_for_callsigns(app, ["VK5DGR"])
                clear_filter(app, SNR)

                # Heading, split between the two that are furthest round the
                # compass. (What's displayed is rounded, so not right on one.)
                headings = {row[CALLSIGN]: int(row[HEADING]) for row in app.rows(LIST)}
                lowest, middle, highest = sorted(headings.values())
                check(highest - middle > 2, "Headings are %r" % headings)
                threshold = (middle + highest) // 2
                set_filter(app, HEADING, ">= (Greater or equal to)", str(threshold))
                wait_for_callsigns(app, [callsign for callsign in everyone if headings[callsign] > threshold])
                set_filter(app, HEADING, "<  (Less than)", str(threshold))
                wait_for_callsigns(app, [callsign for callsign in everyone if headings[callsign] < threshold])
                clear_filter(app, HEADING)

                # A filter that isn't a number is ignored.
                set_filter(app, DISTANCE, ">  (Greater than)", "far")
                time.sleep(1)
                check_equal(callsigns(app), everyone, "Callsigns listed with a distance filter that isn't a number")
                clear_filter(app, DISTANCE)

                # A station that's selected and then filtered out isn't selected when it
                # comes back. (What's selected in the meantime depends on the version of
                # wxWidgets: nothing, or the row that took its place.)
                app.select_row(LIST, (CALLSIGN, "K1ABC"))
                check_equal([row[CALLSIGN] for row in app.rows(LIST) if row["selected"]], ["K1ABC"],
                            "Selected station")
                set_filter(app, CALLSIGN, "!= (Not equal to)", "K1ABC")
                wait_for_callsigns(app, ["VK5DGR", "W6XYZ"])
                print("Selected once K1ABC is filtered out: %r"
                      % [row[CALLSIGN] for row in app.rows(LIST) if row["selected"]], flush=True)
                clear_filter(app, CALLSIGN)
                check("K1ABC" not in [row[CALLSIGN] for row in app.rows(LIST) if row["selected"]],
                      "K1ABC is selected again on being listed again")

                quit_and_check_log(app)


def test_status_message_flags():
    """Regional flags in a status message are several characters that have
    to be removed together, or what's left of one isn't a flag any more."""
    black_flag = "\U0001F3F4"
    scotland = black_flag + "".join(chr(0xE0000 + ord(letter)) for letter in "gbsct") + "\U000E007F"

    with ReporterServer() as server:
        with freedvtest.start(settings=reporter_settings(server.hostname)) as app:
            open_reporter(app)

            # A whole flag is left alone, wherever it is.
            for message in [scotland, "Hello from " + scotland, scotland + " and " + scotland]:
                app.set_value("reporter.cbStatusMessage", message)
                check_equal(app.get("reporter.cbStatusMessage")["value"], message, "Status message with a flag")

            # One that's lost its end (as backspacing over it does) goes entirely.
            app.set_value("reporter.cbStatusMessage", "Hello from " + scotland[:-1] + "!")
            check_equal(app.get("reporter.cbStatusMessage")["value"], "Hello from !",
                        "Status message after part of a flag was removed")
            app.set_value("reporter.cbStatusMessage", scotland[:-2])
            check_equal(app.get("reporter.cbStatusMessage")["value"], "", "Status message that was only part of a flag")

            # As do parts of one that are left over.
            app.set_value("reporter.cbStatusMessage", "Hello" + scotland[2:4] + " there")
            check_equal(app.get("reporter.cbStatusMessage")["value"], "Hello there",
                        "Status message after the start of a flag was removed")

            # A plain black flag is a flag in its own right.
            app.set_value("reporter.cbStatusMessage", black_flag + " ahoy")
            check_equal(app.get("reporter.cbStatusMessage")["value"], black_flag + " ahoy", "Status message")

            quit_and_check_log(app)


def test_text_filters():
    """Filtering by each of the columns that hold text."""
    with ReporterServer() as server:
        k1abc = server.station("K1ABC", "FN42", version="alpha 3")
        k1abc.set_frequency(14236000)
        k1abc.set_message("Calling CQ")
        vk5dgr = server.station("VK5DGR", "PF95", version="alpha 1", rx_only=True)
        vk5dgr.set_frequency(7177000)
        w6xyz = server.station("W6XYZ", "DM13", version="alpha 2")
        w6xyz.set_frequency(28330000)
        w6xyz.set_message("Listening")
        everyone = ["VK5DGR", "K1ABC", "W6XYZ"]

        def set_filter(app, column, operator, value):
            app.column_context_menu(LIST, column, choose=operator)
            app.wait_for_window("@Set Column Filter")
            app.set_value("@Set Column Filter/wxTextCtrl", value)
            app.click("@Set Column Filter/OK")
            app.wait_for_window_closed("@Set Column Filter")
            app.activate("reporter")

        with freedvtest.start(settings=reporter_settings(server.hostname)) as app:
            open_reporter(app)
            wait_for_callsigns(app, everyone)
            k1abc.transmit("RADEV1", True)
            wait_for_cell(app, "K1ABC", STATUS, "TX")
            vk5dgr.receive("K1ABC", "RADEV1", 9)
            wait_for_cell(app, "VK5DGR", RX_CALLSIGN, "K1ABC")
            wait_for_cell(app, "W6XYZ", MESSAGE, "Listening")

            equal, not_equal, after = "=  (Equal to)", "!= (Not equal to)", ">  (Greater than)"
            for column, operator, value, listed in [
                    (LOCATOR, equal, "fn42", ["K1ABC"]),                # whatever its case
                    (LOCATOR, not_equal, "FN42", ["VK5DGR", "W6XYZ"]),
                    (VERSION, equal, "alpha 2", ["W6XYZ"]),
                    (VERSION, after, "alpha 1", ["K1ABC", "W6XYZ"]),    # in alphabetical order
                    (TX_MODE, equal, "RADEV1", ["K1ABC"]),
                    (STATUS, equal, "RX Only", ["VK5DGR"]),
                    (STATUS, equal, "TX", ["K1ABC"]),
                    (MESSAGE, equal, "listening", ["W6XYZ"]),
                    (MESSAGE, not_equal, "", ["K1ABC", "W6XYZ"]),
                    (RX_CALLSIGN, equal, "K1ABC", ["VK5DGR"]),
                    (RX_CALLSIGN, equal, "", ["K1ABC", "W6XYZ"])]:
                set_filter(app, column, operator, value)
                wait_for_callsigns(app, listed)
                app.column_context_menu(LIST, column, choose="Clear Column Filter")
                wait_for_callsigns(app, everyone)

            # The dates can be filtered by as well, as they're displayed.
            last_update = row_for(app, "W6XYZ")[LAST_UPDATE]
            set_filter(app, LAST_UPDATE, not_equal, last_update)
            app.wait_until(lambda: "W6XYZ" not in callsigns(app) or row_for(app, "W6XYZ")[LAST_UPDATE] != last_update,
                           timeout=15, description="W6XYZ to be filtered out by when it was last updated")
            app.column_context_menu(LIST, LAST_UPDATE, choose="Clear Column Filter")
            wait_for_callsigns(app, everyone)
            set_filter(app, LAST_TX, equal, "")
            wait_for_callsigns(app, ["VK5DGR", "W6XYZ"])
            app.column_context_menu(LIST, LAST_TX, choose="Clear Column Filter")
            wait_for_callsigns(app, everyone)

            quit_and_check_log(app)


def test_saved_filters():
    """The window opening with filters that were left on the last time."""
    with ReporterServer() as server:
        server.station("K1ABC", "FN42").set_frequency(14236000)
        server.station("VK5DGR", "PF95").set_frequency(7177000)
        server.station("W6XYZ", "DM13").set_frequency(14240000)

        saved = {"/Reporting/FreeDV/BandFilterTracksFrequency": True,
                 "/Reporting/FreeDV/BandFilterTracking/TracksFreqBand": True,
                 "/Reporting/FreeDV/BandFilterTracking/TracksExactFreq": False,
                 "/Reporting/FreeDV/EnableMaxIdleFilter": True,
                 "/Reporting/FreeDV/MaxIdleMinutes": 45,
                 "/Windows/FreeDVReporter/msgEditDialogWidth": 520,
                 "/Reporting/FreeDV/RecentStatusTexts": "73"}
        with freedvtest.start(settings=reporter_settings(server.hostname, **saved)) as app:
            open_reporter(app)

            # Following FreeDV's band (20m), so only who's on it.
            wait_for_callsigns(app, ["K1ABC", "W6XYZ"])
            check_equal(app.get("reporter.ckTrackFrequency")["value"], True, "Track checked")
            check_equal(app.get("reporter.cbBandFilter")["enabled"], False, "Band filter enabled while tracking")
            check_equal((app.get("reporter.rbTrackFreqBand")["value"], app.get("reporter.rbTrackExactFreq")["value"]),
                        (True, False), "Tracking by band and by exact frequency")

            # An idle time that isn't one of those on offer counts as custom.
            check_equal(label(app, "reporter.stFilterStatus"), "Idle 45", "Idle filter status")
            idle_menu = "Filter/Idle more than (minutes).../"
            for item in ["Disabled", "30", "60", "90", "120", "Custom..."]:
                check_equal(app.menu_checked(idle_menu + item, window="reporter"), item == "Custom...",
                            "Idle filter '%s' checked" % item)

            # Following the exact frequency instead leaves just the one.
            app.set_value("reporter.rbTrackExactFreq", True)
            wait_for_callsigns(app, ["K1ABC"])
            app.set_value("reporter.rbTrackFreqBand", True)
            wait_for_callsigns(app, ["K1ABC", "W6XYZ"])

            # Not following it at all gives the band filter back.
            app.set_value("reporter.ckTrackFrequency", False)
            check_equal(app.get("reporter.cbBandFilter")["enabled"], True, "Band filter enabled without tracking")
            wait_for_callsigns(app, ["VK5DGR", "K1ABC", "W6XYZ"])

            # The window for a new saved message comes back as wide as it was left.
            app.combo_popup("reporter.cbStatusMessage", True)
            app.wait_until(lambda: app.get("reporter.lbSavedMessages")["shown"],
                           description="the saved messages to be listed")
            app.context_menu("reporter.lbSavedMessages", item=0, choose="Add new message")
            dialog = app.wait_for_window("@Add Message")
            check_equal(dialog["label"], "Add Message", "Window title")
            app.click("@Add Message/Cancel")
            app.wait_for_window_closed("@Add Message")
            app.activate("reporter")

            quit_and_check_log(app)


def test_selection():
    """What unselects a station, other than selecting another."""
    with audio_devices(), ReporterServer() as server:
        k1abc = server.station("K1ABC", "FN42")
        k1abc.set_frequency(14240000)
        server.station("VK5DGR", "PF95").set_frequency(7177000)

        settings = reporter_settings(server.hostname)
        settings.update(audio_settings())
        with freedvtest.start(settings=settings) as app:
            open_reporter(app)
            wait_for_callsigns(app, ["VK5DGR", "K1ABC"])
            start_modem(app)
            app.wait_until(lambda: CALLSIGN_OF_FREEDV in callsigns(app), description="FreeDV to be listed")

            def selected():
                return [row[CALLSIGN] for row in app.rows(LIST) if row["selected"]]

            def select(callsign):
                app.select_row(LIST, (CALLSIGN, callsign))
                check_equal(selected(), [callsign], "Selected station")
                check_equal(app.get("reporter.btnSendQSY")["enabled"], True, "Request QSY enabled")

            # Clicking somewhere else in the window
            select("K1ABC")
            app.mouse("reporter", "left_down", x=0.5, y=0.02)
            app.wait_until(lambda: selected() == [], timeout=5, description="K1ABC to be unselected")
            check_equal(app.get("reporter.btnSendQSY")["enabled"], False, "Request QSY enabled with nobody selected")

            # Clicking on a column's heading
            select("VK5DGR")
            app.click_column(LIST, CALLSIGN)
            app.wait_until(lambda: selected() == [], timeout=5, description="VK5DGR to be unselected")
            check_equal(app.get("reporter.btnSendQSY")["enabled"], False, "Request QSY enabled after sorting")

            # The station leaving. (What's selected afterwards depends on
            # the version of wxWidgets, but it isn't them.)
            select("K1ABC")
            k1abc.disconnect()
            app.wait_until(lambda: "K1ABC" not in callsigns(app), timeout=15, description="K1ABC to go")
            check("K1ABC" not in selected(), "K1ABC is still selected after leaving")

            stop_modem(app)
            quit_and_check_log(app)


def test_window():
    """Opening and closing the window, and it remembering how it was left."""
    config_dir = tempfile.mkdtemp(prefix="freedvtest-config-")
    try:
        config = os.path.join(config_dir, "freedv.conf")
        with ReporterServer() as server:
            for callsign, frequency in [("K1ABC", 14236000), ("W2DEF", 14240000), ("VK5DGR", 7177000)]:
                server.station(callsign, "FN42").set_frequency(frequency)

            freedvtest.write_config(config, reporter_settings(server.hostname))
            with freedvtest.start(config=config) as app:
                app.wait_for_window("main")
                time.sleep(1)
                check(not app.window_exists("reporter"), "The window opened by itself")

                open_reporter(app)
                check_equal(app.get("reporter")["label"], "FreeDV Reporter (freedv.conf)", "Window title")
                wait_for_callsigns(app, ["VK5DGR", "K1ABC", "W2DEF"])

                # The Close button and the title bar's close button both
                # just hide it.
                app.click("reporter.btnClose")
                app.wait_for_window_closed("reporter")
                open_reporter(app)
                wait_for_callsigns(app, ["VK5DGR", "K1ABC", "W2DEF"])
                app.close("reporter")
                app.wait_for_window_closed("reporter")
                open_reporter(app)

                # Leave it open, sorted and filtered, with a column hidden.
                app.click_column(LIST, CALLSIGN)
                app.click_column(LIST, CALLSIGN)
                app.set_value("reporter.cbBandFilter", "20 m")
                app.menu("Show/Version", window="reporter")
                wait_for_callsigns(app, ["W2DEF", "K1ABC"])
                quit_and_check_log(app)

            with freedvtest.start(config=config) as app:
                # It was open, so it opens again, just as it was.
                app.wait_for_window("reporter")
                app.activate("reporter")
                check_equal(sort_order(app), (CALLSIGN, False), "Sort order after restarting")
                check_equal(app.get("reporter.cbBandFilter")["value"], "20 m", "Band filter after restarting")
                check("Version" not in column_titles(app), "The hidden column is back after restarting")
                check_equal(app.menu_checked("Show/Version", window="reporter"), False, "Show/Version checked")
                wait_for_callsigns(app, ["W2DEF", "K1ABC"])

                # Closed this time, so it should stay closed.
                app.click("reporter.btnClose")
                app.wait_for_window_closed("reporter")
                quit_and_check_log(app)

            with freedvtest.start(config=config) as app:
                app.wait_for_window("main")
                time.sleep(1)
                check(not app.window_exists("reporter"), "The window opened by itself after being closed")
                quit_and_check_log(app)
    finally:
        shutil.rmtree(config_dir, ignore_errors=True)


def test_damaged_layout():
    """Saved column orders and window positions that make no sense (as from
    another version of FreeDV, or a screen that's no longer there)."""
    config_dir = tempfile.mkdtemp(prefix="freedvtest-config-")
    try:
        config = os.path.join(config_dir, "freedv.conf")
        with ReporterServer() as server:
            server.station("K1ABC", "FN42").set_frequency(14236000)

            freedvtest.write_config(config, reporter_settings(server.hostname))
            with freedvtest.start(config=config) as app:
                open_reporter(app)
                wait_for_callsigns(app, ["K1ABC"])
                titles = column_titles(app, shown_only=False)
                quit_and_check_log(app)
            saved = freedvtest.read_config(config)
            order = [entry for entry in str(saved["/Reporting/FreeDV/ColumnOrder"]).split(",") if entry != ""]
            check_equal(len(order), len(titles), "Columns in the saved order (%r)" % saved["/Reporting/FreeDV/ColumnOrder"])

            # Columns that don't exist, one that's there twice and so one
            # that's missing; and then only a few of them.
            for damaged, where in [(order[:3] + ["99", order[3]] + order[3:-1], (30000, 30000)),
                                   (order[5:8], (-30000, -500)),
                                   (list(reversed(order)), (40, 50))]:
                saved["/Reporting/FreeDV/ColumnOrder"] = ",".join(damaged)
                saved["/Windows/FreeDVReporter/left"], saved["/Windows/FreeDVReporter/top"] = where
                freedvtest.write_config(config, saved)
                with freedvtest.start(config=config) as app:
                    app.wait_for_window("reporter")
                    app.activate("reporter")
                    wait_for_callsigns(app, ["K1ABC"])
                    check_equal(sorted(column_titles(app, shown_only=False)), sorted(titles),
                                "Columns with a saved order of %s" % ",".join(damaged))
                    check_equal(row_for(app, "K1ABC")[LOCATOR], "FN42", "K1ABC's locator")
                    quit_and_check_log(app)
                saved = freedvtest.read_config(config)
                left, top = int(saved["/Windows/FreeDVReporter/left"]), int(saved["/Windows/FreeDVReporter/top"])
                check(-100 <= left <= 4096 and -100 <= top <= 4096,
                      "Asked to be at %r, the window was left at %d,%d" % (where, left, top))
    finally:
        shutil.rmtree(config_dir, ignore_errors=True)


def test_connection_burst():
    """Attempts to reproduce issue #1495: FreeDV crashing in this window
    while a lot of stations come and go (including going and coming straight
    back, which reuses their place in the list) and when it reconnects to a
    busy server.

    Runs for $FREEDV_BURST_SECONDS (default 20) seconds per round. What's
    done is random but the same from run to run; set $FREEDV_BURST_SEED to
    try something else.
    """
    duration = float(os.environ.get("FREEDV_BURST_SECONDS", "20"))
    rng = random.Random(int(os.environ.get("FREEDV_BURST_SEED", "1495")))
    grid_squares = ["FN42", "PF95", "DM13", "IO91", "JN58", "PM95", "QF22", "EM48", "KP20", "GG87"]
    frequencies = [1997000, 3625000, 3693000, 5403500, 7177000, 7197000, 14236000, 14240000, 18118000, 21313000,
                   24933000, 28330000, 28720000, 50160000]

    with ReporterServer() as server:
        with freedvtest.start(settings=reporter_settings(server.hostname)) as app:
            open_reporter(app)

            stations = {}  # by callsign

            def connect(callsign):
                station = server.station(callsign, rng.choice(grid_squares), rx_only=rng.random() < 0.2)
                station.set_frequency(rng.choice(frequencies))
                stations[callsign] = station

            def check_list():
                """FreeDV should still be there, listing exactly who's on."""
                expected = sorted(stations)
                try:
                    app.wait_until(lambda: sorted(callsigns(app)) == expected, timeout=60)
                except freedvtest.FreeDVError:
                    check_equal(sorted(callsigns(app)), expected, "Callsigns listed")

            def churn(seconds):
                actions = 0
                deadline = time.monotonic() + seconds
                while time.monotonic() < deadline:
                    callsign = rng.choice(sorted(stations)) if stations else None
                    action = rng.random()
                    if callsign is None or (action < 0.15 and len(stations) < 60):
                        new_callsign = "W%d%s" % (rng.randrange(10), "".join(rng.choice("ABCDEFGH") for _ in range(3)))
                        if new_callsign not in stations:
                            connect(new_callsign)
                    elif action < 0.25:
                        stations.pop(callsign).disconnect()
                    elif action < 0.40:
                        # Gone and straight back as the same connection...
                        stations[callsign].hide()
                        if rng.random() < 0.5:
                            time.sleep(rng.random() * 1.5)
                        stations[callsign].show()
                    elif action < 0.50:
                        # ...or as a new one.
                        stations.pop(callsign).disconnect()
                        connect(callsign)
                    elif action < 0.65:
                        stations[callsign].set_frequency(rng.choice(frequencies))
                    elif action < 0.75:
                        stations[callsign].transmit("RADEV1", rng.random() < 0.5)
                    elif action < 0.82:
                        stations[callsign].receive(rng.choice(sorted(stations)), "RADEV1", rng.randrange(-5, 30))
                    elif action < 0.88:
                        stations[callsign].set_message(rng.choice(["", "CQ", "QRV", "testing 1 2 3"]))
                    elif action < 0.94:
                        # Things that make the list sort or filter itself again.
                        app.click_column(LIST, rng.choice([CALLSIGN, DISTANCE, FREQUENCY, STATUS, LAST_UPDATE]))
                    elif action < 0.97:
                        app.set_value("reporter.cbBandFilter", rng.choice(["All", "20 m", "40 m", "All"]))
                    else:
                        app.rows(LIST)

                    actions += 1
                    if actions % 50 == 0:
                        app.command("ping")
                return actions

            # A busy server to begin with...
            for number in range(40):
                connect("K%dAB%s" % (number % 10, "ABCDEFGHIJ"[number // 10]))
            check_list()

            for round_number in range(4):
                actions = churn(duration)
                print("Round %d: %d actions, %d stations on" % (round_number + 1, actions, len(stations)), flush=True)
                app.set_value("reporter.cbBandFilter", "All")
                check_list()

                if round_number in (0, 2):
                    # ...which FreeDV then loses its connection to. It
                    # reconnects after a few seconds to find mostly the same
                    # stations as before, still coming and going.
                    server.interrupt_freedv()
                    churn(8)
                    app.set_value("reporter.cbBandFilter", "All")
                    check_list()
                elif round_number == 1:
                    # Or the server itself goes away, so that everyone has to
                    # reconnect to it at once.
                    stations.clear()
                    server.restart()
                    for number in range(40):
                        connect("N%dXY%s" % (number % 10, "ABCDEFGHIJ"[number // 10]))
                    check_list()

            for callsign in sorted(stations):
                stations.pop(callsign).disconnect()
            check_list()

            quit_and_check_log(app)


SCENARIOS = {
    "live_view": test_live_view,
    "stations": test_stations,
    "sorting": test_sorting,
    "sorted_updates": test_sorted_updates,
    "damaged_layout": test_damaged_layout,
    "display_options": test_display_options,
    "band_filter": test_band_filter,
    "column_filters": test_column_filters,
    "idle_filter": test_idle_filter,
    "columns": test_columns,
    "status_message": test_status_message,
    "qsy": test_qsy,
    "row_actions": test_row_actions,
    "turn_off": test_turn_off,
    "off_at_start": test_off_at_start,
    "qsy_with_radio": test_qsy_with_radio,
    "new_callsign": test_new_callsign,
    "sort_every_column": test_sort_every_column,
    "numeric_filters": test_numeric_filters,
    "status_message_flags": test_status_message_flags,
    "text_filters": test_text_filters,
    "saved_filters": test_saved_filters,
    "selection": test_selection,
    "window": test_window,
    "connection_burst": test_connection_burst,
}


def main():
    names = sys.argv[1:] or list(SCENARIOS)
    for name in names:
        if name not in SCENARIOS:
            sys.exit("Unknown scenario '%s'. Scenarios: %s" % (name, ", ".join(SCENARIOS)))
    for name in names:
        print("Running %s..." % name, flush=True)
        try:
            SCENARIOS[name]()
        except freedvtest.audio.Unavailable as e:
            # What it needs of the audio devices can't be had here (see freedvtest.audio).
            print("SKIPPED: %s" % e, flush=True)
            sys.exit(77)
    print("PASS")


if __name__ == "__main__":
    main()
