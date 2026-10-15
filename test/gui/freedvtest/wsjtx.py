"""Stands in for a logging program that takes contacts from WSJT-X, which
is the form FreeDV sends them in.

    with WsjtxListener() as logger:
        # FreeDV set up with /Reporting/UDP/Port = logger.port
        ...
        contact = logger.wait_for(WsjtxListener.QSO_LOGGED)
        assert contact["dx_call"] == "K1ABC"
"""

import datetime
import socket
import struct
import threading
import time

MAGIC = 0xadbccbda

# 1970-01-01 as a Julian day number, which is how dates are sent.
_JULIAN_DAY_OF_UNIX_EPOCH = 2440588


class WsjtxError(Exception):
    pass


class _Reader:
    def __init__(self, data):
        self._data = data
        self._position = 0

    def _take(self, layout):
        size = struct.calcsize(layout)
        if self._position + size > len(self._data):
            raise WsjtxError("Message from FreeDV is too short")
        value = struct.unpack_from(layout, self._data, self._position)[0]
        self._position += size
        return value

    def uint8(self):
        return self._take(">B")

    def boolean(self):
        return self._take(">B") != 0

    def uint32(self):
        return self._take(">I")

    def int32(self):
        return self._take(">i")

    def uint64(self):
        return self._take(">Q")

    def double(self):
        return self._take(">d")

    def string(self):
        length = self.uint32()
        if length == 0xffffffff:
            return ""
        if self._position + length > len(self._data):
            raise WsjtxError("Message from FreeDV is too short")
        value = self._data[self._position:self._position + length].decode("utf-8")
        self._position += length
        return value

    def time(self):
        """A date and time, returned as seconds since 1970 (UTC)."""
        day = self._take(">q")
        milliseconds = self.uint32()
        time_spec = self.uint8()
        if time_spec != 1:
            raise WsjtxError("Time from FreeDV isn't given as UTC (%d)" % time_spec)
        return (day - _JULIAN_DAY_OF_UNIX_EPOCH) * 86400 + milliseconds / 1000.0

    def finished(self):
        return self._position == len(self._data)


def parse(data):
    """Returns a message from FreeDV as a dict: its "type", the "id" of who
    sent it and whatever else that type of message has."""
    reader = _Reader(data)
    if reader.uint32() != MAGIC:
        raise WsjtxError("Message from FreeDV doesn't start as it should")
    message = {"schema": reader.uint32(), "type": reader.uint32(), "id": reader.string()}

    if message["type"] == WsjtxListener.HEARTBEAT:
        message["max_schema"] = reader.uint32()
        message["version"] = reader.string()
        message["revision"] = reader.string()
    elif message["type"] == WsjtxListener.STATUS:
        message["frequency"] = reader.uint64()
        message["mode"] = reader.string()
        message["dx_call"] = reader.string()
        message["report"] = reader.string()
        message["tx_mode"] = reader.string()
        message["tx_enabled"] = reader.boolean()
        message["transmitting"] = reader.boolean()
        message["decoding"] = reader.boolean()
        message["rx_df"] = reader.uint32()
        message["tx_df"] = reader.uint32()
        message["my_call"] = reader.string()
        message["my_grid"] = reader.string()
        message["dx_grid"] = reader.string()
        message["tx_watchdog"] = reader.boolean()
        message["submode"] = reader.string()
        message["fast_mode"] = reader.boolean()
        message["special_operation_mode"] = reader.uint8()
        message["frequency_tolerance"] = reader.uint32()
        message["tr_period"] = reader.uint32()
        message["configuration_name"] = reader.string()
        message["tx_message"] = reader.string()
    elif message["type"] == WsjtxListener.DECODE:
        message["new"] = reader.boolean()
        message["time_ms"] = reader.uint32()
        message["snr"] = reader.int32()
        message["delta_time"] = reader.double()
        message["delta_frequency"] = reader.uint32()
        message["mode"] = reader.string()
        message["message"] = reader.string()
        message["low_confidence"] = reader.boolean()
        message["off_air"] = reader.boolean()
    elif message["type"] == WsjtxListener.QSO_LOGGED:
        message["time_off"] = reader.time()
        message["dx_call"] = reader.string()
        message["dx_grid"] = reader.string()
        message["frequency"] = reader.uint64()
        message["mode"] = reader.string()
        message["report_sent"] = reader.string()
        message["report_received"] = reader.string()
        message["tx_power"] = reader.string()
        message["comments"] = reader.string()
        message["name"] = reader.string()
        message["time_on"] = reader.time()
        message["operator_call"] = reader.string()
        message["my_call"] = reader.string()
        message["my_grid"] = reader.string()
        message["exchange_sent"] = reader.string()
        message["exchange_received"] = reader.string()
        message["propagation_mode"] = reader.string()
    else:
        return message

    if not reader.finished():
        raise WsjtxError("Message of type %d from FreeDV is longer than it should be" % message["type"])
    return message


def utc(seconds):
    """Returns a time from a message as text, e.g. "2026-01-31 12:34:56"."""
    moment = datetime.datetime.fromtimestamp(seconds, datetime.timezone.utc)
    return moment.strftime("%Y-%m-%d %H:%M:%S")


class WsjtxListener:
    """Listens on a UDP port of its own for the duration of a "with" block
    and collects what FreeDV sends there."""

    HEARTBEAT = 0
    STATUS = 1
    DECODE = 2
    QSO_LOGGED = 5

    def __init__(self):
        self.port = None
        self.errors = []
        self._socket = None
        self._thread = None
        self._lock = threading.Lock()
        self._messages = []

    def __enter__(self):
        self._socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._socket.bind(("127.0.0.1", 0))
        self._socket.settimeout(0.2)
        self.port = self._socket.getsockname()[1]
        self._thread = threading.Thread(target=self._receive, daemon=True)
        self._thread.start()
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        listening, self._socket = self._socket, None
        self._thread.join()
        listening.close()
        return False

    def _receive(self):
        while True:
            listening = self._socket
            if listening is None:
                return
            try:
                data = listening.recv(65536)
            except socket.timeout:
                continue
            except OSError:
                return
            try:
                message = parse(data)
            except WsjtxError as e:
                with self._lock:
                    self.errors.append(str(e))
                continue
            with self._lock:
                self._messages.append(message)

    def messages(self, message_type=None, clear=False):
        """Returns what's arrived so far (of one type, if given), oldest
        first, optionally forgetting all of it afterwards."""
        with self._lock:
            result = [m for m in self._messages if message_type is None or m["type"] == message_type]
            if clear:
                self._messages = []
        return result

    def wait_for(self, message_type, timeout=10):
        """Waits for a message of the given type and returns the first."""
        deadline = time.time() + timeout
        while time.time() < deadline:
            found = self.messages(message_type)
            if found:
                return found[0]
            time.sleep(0.05)
        raise WsjtxError("No message of type %d arrived from FreeDV within %d s (errors: %r)"
                         % (message_type, timeout, self.errors))
