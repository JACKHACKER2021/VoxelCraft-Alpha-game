#!/usr/bin/env python3
"""Sounds for the 26 creatures of VoxelCraft (models_2.8.0/mobs), synthesised here
with numpy: voices from harmonics shaped by vowel formants, noise, bubbles,
clicks. No recordings, nothing from other games.

For every creature:  mob_<name>_say_1.ogg, mob_<name>_say_2.ogg (idle sounds),
                     mob_<name>_hurt.ogg, mob_<name>_death.ogg
Mono Ogg Vorbis, 44.1 kHz, like the other sounds of the game, written to
assets_src/sounds/mobs/, and a demo of all of them, assets_src/sounds/mob_sounds_demo.mp3.

    python3 tools/mob_sounds.py
"""
import os
import subprocess
import sys
import wave

import numpy as np

sys.dont_write_bytecode = True
SR = 44100
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "assets_src", "sounds", "mobs")
DEMO = os.path.join(ROOT, "assets_src", "sounds", "mob_sounds_demo.mp3")
rng = np.random.default_rng(2808)

# ------------------------------------------------------------- building blocks ---

VOWELS = {  # formants (Hz, amplitude, bandwidth)
    "a": ((800, 1.0, 110), (1150, 0.5, 120), (2900, 0.10, 180)),
    "o": ((450, 1.0, 90), (800, 0.45, 100), (2830, 0.06, 180)),
    "u": ((325, 1.0, 80), (700, 0.25, 100), (2530, 0.04, 180)),
    "e": ((400, 1.0, 90), (2000, 0.35, 140), (2550, 0.15, 180)),
    "i": ((300, 1.0, 70), (2300, 0.35, 150), (3000, 0.20, 200)),
    "m": ((250, 1.0, 70), (1000, 0.08, 150), (2200, 0.03, 200)),  # nasal hum
    "n": ((300, 1.0, 80), (1400, 0.12, 150), (2600, 0.04, 200)),
    "ae": ((660, 1.0, 100), (1700, 0.5, 130), (2400, 0.15, 180)),
}


def T(dur):
    return np.arange(int(dur * SR)) / SR


def curve(points, dur):
    """Piecewise linear curve through (time fraction, value) points."""
    t = T(dur) / dur
    xs, ys = zip(*points)
    return np.interp(t, xs, ys)


def lowpass_noise(n, cutoff, seed=None):
    r = np.random.default_rng(seed) if seed is not None else rng
    spec = np.fft.rfft(r.standard_normal(n))
    f = np.fft.rfftfreq(n, 1 / SR)
    spec *= 1 / (1 + (f / cutoff) ** 4)
    y = np.fft.irfft(spec, n)
    return y / (np.abs(y).max() + 1e-9)


def band_noise(dur, lo, hi):
    n = int(dur * SR)
    spec = np.fft.rfft(rng.standard_normal(n))
    f = np.fft.rfftfreq(n, 1 / SR)
    spec *= ((f >= lo) & (f <= hi)) * 1.0
    y = np.fft.irfft(spec, n)
    return y / (np.abs(y).max() + 1e-9)


def shape(n, attack, release, hold_curve=None):
    e = np.ones(n)
    a, r = min(n, max(1, int(attack * SR))), min(n, max(1, int(release * SR)))
    e[:a] = np.linspace(0, 1, a)
    e[-r:] *= np.linspace(1, 0, r) ** 1.5
    return e


