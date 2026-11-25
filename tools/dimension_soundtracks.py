#!/usr/bin/env python3
"""Four new, original pieces for the dimensions of VoxelCraft, composed and
synthesised here with numpy (no samples, nothing from other games). Each one has
its own melody, harmony, key, metre and instruments:

  cornerstone      "Žhavé kameny"    D harmonic minor, 3/4, 66 BPM: stone marimba ostinato, horn, thuds
  crimson_void     "Oko prázdnoty"   E Phrygian clusters, 54 BPM: glass motif, heartbeat that speeds up
  celestial_reach  "Nad oblaky"      G Lydian waltz, 96 BPM: rising arpeggios, choir, key change up
  caves            "Hlubiny"         F# minor, 50 BPM: sparse piano, music box, drips, echo

    python3 tools/dimension_soundtracks.py [name ...]  ->  assets_src/music/<name>.mp3
"""
import os
import re
import subprocess
import sys
import wave

import numpy as np

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sandbox_soundtrack import SR, OUT, Mix, env, hz, piano, bell, pad, bass, reverb  # noqa: E402

rng = np.random.default_rng(31)


# ------------------------------------------------------- new instruments ---

def horn(m, dur, vel):
    """Low brass-like lead: band-limited saw that opens up slowly."""
    f = hz(m)
    n = int((dur + 0.8) * SR)
    t = np.arange(n) / SR
    bright = 1 - np.exp(-t / 0.25)
    out = np.zeros(n)
    for k in range(1, 10):
        if f * k > 6000:
            break
        out += np.sin(2 * np.pi * f * k * t) / k * (bright ** (k * 0.5))
    out *= 1 + 0.04 * np.sin(2 * np.pi * 5.0 * t) * np.minimum(1, t / 0.6)  # vibrato swell
    return out * env(n, 0.08, 0.5) * vel * 0.13


def lava_pad(m, dur, vel):
    """Dark pad with a slow, uneven tremolo like bubbling lava."""
    p = pad(m, dur, vel)
    t = np.arange(len(p)) / SR
    trem = 1 + 0.25 * np.sin(2 * np.pi * 0.7 * t + 0.6 * np.sin(2 * np.pi * 0.13 * t))
    return p * trem


def thud(vel):
    """Deep stone thud (a soft tom/timpani)."""
    n = int(1.4 * SR)
    t = np.arange(n) / SR
    f = 46 + 40 * np.exp(-t / 0.05)
    ph = 2 * np.pi * np.cumsum(f) / SR
    out = np.sin(ph) * np.exp(-t / 0.35)
    out += rng.standard_normal(n) * np.exp(-t / 0.015) * 0.08
    return out * vel * 0.55


def glass(m, dur, vel):
    """Detuned, wobbling glass bell for the Crimson Void."""
    f = hz(m)
    n = int((dur + 3.0) * SR)
    t = np.arange(n) / SR
    wob = 1 + 0.004 * np.sin(2 * np.pi * 0.8 * t)
    out = np.zeros(n)
    for ratio, a, d in ((1.0, 1.0, 2.0), (2.76, 0.5, 0.9), (5.4, 0.25, 0.4), (1.012, 0.7, 2.0)):
        out += a * np.sin(2 * np.pi * f * ratio * wob * t) * np.exp(-t / d)
    return out * env(n, 0.004, 0.6) * vel * 0.10


def swell(m, dur, vel):
    """Reversed-sounding swell: grows and cuts off."""
    p = pad(m, dur, 1.0)[: int(dur * SR)]
    shape = np.linspace(0, 1, len(p)) ** 3
    shape[-int(0.05 * SR):] *= np.linspace(1, 0, int(0.05 * SR))
    return p * shape * vel * 1.6


def heartbeat(vel):
    n = int(1.0 * SR)
    t = np.arange(n) / SR
    beat = lambda d: np.where(t >= d, np.sin(2 * np.pi * 52 * (t - d)) * np.exp(-np.maximum(t - d, 0) / 0.09), 0)
    return (beat(0.0) + 0.7 * beat(0.26)) * vel * 0.5


