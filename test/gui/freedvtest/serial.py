"""Serial ports for FreeDV to use during tests (Linux only).

These are pseudo-terminals, which FreeDV can open as it would a real port.
What they lack is a real port's control lines, so FreeDV is run with a
small library (serial_shim.c, built here with the system's C compiler) that
keeps track of the ones it sets and lets the test set the one it reads.

    with SerialPorts() as ports:
        radio = ports.add()
        with freedvtest.start(settings={"/Rig/Port": radio.name, ...}, env=ports.env) as app:
            ...
            assert radio.rts
"""

import os
import shutil
import subprocess
import tempfile


class SerialError(Exception):
    pass


class SerialPort:
    def __init__(self, directory):
        self._control, self._port = os.openpty()
        self.name = os.ttyname(self._port)
        self._prefix = os.path.join(directory, self.name.rsplit("/", 1)[1])
        # Whatever had this name before is nothing to do with this port.
        for suffix in (".lines", ".history", ".cts"):
            if os.path.exists(self._prefix + suffix):
                os.remove(self._prefix + suffix)

    def close(self):
        os.close(self._port)
        os.close(self._control)

    @staticmethod
    def _parse(line):
        return {key.lower(): value == "1" for key, value in (item.split("=") for item in line.split())}

    def history(self):
        """Returns every state FreeDV has put RTS and DTR in, oldest first,
        each as a dict with "rts" and "dtr"."""
        try:
            with open(self._prefix + ".history") as f:
                return [self._parse(line) for line in f.read().splitlines() if line]
        except OSError:
            return []

    @property
    def lines(self):
        """RTS and DTR as FreeDV last set them (both off if it never has)."""
        history = self.history()
        return history[-1] if history else {"rts": False, "dtr": False}

    @property
    def rts(self):
        return self.lines["rts"]

    @property
    def dtr(self):
        return self.lines["dtr"]

    def set_cts(self, raised):
        """Sets what FreeDV reads CTS as."""
        with open(self._prefix + ".cts.new", "w") as f:
            f.write("1" if raised else "0")
        os.replace(self._prefix + ".cts.new", self._prefix + ".cts")


class SerialPorts:
    """Provides serial ports for the duration of a "with" block. FreeDV needs
    starting with env for their control lines to work."""

    def __init__(self):
        self._directory = None
        self._ports = []
        self.env = {}

    def __enter__(self):
        self._directory = tempfile.mkdtemp(prefix="freedvtest-serial-")
        try:
            compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
            if compiler is None:
                raise SerialError("A C compiler is needed for serial port tests")
            shim = os.path.join(self._directory, "serial_shim.so")
            source = os.path.join(os.path.dirname(os.path.abspath(__file__)), "serial_shim.c")
            result = subprocess.run([compiler, "-shared", "-fPIC", "-O1", "-o", shim, source, "-ldl", "-lpthread"],
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            if result.returncode != 0:
                raise SerialError("Could not build serial_shim.c:\n" + result.stdout.decode(errors="replace"))
        except BaseException:
            shutil.rmtree(self._directory, ignore_errors=True)
            raise

        self.env = {"LD_PRELOAD": shim, "FREEDVTEST_SERIAL_DIR": self._directory}
        # A FreeDV built with the address sanitizer expects that to be what's
        # loaded first.
        options = os.environ.get("ASAN_OPTIONS", "")
        self.env["ASAN_OPTIONS"] = (options + ":" if options else "") + "verify_asan_link_order=0"
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        for port in self._ports:
            port.close()
        self._ports = []
        shutil.rmtree(self._directory, ignore_errors=True)
        return False

    def add(self):
        """Creates a port and returns it; its name is what FreeDV is given."""
        port = SerialPort(self._directory)
        self._ports.append(port)
        return port