def voice(dur, f0, vowels, rough=0.0, breath=0.0, vib=(0.0, 0.0), bright=1.0, fmax=7000):
    """A voice: harmonics of f0 (number or (t, value) points), weighted by vowel
    formants that glide from one vowel to the next."""
    n = int(dur * SR)
    f = curve(f0, dur) if isinstance(f0, (list, tuple)) else np.full(n, float(f0))
    t = T(dur)
    if vib[0]:
        f = f * (1 + vib[1] * np.sin(2 * np.pi * vib[0] * t))
    if rough:
        f = f * (1 + rough * 0.06 * lowpass_noise(n, 40))
    ph = 2 * np.pi * np.cumsum(f) / SR
    # formant tracks
    keys = [VOWELS[v] for v in vowels]
    pos = np.linspace(0, len(keys) - 1, n)
    lo = np.floor(pos).astype(int)
    hi = np.minimum(lo + 1, len(keys) - 1)
    fr = pos - lo
    tracks = []
    for j in range(3):
        fc = np.array([k[j][0] for k in keys])
        am = np.array([k[j][1] for k in keys])
        bw = np.array([k[j][2] for k in keys])
        tracks.append(((fc[lo] * (1 - fr) + fc[hi] * fr), (am[lo] * (1 - fr) + am[hi] * fr),
                       (bw[lo] * (1 - fr) + bw[hi] * fr)))
    out = np.zeros(n)
    for k in range(1, 80):
        fk = f * k
        if fk.min() > fmax:
            break
        g = sum(a * np.exp(-((fk - fc) / bw) ** 2 / 2) for fc, a, bw in tracks) + 0.03 * bright / k
        out += g * np.sin(k * ph) * (fk < fmax)
    if rough:
        out *= 1 + rough * 0.5 * lowpass_noise(n, 70)
    if breath:
        nz = band_noise(dur, 300, 5000)
        out += breath * nz * np.abs(out).max()
    return out * shape(n, 0.015, min(0.08, dur / 3))


def tone(dur, f, harm=(1.0,), decay=None, vib=(0, 0)):
    n = int(dur * SR)
    fr = curve(f, dur) if isinstance(f, (list, tuple)) else np.full(n, float(f))
    t = T(dur)
    if vib[0]:
        fr = fr * (1 + vib[1] * np.sin(2 * np.pi * vib[0] * t))
    ph = 2 * np.pi * np.cumsum(fr) / SR
    out = sum(a * np.sin((k + 1) * ph) for k, a in enumerate(harm))
    if decay:
        out *= np.exp(-t / decay)
    return out * shape(n, 0.005, min(0.05, dur / 3))


def noise(dur, lo, hi, attack=0.01, release=0.1, decay=None):
    n = int(dur * SR)
    out = band_noise(dur, lo, hi) * shape(n, attack, release)
    if decay:
        out *= np.exp(-T(dur) / decay)
    return out


def bubble(f0, dur=0.12):
    t = T(dur)
    f = f0 * (1 + 1.5 * (1 - np.exp(-t / 0.03)))
    return np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t / (dur / 3)) * shape(len(t), 0.002, 0.02)


def click(dur=0.012, lo=1500, hi=9000):
    return noise(dur, lo, hi, 0.0005, 0.008, decay=0.003)


def thump(f0=60, dur=0.4):
    t = T(dur)
    f = f0 + 70 * np.exp(-t / 0.03)
    return np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t / (dur / 3.5))


def seq(*parts):
    """Lays parts end to end; a number is a pause in seconds, (signal, offset) overlaps."""
    out = np.zeros(0)
    for p in parts:
        if isinstance(p, (int, float)):
            out = np.concatenate([out, np.zeros(int(p * SR))])
        elif isinstance(p, tuple):
            sig, off = p
            start = max(0, len(out) + int(off * SR))
            end = start + len(sig)
            if end > len(out):
                out = np.concatenate([out, np.zeros(end - len(out))])
            out[start:end] += sig
        else:
            out = np.concatenate([out, p])
    return out


def mixdown(*layers):
    n = max(len(x) for x, _ in layers)
    out = np.zeros(n)
    for x, g in layers:
        out[: len(x)] += g * x / (np.abs(x).max() + 1e-9)
    return out


def tremolo(x, rate, depth):
    return x * (1 - depth + depth * (0.5 + 0.5 * np.sin(2 * np.pi * rate * np.arange(len(x)) / SR)))


def echo(x, delay, gain, times=3):
    d = int(delay * SR)
    out = np.concatenate([x, np.zeros(d * times)])
    for k in range(1, times + 1):
        out[d * k: d * k + len(x)] += x * gain ** k
    return out


