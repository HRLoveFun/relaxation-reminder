"""Generate src/resources/rest_music.wav, the built-in rest-end music.

The clip is synthesized from scratch (no samples, no third-party audio):
a bright 1049 Hz chime with two fast-decaying partials, followed by four
short low notes with a small downward glide.

Usage: python tools/make_music.py
"""

import math
import struct
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "src" / "resources" / "rest_music.wav"

RATE = 24000
DURATION = 5.066
PEAK = 0.95


def render() -> list[float]:
    buf = [0.0] * int(RATE * DURATION)

    def add(start_s, dur_s, freq_fn, amp, decay, harmonics, attack):
        i0 = int(start_s * RATE)
        phase = 0.0
        for k in range(int(dur_s * RATE)):
            idx = i0 + k
            if idx >= len(buf):
                break
            t = k / RATE
            phase += 2 * math.pi * freq_fn(t) / RATE
            env = min(1.0, t / attack) * math.exp(-t / decay)
            value = 0.0
            for mult, hamp, hdecay in harmonics:
                value += hamp * math.exp(-t / hdecay) * math.sin(mult * phase)
            buf[idx] += amp * env * value

    # Chime: fundamental plus 2449 Hz and 4131 Hz partials that die faster.
    add(0.02, 2.30, lambda t: 1049.0, 7600.0, 2.9,
        [(1.0, 1.0, 2.9), (2.335, 0.40, 0.85), (3.938, 0.48, 0.70)],
        attack=0.05)

    # Four short notes, each gliding down slightly.
    for start, dur, f0, glide in [
        (3.26, 0.18, 246.0, 0.93),
        (3.48, 0.18, 228.0, 0.93),
        (3.70, 0.17, 257.0, 0.97),
        (3.90, 0.19, 285.0, 0.90),
    ]:
        add(start, dur + 0.25,
            lambda t, f0=f0, glide=glide: f0 * (glide + (1.0 - glide) * min(1.0, t / 0.12)),
            22000.0, 0.12,
            [(1.0, 1.0, 0.12), (2.0, 0.22, 0.06), (3.0, 0.08, 0.05)],
            attack=0.008)
    return buf


def main() -> None:
    buf = render()
    scale = PEAK * 32767.0 / max(abs(v) for v in buf)
    pcm = bytearray()
    for v in buf:
        s = max(-32768, min(32767, int(round(v * scale))))
        pcm += struct.pack("<h", s)
    with wave.open(str(OUT), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(bytes(pcm))
    print(f"wrote {OUT} ({len(pcm) + 44} bytes)")


if __name__ == "__main__":
    main()