def choir(m, dur, vel):
    """Soft 'aah' choir: harmonics weighted by vowel formants."""
    f = hz(m)
    n = int((dur + 1.8) * SR)
    t = np.arange(n) / SR
    out = np.zeros(n)
    formants = ((800, 1.0, 120), (1150, 0.5, 140), (2900, 0.12, 200))
    for voice, det in enumerate((-0.08, 0.0, 0.09)):
        ff = f * 2 ** (det / 12)
        vib = 1 + 0.005 * np.sin(2 * np.pi * (4.8 + voice * 0.3) * t)
        ph = 2 * np.pi * ff * np.cumsum(vib) / SR
        for k in range(1, 30):
            fk = ff * k
            if fk > 5000:
                break
            g = sum(a * np.exp(-((fk - fc) / bw) ** 2 / 2) for fc, a, bw in formants) + 0.02 / k
            out += g * np.sin(k * ph)
    return out * env(n, 0.9, 1.4) * vel * 0.05


def shimmer(m, dur, vel):
    """Tiny high sparkle."""
    f = hz(m)
    n = int((dur + 1.5) * SR)
    t = np.arange(n) / SR
    out = np.sin(2 * np.pi * f * t) + 0.3 * np.sin(2 * np.pi * f * 4 * t) * np.exp(-t / 0.2)
    return out * env(n, 0.002, 0.3, decay=0.7) * vel * 0.09


def drip(vel):
    """A drop of water: a short sine falling... then rising in pitch."""
    n = int(0.25 * SR)
    t = np.arange(n) / SR
    f0 = rng.uniform(1100, 1900)
    f = f0 * (1 + 0.8 * (1 - np.exp(-t / 0.02)))
    ph = 2 * np.pi * np.cumsum(f) / SR
    return np.sin(ph) * env(n, 0.001, 0.05, decay=0.04) * vel * 0.18


def drone(m, seconds, vel):
    f = hz(m)
    n = int(seconds * SR)
    t = np.arange(n) / SR
    out = np.sin(2 * np.pi * f * t) + 0.5 * np.sin(2 * np.pi * f * 1.5 * t + 0.3 * np.sin(2 * np.pi * 0.05 * t))
    out += 0.3 * np.sin(2 * np.pi * f * 2.003 * t)
    out *= 0.75 + 0.25 * np.sin(2 * np.pi * 0.07 * t)
    return out * env(n, 4.0, 4.0) * vel * 0.10


def noise_wind(seconds, vel, cutoff=300):
    """Soft wind: noise low-passed in the frequency domain, slowly breathing."""
    n = int(seconds * SR)
    spec = np.fft.rfft(rng.standard_normal(n))
    f = np.fft.rfftfreq(n, 1 / SR)
    spec *= 1 / (1 + (f / cutoff) ** 2)
    y = np.fft.irfft(spec, n)
    t = np.arange(n) / SR
    y *= 0.6 + 0.4 * np.sin(2 * np.pi * 0.06 * t)
    return y / (np.abs(y).max() + 1e-9) * env(n, 3.0, 3.0) * vel * 0.05


def marimba(m, dur, vel):
    """Stone marimba: wooden/stony bar with a quick bright overtone."""
    f = hz(m)
    n = int((dur + 1.0) * SR)
    t = np.arange(n) / SR
    out = np.sin(2 * np.pi * f * t) * np.exp(-t / 0.55)
    out += 0.35 * np.sin(2 * np.pi * f * 3.93 * t) * np.exp(-t / 0.08)
    out += 0.12 * np.sin(2 * np.pi * f * 9.8 * t) * np.exp(-t / 0.02)
    return out * env(n, 0.002, 0.2) * vel * 0.22