def room(x, seconds=0.8, wet=0.3, seed=1):
    r = np.random.default_rng(seed)
    n = int(seconds * SR)
    ir = r.standard_normal(n) * np.exp(-T(seconds) / (seconds / 6.9))
    ir /= np.sqrt(np.sum(ir ** 2))
    size = 1 << int(np.ceil(np.log2(len(x) + n)))
    y = np.fft.irfft(np.fft.rfft(x, size) * np.fft.rfft(ir, size), size)[: len(x) + n]
    out = np.concatenate([x, np.zeros(n)]) * (1 - wet) + y * wet
    return out


def pitch(x, factor):
    """Resample (changes speed and pitch together)."""
    idx = np.arange(0, len(x) - 1, factor)
    return np.interp(idx, np.arange(len(x)), x)


def crackle(dur, rate=25):
    out = np.zeros(int(dur * SR))
    for _ in range(int(dur * rate)):
        c = click(rng.uniform(0.004, 0.012), 2000, 12000) * rng.uniform(0.3, 1)
        i = rng.integers(0, max(1, len(out) - len(c)))
        out[i:i + len(c)] += c
    return out


def distort(x, drive):
    return np.tanh(x / (np.abs(x).max() + 1e-9) * drive)


# ---------------------------------------------------------------- creatures ---
# Each returns (say_1, say_2, hurt, death).

def pig():
    def oink(f, d):
        return voice(d, [(0, f), (0.4, f * 1.25), (1, f * 0.8)], ["n", "o", "u"], rough=1.2, breath=0.05)
    return (seq(oink(160, 0.22), 0.06, oink(150, 0.18)),
            seq(oink(145, 0.3)),
            voice(0.28, [(0, 380), (0.3, 520), (1, 300)], ["e", "i", "e"], rough=1.5, breath=0.1),
            seq(voice(0.35, [(0, 420), (1, 260)], ["i", "e"], rough=1.5, breath=0.1), 0.04,
                voice(0.5, [(0, 180), (1, 90)], ["o", "u"], rough=2.0)))


def cow():
    def moo(f, d, end="u"):
        return voice(d, [(0, f), (0.2, f * 1.08), (0.8, f), (1, f * 0.85)], ["m", "u", end, "u"], rough=0.6,
                     vib=(5, 0.01))
    return (moo(110, 1.1, "o"), moo(98, 1.4, "a"),
            voice(0.4, [(0, 160), (0.5, 210), (1, 120)], ["o", "a", "o"], rough=1.0),
            voice(1.0, [(0, 140), (0.4, 120), (1, 70)], ["o", "u", "u"], rough=1.4))


def sheep():
    def baa(f, d):
        x = voice(d, [(0, f), (0.15, f * 1.1), (1, f * 0.92)], ["m", "ae", "ae", "e"], rough=0.4, breath=0.04)
        return tremolo(x, 13, 0.7)
    return (baa(330, 0.7), baa(290, 0.9),
            tremolo(voice(0.35, [(0, 430), (1, 360)], ["ae", "e"]), 16, 0.6),
            tremolo(voice(0.9, [(0, 340), (1, 180)], ["ae", "e", "u"], rough=0.8), 11, 0.7))


def chicken():
    def cluck(f, d=0.07):
        return voice(d, [(0, f), (1, f * 0.85)], ["ae", "o"], rough=0.8, breath=0.15)
    return (seq(cluck(620), 0.08, cluck(600), 0.1, cluck(640), 0.07, cluck(580)),
            seq(cluck(560), 0.12, voice(0.35, [(0, 650), (0.6, 820), (1, 600)], ["ae", "a", "o"], rough=0.6)),
            voice(0.22, [(0, 900), (1, 700)], ["ae", "a"], rough=1.0, breath=0.3),
            seq(voice(0.25, [(0, 950), (1, 780)], ["ae", "a"], rough=1.0, breath=0.3), 0.05,
                noise(0.3, 1500, 6000, 0.005, 0.2) * 0.4, cluck(400, 0.15)))


