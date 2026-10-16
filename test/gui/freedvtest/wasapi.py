"""Lists, plays into and records from Windows audio devices through WASAPI
(the interface FreeDV uses there), for freedvtest.audio. Needs the
sounddevice module (pip install sounddevice).

Devices are referred to by name, as FreeDV lists them (e.g. "Line 1 (Virtual
Audio Cable)"). Whatever the rate of a file or recording, Windows converts it
to and from the device's own.

Playing is done in a thread of the test's own, rather than by a program run
for it as elsewhere: in a virtual environment, Windows' python.exe runs the
real one as a program of its own, which stopping the first leaves running.
"""

import threading
import time
import wave

import sounddevice


def _host_api():
    for index, api in enumerate(sounddevice.query_hostapis()):
        if api["name"] == "Windows WASAPI":
            return index
    raise OSError("WASAPI isn't available")


def devices():
    """Returns the audio devices, each with its "name" and whether it's an
    "input" and/or an "output". (Windows lists a device that does both as two,
    which may or may not have the same name.)"""
    api = _host_api()
    return [{"name": device["name"], "input": device["max_input_channels"] > 0,
             "output": device["max_output_channels"] > 0}
            for device in sounddevice.query_devices() if device["hostapi"] == api]


def default_output():
    """Returns the name of the default output device."""
    index = sounddevice.query_hostapis(_host_api())["default_output_device"]
    return sounddevice.query_devices(index)["name"] if index >= 0 else None


def _find(name, output):
    api = _host_api()
    for device in sounddevice.query_devices():
        channels = device["max_output_channels"] if output else device["max_input_channels"]
        if device["hostapi"] == api and device["name"] == name and channels > 0:
            return device["index"]
    raise OSError("No %s device named %r" % ("output" if output else "input", name))


def sample_rate(name):
    """Returns the sample rate that an output device runs at (as set in
    Windows' sound settings)."""
    return int(sounddevice.query_devices(_find(name, True))["default_samplerate"])


def _settings():
    # Lets Windows convert to and from the device's own rate and channels.
    return sounddevice.WasapiSettings(auto_convert=True)


class Player:
    """Plays a WAV file into an output device in the background. Stopped
    like the program that plays elsewhere: with kill() (or terminate()),
    then wait()."""

    def __init__(self, name, path):
        # (Read now, so that the file can be deleted while it plays.)
        with wave.open(path, "rb") as f:
            self._rate, self._channels = f.getframerate(), f.getnchannels()
            self._data = f.readframes(f.getnframes())
        self._device = _find(name, True)
        self._stop = threading.Event()
        self._thread = threading.Thread(target=self._play, daemon=True)
        self._thread.start()

    def _play(self):
        frame = 2 * self._channels
        block = self._rate // 20 * frame
        with sounddevice.RawOutputStream(device=self._device, samplerate=self._rate, channels=self._channels,
                                         dtype="int16", extra_settings=_settings()) as stream:
            for start in range(0, len(self._data), block):
                if self._stop.is_set():
                    return
                stream.write(self._data[start:start + block])
            # Let what's been written be played before the stream's closed.
            self._stop.wait(stream.latency + 0.1)

    def kill(self):
        self._stop.set()

    terminate = kill

    def poll(self):
        return None if self._thread.is_alive() else 0

    def wait(self, timeout=None):
        self._thread.join(timeout)
        return 0


def record(name, seconds, rate, channels):
    """Records from an input device for the given time, and returns the
    16 bit samples, interleaved, as bytes."""
    with sounddevice.RawInputStream(device=_find(name, False), samplerate=rate, channels=channels,
                                    dtype="int16", extra_settings=_settings()) as stream:
        data, _ = stream.read(int(seconds * rate))
    return bytes(data)
