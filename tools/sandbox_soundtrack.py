#!/usr/bin/env python3
"""VoxelCraft sandbox soundtrack "Otevřený obzor" (Open Horizon), composed and
synthesised here note by note with numpy (no samples, nothing from other games).

Calm C major at 72 BPM, about two minutes: intro, theme A, theme B, a quiet
piano break, theme A again with bells, outro.

    python3 tools/sandbox_soundtrack.py   ->  assets_src/music/otevreny_obzor.mp3
"""
import os
import subprocess
import wave

import numpy as np

SR = 44100
BPM = 72
BEAT = 60.0 / BPM
BAR = 4 * BEAT
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "assets_src", "music")
rng = np.random.default_rng(2026)


def hz(m):
    return 440.0 * 2 ** ((m - 69) / 12)


# ------------------------------------------------------------- instruments ---

def env(n, attack, release, decay=None):
    t = np.arange(n) / SR
    e = np.minimum(1.0, t / max(attack, 1e-4))
    if decay:
        e *= np.exp(-t / decay)
    r = int(release * SR)
    if r and r < n:
        e[-r:] *= np.linspace(1, 0, r) ** 2
    return e


def piano(m, dur, vel):
    """Soft felt piano: a few harmonics, the higher ones die out faster."""
    f = hz(m)
    n = int((dur + 1.2) * SR)
    t = np.arange(n) / SR
    out = np.zeros(n)
    for k, a in enumerate((1.0, 0.45, 0.22, 0.10, 0.05), start=1):
        fk = f * k * (1 + 0.0004 * k * k)  # a touch of string stiffness
        if fk > 9000:
            break
        out += a * np.sin(2 * np.pi * fk * t + rng.uniform(0, 6.28)) * np.exp(-t * (0.9 + 1.4 * k) * (f / 440) ** 0.3)
    out *= env(n, 0.006, 0.25)
    hold = np.ones(n)
    cut = int(dur * SR)
    hold[cut:] = np.exp(-np.arange(n - cut) / SR / 0.35)  # damper
    return out * hold * vel * 0.32


def bell(m, dur, vel):
    """Glassy FM bell."""
    f = hz(m)
    n = int((dur + 2.5) * SR)
    t = np.arange(n) / SR
    idx = 2.2 * np.exp(-t / 0.6)
    out = np.sin(2 * np.pi * f * t + idx * np.sin(2 * np.pi * f * 3.5 * t))
    out += 0.3 * np.sin(2 * np.pi * f * 2.0 * t) * np.exp(-t / 0.5)
    return out * env(n, 0.003, 0.4, decay=1.4) * vel * 0.16


def pad(m, dur, vel):
    """Warm string pad: detuned band-limited saws, slow swell."""
    f = hz(m)
    n = int((dur + 1.5) * SR)
    t = np.arange(n) / SR
    out = np.zeros(n)
    for det in (-0.12, 0.0, 0.13):
        ff = f * 2 ** (det / 12)
        ph = rng.uniform(0, 6.28)
        for k in range(1, 7):
            out += np.sin(2 * np.pi * ff * k * t + ph * k) / k ** 1.6
    out *= 1 + 0.08 * np.sin(2 * np.pi * 0.23 * t)  # slow breathing
    return out * env(n, 1.4, 1.5) * vel * 0.035


def bass(m, dur, vel):
    f = hz(m)
    n = int((dur + 0.4) * SR)
    t = np.arange(n) / SR
    out = np.sin(2 * np.pi * f * t) + 0.25 * np.sin(4 * np.pi * f * t)
    return out * env(n, 0.03, 0.35, decay=3.0) * vel * 0.20


# ------------------------------------------------------------------ score ---

CH = {  # bass, voicing
    "Cmaj9": (36, (52, 55, 59, 62)),
    "Am7": (33, (52, 55, 57, 60)),
    "Fmaj7": (41, (57, 60, 64, 67)),
    "Gsus": (43, (50, 55, 60, 62)),
    "G6": (43, (50, 55, 59, 64)),
    "Em7": (40, (50, 55, 59, 64)),
    "Dm9": (38, (53, 57, 60, 64)),
}