def fox(base=1.0, ash=False):
    def yip(f, d=0.12):
        return voice(d, [(0, f), (0.4, f * 1.35), (1, f * 0.9)], ["i", "a", "o"], rough=0.3)
    say1 = seq(yip(700 * base), 0.09, yip(760 * base), 0.12, yip(680 * base, 0.16))
    say2 = voice(0.6, [(0, 500 * base), (0.5, 900 * base), (1, 600 * base)], ["u", "a", "o"], vib=(7, 0.02))
    hurt = voice(0.2, [(0, 1200 * base), (1, 800 * base)], ["i", "a"], rough=0.8, breath=0.1)
    death = voice(0.7, [(0, 1000 * base), (1, 380 * base)], ["i", "a", "u"], rough=0.6, vib=(9, 0.03))
    out = [say1, say2, hurt, death]
    if ash:  # embers crackle around the ash fox
        out = [mixdown((x, 1.0), (crackle(len(x) / SR + 0.2, 30), 0.25)) for x in out]
    return tuple(out)


def bat():
    def squeak(f, d=0.04):
        return tone(d, [(0, f), (1, f * 1.3)], (1, 0.3), decay=0.02)
    flap = tremolo(noise(0.5, 200, 1500, 0.02, 0.1), 14, 1.0) * 0.4
    return (seq(squeak(6200), 0.05, squeak(6800), 0.04, squeak(6000)),
            mixdown((flap, 0.6), (seq(0.15, squeak(7000), 0.06, squeak(6500)), 1.0)),
            seq(squeak(5200, 0.08), 0.02, squeak(5800, 0.06)),
            seq(tone(0.3, [(0, 6000), (1, 2500)], (1, 0.3)), 0.05, flap[: int(0.3 * SR)] * 0.5))


def villager():
    def hm(f, d, v=("m", "m")):
        return voice(d, [(0, f), (0.5, f * 1.15), (1, f * 0.95)], list(v), rough=0.2)
    return (seq(hm(140, 0.35), 0.03, hm(125, 0.3, ("m", "a"))),
            hm(150, 0.55, ("m", "u", "m")),
            voice(0.25, [(0, 210), (1, 170)], ["a", "e"], rough=0.6),
            voice(0.8, [(0, 180), (1, 95)], ["a", "o", "u"], rough=0.8))


def zombie(fuse=False, pitch_k=1.0):
    def groan(f, d):
        return voice(d, [(0, f), (0.5, f * 1.1), (1, f * 0.8)], ["u", "a", "u"], rough=2.5, breath=0.12)
    out = [groan(85 * pitch_k, 1.0), groan(75 * pitch_k, 1.2),
           voice(0.3, [(0, 140), (1, 95)], ["a", "o"], rough=2.5, breath=0.1),
           voice(1.1, [(0, 110), (1, 55)], ["a", "o", "u"], rough=3.0, breath=0.15)]
    if fuse:  # the fuse hisses on its head
        out[0] = mixdown((out[0], 1.0), (noise(len(out[0]) / SR, 3000, 9000, 0.05, 0.1) * crackle(len(out[0]) / SR, 40)[: len(out[0])], 0.0),
                         (tremolo(noise(len(out[0]) / SR, 3000, 9000, 0.05, 0.1), 30, 0.4), 0.18))
        out[3] = seq(out[3], (thump(45, 1.2) + noise(1.2, 60, 3000, 0.002, 0.9, decay=0.25) * 0.8, -0.2))
    return tuple(out)


def spider():
    def chitter(n=8):
        parts = []
        for _ in range(n):
            parts += [click(0.01, 2500, 9000) * rng.uniform(0.5, 1), rng.uniform(0.02, 0.05)]
        return seq(*parts)
    hiss = noise(0.6, 2500, 9000, 0.05, 0.25)
    return (chitter(10), mixdown((hiss, 0.5), (chitter(6), 1.0)),
            seq(noise(0.15, 3000, 9000, 0.005, 0.08), chitter(4)),
            seq(noise(0.5, 2000, 8000, 0.01, 0.4), chitter(12)))


