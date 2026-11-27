#!/usr/bin/env python3
"""Player sounds for VoxelCraft: damage, falls, freezing, fire, death, respawn,
hunger and eating. Short, dry and punchy, in the style of classic block games.

Made from physical models instead of plain tones, so they sound natural:
- the voice: glottal pulses (with jitter, shimmer and breath) through the
  resonances of the vocal tract (a Klatt-style formant cascade),
- impacts, crunches, ice and fire: noise and tiny impulses through resonators,
- the respawn: plucked strings (Karplus-Strong).
No recordings, nothing from other games. Needs numpy and scipy.

    python3 tools/player_sounds.py   ->  assets_src/sounds/player/*.ogg
                                         assets_src/sounds/player_sounds_demo.mp3
"""
import os
import subprocess
import wave

import numpy as np
from scipy.signal import butter, lfilter, sosfilt

SR = 44100
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "assets_src", "sounds", "player")
DEMO = os.path.join(ROOT, "assets_src", "sounds", "player_sounds_demo.mp3")
rng = np.random.default_rng(1207)


# ------------------------------------------------------------- basic tools ---

def n_of(d):
    return max(1, int(d * SR))


def interp(points, n):
    xs, ys = zip(*points)
    return np.interp(np.linspace(0, 1, n), xs, ys)


def bandpass(x, lo, hi, order=2):
    sos = butter(order, [max(20, lo), min(hi, SR / 2 - 100)], "bandpass", fs=SR, output="sos")
    return sosfilt(sos, x)


def lowpass(x, f, order=2):
    return sosfilt(butter(order, min(f, SR / 2 - 100), "lowpass", fs=SR, output="sos"), x)


def highpass(x, f, order=2):
    return sosfilt(butter(order, f, "highpass", fs=SR, output="sos"), x)


def env_ad(n, attack, decay_s, curve=1.0):
    t = np.arange(n) / SR
    e = np.exp(-t / decay_s) ** curve
    a = min(n, n_of(attack))
    e[:a] *= np.linspace(0, 1, a)
    return e


def fade(x, fin=0.003, fout=0.02):
    x = x.copy()
    a, b = min(len(x), n_of(fin)), min(len(x), n_of(fout))
    x[:a] *= np.linspace(0, 1, a)
    x[-b:] *= np.linspace(1, 0, b)
    return x


def norm(x):
    return x / (np.abs(x).max() + 1e-12)


def at(*items):
    """Places (time, signal, gain) items on one track."""
    n = max(n_of(t) + len(x) for t, x, *_ in items)
    out = np.zeros(n)
    for t, x, *g in items:
        i = n_of(t) if t > 0 else 0
        out[i:i + len(x)] += x * (g[0] if g else 1.0)
    return out


def resonator(x, f, bw):
    """Klatt two-pole resonator (unity gain at DC)."""
    T = 1 / SR
    C = -np.exp(-2 * np.pi * bw * T)
    B = 2 * np.exp(-np.pi * bw * T) * np.cos(2 * np.pi * f * T)
    A = 1 - B - C
    return lfilter([A], [1, -B, -C], x)


def ping(f, decay, bw=None):
    """An impulse through a resonator: a natural 'tick', 'tink' or 'pop'."""
    n = n_of(decay * 5)
    imp = np.zeros(n)
    imp[0] = 1
    bw = bw or 1 / (np.pi * decay)
    return norm(resonator(imp, f, bw) - resonator(imp, f, bw).mean())


# ------------------------------------------------------------------ voice ---

FORMANTS = {  # an adult voice (Hz): F1..F4 and bandwidths
    "u": (320, 870, 2250, 3300), "o": (520, 880, 2420, 3300), "oo": (450, 900, 2400, 3300),
    "a": (720, 1100, 2450, 3400), "uh": (620, 1180, 2400, 3300), "e": (530, 1800, 2480, 3400),
    "eh": (580, 1700, 2450, 3400),
    "cl": (470, 1050, 2750, 3850), "cl2": (400, 950, 2650, 3800),  # the bright classic 'oof'
 "i": (300, 2250, 2950, 3500), "m": (260, 1050, 2300, 3300),
}
BANDWIDTHS = (80, 100, 140, 200)


