"""What freedvtest.audio needs from Core Audio on macOS: the devices there
are, which one is the default output, and their sample rates."""

import ctypes
import ctypes.util
import struct

_core_audio = ctypes.CDLL(ctypes.util.find_library("CoreAudio"))
_core_foundation = ctypes.CDLL(ctypes.util.find_library("CoreFoundation"))

_core_foundation.CFStringGetCString.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_long, ctypes.c_uint32]
_core_foundation.CFStringGetCString.restype = ctypes.c_bool
_core_foundation.CFRelease.argtypes = [ctypes.c_void_p]


class _Address(ctypes.Structure):
    _fields_ = [("selector", ctypes.c_uint32), ("scope", ctypes.c_uint32), ("element", ctypes.c_uint32)]


_core_audio.AudioObjectGetPropertyDataSize.argtypes = [
    ctypes.c_uint32, ctypes.POINTER(_Address), ctypes.c_uint32, ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint32)]
_core_audio.AudioObjectGetPropertyData.argtypes = [
    ctypes.c_uint32, ctypes.POINTER(_Address), ctypes.c_uint32, ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint32),
    ctypes.c_void_p]
_core_audio.AudioObjectSetPropertyData.argtypes = [
    ctypes.c_uint32, ctypes.POINTER(_Address), ctypes.c_uint32, ctypes.c_void_p, ctypes.c_uint32, ctypes.c_void_p]


def _code(text):
    return struct.unpack(">I", text.encode("ascii"))[0]


_SYSTEM_OBJECT = 1
_GLOBAL = _code("glob")
_INPUT = _code("inpt")
_OUTPUT = _code("outp")
_DEVICES = _code("dev#")
_DEFAULT_OUTPUT = _code("dOut")
_NAME = _code("lnam")
_STREAMS = _code("stm#")
_NOMINAL_RATE = _code("nsrt")


def _size(device, selector, scope=_GLOBAL):
    size = ctypes.c_uint32(0)
    if _core_audio.AudioObjectGetPropertyDataSize(device, ctypes.byref(_Address(selector, scope, 0)), 0, None,
                                                  ctypes.byref(size)) != 0:
        return 0
    return size.value


def _get(device, selector, value, scope=_GLOBAL):
    size = ctypes.c_uint32(ctypes.sizeof(value))
    status = _core_audio.AudioObjectGetPropertyData(
        device, ctypes.byref(_Address(selector, scope, 0)), 0, None, ctypes.byref(size), ctypes.byref(value))
    if status != 0:
        raise OSError("Core Audio error %d" % status)
    return value


def _name(device):
    string = _get(device, _NAME, ctypes.c_void_p())
    buffer = ctypes.create_string_buffer(1024)
    try:
        _core_foundation.CFStringGetCString(string, buffer, len(buffer), 0x08000100)  # UTF-8
    finally:
        _core_foundation.CFRelease(string)
    return buffer.value.decode("utf-8")


def devices():
    """Returns the audio devices there are, each with its "name", "id" and
    whether it has "input" and "output"."""
    count = _size(_SYSTEM_OBJECT, _DEVICES) // 4
    ids = _get(_SYSTEM_OBJECT, _DEVICES, (ctypes.c_uint32 * count)())
    return [{"id": device, "name": _name(device), "input": _size(device, _STREAMS, _INPUT) > 0,
             "output": _size(device, _STREAMS, _OUTPUT) > 0} for device in ids]


def _find(name):
    for device in devices():
        if device["name"] == name:
            return device["id"]
    raise RuntimeError("There's no audio device called %s" % name)


def default_output():
    """Returns the name of the system's default output device."""
    return _name(_get(_SYSTEM_OBJECT, _DEFAULT_OUTPUT, ctypes.c_uint32()).value)


def sample_rate(name):
    """Returns the sample rate a device is running at."""
    return _get(_find(name), _NOMINAL_RATE, ctypes.c_double()).value


def set_sample_rate(name, rate):
    """Changes the sample rate a device runs at (for everyone using it)."""
    value = ctypes.c_double(rate)
    status = _core_audio.AudioObjectSetPropertyData(
        _find(name), ctypes.byref(_Address(_NOMINAL_RATE, _GLOBAL, 0)), 0, None, ctypes.sizeof(value),
        ctypes.byref(value))
    if status != 0:
        raise OSError("Could not set %s to %d Hz: Core Audio error %d" % (name, rate, status))
