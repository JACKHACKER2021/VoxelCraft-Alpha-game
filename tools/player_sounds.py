#!/usr/bin/env python3
"""Player sounds for VoxelCraft: damage of every kind, death, respawn, hunger and
eating, synthesised with the building blocks of tools/mob_sounds.py (numpy only,
no recordings).

    python3 tools/player_sounds.py   ->  assets_src/sounds/player/*.ogg
                                         assets_src/sounds/player_sounds_demo.mp3
"""
import os
import subprocess
import sys

import numpy as np

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mob_sounds as ms  # noqa: E402
from mob_sounds import (SR, T, voice, tone, noise, bubble, click, thump, seq, mixdown, tremolo,  # noqa: E402
                        room, crackle, finish, write_wav)

rng = np.random.default_rng(1207)
ms.rng = rng
OUT = os.path.join(ms.ROOT, "assets_src", "sounds", "player")
DEMO = os.path.join(ms.ROOT, "assets_src", "sounds", "player_sounds_demo.mp3")


def at(*items):
    """Places (time in seconds, signal) items on one track."""
    n = max(int(t * SR) + len(x) for t, x in items)
    out = np.zeros(n)
    for t, x in items:
        i = int(t * SR)
        out[i:i + len(x)] += x
    return out


def grunt(f=140, d=0.22, v=("a", "u"), rough=1.0):
    """The player's short pained 'uh'."""
    return voice(d, [(0, f * 1.15), (0.3, f), (1, f * 0.8)], list(v), rough=rough, breath=0.25)


def body_hit(f0=70, weight=1.0):
    return mixdown((thump(f0, 0.35 * weight), 1.0), (noise(0.12, 150, 1800, 0.001, 0.1, decay=0.03), 0.5 * weight))


def sizzle(d, rate=45):
    hiss = tremolo(noise(d, 2500, 10000, 0.01, d / 2), 13, 0.5)
    return mixdown((hiss, 0.6), (crackle(d, rate), 0.7))


def whoosh(d, lo=200, hi=3000, rise=True):
    x = noise(d, lo, hi, 0.005, 0.01)
    env = np.linspace(0, 1, len(x)) ** 2 if rise else np.linspace(1, 0, len(x)) ** 2
    return x * env


# ------------------------------------------------------------------ sounds ---

def damage(k=0):
    """An ordinary hit (a mob, a sword, an arrow): a punchy impact and a short 'uh'."""
    f = (140, 128, 152)[k]
    v = (("a", "u"), ("o", "u"), ("e", "a"))[k]
    punch = mixdown((thump(85 + 10 * k, 0.25), 1.0), (noise(0.07, 400, 4000, 0.0008, 0.05, decay=0.015), 0.8))
    return mixdown((punch, 0.9), (seq(0.015, grunt(f, 0.19, v, rough=1.2)), 0.8))


def freeze_hit():
    crack = noise(0.06, 3000, 12000, 0.0005, 0.04, decay=0.012)
    tinkle = at(*[(k * 0.04 + rng.uniform(0, 0.015), tone(0.35, f, (1, 0.2), decay=0.1) * rng.uniform(0.3, 0.7))
                  for k, f in enumerate((3520, 4186, 2960, 4700, 3950))])
    breath = noise(0.45, 600, 5000, 0.02, 0.3) * 0.35  # a cold 'hhh'
    return room(mixdown((crack, 1.0), (body_hit(80, 0.7), 0.7), (seq(0.02, tinkle), 0.6),
                        (seq(0.05, grunt(150, 0.18, ("e", "u"))), 0.6), (seq(0.12, breath), 0.4)), 0.7, 0.3)


def fall_small():
    return mixdown((body_hit(75, 0.8), 1.0), (noise(0.18, 1500, 6000, 0.002, 0.12, decay=0.05), 0.25),
                   (seq(0.02, grunt(135, 0.16)), 0.55))


def fall_big():
    crack = seq(click(0.02, 1500, 9000), 0.015, click(0.015, 1200, 7000))  # a nasty crack
    gravel = seq(0.03, *[p for _ in range(9) for p in (click(0.012, 800, 5000) * rng.uniform(0.2, 0.6), rng.uniform(0.01, 0.04))])
    return mixdown((body_hit(50, 1.6), 1.0), (thump(38, 0.8), 0.7), (crack, 0.7), (gravel, 0.4),
                   (seq(0.04, grunt(150, 0.4, ("a", "o", "u"), rough=1.6)), 0.8))