def ghost():
    def wail(f, d):
        x = tone(d, [(0, f), (0.4, f * 1.4), (1, f * 0.9)], (1, 0.25, 0.1), vib=(5, 0.025))
        return room(mixdown((x, 1.0), (noise(d, 300, 2500, d / 3, d / 3), 0.25)) * shape(int(d * SR), d / 3, d / 2), 1.5, 0.5)
    return (wail(420, 1.6), wail(380, 2.0),
            room(tone(0.4, [(0, 700), (1, 450)], (1, 0.3), vib=(9, 0.03)), 1.0, 0.4),
            room(tone(1.6, [(0, 600), (1, 120)], (1, 0.3, 0.1), vib=(6, 0.04)) * np.linspace(1, 0, int(1.6 * SR)), 2.0, 0.55))


def snowman():
    crunch = lambda d: tremolo(noise(d, 800, 7000, 0.01, 0.05), 35, 0.8) * 0.5
    hum = voice(0.6, [(0, 200), (0.5, 240), (1, 210)], ["o", "o"], rough=0.1)
    return (seq(crunch(0.25), 0.05, crunch(0.2)),
            mixdown((hum, 1.0), (crunch(0.6), 0.2)),
            seq(crunch(0.15), voice(0.2, [(0, 320), (1, 260)], ["o", "a"])),
            seq(noise(0.7, 400, 6000, 0.01, 0.5, decay=0.25), (crunch(0.5), -0.5)))


def pumpkin_slime():
    def squelch(f, d=0.3):
        x = tone(d, [(0, f * 1.6), (1, f)], (1, 0.5, 0.2), decay=d / 2)
        return mixdown((x, 1.0), (noise(d, 200, 1500, 0.005, d / 2, decay=0.06), 0.4))
    return (seq(squelch(110), 0.15, squelch(90)),
            seq(*[p for f in (300, 380, 260) for p in (bubble(f, 0.1), 0.05)], squelch(120, 0.25)),
            squelch(180, 0.2),
            seq(squelch(140, 0.25), 0.03, squelch(90, 0.3), 0.03, squelch(60, 0.5)))


def forgotten_man():
    def whisper(d, v):
        return voice(d, [(0, 120), (1, 100)], v, breath=1.2, rough=0.3) * 0.6
    return (room(seq(whisper(0.4, ["a", "o"]), 0.1, whisper(0.5, ["e", "u"])), 1.2, 0.5),
            room(voice(1.2, [(0, 110), (0.5, 125), (1, 90)], ["o", "u", "m"], rough=1.0, breath=0.4), 1.2, 0.45),
            room(voice(0.3, [(0, 170), (1, 120)], ["a", "o"], rough=1.2, breath=0.4), 0.8, 0.3),
            room(voice(1.4, [(0, 140), (1, 60)], ["a", "o", "u"], rough=1.5, breath=0.6), 1.6, 0.5))


def husking():
    breath = lambda d: noise(d, 400, 3000, d / 3, d / 2) * 0.6
    spit = seq(noise(0.08, 1000, 8000, 0.002, 0.04), (noise(0.5, 100, 2500, 0.01, 0.3, decay=0.15), 0.0))
    return (seq(breath(0.6), 0.1, breath(0.5)),
            seq(voice(0.4, [(0, 95), (1, 85)], ["a", "e"], rough=2.0, breath=0.8), 0.1, spit),
            voice(0.3, [(0, 160), (1, 110)], ["e", "a"], rough=2.5, breath=0.5),
            seq(voice(0.9, [(0, 130), (1, 50)], ["e", "a", "o"], rough=3.0, breath=0.6), (crackle(0.8, 30), -0.6)))