INTRO = ["Cmaj9", "Fmaj7", "Cmaj9", "Gsus"]
A = ["Cmaj9", "Am7", "Fmaj7", "G6", "Cmaj9", "Em7", "Fmaj7", "Gsus"]
B = ["Fmaj7", "Em7", "Dm9", "Gsus", "Fmaj7", "Em7", "Am7", "G6"]
BREAK = ["Fmaj7", "Am7", "Fmaj7", "Gsus"]
OUTRO = ["Fmaj7", "Gsus", "Cmaj9", "Cmaj9"]

# melody: bar -> [(beat, length in beats, midi)]
MEL_A = [
    [(0, 1.5, 76), (1.5, 0.5, 74), (2, 1, 76), (3, 1, 79)],
    [(0, 1.5, 69), (1.5, 0.5, 72), (2, 2, 76)],
    [(0, 1, 79), (1, 1, 76), (2, 1.5, 72), (3.5, 0.5, 74)],
    [(0, 3, 74), (3, 1, 67)],
    [(0, 1.5, 76), (1.5, 0.5, 79), (2, 1, 84), (3, 1, 83)],
    [(0, 2, 79), (2, 1, 76), (3, 1, 74)],
    [(0, 1, 72), (1, 1, 74), (2, 1, 76), (3, 1, 81)],
    [(0, 4, 79)],
]
MEL_B = [
    [(0, 2, 81), (2, 2, 79)],
    [(0, 2, 83), (2, 1, 79), (3, 1, 76)],
    [(0, 1.5, 77), (1.5, 0.5, 76), (2, 2, 74)],
    [(0, 2, 74), (2, 2, 72)],
    [(0, 1, 81), (1, 1, 84), (2, 2, 88)],
    [(0, 1.5, 86), (1.5, 0.5, 83), (2, 2, 79)],
    [(0, 2, 84), (2, 1, 81), (3, 1, 79)],
    [(0, 3, 79)],
]
ARP = [0, 1, 2, 3, 2, 1, 2, 3]


class Mix:
    def __init__(self, seconds):
        self.L = np.zeros(int(seconds * SR))
        self.R = np.zeros_like(self.L)

    def add(self, sig, start, pan=0.0):
        i = int(start * SR)
        j = min(len(self.L), i + len(sig))
        sig = sig[: j - i]
        self.L[i:j] += sig * np.sqrt((1 - pan) / 2)
        self.R[i:j] += sig * np.sqrt((1 + pan) / 2)


def human(x, amount=0.012):
    return x + rng.uniform(-amount, amount)


