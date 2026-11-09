"""Generate src/resources/rest_music.wav, the built-in rest-end music.

The clip is synthesized from scratch (no samples, no third-party audio). It
recreates the first two seconds of the reference chime with a lighter, softer
character: a 1049 Hz tone with 2449 Hz and 4131 Hz partials, a gentler attack
and a slightly longer, even decay so the partials melt into the fundamental
instead of ringing out brightly.

Usage: python tools/make_music.py
"""

import math
import struct
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "src" / "resources" / "rest_music.wav"

RATE = 24000
DURATION = 2.0
PEAK = 0.95

FUNDAMENTAL = 1049.0
# (frequency multiple, amplitude, decay seconds). The partials are quieter and
# decay only a little faster than the fundamental, which keeps the timbre
# soft rather than piercing.
PARTIALS = [
    (1.0, 1.00, 2.6),
    (2.335, 0.22, 1.4),
    (3.938, 0.24, 1.1),
]


def render() -> list[float]:
    buf = [0.0] * int(RATE * DURATION)
    i0 = int(0.02 * RATE)
    phase = 0.0
    for k in range(int(1.95 * RATE)):
        idx = i0 + k
        if idx >= len(buf):
            break
        t = k / RATE
        phase += 2 * math.pi * FUNDAMENTAL / RATE
        # Soft 120 ms attack, then a gentle exponential decay.
        env = min(1.0, t / 0.12) * math.exp(-t / 2.4)
        value = 0.0
        for mult, amp, decay in PARTIALS:
            value += amp * math.exp(-t / decay) * math.sin(mult * phase)
        buf[idx] += 7600.0 * env * value
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
