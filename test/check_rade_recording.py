#!/usr/bin/env python3
"""Checks what rade_loss recorded against what FreeDV sent, to tell a
failure of FreeDV's from one of the machine it ran on.

    check_rade_recording.py <FreeDV's own TX output> <the recording> [FreeDV logs...]

FreeDV's TX output (from -txoutfile) is the modem signal as it left the
pipeline; the recording is what arrived at the other end of the loopback
sound device. If the recording has stretches missing or extra compared with
it, or holes of silence, those happened between FreeDV and the recording
(FreeDV's sound card callback, the loopback driver, the recorder), and the
place and size of each is reported. The logs are searched for FreeDV's plot
taps dropping samples, which happens when its threads haven't been able to
run for a while: on a CI machine, usually the machine stalling.

Prints a summary that ends with a line starting "RECORDING CHECK:". It never
prints PASS or FAIL (rade_loss's own verdict is looked for with those), and
always exits 0.
"""

import re
import struct
import sys

import numpy as np

RATE = 48000


def load(path):
    """Returns a WAV file's first channel as floats, whatever its format
    (sox writes WAVE_FORMAT_EXTENSIBLE, 32 bit, and doesn't always fill in
    the length)."""
    raw = open(path, "rb").read()
    pos = 12
    channels, bits, tag, rate = 1, 16, 1, RATE
    while pos + 8 <= len(raw):
        chunk, size = raw[pos:pos + 4], struct.unpack("<I", raw[pos + 4:pos + 8])[0]
        if chunk == b"fmt ":
            tag, channels, rate = struct.unpack("<HHI", raw[pos + 8:pos + 16])
            bits = struct.unpack("<H", raw[pos + 22:pos + 24])[0]
            if tag == 0xFFFE:
                tag = struct.unpack("<H", raw[pos + 32:pos + 34])[0]
        elif chunk == b"data":
            data = raw[pos + 8:]
            width = bits // 8
            data = data[:len(data) // (width * channels) * width * channels]
            if bits == 16:
                samples = np.frombuffer(data, dtype="<i2").astype(np.float64) / 32768.0
            elif tag == 3:
                samples = np.frombuffer(data, dtype="<f4").astype(np.float64)
            else:
                samples = np.frombuffer(data, dtype="<i4").astype(np.float64) / 2147483648.0
            return samples[::channels], rate
        pos += 8 + size + (size & 1)
    raise ValueError("%s has no audio in it" % path)


def signal_bounds(samples):
    loud = np.nonzero(np.abs(samples) > 0.005)[0]
    return (int(loud[0]), int(loud[-1]) + 1) if len(loud) else (0, 0)


def best_lag(reference, recording, ref_start, length, centre, span):
    """Where `length` samples of the reference starting at ref_start best
    line up in the recording, within `span` of `centre` (as an offset from
    ref_start), and how well (normalised correlation)."""
    piece = reference[ref_start:ref_start + length]
    lo = max(0, ref_start + centre - span)
    hi = min(len(recording), ref_start + centre + span + length)
    if hi - lo < length or not np.any(piece):
        return None, 0.0
    region = recording[lo:hi]
    n = 1 << int(np.ceil(np.log2(len(region) + length)))
    corr = np.fft.irfft(np.fft.rfft(region, n) * np.conj(np.fft.rfft(piece, n)), n)[:len(region) - length + 1]
    energy = np.convolve(region * region, np.ones(length), "valid")
    score = corr / np.sqrt(np.maximum(energy, 1e-12) * np.dot(piece, piece))
    index = int(np.argmax(score))
    return lo + index - ref_start, float(score[index])


def tap_drops(log_paths):
    """Each time FreeDV's plot taps dropped samples while it was sending or
    receiving, by log and time: as (log, time, how many taps, longest drop in
    samples).

    The taps are fed by the TX/RX thread and emptied by a thread of ordinary
    priority, so a tap only drops samples when the first carries on while
    the second doesn't get to run: a sign that ordinary threads (the
    recorder's among them) were held up. Drops while FreeDV is stopping
    (when the tap's thread goes first) don't count.
    """
    drops = {}
    for path in log_paths:
        try:
            lines = open(path, errors="replace").read().splitlines()
        except OSError:
            continue
        # Sending: from the first "Firing PTT" to the second. Receiving:
        # from "Firing start" to the counters being reported at the end.
        ptt = [i for i, line in enumerate(lines) if "Firing PTT" in line]
        if len(ptt) >= 2:
            first, last = ptt[0], ptt[1]
        else:
            starts = [i for i, line in enumerate(lines) if "Firing start" in line]
            ends = [i for i, line in enumerate(lines) if "Sound card FIFOs" in line or "Firing stop" in line]
            first = starts[0] if starts else 0
            last = ends[0] if ends else len(lines)
        for line in lines[first:last]:
            match = re.search(r"^(\S+) .*Tap FIFO drops ended; tap is missing (\d+) samples", line)
            if match:
                taps, longest = drops.get((path, match.group(1)), (0, 0))
                drops[(path, match.group(1))] = (taps + 1, max(longest, int(match.group(2))))
    return [(path, when, taps, longest) for (path, when), (taps, longest) in drops.items()]


def main():
    sent, sent_rate = load(sys.argv[1])
    recorded, recorded_rate = load(sys.argv[2])
    if sent_rate != RATE or recorded_rate != RATE:
        print("RECORDING CHECK: not done (sample rates %d and %d, expected %d)" % (sent_rate, recorded_rate, RATE))
        return

    sent_start, sent_end = signal_bounds(sent)
    rec_start, rec_end = signal_bounds(recorded)
    print("FreeDV sent %.2f s of signal; %.2f s of signal was recorded" %
          ((sent_end - sent_start) / RATE, (rec_end - rec_start) / RATE))

    # Line the start of the two up, then follow the signal along: wherever
    # the recording's offset from what was sent changes, something was lost
    # (or added) between them.
    window = RATE // 10
    lag, score = best_lag(sent, recorded, sent_start + RATE // 2, window, rec_start - sent_start, RATE)
    problems = []
    extras = []     # (ms, description) of stretches the recording has and the copy doesn't
    if lag is None or score < 0.9:
        print("Couldn't find the start of what was sent in the recording (best match %.2f)" % score)
        problems.append("the recording doesn't start with what was sent")
    else:
        last_good = sent_start + RATE // 2
        # (Not the last second: the end of a transmission isn't speech, and
        # the recorder may well have stopped part of the way through it.)
        for position in range(sent_start + RATE, sent_end - RATE - window, RATE // 4):
            new_lag, new_score = best_lag(sent, recorded, position, window, lag, RATE)
            if new_lag is None or new_score < 0.9:
                continue  # (quiet stretches, or the recording ran out)
            if abs(new_lag - lag) > RATE // 1000:  # (a sample or two either way is neither)
                change_ms = (new_lag - lag) * 1000.0 / RATE
                where = (last_good - sent_start) / RATE, (position - sent_start) / RATE
                what = "missing" if change_ms < 0 else "extra"
                description = "%.1f ms %s between %.2f and %.2f s into the signal" % (abs(change_ms), what, where[0], where[1])
                problems.append(description)
                if change_ms > 0:
                    extras.append((change_ms, description))
                lag = new_lag
            last_good = position

    # Holes of silence in the recording within the signal.
    quiet = np.abs(recorded[rec_start:rec_end]) < 0.001
    run_starts = np.flatnonzero(np.diff(np.concatenate(([0], quiet.astype(np.int8)))) == 1)
    run_ends = np.flatnonzero(np.diff(np.concatenate((quiet.astype(np.int8), [0]))) == -1) + 1
    for start, end in zip(run_starts, run_ends):
        if end - start >= RATE // 200 and end < rec_end - rec_start - RATE:
            problems.append("%.1f ms of silence %.2f s into the recorded signal" % ((end - start) * 1000.0 / RATE, start / RATE))

    drops = tap_drops(sys.argv[3:])

    # FreeDV's copy of what it sent is written by one of its taps too, so
    # where the taps dropped samples while sending, the copy is missing them
    # rather than the recording having anything extra.
    copy_gaps = []
    sending_drops = [samples * 1000.0 / RATE for path, when, taps, samples in drops if "tx" in path]
    for ms, description in extras:
        if any(abs(ms - dropped) <= 3.0 for dropped in sending_drops):
            problems.remove(description)
            copy_gaps.append(ms)

    for problem in problems:
        print("  " + problem)
    for ms in copy_gaps:
        print("  FreeDV's copy is missing %.1f ms, which its tap dropped (not a problem with the recording)" % ms)

    for path, when, taps, samples in drops:
        print("  FreeDV's plot taps (%d of them) couldn't keep up at %s (%s): up to %.0f ms dropped"
              % (taps, when, path, samples * 1000.0 / RATE))

    if not problems and copy_gaps:
        print("RECORDING CHECK: the recording is what FreeDV sent, as far as can be told (FreeDV's own copy is missing "
              "%s ms that its tap dropped)" % ", ".join("%.1f" % ms for ms in copy_gaps))
    elif not problems:
        print("RECORDING CHECK: the recording is what FreeDV sent")
    elif drops:
        print("RECORDING CHECK: the recording differs from what FreeDV sent (%s), and meanwhile FreeDV's TX/RX thread "
              "carried on while its ordinary threads were held up: most likely the machine stalled, and the recorder "
              "with them" % "; ".join(problems))
    else:
        print("RECORDING CHECK: the recording differs from what FreeDV sent (%s)" % "; ".join(problems))


if __name__ == "__main__":
    try:
        main()
    except Exception as e:  # never stand in the way of the test's own result
        print("RECORDING CHECK: not done (%s)" % e)