def tar_monster():
    def gurgle(d):
        parts = []
        t = 0.0
        while t < d:
            parts.append((bubble(rng.uniform(90, 220), rng.uniform(0.1, 0.2)), 0.0))
            gap = rng.uniform(0.04, 0.1)
            parts.append(gap)
            t += gap + 0.05
        return seq(*[p for p in parts])
    low = voice(1.0, [(0, 60), (1, 50)], ["o", "u"], rough=3.0) * 0.7
    return (gurgle(0.8), mixdown((low, 0.8), (gurgle(1.0), 0.7)),
            seq(bubble(300, 0.1), voice(0.3, [(0, 90), (1, 70)], ["o", "u"], rough=3.0)),
            mixdown((voice(1.3, [(0, 80), (1, 35)], ["o", "u"], rough=3.5), 1.0), (gurgle(1.4), 0.8)))


def hogman():
    def grunt(f, d):
        return voice(d, [(0, f), (0.3, f * 1.2), (1, f * 0.75)], ["n", "o", "u"], rough=2.0, breath=0.08)
    return (seq(grunt(95, 0.35), 0.1, grunt(88, 0.3)),
            seq(grunt(100, 0.5), 0.05, voice(0.4, [(0, 110), (1, 85)], ["a", "o"], rough=2.0)),
            voice(0.3, [(0, 190), (1, 130)], ["a", "e"], rough=2.2, breath=0.1),
            seq(voice(0.5, [(0, 200), (1, 120)], ["e", "a"], rough=2.0), 0.03, grunt(80, 0.8)))


def cornerstone_golem():
    grind = lambda d: tremolo(noise(d, 60, 900, d / 4, d / 3), 9, 0.6)
    return (seq(thump(50, 0.5), (grind(0.9), -0.3)),
            mixdown((grind(1.2), 1.0), (voice(1.2, [(0, 55), (1, 48)], ["o", "u"], rough=2.0), 0.6)),
            seq(noise(0.12, 800, 6000, 0.001, 0.08, decay=0.03), (thump(70, 0.4), -0.08)),
            seq(grind(0.6), thump(45, 0.6), 0.05, thump(40, 0.8), (noise(1.0, 300, 5000, 0.005, 0.7, decay=0.3), -0.8)))


def moss_lurker():
    rustle = lambda d: tremolo(noise(d, 1200, 7000, 0.05, 0.1), 18, 0.7) * 0.5
    croak = lambda f, d: voice(d, [(0, f), (1, f * 0.85)], ["o", "a"], rough=3.0)
    return (seq(rustle(0.5), 0.05, croak(70, 0.4)),
            mixdown((rustle(0.9), 0.6), (seq(0.3, croak(65, 0.5)), 1.0)),
            seq(rustle(0.1), voice(0.25, [(0, 140), (1, 100)], ["a", "o"], rough=3.0)),
            seq(croak(90, 0.4), 0.02, croak(55, 0.9), (rustle(0.8), -0.6)))


def sporeling():
    puff = lambda: noise(0.25, 500, 4000, 0.02, 0.18, decay=0.08)
    peep = lambda f: voice(0.1, [(0, f), (1, f * 1.2)], ["i", "u"])
    return (seq(puff(), 0.1, peep(900)),
            seq(peep(820), 0.06, peep(980), 0.2, puff()),
            seq(peep(1200), (puff(), -0.05)),
            seq(voice(0.4, [(0, 1100), (1, 500)], ["i", "u"]), puff(), 0.02, puff()))


def ember_moth():
    flutter = lambda d: tremolo(noise(d, 150, 2500, 0.03, 0.08), 26, 0.9) * 0.6
    return (mixdown((flutter(0.8), 1.0), (crackle(0.8, 20), 0.3)),
            mixdown((flutter(1.0), 0.8), (tone(0.4, [(0, 3000), (1, 3600)], (1,), decay=0.15), 0.25)),
            seq(crackle(0.15, 80), flutter(0.15)),
            seq(mixdown((flutter(0.4), 1.0), (crackle(0.4, 60), 0.6)), noise(0.6, 500, 6000, 0.005, 0.5, decay=0.15) * 0.5))