def compose():
    sections = [("intro", INTRO), ("A", A), ("B", B), ("break", BREAK), ("A2", A), ("outro", OUTRO)]
    total_bars = sum(len(c) for _, c in sections)
    mix = Mix(total_bars * BAR + 8)
    bar = 0
    for name, chords in sections:
        for i, c in enumerate(chords):
            t0 = bar * BAR
            b, voicing = CH[c]
            last = name == "outro" and i == len(chords) - 1
            hold = BAR * (2.5 if last else 1.0)
            fade = 1.0
            if name == "outro":
                fade = 1.0 - 0.18 * i
            # pad everywhere, softer in the break
            for k, m in enumerate(voicing):
                mix.add(pad(m, hold, (0.55 if name == "break" else 1.0) * fade), t0, pan=-0.5 + k / 3)
            if name not in ("intro", "break"):
                mix.add(bass(b, BAR * 0.95 if not last else hold, fade), t0)
            # piano
            if name in ("A", "B", "A2"):
                for s, idx in enumerate(ARP):
                    m = voicing[idx] + 12 if s else b + 24
                    v = (0.55 if s % 2 else 0.7) * (0.8 if name == "B" else 1.0)
                    mix.add(piano(m, BEAT * 0.9, human(v, 0.08)), human(t0 + s * BEAT / 2), pan=-0.25 + 0.15 * idx)
            elif name == "break" or name == "outro":
                for k, m in enumerate((b + 12,) + voicing):
                    mix.add(piano(m + (12 if k else 0), BAR * 0.95 if not last else hold, 0.5 * fade),
                            t0 + k * 0.09, pan=-0.3 + 0.15 * k)
            # melody
            if name in ("A", "A2"):
                for beat, ln, m in MEL_A[i]:
                    mix.add(piano(m, ln * BEAT, human(0.95, 0.06)), human(t0 + beat * BEAT), pan=0.1)
                    if name == "A2":
                        mix.add(bell(m + 12, ln * BEAT, 0.5), t0 + beat * BEAT + 0.02, pan=0.45)
            elif name == "B":
                for beat, ln, m in MEL_B[i]:
                    mix.add(bell(m, ln * BEAT, 0.9), t0 + beat * BEAT, pan=0.3)
                    mix.add(pad(m - 12, ln * BEAT, 0.6), t0 + beat * BEAT, pan=-0.2)
            elif name == "intro":
                for beat, m in ((0, voicing[3] + 24), (1.5, voicing[2] + 24), (3, voicing[1] + 24)):
                    mix.add(bell(m, BEAT, 0.45), t0 + beat * BEAT, pan=0.5 - beat / 4)
            if last:
                mix.add(bell(84, BAR, 0.35), t0 + BEAT, pan=0.4)
                mix.add(bell(91, BAR, 0.25), t0 + 2.5 * BEAT, pan=-0.4)
            bar += 1
    return mix


def reverb(x, seconds=2.6, seed=1):
    """Convolution with a decaying noise tail (a large quiet hall)."""
    r = np.random.default_rng(seed)
    n = int(seconds * SR)
    t = np.arange(n) / SR
    ir = r.standard_normal(n) * np.exp(-t / (seconds / 6.9))
    ir[: int(0.012 * SR)] = 0  # pre-delay
    # darker tail: simple one-pole low pass on the impulse response
    for _ in range(2):
        ir = np.convolve(ir, np.ones(6) / 6, mode="same")
    ir /= np.sqrt(np.sum(ir ** 2))
    size = 1 << int(np.ceil(np.log2(len(x) + n)))
    y = np.fft.irfft(np.fft.rfft(x, size) * np.fft.rfft(ir, size), size)[: len(x)]
    return y


def main():
    os.makedirs(OUT, exist_ok=True)
    mix = compose()
    L = mix.L * 0.72 + reverb(mix.L, seed=1) * 0.38
    R = mix.R * 0.72 + reverb(mix.R, seed=2) * 0.38
    # trim the silent tail, fade the very end
    end = len(L)
    level = np.abs(L) + np.abs(R)
    while end > SR and level[end - SR:end].max() < 1e-4:
        end -= SR // 10
    L, R = L[:end], R[:end]
    peak = max(np.abs(L).max(), np.abs(R).max())
    L, R = L / peak * 0.89, R / peak * 0.89
    f = int(3 * SR)
    L[-f:] *= np.linspace(1, 0, f)
    R[-f:] *= np.linspace(1, 0, f)
    pcm = (np.stack([L, R], axis=1) * 32767).astype("<i2")
    wav = os.path.join(OUT, "otevreny_obzor.wav")
    with wave.open(wav, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(pcm.tobytes())
    mp3 = os.path.join(OUT, "otevreny_obzor.mp3")
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", wav, "-af", "highpass=f=35", "-b:a", "192k",
                    "-metadata", "title=Otevřený obzor", "-metadata", "album=VoxelCraft", mp3], check=True)
    os.remove(wav)  # only the mp3 is kept
    print(mp3, f"{len(L) / SR:.1f} s")


if __name__ == "__main__":
    main()
