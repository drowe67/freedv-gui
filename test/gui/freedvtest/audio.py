"""Virtual audio devices for FreeDV to use during tests (Linux only)."""

import array
import math
import os
import subprocess
import tempfile
import time
import wave


def _pactl(*args):
    return subprocess.check_output(("pactl",) + args).decode()


def default_output():
    """Returns the name of the system's default output device."""
    return _pactl("get-default-sink").strip()


def record_level(device, seconds=1.0):
    """Listens to an input device (or input_name() of an output device) and
    returns how loud what it hears is, as the RMS of 16 bit samples: 0 for
    silence, around 23000 for a full-scale tone."""
    with tempfile.TemporaryDirectory(prefix="freedvtest-audio-") as folder:
        path = os.path.join(folder, "recording.wav")
        recorder = subprocess.Popen([
            "parecord", "--device=" + device, "--file-format=wav", "--channels=1", "--rate=8000",
            "--format=s16le", "--latency-msec=20", path])
        time.sleep(seconds)
        recorder.terminate()
        recorder.wait()

        # The recorder doesn't get to fill in the file's length, so don't rely on it.
        with open(path, "rb") as f:
            data = f.read()[44:]
    samples = array.array("h", data[:len(data) // 2 * 2])
    if not samples:
        return 0.0
    return math.sqrt(sum(sample * sample for sample in samples) / len(samples))


def write_tone(path, seconds=2.0, rate=16000, frequency=1000.0, channels=1, amplitude=16000):
    """Creates a WAV file containing a tone."""
    samples = array.array("h")
    for n in range(int(rate * seconds)):
        samples.extend([int(amplitude * math.sin(2 * math.pi * frequency * n / rate))] * channels)
    with wave.open(path, "wb") as f:
        f.setnchannels(channels)
        f.setsampwidth(2)
        f.setframerate(rate)
        f.writeframes(samples.tobytes())


def file_level(path):
    """Returns how loud a 16 bit WAV file is (see record_level()) and how
    long it is in seconds."""
    with wave.open(path, "rb") as f:
        rate = f.getframerate()
        samples = array.array("h", f.readframes(f.getnframes()))
    if not samples:
        return (0.0, 0.0)
    return (math.sqrt(sum(sample * sample for sample in samples) / len(samples)), len(samples) / float(rate))


class Loopback:
    """Feeds what's played into one output device to another for the
    duration of a "with" block, e.g. so that FreeDV hears what it transmits."""

    def __init__(self, from_device, to_device):
        self._args = ["source=" + VirtualAudioDevices.input_name(from_device), "sink=" + to_device, "latency_msec=20"]
        self._module = None

    def __enter__(self):
        self._module = _pactl("load-module", "module-loopback", *self._args).strip()
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        if self._module is not None:
            subprocess.call(["pactl", "unload-module", self._module])
        return False


class Tone:
    """Plays a tone into an output device for the duration of a "with" block
    (at most max_seconds)."""

    def __init__(self, device, frequency=1000.0, max_seconds=30):
        self._device = device
        self._frequency = frequency
        self._seconds = max_seconds
        self._folder = None
        self._player = None

    def __enter__(self):
        self._folder = tempfile.TemporaryDirectory(prefix="freedvtest-audio-")
        path = os.path.join(self._folder.name, "tone.wav")
        rate = 8000
        samples = array.array("h", (
            int(16000 * math.sin(2 * math.pi * self._frequency * n / rate)) for n in range(rate * self._seconds)))
        with wave.open(path, "wb") as f:
            f.setnchannels(1)
            f.setsampwidth(2)
            f.setframerate(rate)
            f.writeframes(samples.tobytes())
        self._player = subprocess.Popen(["paplay", "--device=" + self._device, path])
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        if self._player is not None:
            self._player.terminate()
            self._player.wait()
        self._folder.cleanup()
        return False


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


def streams(pid):
    """Returns the audio streams that a program has open: for each, whether
    it's a "playback" or "record" stream, its "index" and the name of the
    "device" it's connected to."""
    import json

    found = []
    for kind, listing, devices, key in [("playback", "sink-inputs", "sinks", "sink"),
                                        ("record", "source-outputs", "sources", "source")]:
        names = {device["index"]: device["name"] for device in json.loads(_pactl("--format=json", "list", devices))}
        for stream in json.loads(_pactl("--format=json", "list", listing)):
            if str(stream.get("properties", {}).get("application.process.id")) == str(pid):
                found.append({"kind": kind, "index": stream["index"], "device": names.get(stream[key])})
    return found


def move_stream(stream, device):
    """Connects one of streams() to a different device, as the system's
    sound settings (or the device it was using going away) would."""
    command = "move-sink-input" if stream["kind"] == "playback" else "move-source-output"
    _pactl(command, str(stream["index"]), device)