def glottal(f0, n, jitter=0.012, shimmer=0.12, open_q=0.6):
    """Glottal flow derivative pulses (Rosenberg) with natural irregularity."""
    f = f0 * (1 + jitter * rng.standard_normal(n).cumsum() / np.sqrt(np.arange(1, n + 1)))  # slow wander
    f = f * (1 + jitter * 0.6 * rng.standard_normal(n))
    phase = np.cumsum(f / SR)
    ph = phase % 1.0
    period_id = np.floor(phase).astype(int)
    amp = 1 + shimmer * rng.standard_normal(period_id.max() + 2)
    tp, tn = open_q * 0.66, open_q * 0.34
    flow = np.where(ph < tp, 0.5 * (1 - np.cos(np.pi * ph / tp)),
                    np.where(ph < tp + tn, np.cos(np.pi / 2 * (ph - tp) / tn), 0.0))
    return np.diff(flow * amp[period_id], prepend=0.0)


def voice(dur, f0, vowels, breath=0.15, jitter=0.012, shimmer=0.12, h_start=0.03, fry=0.0, scale=1.0):
    """f0: Hz or [(t, Hz)]; vowels: list of vowel names it glides through.
    breath: aspiration noise; h_start: seconds of breathy onset; fry: creaky end."""
    n = n_of(dur)
    f0c = interp(f0, n) if isinstance(f0, (list, tuple)) else np.full(n, float(f0))
    if fry:
        k = int(n * (1 - fry))
        f0c[k:] *= np.linspace(1, 0.55, n - k)  # pitch sinks into vocal fry
        jitter_c = np.full(n, jitter)
        jitter_c[k:] = jitter * 6
        jitter = float(np.mean(jitter_c))
    src = glottal(f0c, n, jitter, shimmer)
    src = norm(src)
    asp = rng.standard_normal(n) * breath
    hs = n_of(h_start)
    if h_start:
        src[:hs] *= np.linspace(0, 1, hs) ** 2
        asp[:hs] *= 6 * np.linspace(1, 0.3, hs)
    exc = src + highpass(asp, 600) * np.abs(src).mean() * 8
    # time-varying formant cascade, 4 ms blocks
    tracks = np.array([FORMANTS[v] for v in vowels], float) * scale
    pos = np.linspace(0, len(vowels) - 1, n)
    out = np.empty(n)
    blk = n_of(0.004)
    zis = [np.zeros(2) for _ in range(4)]
    for i in range(0, n, blk):
        p = pos[i]
        lo, hi = int(np.floor(p)), min(int(np.floor(p)) + 1, len(vowels) - 1)
        fr = p - lo
        F = tracks[lo] * (1 - fr) + tracks[hi] * fr
        y = exc[i:i + blk]
        for j in range(4):
            T = 1 / SR
            C = -np.exp(-2 * np.pi * BANDWIDTHS[j] * T)
            B = 2 * np.exp(-np.pi * BANDWIDTHS[j] * T) * np.cos(2 * np.pi * F[j] * T)
            A = 1 - B - C
            y, zis[j] = lfilter([A], [1, -B, -C], y, zi=zis[j])
        out[i:i + blk] = y
    out = np.diff(out, prepend=0.0)  # radiation from the lips
    e = np.ones(n)
    e[-n_of(0.04):] = np.linspace(1, 0, n_of(0.04)) ** 2
    return norm(out * e)


def oof(f=175, d=0.3, v=("cl", "cl2"), fry=0.0, breath=0.08):
    """The classic short 'oof': bright, swelling to its loudest in the middle."""
    x = voice(d, [(0, f * 0.95), (0.45, f * 1.06), (1, f * 0.86)], list(v), breath=breath, fry=fry, h_start=0.01,
              jitter=0.006, shimmer=0.06)
    x = norm(x + 2.2 * norm(bandpass(x, 2200, 4200)) * np.abs(x).max() * 0.6)  # the bright upper band
    n = len(x)
    return x * interp([(0, 0.15), (0.2, 0.4), (0.5, 1.0), (0.7, 0.8), (1, 0.0)], n)


def blip(lo=300, hi=3000, d=0.006):
    n = n_of(d * 5)
    return norm(bandpass(rng.standard_normal(n), lo, hi)) * env_ad(n, 0.0003, d)