def music_box(m, dur, vel):
    f = hz(m)
    n = int((dur + 2.0) * SR)
    t = np.arange(n) / SR
    out = np.sin(2 * np.pi * f * t) + 0.4 * np.sin(2 * np.pi * f * 5.04 * t) * np.exp(-t / 0.15)
    return out * env(n, 0.001, 0.5, decay=1.1) * vel * 0.10


# ------------------------------------------------------------------ helpers ---

NOTE = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}


def notes(text):
    """'D5:1 F#5:.5 r:2 | ...' -> list of bars of (beat, length, midi); '|' ends a bar."""
    bars, cur, beat = [], [], 0.0
    for tok in text.split():
        if tok == "|":
            bars.append(cur)
            cur, beat = [], 0.0
            continue
        name, ln = tok.split(":")
        ln = float(ln)
        if name != "r":
            mm = re.fullmatch(r"([A-G])([#b]?)(-?\d)", name)
            midi = 12 * (int(mm.group(3)) + 1) + NOTE[mm.group(1)] + {"#": 1, "b": -1, "": 0}[mm.group(2)]
            cur.append((beat, ln, midi))
        beat += ln
    if cur:
        bars.append(cur)
    return bars


def h(x, a=0.012):
    return x + rng.uniform(-a, a)


class Song:
    def __init__(self, bpm, meter, bars):
        self.beat = 60.0 / bpm
        self.meter = meter
        self.bar = meter * self.beat
        self.seconds = bars * self.bar + 12
        self.mix = Mix(self.seconds)

    def t(self, bar, beat=0.0):
        return bar * self.bar + beat * self.beat

    def add(self, sig, t, pan=0.0):
        self.mix.add(sig, max(0.0, t), pan)

    def melody(self, bar0, bars, inst, vel=1.0, shift=0, pan=0.0, loose=0.012):
        for k, bar in enumerate(bars):
            for bt, ln, m in bar:
                self.add(inst(m + shift, ln * self.beat, h(vel, 0.05)), h(self.t(bar0 + k, bt), loose), pan)


# ------------------------------------------------------------ Cornerstone ---