def celestling():
    def chirp(f, d=0.09):
        return tone(d, [(0, f), (1, f * 1.25)], (1, 0.2, 0.05))
    bell = lambda f: tone(0.6, f, (1, 0.0, 0.25), decay=0.25)
    return (seq(chirp(2000), 0.05, chirp(2300), 0.05, chirp(2600), (bell(1760), 0.0)),
            room(seq(tone(0.5, [(0, 1500), (0.5, 2200), (1, 1800)], (1, 0.2), vib=(12, 0.02)), 0.05, chirp(2400)), 1.0, 0.35),
            seq(tone(0.15, [(0, 2600), (1, 1800)], (1, 0.3))),
            room(seq(tone(0.6, [(0, 2400), (1, 800)], (1, 0.3), vib=(14, 0.03)), (bell(880), -0.3)), 1.5, 0.45))


def reach_stalker():
    def blink():  # a sound pulled backwards: grows and snaps off
        x = noise(0.5, 300, 6000, 0.4, 0.005) * np.linspace(0, 1, int(0.5 * SR)) ** 2
        y = tone(0.5, [(0, 200), (1, 1600)], (1, 0.4), vib=(30, 0.05)) * np.linspace(0, 1, int(0.5 * SR)) ** 3
        return mixdown((x, 0.6), (y, 0.5))
    hum = lambda d, f: tremolo(tone(d, f, (1, 0.5, 0.3), vib=(0.7, 0.01)), 6, 0.4)
    return (room(hum(1.2, 110), 1.0, 0.35), room(blink(), 1.0, 0.4),
            seq(tone(0.25, [(0, 900), (1, 300)], (1, 0.5), vib=(40, 0.08))),
            room(seq(blink(), (tone(0.9, [(0, 400), (1, 60)], (1, 0.5, 0.3), vib=(25, 0.06)), 0.0)), 1.5, 0.45))


def sky_drifter():
    def call(f, d):
        return room(voice(d, [(0, f), (0.4, f * 1.5), (1, f * 1.1)], ["u", "o", "u"], vib=(3, 0.015)), 2.0, 0.55)
    wind = noise(1.2, 200, 2000, 0.4, 0.5) * 0.3
    return (call(220, 1.6), mixdown((call(260, 1.3), 1.0), (wind, 0.3)),
            room(voice(0.4, [(0, 380), (1, 300)], ["o", "a"]), 1.0, 0.4),
            room(voice(1.6, [(0, 300), (1, 120)], ["o", "u", "u"], vib=(4, 0.02)), 2.5, 0.6))


def sky_guardian():
    def horn_call(f, d):
        x = voice(d, [(0, f * 0.9), (0.15, f), (1, f * 0.97)], ["o", "a", "o"], vib=(4.5, 0.01), bright=2.0)
        y = voice(d, [(0, f * 1.5), (1, f * 1.48)], ["a", "o"], vib=(5, 0.012))
        return room(mixdown((x, 1.0), (y, 0.45)), 2.5, 0.5)
    return (horn_call(110, 1.8), seq(thump(45, 0.6), (horn_call(98, 1.6), -0.3)),
            room(voice(0.5, [(0, 160), (1, 120)], ["a", "o"], rough=1.0, bright=2.0), 1.5, 0.4),
            room(seq(horn_call(130, 1.0), (voice(2.0, [(0, 120), (1, 40)], ["a", "o", "u"], rough=1.0), -0.4),
                     (noise(1.5, 500, 8000, 0.3, 1.0) * 0.3, -1.6)), 3.0, 0.55))