def flesh_hit(k=0):
    """A hit on the body: one sharp wet slap, then a few tiny squelches."""
    slap = at((0, blip(150, 7000, 0.006), 1.0), (0, thud(0.25, 110), 0.5))
    items = [(0, slap, 1.0)]
    for _ in range(4 + k):
        items.append((rng.uniform(0.03, 0.2), blip(400, rng.uniform(2000, 5000), rng.uniform(0.002, 0.005)),
                      rng.uniform(0.05, 0.2)))
    return at(*items)


# ---------------------------------------------------------------- impacts ---

def thud(weight=1.0, f=75):
    """A body hitting the ground: low knock + muffled noise + cloth."""
    n = n_of(0.35 * weight)
    t = np.arange(n) / SR
    knock = np.sin(2 * np.pi * np.cumsum(f + 60 * np.exp(-t / 0.02)) / SR) * env_ad(n, 0.001, 0.07 * weight)
    body = lowpass(rng.standard_normal(n), 400) * env_ad(n, 0.001, 0.05 * weight)
    cloth = bandpass(rng.standard_normal(n_of(0.12)), 1500, 6000) * env_ad(n_of(0.12), 0.002, 0.03)
    return norm(at((0, norm(knock), 1.0), (0, norm(body), 0.7), (0.003, norm(cloth), 0.12)))


def punch():
    """A blow to the body: fast muffled slap."""
    n = n_of(0.12)
    slap = bandpass(rng.standard_normal(n), 300, 3500) * env_ad(n, 0.0005, 0.018)
    return norm(at((0, norm(slap), 1.0), (0, thud(0.4, 95), 0.8)))


def grains(dur, rate, lo, hi, decay=(0.0008, 0.003), env=None):
    """Many tiny resonant ticks: crunches, crackles, gravel."""
    n = n_of(dur)
    out = np.zeros(n)
    for _ in range(int(dur * rate)):
        g = ping(rng.uniform(lo, hi), rng.uniform(*decay)) * rng.uniform(0.2, 1.0) ** 2
        i = rng.integers(0, max(1, n - len(g)))
        out[i:i + len(g)] += g[: n - i]
    if env is not None:
        out *= env
    return out


# ------------------------------------------------------------------ fire ---

def fire(dur, intensity=1.0):
    n = n_of(dur)
    roar = lowpass(rng.standard_normal(n), 900) * (0.6 + 0.4 * lowpass(rng.standard_normal(n), 6) * 3)
    hiss = bandpass(rng.standard_normal(n), 2500, 9000) * 0.35
    pops = grains(dur, 35 * intensity, 600, 4000, (0.001, 0.006))
    e = np.minimum(1, np.arange(n) / n_of(0.03)) * np.linspace(1, 0.2, n)
    return norm(at((0, norm(roar) * e, 0.6), (0, norm(hiss) * e, 0.35), (0, norm(pops) * e, 0.8)))


def whoosh(dur, f_from, f_to, q=3.0):
    """Air swept through a moving band (a flying fireball, a gust)."""
    n = n_of(dur)
    x = rng.standard_normal(n)
    out = np.empty(n)
    fc = np.geomspace(f_from, f_to, n)
    blk = n_of(0.005)
    zi = np.zeros(2)
    for i in range(0, n, blk):
        f = fc[i]
        y, zi = lfilter(*_rbj_bp(f, q), x[i:i + blk], zi=zi)
        out[i:i + blk] = y
    return norm(out) * np.sin(np.linspace(0, np.pi, n)) ** 1.5


def _rbj_bp(f, q):
    w = 2 * np.pi * f / SR
    al = np.sin(w) / (2 * q)
    b = np.array([al, 0, -al])
    a = np.array([1 + al, -2 * np.cos(w), 1 - al])
    return b / a[0], a / a[0]


# ------------------------------------------------------------------- misc ---

def pluck(f, dur=1.2, bright=0.5):
    """Karplus-Strong plucked string."""
    n = n_of(dur)
    p = int(SR / f)
    buf = rng.uniform(-1, 1, p)
    buf = lowpass(buf, 2000 + 6000 * bright)
    out = np.empty(n)
    for i in range(n):
        out[i] = buf[i % p]
        buf[i % p] = 0.4985 * (buf[i % p] + buf[(i + 1) % p])
    return norm(out) * env_ad(n, 0.001, dur / 3)