def cornerstone():
    CH = {"Dm": (38, (53, 57, 62)), "Bb": (34, (53, 58, 62)), "Gm": (43, (55, 58, 62)),
          "A7": (45, (55, 61, 64)), "C": (36, (55, 60, 64)), "F": (41, (57, 60, 65))}
    intro = ["Dm", "Dm", "Bb", "A7"]
    A = ["Dm", "Dm", "Bb", "Bb", "Gm", "Gm", "A7", "A7"]
    B = ["Bb", "C", "F", "A7", "Gm", "Bb", "A7", "A7"]
    outro = ["Dm", "Bb", "A7", "Dm"]
    melA = notes("A4:2 D5:1 | F5:1.5 E5:.5 D5:1 | F5:2 G5:1 | F5:3 | G5:2 Bb5:1 | A5:1.5 G5:.5 F5:1 | E5:2 C#5:1 | A4:3")
    melB = notes("D6:2 C6:1 | C6:1 Bb5:1 G5:1 | A5:3 | C#6:1.5 E6:.5 C#6:1 | D6:2 Bb5:1 | F6:1.5 D6:.5 Bb5:1 | A5:1 G5:1 E5:1 | C#5:3")
    form = [("intro", intro), ("A", A), ("B", B), ("A2", A), ("B", B), ("A2", A), ("outro", outro)]
    s = Song(66, 3, sum(len(c) for _, c in form))
    s.add(drone(26, s.seconds - 8, 0.8), 0)  # low D
    bar = 0
    for sec, chords in form:
        for i, c in enumerate(chords):
            b, v = CH[c]
            t0 = s.t(bar)
            last = sec == "outro" and i == len(chords) - 1
            hold = s.bar * (3 if last else 1)
            fade = 1 - 0.15 * i if sec == "outro" else 1
            for k, m in enumerate(v):
                s.add(lava_pad(m - 12, hold, 0.8 * fade), t0, pan=-0.5 + k / 2)
            # the ostinato in eighths (sixteenths in A2)
            pattern = [b + 12, v[0], v[1], v[2], v[1], v[0]]
            steps = 12 if sec == "A2" else 6
            if not last:
                for st in range(steps):
                    m = pattern[st % 6] + (12 if sec == "A2" and st % 2 else 0)
                    s.add(marimba(m, s.beat / 2, h(0.85 if st % 6 == 0 else 0.55, 0.06) * fade),
                          h(t0 + st * s.bar / steps, 0.006), pan=-0.4 + 0.8 * (st % 6) / 5)
            if sec != "intro":
                s.add(bass(b, s.bar * 0.95 if not last else hold, 1.1 * fade), t0)
            # thuds: 1 in intro/outro, 1 + 3 in A/B, every beat in A2
            beats = {"intro": (0,), "outro": (0,), "A": (0, 2), "B": (0, 2), "A2": (0, 1, 2)}[sec]
            for bt in beats:
                s.add(thud(h(0.9 if bt == 0 else 0.5, 0.05) * fade), t0 + bt * s.beat)
            if last:
                s.add(horn(62, s.bar * 2, 0.9), t0 + s.beat)
            bar += 1
    s.melody(4, melA, horn, 1.0, shift=-12, pan=0.1)
    s.melody(12, melB, horn, 0.9, shift=-12, pan=0.1)
    s.melody(12, melB, piano, 0.5, shift=0, pan=0.35)
    s.melody(20, melA, horn, 1.0, shift=-12, pan=0.1)
    s.melody(20, melA, piano, 0.55, shift=12, pan=-0.3)  # countermelody an octave above
    s.melody(28, melB, piano, 0.7, shift=0, pan=0.2)     # second time: piano leads, horn an octave down
    s.melody(28, melB, horn, 0.7, shift=-24, pan=-0.1)
    s.melody(36, melA, horn, 1.0, shift=-12, pan=0.1)
    s.melody(36, melA, marimba, 0.8, shift=12, pan=-0.3)
    return s, "Žhavé kameny", 3.2, 0.42


# ----------------------------------------------------------- Crimson Void ---

