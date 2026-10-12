"""Drives FreeDV's GUI from test scripts.

start() launches FreeDV and returns an object that performs UI actions (e.g.
clicking a button) and retrieves the state of the UI and of FreeDV itself:

    import freedvtest

    with freedvtest.start() as app:
        app.wait_for_window("wizard")
        app.click("wizard.btnNext")
        ...
        app.click("main.btnOnOff")
        app.wait_until(lambda: app.state()["running"])
        app.quit()

Controls are referred to by the names given to them in FreeDV's source code
with SetName(); widgets() lists those in a window. A failed check or action
raises an exception, so a script that runs to completion has passed.

FreeDV must have been built with -DUNITTEST=1.
"""

import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time


class FreeDVError(Exception):
    """FreeDV could not be started, stopped responding or exited."""


class CommandError(FreeDVError):
    """FreeDV could not carry out a command, e.g. clicking a disabled button."""


def default_binary():
    """Returns the FreeDV to test: $FREEDV_BINARY, or the one in the build
    folder that the script was run from."""
    binary = os.environ.get("FREEDV_BINARY")
    if binary:
        return binary
    if sys.platform == "darwin":
        return "src/FreeDV.app/Contents/MacOS/FreeDV"
    return "src/freedv"


def start(binary=None, config=None, args=(), env=None, user_config_files=None, timeout=60):
    """Launches FreeDV and returns once it's ready to take commands.

    config is the configuration file to use. If not given, FreeDV starts
    without one, as it does when run for the first time.

    user_config_files maps names to contents of files to place where FreeDV
    looks for other programs' settings (see FreeDV.user_config_dir). FreeDV
    never sees the real ones.
    """
    return FreeDV(binary or default_binary(), config, args, env, user_config_files, timeout)