def bubble(f0, dur=0.08):
    n = n_of(dur)
    t = np.arange(n) / SR
    f = f0 * (1 + 0.6 * t / dur)  # a bubble rises in pitch as it shrinks
    return np.sin(2 * np.pi * np.cumsum(f) / SR) * env_ad(n, 0.002, dur / 4)


# ------------------------------------------------------------------ sounds ---

def damage(k=0):
    """Like the newer games: just the hit on the body, no voice."""
    return flesh_hit(k)


def hurt_classic():
    """The old-school hurt: the 'oof' voice alone."""
    return oof()


def freeze_hit():
    n = n_of(0.5)
    crack = grains(0.05, 900, 2500, 9000, (0.0005, 0.002))           # the ice cracks
    tinkle = grains(0.5, 45, 3500, 8000, (0.01, 0.03), np.exp(-np.arange(n) / SR / 0.12))  # shards
    frost = bandpass(rng.standard_normal(n), 3000, 10000) * env_ad(n, 0.01, 0.1) * 0.5
    return at((0, norm(crack), 0.9), (0, punch(), 0.5), (0.01, norm(tinkle), 0.35), (0, norm(frost), 0.25),
              (0, flesh_hit(), 0.6), (0.02, oof(180), 0.7))


def fall_small():
    """Two quick broadband knocks: the feet, then the body."""
    return at((0, blip(80, 6000, 0.008), 1.0), (0, thud(0.2, 85), 0.5), (0.025, blip(80, 5000, 0.01), 0.9),
              (0.04, blip(100, 3000, 0.015), 0.25))


def fall_big():
    """A run of crunchy knocks within a third of a second."""
    items = [(0, thud(0.5, 60), 0.7)]
    for k, t in enumerate((0.0, 0.03, 0.05, 0.1, 0.16, 0.24)):
        crunch = grains(0.03, 1500, 500, 6000, (0.0004, 0.002), env_ad(n_of(0.03), 0.001, 0.008))
        items.append((t + rng.uniform(0, 0.01), norm(crunch), rng.uniform(0.5, 1.0) * (1 - 0.08 * k)))
    return at(*items)


def death():
    ugh = voice(0.55, [(0, 170), (0.15, 182), (1, 95)], ["uh", "a", "o", "u"], breath=0.25, fry=0.4, h_start=0.03)
    sigh = voice(0.5, [(0, 105), (1, 80)], ["uh", "m"], breath=0.6, fry=0.5) * np.linspace(1, 0, n_of(0.5))
    return at((0, flesh_hit(1), 0.7), (0.01, oof(170, 0.34), 0.9), (0.38, thud(1.4, 60), 0.8), (0.48, sigh, 0.2))


def burn(k=0):
    return at((0, fire(0.5, 1.2), 0.7), (0, flesh_hit(k), 0.6), (0.01, oof((175, 168)[k]), 0.7))


def husking_flame():
    fly = whoosh(0.32, 400, 2200, 2.5)
    n = n_of(0.5)
    blast = lowpass(rng.standard_normal(n), 700) * env_ad(n, 0.002, 0.09)
    return at((0, fly, 0.5), (0.3, norm(blast), 1.0), (0.3, thud(0.8, 60), 0.6), (0.3, fire(0.8, 1.5), 0.6),
              (0.3, flesh_hit(2), 0.5), (0.33, oof(172), 0.7))


def death_fire():
    ugh = voice(0.6, [(0, 185), (0.15, 200), (1, 100)], ["eh", "a", "o", "u"], breath=0.3, fry=0.4)
    return at((0, fire(1.6, 1.4), 0.6), (0, whoosh(0.5, 300, 1200, 2.0), 0.4), (0.02, ugh, 0.9),
              (0.55, thud(1.3, 60), 0.8))


def respawn():
    gust = whoosh(0.7, 300, 3000, 1.5)
    chord = at(*[(0.35 + 0.07 * k, pluck(f, 1.6, 0.4), 0.5) for k, f in enumerate((261.63, 329.63, 392.0, 523.25))])
    return at((0, gust, 0.35), (0, chord, 1.0))


