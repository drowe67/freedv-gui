"""Virtual audio devices for FreeDV to use during tests (Linux only)."""

import subprocess
import time


def _pactl(*args):
    return subprocess.check_output(("pactl",) + args).decode()


def default_output():
    """Returns the name of the system's default output device."""
    return _pactl("get-default-sink").strip()


class VirtualAudioDevices:
    """Creates PulseAudio/PipeWire devices for the duration of a "with" block.

    Each positional argument is the name of an output device that discards
    what's played into it. It can also be used as an input device (which
    hears whatever was played into it) under input_name().

    inputs maps names to descriptions of devices that are only inputs, as a
    radio's or microphone's would be, and outputs does the same for further
    output devices. The description is what a device is displayed as, which
    is otherwise its name.
    """

    def __init__(self, *names, outputs=None, inputs=None):
        self._outputs = {name: name for name in names}
        self._outputs.update(outputs or {})
        self._inputs = dict(inputs or {})
        self._modules = []

    @staticmethod
    def input_name(name):
        return name + ".monitor"

    def __enter__(self):
        try:
            for name, description in self._outputs.items():
                self._load("module-null-sink", "sink_name=" + name, self._properties("sink", description))
            if self._inputs:
                # PipeWire has no module-null-source, but can make a null
                # sink behave as a source instead.
                pipewire = "PipeWire" in _pactl("info")
                for name, description in self._inputs.items():
                    if pipewire:
                        self._load(
                            "module-null-sink", "media.class=Audio/Source/Virtual",
                            "sink_name=" + name, self._properties("sink", description))
                    else:
                        self._load(
                            "module-null-source", "source_name=" + name,
                            self._properties("source", description))
            for name in self._outputs:
                self._wait_for("sinks", name)
            for name in self._inputs:
                self._wait_for("sources", name)
        except BaseException:
            self.__exit__(None, None, None)
            raise
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        for module in reversed(self._modules):
            subprocess.call(["pactl", "unload-module", module])
        self._modules = []
        return False

    def _load(self, *args):
        self._modules.append(_pactl("load-module", *args).strip())

    @staticmethod
    def _properties(kind, description):
        return "%s_properties=\"device.description='%s'\"" % (kind, description)

    @staticmethod
    def _wait_for(kind, name, timeout=30):
        deadline = time.monotonic() + timeout
        while True:
            devices = _pactl("list", "short", kind)
            if any(line.split("\t")[1:2] == [name] for line in devices.splitlines()):
                return
            if time.monotonic() > deadline:
                raise RuntimeError("Audio device %s did not appear" % name)
            time.sleep(0.5)
