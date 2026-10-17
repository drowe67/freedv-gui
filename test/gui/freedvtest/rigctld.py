"""A radio for FreeDV to control during tests: Hamlib's rigctld serving its
"Dummy" rig, which FreeDV reaches as a "NET rigctl" radio.

    with Rigctld() as radio:
        # FreeDV set up with /Hamlib/RigNameStr = Rigctld.RIG_NAME and
        # /Hamlib/SerialPort = radio.address
        ...
        assert radio.frequency() == 14236000
"""

import os
import shutil
import socket
import subprocess
import time


class RigctldError(Exception):
    pass


def find_rigctld():
    """Returns where rigctld is: $RIGCTLD, on the PATH or (with Hamlib having
    been built along with FreeDV) in the build folder. None if it isn't."""
    candidates = [os.environ.get("RIGCTLD"), shutil.which("rigctld"),
                  os.path.join(os.getcwd(), "external", "dist", "bin", "rigctld")]
    for candidate in candidates:
        if candidate and os.access(candidate, os.X_OK):
            return candidate
    return None


class _NoReply(RigctldError):
    pass


class Rigctld:
    """Runs rigctld for the duration of a "with" block."""

    RIG_NAME = "Hamlib NET rigctl"

    def __init__(self):
        self._process = None
        self.port = None

    @property
    def address(self):
        return "127.0.0.1:%d" % self.port

    def __enter__(self):
        program = find_rigctld()
        if program is None:
            raise RigctldError("rigctld was not found")

        # A port that's free now is one that it can have.
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
            probe.bind(("127.0.0.1", 0))
            self.port = probe.getsockname()[1]
        self.start(program)
        return self

    def start(self, program=None):
        """Starts rigctld (again, after stop())."""
        self._process = subprocess.Popen(
            # (Some versions don't answer whether the radio's transmitting
            # unless told how it's keyed.)
            [program or find_rigctld(), "-m", "1", "-P", "RIG", "-T", "127.0.0.1", "-t", str(self.port)],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        deadline = time.monotonic() + 15
        while True:
            try:
                self.command("f")
                return
            except (OSError, RigctldError):
                if self._process.poll() is not None:
                    raise RigctldError("rigctld exited with code %d" % self._process.returncode)
                if time.monotonic() > deadline:
                    self.stop()
                    raise RigctldError("rigctld did not start listening")
                time.sleep(0.2)

    def stop(self):
        """Stops rigctld, as if the radio had been turned off or unplugged."""
        if self._process is not None:
            self._process.kill()
            self._process.wait()
            self._process = None

    def __exit__(self, exc_type, exc_value, traceback):
        self.stop()
        return False

    def command(self, text):
        """Sends rigctld a command of its own (see its manual), separately
        from FreeDV's connection to it, and returns the lines of its reply."""
        # It doesn't always take kindly to a second visitor straight away,
        # and can be slow to answer while FreeDV's asking it things too (its
        # stand-in radio takes its time over each question).
        for attempt in range(5):
            try:
                return self._command(text)
            except (OSError, _NoReply):
                if attempt == 4:
                    raise
                time.sleep(0.3)

    def _command(self, text):
        with socket.create_connection(("127.0.0.1", self.port), timeout=5) as connection:
            connection.sendall(text.encode() + b"\n")
            connection.settimeout(3)
            reply = b""
            try:
                while not reply.endswith(b"\n") or (text[0].isupper() and b"RPRT" not in reply):
                    data = connection.recv(4096)
                    if not data:
                        break
                    reply += data
                    if not text[0].isupper():
                        # Replies to queries are complete once nothing more arrives.
                        connection.settimeout(0.3)
            except socket.timeout:
                pass
        lines = reply.decode(errors="replace").split()
        if not lines:
            raise _NoReply("No reply from rigctld to '%s'" % text)
        if lines[0] == "RPRT" and lines[-1] != "0":
            raise RigctldError("rigctld replied '%s' to '%s'" % (" ".join(lines), text))
        return lines

    def frequency(self):
        """Returns the frequency that the radio is on, in Hz."""
        return int(float(self.command("f")[0]))

    def set_frequency(self, frequency_hz):
        """Changes frequency, as turning the radio's dial would."""
        self.command("F %d" % frequency_hz)

    def mode(self):
        return self.command("m")[0]

    def transmitting(self):
        return self.command("t")[0] != "0"