def hunger():
    """A stomach growl: low resonant gurgling noise."""
    n = n_of(1.2)
    x = rng.standard_normal(n)
    growl = np.zeros(n)
    for f, g in ((180, 1.0), (320, 0.6), (520, 0.3)):
        mod = 1 + 0.15 * np.sin(2 * np.pi * rng.uniform(1.5, 3) * np.arange(n) / SR)
        growl += g * norm(resonator(x, f, 40)) * mod
    am = np.clip(lowpass(rng.standard_normal(n), 8) * 4 + 0.6, 0, None)
    growl = norm(growl * am) * np.sin(np.linspace(0, np.pi, n)) ** 0.7
    gurgles = at(*[(t, bubble(f, 0.07), 0.4) for t, f in ((0.25, 210), (0.5, 160), (0.62, 240), (0.9, 190))])
    return at((0, growl, 1.0), (0, gurgles, 1.0))


def chomp():
    """One bite: a crunchy burst of tiny cracks with a soft mouth knock."""
    d = rng.uniform(0.07, 0.1)
    n = n_of(d)
    e = env_ad(n, 0.003, d / 3)
    crunch = grains(d, 1400, 900, 5500, (0.0004, 0.002), e)
    knock = thud(0.25, 130)
    return at((0, norm(crunch), 1.0), (0, knock, 0.25))


def eat(k=0):
    bites = 3 + (k == 1)
    t, items = 0.0, []
    for _ in range(bites):
        items.append((t, chomp(), rng.uniform(0.75, 1.0)))
        t += rng.uniform(0.15, 0.2)
    return at(*items)


def eat_finish():
    gulp = at((0, bubble(140, 0.1), 1.0), (0, lowpass(rng.standard_normal(n_of(0.08)), 600) * env_ad(n_of(0.08), 0.003, 0.02), 0.5))
    burp = voice(0.38, [(0, 95), (0.3, 88), (1, 70)], ["o", "oo", "uh"], breath=0.1, jitter=0.05, shimmer=0.35,
                 h_start=0.0, fry=0.6)
    return at((0, gulp, 0.6), (0.15, burp, 0.9))


SOUNDS = {
    "player_damage_1": lambda: damage(0),
    "player_damage_2": lambda: damage(1),
    "player_damage_3": lambda: damage(2),
    "player_hurt_classic": hurt_classic,
    "player_freeze_hit": freeze_hit,
    "player_fall": fall_small,
    "player_fall_big": fall_big,
    "player_death": death,
    "player_burn_1": lambda: burn(0),
    "player_burn_2": lambda: burn(1),
    "player_husking_flame": husking_flame,
    "player_death_fire": death_fire,
    "player_respawn": respawn,
    "player_hunger": hunger,
    "player_eat_1": lambda: eat(0),
    "player_eat_2": lambda: eat(1),
    "player_eat_3": lambda: eat(2),
    "player_eat_finish": eat_finish,
}


def finish(x, loud=-15.0, peak=0.92):
    x = highpass(np.asarray(x, float), 45)
    lvl = np.abs(x)
    end = len(x)
    while end > SR // 20 and lvl[end - SR // 100:end].max() < lvl.max() * 3e-3:
        end -= SR // 200
    x = fade(x[:end], 0.001, 0.015)
    x = norm(x)
    win = n_of(0.03)
    frames = [np.sqrt(np.mean(x[i:i + win] ** 2)) for i in range(0, max(1, len(x) - win), win)]
    x = x * 10 ** (loud / 20) / (np.percentile(frames, 85) + 1e-9)
    return np.tanh(x / peak) * peak


def write_wav(path, x):
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes((np.clip(x, -1, 1) * 32767).astype("<i2").tobytes())


def main():
    os.makedirs(OUT, exist_ok=True)
    demo, t = [], 0.0
    for name, make in SOUNDS.items():
        x = finish(make())
        wav = os.path.join(OUT, name + ".wav")
        write_wav(wav, x)
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", wav, "-c:a", "libvorbis", "-q:a", "5",
                        wav[:-4] + ".ogg"], check=True)
        os.remove(wav)
        print(f"{int(t // 60)}:{int(t % 60):02d}  {name}  ({len(x) / SR:.2f} s)")
        demo += [x, np.zeros(n_of(0.9))]
        t += len(x) / SR + 0.9
    wav = DEMO[:-4] + ".wav"
    write_wav(wav, np.concatenate(demo))
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", wav, "-b:a", "160k",
                    "-metadata", "title=VoxelCraft – zvuky hráče", DEMO], check=True)
    os.remove(wav)
    print(DEMO, f"{t:.0f} s")


if __name__ == "__main__":
    main()
