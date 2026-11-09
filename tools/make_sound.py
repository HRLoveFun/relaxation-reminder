"""Generate src/resources/rest_music.wav, the built-in rest-end chime.

The chime is a short, gentle wind-chime arpeggio: four struck bell tones
(A5, C#6, E6, A6) with slightly inharmonic partials and exponential decay.
It is synthesised from pure math so the file is reproducible, carries no
metadata and has no licensing constraints.

The WAV must stay PCM (format 1), 22.05 kHz mono 16-bit, because playback
uses PlaySoundW and the resource is embedded directly in the executable.

Usage: python tools/make_sound.py
"""

import math
import wave
from array import array
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "src" / "resources" / "rest_music.wav"

SAMPLE_RATE = 22050
DURATION = 3.0  # seconds -> about 132 KB of PCM

# A major add9 spread: A5, C#6, E6, A6.
NOTES = [
    (880.00, 0.00, 1.00),   # A5
    (1108.73, 0.14, 0.82),  # C#6
    (1318.51, 0.28, 0.70),  # E6
    (1760.00, 0.42, 0.55),  # A6
]

# Ratio, amplitude and decay time constant for each bell partial.
PARTIALS = [
    (1.00, 1.00, 1.60),
    (2.00, 0.42, 1.05),
    (2.76, 0.24, 0.80),
    (3.00, 0.14, 0.62),
    (5.40, 0.07, 0.42),
]

ATTACK = 0.008       # seconds
DETUNE = 0.0012      # relative detune between the two oscillators
MASTER_PEAK = 0.42   # roughly -7.5 dBFS, deliberately soft
FADE_OUT = 0.25      # seconds


def bell_envelope(phase_time: float, decay: float) -> float:
    """Short click-free attack followed by exponential decay."""
    if phase_time < 0.0:
        return 0.0
    attack = min(1.0, phase_time / ATTACK)
    envelope = attack * math.exp(-phase_time / decay)
    return envelope


def render() -> array:
    total = int(SAMPLE_RATE * DURATION)
    samples = [0.0] * total

    for frequency, onset, note_gain in NOTES:
        for ratio, amplitude, decay in PARTIALS:
            partial_frequency = frequency * ratio
            if partial_frequency >= SAMPLE_RATE / 2.0:
                continue
            gain = note_gain * amplitude
            start = int(onset * SAMPLE_RATE)
            # Two slightly detuned oscillators produce a soft, bell-like beat.
            for index in range(start, total):
                t = (index - start) / SAMPLE_RATE
                envelope = bell_envelope(t, decay)
                if envelope <= 0.0:
                    continue
                angle = 2.0 * math.pi * partial_frequency * t
                beat = math.sin(angle) + 0.35 * math.sin(angle * (1.0 + DETUNE))
                samples[index] += gain * envelope * beat

    # Normalise to a fixed, gentle peak so the sound never clips.
    peak = max(abs(value) for value in samples) or 1.0
    scale = MASTER_PEAK / peak

    fade_start = total - int(FADE_OUT * SAMPLE_RATE)
    pcm = array("h")
    for index, value in enumerate(samples):
        envelope = scale
        if index >= fade_start:
            envelope *= max(0.0, (total - index) / (total - fade_start))
        pcm.append(max(-32768, min(32767, int(round(value * envelope * 32767)))))

    return pcm


def main() -> None:
    OUT.parent.mkdir(parents=True, exist_ok=True)
    pcm = render()
    with wave.open(str(OUT), "wb") as handle:
        handle.setnchannels(1)
        handle.setsampwidth(2)
        handle.setframerate(SAMPLE_RATE)
        handle.writeframes(pcm.tobytes())
    print(f"wrote {OUT} ({OUT.stat().st_size} bytes, {DURATION:.1f}s)")


if __name__ == "__main__":
    main()