def crimson_void():
    CH = {"Em": (40, (52, 55, 57, 59)), "F": (41, (53, 57, 59, 64)), "Bbm": (46, (53, 58, 61, 65))}
    form = [("intro", ["Em", "Em", "F", "Em"]),
            ("A", ["Em", "F", "Em", "F", "Bbm", "Bbm", "F", "Em"]),
            ("B", ["Em", "F", "Em", "F", "Bbm", "F", "Bbm", "F"]),
            ("break", ["Em", "Em"]),
            ("A2", ["Em", "F", "Em", "F"]),
            ("outro", ["Em", "F", "Em", "Em"])]
    motif = notes("E5:1 F5:1 B4:2 | r:1 G5:.5 F5:.5 E5:2")
    up = lambda bars, k: [[(bt, ln, m + k) for bt, ln, m in b] for b in bars]
    inv = lambda bars, axis: [[(bt, ln, 2 * axis - m) for bt, ln, m in b] for b in bars]
    s = Song(54, 4, sum(len(c) for _, c in form))
    s.add(drone(28, s.seconds - 8, 0.9), 0)
    s.add(noise_wind(s.seconds - 8, 1.2, 180), 0)
    bar = 0
    for sec, chords in form:
        for i, c in enumerate(chords):
            b, v = CH[c]
            t0 = s.t(bar)
            last = sec == "outro" and i == len(chords) - 1
            hold = s.bar * (2.5 if last else 1)
            fade = 1 - 0.18 * i if sec == "outro" else 1
            for k, m in enumerate(v[:3]):
                s.add(pad(m - 12, hold, 0.65 * fade), t0, pan=-0.6 + 0.6 * k)
            if i % 2 == 1:
                s.add(swell(v[3] + 12, s.bar, 0.7 * fade), t0, pan=0.5 if i % 4 == 1 else -0.5)
            if sec in ("A", "A2"):
                s.add(heartbeat(0.9), t0)
                s.add(heartbeat(0.7), t0 + 2 * s.beat)
                s.add(bass(b, s.bar * 0.9, 0.7), t0)
            if sec == "B":
                # the Crimson Man is coming: the heart beats faster every bar
                beats = 2 + i
                for k in range(beats):
                    s.add(heartbeat(0.6 + 0.05 * i), t0 + k * s.bar / beats)
                for k, m in enumerate(v):
                    s.add(choir(m - 12, s.bar, 0.6), t0, pan=-0.5 + k / 3)
                for _ in range(2 + i // 2):
                    s.add(glass(int(rng.choice(v)) + 24, s.beat, 0.35), t0 + rng.uniform(0, s.bar), pan=rng.uniform(-0.8, 0.8))
            if sec == "break" and i == 1:
                s.add(glass(76, s.bar, 0.6), t0 + s.beat)
            bar += 1
    s.melody(4, motif, glass, 1.0)
    s.melody(4, motif, piano, 0.35, shift=-24)
    s.melody(6, motif, glass, 0.9, shift=12, pan=-0.3)
    s.melody(8, up(motif, 6), glass, 1.0, pan=0.3)          # a tritone higher
    s.melody(10, inv(motif, 76), glass, 0.9, pan=-0.2)      # upside down
    s.melody(10, inv(motif, 76), piano, 0.3, shift=-24)
    s.melody(22, motif, glass, 0.8, shift=-12)
    s.melody(24, up(motif, 6), glass, 0.6, shift=-12, pan=0.4)
    s.melody(28, [motif[0]], glass, 0.5)
    return s, "Oko prázdnoty", 4.5, 0.55


# -------------------------------------------------------- Celestial Reach ---

def celestial_reach():
    CH = {"Gmaj7": (43, (59, 62, 66, 69)), "A/G": (43, (57, 61, 64, 69)), "Em9": (40, (55, 59, 62, 66)),
          "C#11": (36, (55, 59, 64, 66)), "D/F#": (42, (57, 62, 66, 69))}
    A = ["Gmaj7", "A/G", "Em9", "C#11", "Gmaj7", "A/G", "C#11", "D/F#"]
    B = ["Em9", "C#11", "Gmaj7", "D/F#", "Em9", "C#11", "A/G", "A/G"]
    form = [("intro", ["Gmaj7", "A/G", "Gmaj7", "A/G"], 0), ("A", A, 0), ("B", B, 0), ("A", A, 0),
            ("break", ["Em9", "C#11", "Em9", "D/F#"], 0), ("A2", A, 2), ("B2", B, 2), ("A2", A, 2),
            ("outro", ["Gmaj7", "C#11", "Gmaj7", "Gmaj7"], 2)]
    melA = notes("D5:1 G5:1 A5:1 | B5:2 A5:1 | G5:1 F#5:1 E5:1 | F#5:3 | D5:1 G5:1 B5:1 | C#6:2 B5:1 | A5:1 G5:1 E5:1 | D5:3")
    melB = notes("B5:3 | E6:3 | D6:2 B5:1 | A5:3 | B5:2 G5:1 | F#5:3 | E5:1 F#5:1 A5:1 | C#6:3")
    s = Song(96, 3, sum(len(c) for _, c, _ in form))
    s.add(noise_wind(s.seconds - 8, 0.7, 900), 0, pan=0.2)
    bar = 0
    for sec, chords, tr in form:
        for i, c in enumerate(chords):
            b, v = CH[c]
            b, v = b + tr, [m + tr for m in v]
            t0 = s.t(bar)
            last = sec == "outro" and i == len(chords) - 1
            hold = s.bar * (4 if last else 1)
            fade = 1 - 0.15 * i if sec == "outro" else 1
            for k, m in enumerate(v):
                s.add(choir(m, hold, (0.55 if sec != "B2" else 0.75) * fade), t0, pan=-0.6 + 0.4 * k)
            if sec not in ("intro", "break"):
                s.add(bass(b + 12, s.bar * 0.9 if not last else hold, 0.45 * fade), t0)
                s.add(piano(b + 24, s.bar, 0.35 * fade), t0)  # waltz: low note on 1, chord on 2 and 3
                for bt in (1, 2):
                    for k, m in enumerate(v[:3]):
                        s.add(piano(m + 12, s.beat * 0.8, h(0.25, 0.04) * fade), h(t0 + bt * s.beat), pan=-0.2 + 0.2 * k)
            # rising arpeggio in eighths, all the way up
            if not last:
                arp = [v[0] + 12, v[1] + 12, v[2] + 12, v[3] + 12, v[1] + 24, v[2] + 24]
                for st in range(6):
                    s.add(shimmer(arp[st], s.beat / 2, h(0.6 if st == 0 else 0.45, 0.06) * fade),
                          s.t(bar, st / 2), pan=-0.7 + 0.28 * st)
            else:
                for k, m in enumerate(v + [v[0] + 12]):
                    s.add(bell(m + 12, s.bar * 2, 0.4), t0 + k * 0.18, pan=-0.5 + 0.2 * k)
            bar += 1
    s.melody(4, melA, bell, 0.9, pan=0.25)
    s.melody(4, melA, piano, 0.5, pan=-0.1)
    s.melody(12, melB, choir, 0.9)
    s.melody(12, melB, bell, 0.6, shift=12, pan=0.3)
    s.melody(20, melA, piano, 0.7, shift=12, pan=0.1)     # second time: piano up high, bells answer
    s.melody(20, melA, bell, 0.45, shift=-12, pan=-0.3)
    s.melody(32, melA, bell, 1.0, shift=2, pan=0.25)      # after the break: one tone higher
    s.melody(32, melA, piano, 0.6, shift=2, pan=-0.1)
    s.melody(40, melB, choir, 1.0, shift=2)
    s.melody(40, melB, bell, 0.8, shift=14, pan=0.3)
    s.melody(48, melA, bell, 1.0, shift=2, pan=0.25)
    s.melody(48, melA, choir, 0.6, shift=2)
    return s, "Nad oblaky", 5.0, 0.55


# ------------------------------------------------------------------ Caves ---

def caves():
    CH = {"F#m9": (30, (57, 61, 64, 68)), "Dmaj7": (38, (54, 57, 61, 62)), "Bm9": (35, (50, 54, 57, 61)),
          "C#sus": (37, (54, 56, 61, 66)), "E6": (40, (56, 59, 61, 64))}
    A = ["F#m9", "Dmaj7", "Bm9", "C#sus", "F#m9", "Dmaj7", "Bm9", "C#sus"]
    form = [("intro", ["F#m9", "F#m9", "Dmaj7", "C#sus"]), ("A", A),
            ("B", ["Dmaj7", "E6", "F#m9", "F#m9", "Bm9", "Dmaj7", "C#sus", "C#sus"]),
            ("A2", A[:4]), ("outro", ["F#m9", "Dmaj7", "C#sus", "F#m9"])]
    mel = notes("C#5:1 r:.5 E5:.5 F#5:2 | r:2 A5:1 G#5:1 | F#5:1.5 E5:.5 D5:2 | C#5:4 | "
                "C#5:1 r:.5 E5:.5 A5:2 | r:1 G#5:1 F#5:1 E5:1 | D5:2 F#5:1 E5:1 | C#5:4")
    box = notes("A5:1 C#6:1 F#6:2 | E6:2 B5:2 | C#6:4 | r:4 | D6:1 C#6:1 B5:2 | A5:2 F#5:2 | G#5:4 | r:4")
    s = Song(50, 4, sum(len(c) for _, c in form))
    s.add(drone(30, s.seconds - 8, 1.0), 0)
    s.add(noise_wind(s.seconds - 8, 0.9, 150), 0)
    bar = 0
    for sec, chords in form:
        for i, c in enumerate(chords):
            b, v = CH[c]
            t0 = s.t(bar)
            last = sec == "outro" and i == len(chords) - 1
            hold = s.bar * (2 if last else 1)
            fade = 1 - 0.18 * i if sec == "outro" else 1
            for k, m in enumerate(v[:3]):
                s.add(pad(m - 12, hold, 0.55 * fade), t0, pan=-0.5 + 0.5 * k)
            if sec in ("A", "A2"):
                s.add(bass(b + 12, s.bar * 0.9, 0.7), t0)
                s.add(piano(v[0], s.bar, 0.3), t0 + 2 * s.beat, pan=-0.3)
            if sec == "B":
                for bt in (0, 1.5, 3):  # deep slow pulse
                    s.add(bass(b + 12, s.beat * 1.2, 0.8 if bt == 0 else 0.5), t0 + bt * s.beat)
            if last:
                for k, m in enumerate(v):
                    s.add(piano(m, s.bar * 2, 0.4), t0 + k * 0.25, pan=-0.3 + 0.2 * k)
            for _ in range(int(rng.integers(1, 4))):
                s.add(drip(rng.uniform(0.4, 1.0)), t0 + rng.uniform(0, s.bar), pan=rng.uniform(-0.9, 0.9))
            bar += 1
    s.melody(4, mel, piano, 0.85, pan=0.1)
    s.melody(12, box, music_box, 1.0, pan=0.3)
    s.melody(12, box, piano, 0.3, shift=-12, pan=-0.2)
    s.melody(20, mel[:4], piano, 0.75, pan=0.1)
    s.melody(20, mel[:4], music_box, 0.4, shift=12, pan=-0.3)
    return s, "Hlubiny", 3.8, 0.5


SONGS = {"cornerstone": cornerstone, "crimson_void": crimson_void,
         "celestial_reach": celestial_reach, "caves": caves}


def render(name):
    s, title, rev, wet = SONGS[name]()
    L, R = s.mix.L.copy(), s.mix.R.copy()
    if name == "caves":
        d = int(s.beat * 0.75 * SR)  # echo across the cave
        for k, g in ((1, 0.35), (2, 0.18), (3, 0.09)):
            src_l, src_r = (s.mix.R, s.mix.L) if k % 2 else (s.mix.L, s.mix.R)
            L[d * k:] += g * src_l[: -d * k]
            R[d * k:] += g * src_r[: -d * k]
    dry = 1 - wet * 0.6
    L2 = L * dry + reverb(L, rev, seed=11) * wet
    R2 = R * dry + reverb(R, rev, seed=12) * wet
    end = len(L2)
    lvl = np.abs(L2) + np.abs(R2)
    while end > SR and lvl[end - SR:end].max() < 1e-4 * lvl.max():
        end -= SR // 10
    L2, R2 = L2[:end], R2[:end]
    peak = max(np.abs(L2).max(), np.abs(R2).max())
    L2, R2 = L2 / peak * 0.89, R2 / peak * 0.89
    f = int(4 * SR)
    L2[-f:] *= np.linspace(1, 0, f)
    R2[-f:] *= np.linspace(1, 0, f)
    wav = os.path.join(OUT, name + ".wav")
    with wave.open(wav, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes((np.stack([L2, R2], axis=1) * 32767).astype("<i2").tobytes())
    mp3 = os.path.join(OUT, name + ".mp3")
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", wav, "-af", "highpass=f=30", "-b:a", "192k",
                    "-metadata", "title=" + title, "-metadata", "album=VoxelCraft", mp3], check=True)
    os.remove(wav)
    print(mp3, title, f"{len(L2) / SR:.1f} s")


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    for n in sys.argv[1:] or list(SONGS):
        render(n)
