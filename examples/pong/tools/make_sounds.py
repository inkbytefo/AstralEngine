#!/usr/bin/env python3
"""Pong's sound effects, synthesized (no recorded assets, no licences): writes assets/sounds/*.wav.

    python3 examples/pong/tools/make_sounds.py

Deterministic: running it again gives the same files. 22.05 kHz mono 16-bit (small, plenty for blips).
"""
import math
import pathlib
import random
import struct
import wave

RATE = 22050
OUT = pathlib.Path(__file__).resolve().parent.parent / "assets" / "sounds"


def envelope(t, attack, decay):
    """Fast attack, exponential decay."""
    return min(1.0, t / attack) * math.exp(-t / decay) if attack > 0 else math.exp(-t / decay)


def tone(freq, seconds, decay, attack=0.002, harmonics=((1, 1.0),), glide=0.0, noise=0.0, seed=1):
    rng = random.Random(seed)
    out, phase = [], 0.0
    for i in range(int(seconds * RATE)):
        t = i / RATE
        f = freq * (1.0 + glide * t / seconds)
        phase += 2 * math.pi * f / RATE
        s = sum(a * math.sin(phase * h) for h, a in harmonics)
        s += noise * (rng.random() * 2 - 1)
        out.append(s * envelope(t, attack, decay))
    return out


def sequence(notes, gap):
    """Notes one after another (each a sample list), starting `gap` seconds apart; tails overlap."""
    step = int(gap * RATE)
    total = step * (len(notes) - 1) + max(len(n) for n in notes)
    out = [0.0] * total
    for k, n in enumerate(notes):
        for i, s in enumerate(n):
            out[k * step + i] += s
    return out


def hum(seconds):
    """A seamless loop: whole cycles of every partial over the loop length."""
    n = int(seconds * RATE)
    out = []
    for i in range(n):
        t = i / RATE
        s = 0.5 * math.sin(2 * math.pi * 55 * t) + 0.25 * math.sin(2 * math.pi * 110 * t) \
            + 0.12 * math.sin(2 * math.pi * 165 * t) * (0.5 + 0.5 * math.sin(2 * math.pi * 0.5 * t))
        out.append(s)
    return out


def write(name, samples, peak=0.8):
    m = max(1e-9, max(abs(s) for s in samples))
    OUT.mkdir(parents=True, exist_ok=True)
    with wave.open(str(OUT / name), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", int(s / m * peak * 32767)) for s in samples))


bright = ((1, 1.0), (2, 0.35), (3, 0.12))
write("paddle.wav", tone(660, 0.18, 0.05, harmonics=bright, noise=0.04))
write("wall.wav", tone(240, 0.10, 0.025, harmonics=((1, 1.0), (2, 0.2)), noise=0.15, seed=2), peak=0.6)
write("serve.wav", tone(420, 0.16, 0.08, glide=0.6, harmonics=bright), peak=0.5)
write("point_won.wav", sequence([tone(f, 0.25, 0.09, harmonics=bright) for f in (523, 659, 784)], 0.07))
write("point_lost.wav", sequence([tone(f, 0.3, 0.12, harmonics=bright) for f in (392, 330, 262)], 0.09))
write("match_won.wav", sequence([tone(f, 0.5, 0.18, harmonics=bright) for f in (523, 659, 784, 1047)], 0.11))
write("match_lost.wav", sequence([tone(f, 0.6, 0.22, harmonics=bright) for f in (330, 311, 294, 262)], 0.16))
write("hum.wav", hum(2.0), peak=0.5)
print("wrote", ", ".join(sorted(p.name for p in OUT.glob("*.wav"))))