def crimson_man():
    def speak(f, d, v):
        x = voice(d, [(0, f), (0.5, f * 0.92), (1, f * 0.85)], v, rough=1.5, breath=0.2)
        oct_down = voice(d, [(0, f / 2), (1, f * 0.42)], v, rough=1.5)
        return room(distort(mixdown((x, 1.0), (oct_down, 0.8)), 2.2), 1.8, 0.45)
    swell = noise(1.0, 80, 1200, 0.95, 0.01) * np.linspace(0, 1, int(1.0 * SR)) ** 3
    return (speak(90, 1.4, ["m", "a", "o", "m"]), seq(swell, speak(80, 1.0, ["a", "u"])),
            room(distort(voice(0.35, [(0, 160), (1, 110)], ["a", "e"], rough=2.0), 3.0), 1.0, 0.35),
            room(seq(speak(120, 0.6, ["a", "e"]), (distort(voice(2.2, [(0, 100), (1, 30)], ["a", "o", "u"], rough=2.5), 2.5), -0.2)), 3.0, 0.55))


MOBS = {
    "pig": pig, "cow": cow, "sheep": sheep, "chicken": chicken, "fox": fox,
    "ash_fox": lambda: fox(0.8, ash=True), "bat": bat, "villager": villager, "snowman": snowman,
    "ghost": ghost, "spider": spider, "pumpkin_slime": pumpkin_slime, "forgotten_man": forgotten_man,
    "tnt_zombie": lambda: zombie(fuse=True), "husking": husking, "tar_monster": tar_monster,
    "hogman": hogman, "cornerstone_golem": cornerstone_golem, "moss_lurker": moss_lurker,
    "sporeling": sporeling, "ember_moth": ember_moth, "celestling": celestling,
    "reach_stalker": reach_stalker, "sky_drifter": sky_drifter, "sky_guardian": sky_guardian,
    "crimson_man": crimson_man,
}
KINDS = ("say_1", "say_2", "hurt", "death")


def finish(x, peak=0.8, loud=-17.0):
    """Trims the tail, then levels every sound to about the same loudness
    (RMS of its loud part), with a soft limit so nothing clips."""
    x = np.asarray(x, float)
    x = x - np.mean(x)
    lvl = np.abs(x)
    end = len(x)
    thr = lvl.max() * 4e-3
    while end > SR // 20 and lvl[end - SR // 50:end].max() < thr:
        end -= SR // 100
    x = x[:end]
    f = min(len(x) // 4, int(0.02 * SR))
    x[-f:] *= np.linspace(1, 0, f)
    x = x / (np.abs(x).max() + 1e-9)
    win = int(0.05 * SR)
    frames = [np.sqrt(np.mean(x[i:i + win] ** 2)) for i in range(0, max(1, len(x) - win), win)]
    rms = np.percentile(frames, 80) if frames else 0.1
    x = x * (10 ** (loud / 20) / (rms + 1e-9))
    return np.tanh(x / peak) * peak  # soft limit


def write_wav(path, x, channels=1):
    with wave.open(path, "wb") as w:
        w.setnchannels(channels)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes((np.clip(x, -1, 1) * 32767).astype("<i2").tobytes())


def main():
    os.makedirs(OUT, exist_ok=True)
    demo, index, t = [], [], 0.0
    for name, make in MOBS.items():
        sounds = make()
        index.append((t, name))
        for kind, x in zip(KINDS, sounds):
            x = finish(x, 0.85 if kind != "hurt" else 0.9)
            wav = os.path.join(OUT, f"mob_{name}_{kind}.wav")
            write_wav(wav, x)
            subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", wav, "-c:a", "libvorbis", "-q:a", "4",
                            wav[:-4] + ".ogg"], check=True)
            os.remove(wav)
            demo += [x, np.zeros(int(0.45 * SR))]
            t += len(x) / SR + 0.45
        demo.append(np.zeros(int(0.9 * SR)))
        t += 0.9
    d = np.concatenate(demo)
    wav = DEMO[:-4] + ".wav"
    write_wav(wav, d)
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", wav, "-b:a", "160k",
                    "-metadata", "title=VoxelCraft – zvuky tvorů", mp3 := DEMO], check=True)
    os.remove(wav)
    for t, name in index:
        print(f"{int(t // 60)}:{int(t % 60):02d}  {name}")
    print(DEMO, f"{len(d) / SR:.0f} s")


if __name__ == "__main__":
    main()