class FreeDV:
    """A running copy of FreeDV. Create with start()."""

    def __init__(self, binary, config, args, env, user_config_files, timeout):
        self._temp_dir = tempfile.mkdtemp(prefix="freedvtest-")
        self._next_id = 1
        self._received = b""
        self._connection = None
        self._process = None
        self._log_file = None

        #: The configuration file that FreeDV is using.
        self.config_path = config or os.path.join(self._temp_dir, "freedv.conf")
        #: Everything that FreeDV has printed.
        self.log_path = os.path.join(self._temp_dir, "freedv.log")
        #: Stands in for the user's ~/.config (Linux only), which is where
        #: FreeDV looks for other programs' settings.
        self.user_config_dir = os.path.join(self._temp_dir, "config")

        process_env = dict(os.environ)
        # Labels checked by test scripts are the English ones.
        process_env.update({"LANGUAGE": "en", "LC_ALL": "C.UTF-8", "LANG": "C.UTF-8"})
        # Keep FreeDV away from settings belonging to whoever is running the
        # test, e.g. those of other programs that the Setup Wizard can import.
        os.mkdir(self.user_config_dir)
        for name, contents in (user_config_files or {}).items():
            with open(os.path.join(self.user_config_dir, name), "w") as f:
                f.write(contents)
        process_env["XDG_CONFIG_HOME"] = self.user_config_dir
        process_env.update(env or {})

        listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            listener.bind(("127.0.0.1", 0))
            listener.listen(1)
            listener.settimeout(0.2)
            port = listener.getsockname()[1]

            self._log_file = open(self.log_path, "wb")
            self._process = subprocess.Popen(
                [binary, "-f", self.config_path, "-testport", str(port)] + list(args),
                stdin=subprocess.DEVNULL, stdout=self._log_file, stderr=subprocess.STDOUT,
                env=process_env)

            deadline = time.monotonic() + timeout
            while self._connection is None:
                try:
                    self._connection, _ = listener.accept()
                except socket.timeout:
                    if self._process.poll() is not None:
                        raise FreeDVError(
                            "FreeDV exited with code %d before connecting:\n%s"
                            % (self._process.returncode, self.log()))
                    if time.monotonic() > deadline:
                        raise FreeDVError(
                            "FreeDV did not connect within %d seconds:\n%s" % (timeout, self.log()))
            self._connection.settimeout(timeout)
            self.command("ping")
        except BaseException:
            self.stop()
            raise
        finally:
            listener.close()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        if exc_type is not None and not issubclass(exc_type, (KeyboardInterrupt, SystemExit)):
            sys.stderr.write("----- FreeDV output -----\n%s\n-------------------------\n" % self.log())
        self.stop()
        return False

    # -- Commands ---------------------------------------------------------

    def command(self, cmd, **params):
        """Sends a command to FreeDV and returns its result."""
        if self._connection is None:
            raise FreeDVError("FreeDV is not running")

        request = {"id": self._next_id, "cmd": cmd}
        request.update({key: value for key, value in params.items() if value is not None})
        self._next_id += 1

        try:
            self._connection.sendall(json.dumps(request).encode("utf-8") + b"\n")
            while b"\n" not in self._received:
                data = self._connection.recv(65536)
                if not data:
                    raise FreeDVError("FreeDV closed the connection during '%s'" % cmd)
                self._received += data
        except OSError as e:
            raise FreeDVError("No response from FreeDV to '%s': %s" % (cmd, e))

        line, self._received = self._received.split(b"\n", 1)
        response = json.loads(line.decode("utf-8"))
        if response.get("id") != request["id"]:
            raise FreeDVError("Unexpected response to '%s': %r" % (cmd, response))
        if not response.get("ok"):
            raise CommandError(response.get("error", "unknown error"))
        return response.get("result")

    def windows(self):
        """Returns FreeDV's top-level windows (the main window and dialogs)."""
        return self.command("list_windows")

    def window_exists(self, name):
        """Returns whether the given top-level window is currently displayed."""
        return any(window["name"] == name and window["shown"] for window in self.windows())

    def widgets(self, window, named_only=True):
        """Returns the controls inside the given window. Useful for finding
        out what a test script can refer to."""
        widgets = self.command("list_widgets", name=window)
        if named_only:
            # Controls that weren't given a name have wxWidgets' default one.
            widgets = [widget for widget in widgets if "." in widget["name"]]
        return widgets

    def get(self, name):
        """Returns the state of a window or control: its class, label,
        whether it's enabled and shown and, depending on what it is, its
        value, selection and items."""
        return self.command("get", name=name)

    def click(self, name):
        """Clicks a button, toggle button, checkbox or radio button."""
        self.command("click", name=name)

    def set_value(self, name, value=None, data=None, index=None):
        """Sets a control's value: True/False for checkboxes and toggle
        buttons, or text for text fields and combo boxes. Items in lists can
        also be chosen by position (index) or by the string attached to them
        (data), e.g. an audio device's internal name."""
        self.command("set_value", name=name, value=value, data=data, index=index)

    def close(self, name):
        """Closes a top-level window as its title bar's close button would."""
        self.command("close", name=name)

    def menu(self, path):
        """Selects a menu item in the main window, e.g. "Settings/Setup
        Wizard..."."""
        self.command("menu", path=path)

    def state(self):
        """Returns FreeDV's own state (e.g. whether the modem is running)
        and its current configuration."""
        return self.command("get_state")

    def message_boxes(self, clear=False):
        """Returns the message boxes FreeDV has tried to display so far.
        They aren't actually displayed; FreeDV carries on as if their default
        button had been pressed."""
        return self.command("message_boxes", clear=clear)

    # -- Waiting ----------------------------------------------------------

    def wait_until(self, condition, timeout=30, description=None):
        """Waits for condition() to return something true, and returns it."""
        deadline = time.monotonic() + timeout
        while True:
            result = condition()
            if result:
                return result
            if time.monotonic() > deadline:
                raise FreeDVError("Timed out waiting for %s" % (description or "condition"))
            time.sleep(0.1)

    def wait_for_window(self, name, timeout=30):
        """Waits for a top-level window to be displayed and returns its state."""
        self.wait_until(lambda: self.window_exists(name), timeout, "window '%s' to open" % name)
        return self.get(name)

    def wait_for_window_closed(self, name, timeout=30):
        self.wait_until(lambda: not self.window_exists(name), timeout, "window '%s' to close" % name)

    # -- Lifetime ---------------------------------------------------------

    def log(self):
        """Returns everything that FreeDV has printed so far."""
        if self._log_file is not None:
            self._log_file.flush()
        try:
            with open(self.log_path, "r", errors="replace") as f:
                return f.read()
        except OSError:
            return ""

    def quit(self, timeout=60):
        """Closes FreeDV's main window, waits for FreeDV to exit and returns
        its exit code."""
        self.command("quit")
        try:
            return self._process.wait(timeout)
        except subprocess.TimeoutExpired:
            raise FreeDVError("FreeDV did not exit within %d seconds" % timeout)

    def stop(self):
        """Makes sure FreeDV is no longer running and cleans up after it.
        Called automatically at the end of a "with" block."""
        if self._process is not None and self._process.poll() is None:
            self._process.kill()
            self._process.wait()
        if self._connection is not None:
            self._connection.close()
            self._connection = None
        if self._log_file is not None:
            self._log_file.close()
            self._log_file = None
        shutil.rmtree(self._temp_dir, ignore_errors=True)