def death():
    cry = voice(0.75, [(0, 190), (0.25, 210), (1, 95)], ["a", "o", "u"], rough=1.2, breath=0.2)
    fall = seq(0.62, body_hit(60, 1.2))
    gone = seq(0.8, room(tone(1.2, [(0, 220), (1, 110)], (1, 0.3, 0.1)) * np.linspace(1, 0, int(1.2 * SR)), 1.5, 0.5))
    return mixdown((cry, 1.0), (fall, 0.8), (gone, 0.35))


def burn(k=0):
    f = (150, 160)[k]
    return mixdown((sizzle(0.55), 1.0), (seq(0.03, grunt(f, 0.2, ("e", "a"), rough=1.4)), 0.7))


def husking_flame():
    """The Husking's fireball hits: whoosh, a soft blast, then the burn."""
    fly = whoosh(0.35, 150, 2500)
    blast = mixdown((thump(55, 0.6), 1.0), (noise(0.7, 80, 4000, 0.002, 0.5, decay=0.12), 0.9))
    return room(seq(fly, (blast, -0.02), (sizzle(0.8, 60), -0.55), (grunt(165, 0.25, ("a", "e"), rough=1.6), -0.9)), 0.8, 0.25)


def death_fire():
    cry = voice(0.8, [(0, 210), (0.2, 240), (1, 90)], ["a", "e", "o", "u"], rough=1.6, breath=0.35)
    flames = tremolo(noise(1.8, 100, 3500, 0.3, 1.0), 7, 0.4)
    return mixdown((cry, 1.0), (flames, 0.5), (sizzle(1.8, 35), 0.5), (seq(0.7, body_hit(55, 1.2)), 0.7))


def respawn():
    notes = (523.25, 659.25, 783.99, 1046.5, 1318.5)  # C E G C E rising
    arp = at(*[(k * 0.09, tone(1.2, f, (1, 0.25, 0.08), decay=0.5)) for k, f in enumerate(notes)])
    up = whoosh(0.6, 400, 6000, rise=True)
    glow = tone(1.6, 261.63, (1, 0.5, 0.25)) * np.sin(np.linspace(0, np.pi, int(1.6 * SR))) * 0.6
    return room(mixdown((up, 0.4), (seq(0.45, arp), 1.0), (seq(0.4, glow), 0.5)), 1.6, 0.45)


def hunger():
    """A stomach growl: a low wobbling rumble with gurgles."""
    rumble = voice(1.3, [(0, 55), (0.3, 75), (0.6, 50), (1, 65)], ["u", "o", "u"], rough=3.0)
    gurgles = seq(0.2, *[p for f in (180, 140, 220, 160) for p in (bubble(f, 0.12), rng.uniform(0.08, 0.2))])
    return mixdown((tremolo(rumble, 9, 0.6), 1.0), (gurgles, 0.5))


def eat(k=0):
    """Chewing: a few crunchy bites, the last one followed by a gulp."""
    out = []
    for i in range(3 + k % 2):
        bite = noise(rng.uniform(0.06, 0.09), 700 + 300 * k, 6000, 0.001, 0.05, decay=0.03)
        crumbs = at(*[(j * 0.014, click(0.008, 1500, 7000) * rng.uniform(0.3, 1)) for j in range(4)])
        bite = mixdown((bite, 1.0), (crumbs, 0.4))
        out += [bite, rng.uniform(0.11, 0.16)]
    return seq(*out)


def eat_finish():
    gulp = mixdown((bubble(110, 0.18), 1.0), (voice(0.18, [(0, 120), (1, 90)], ["m", "u"]), 0.4))
    ahh = voice(0.35, [(0, 170), (1, 140)], ["a", "a"], breath=0.5)  # a satisfied 'ahh'
    return seq(gulp, 0.12, ahh * 0.6)


SOUNDS = {
    "player_damage_1": lambda: damage(0),
    "player_damage_2": lambda: damage(1),
    "player_damage_3": lambda: damage(2),
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


def main():
    os.makedirs(OUT, exist_ok=True)
    demo, t = [], 0.0
    for name, make in SOUNDS.items():
        x = finish(make(), 0.9)
        wav = os.path.join(OUT, name + ".wav")
        write_wav(wav, x)
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", wav, "-c:a", "libvorbis", "-q:a", "4",
                        wav[:-4] + ".ogg"], check=True)
        os.remove(wav)
        print(f"{int(t // 60)}:{int(t % 60):02d}  {name}  ({len(x) / SR:.2f} s)")
        demo += [x, np.zeros(int(1.0 * SR))]
        t += len(x) / SR + 1.0
    wav = DEMO[:-4] + ".wav"
    write_wav(wav, np.concatenate(demo))
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", wav, "-b:a", "160k",
                    "-metadata", "title=VoxelCraft – zvuky hráče", DEMO], check=True)
    os.remove(wav)
    print(DEMO, f"{t:.0f} s")


if __name__ == "__main__":
    main()
