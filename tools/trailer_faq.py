#!/usr/bin/env python3
"""VoxelCraft FAQ trailer: where the project is going (about 3 minutes, 1920x1080, English narration).

Pictures: the FAQ pictures (docs/img/faq) and shots of the game; narration: Piper (en_US-joe-medium,
the voice of the earlier trailers); music: the menu theme, quietly under the voice.
Usage: trailer_faq.py <work dir> <shots dir> <voice.onnx> <out.mp4>
"""
import os
import subprocess
import sys
import wave

from PIL import Image, ImageDraw, ImageFont

WORK, SHOTS, VOICE, OUT = sys.argv[1:5]
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FAQ = os.path.join(ROOT, "docs/img/faq")
MUSIC = os.path.join(ROOT, "app/src/main/assets/music/menu_theme.mp3")
W, H = 1920, 1080
BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
BG, GOLD, WHITE, GREEN = (18, 34, 24), (255, 210, 77), (240, 240, 235), (110, 190, 70)

# (picture, caption, narration)
SCENES = [
    ("title", "VoxelCraft FAQ", "Hi! This is VoxelCraft, by Hoppy Hopsy twenty one and VoxelSoftworks. In this video we answer your questions, and show where the project is going."),
    ("shot:intro", "VoxelCraft 2.5.0", "Version two point five is out now. The underground update."),
    ("shot:metrolab", "The underground", "A whole subway network with four kinds of station, an automatic subway, a lost city at the end of the line, and rusted train wrecks all over the world."),
    ("faq:date", "When on Google Play?", "So, when does VoxelCraft come to Google Play? During twenty twenty seven. Maybe on the first of January, or the first of February, when VoxelCraft turns one year old."),
    ("faq:release", "Where are the APKs?", "Until then, every finished version can be downloaded from the release folder on GitHub."),
    ("faq:free", "Will it be free?", "Will the game be free? Yes. Downloading and playing are free, and there are no ads."),
    ("shot:bucks", "Voxel Bucks", "Coins are now called Voxel Bucks, with a little cube as their icon. They can also be bought, if you want to. Free spins and daily rewards stay free."),
    ("faq:saves", "Your worlds stay", "Your worlds, the fixed structure seeds and your inventory all come along to the release."),
    ("faq:phones", "Which phones?", "VoxelCraft runs on Android five point oh and newer, for as long as Google Play works."),
    ("faq:online", "Online multiplayer", "Online multiplayer is coming. For now, you can play together on the same Wi-Fi."),
    ("faq:platforms", "iPhone and PC", "An iPhone and a PC version are planned for the future."),
    ("faq:versions", "Version numbers", "Later, the version numbers will start again, from one point oh point oh point oh."),
    ("faq:dimensions", "Before the release", "Before the release, the Cornerstone and the Crimson Void get many fixes, their own ores, and new creatures."),
    ("faq:preview", "Normal and preview", "The normal version is for everyone. The preview, with the next update, is for the developer."),
    ("faq:bugs", "Found a bug?", "Found a bug? In the settings, tap Report a bug, and the game opens a GitHub issue that is already filled in."),
    ("faq:videos", "Videos and streams", "And yes, you can make videos and stream VoxelCraft."),
    ("shot:menu", "The look stays", "The green menus stay as they are, and the new Tiny Caps font is used everywhere except Classic."),
    ("shot:skins", "New characters", "Every character has a new pixel skin, and Captain Sea joins the crew."),
    ("card:next", "What comes next", "Next comes version two point six, with new creatures in the Cornerstone and a better Rozhlas voice. After it, two point seven and two point eight bring the game closer to the release."),
    ("title:end", "Thank you for playing", "Thank you for watching, and thank you for playing. Made with love by Hoppy Hopsy twenty one and VoxelSoftworks, built together with Claude."),
]


def font(s):
    return ImageFont.truetype(BOLD, s)


def frame(pic, caption):
    im = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(im)
    for y in range(0, H, 40):
        d.line([(0, y), (W, y)], fill=(22, 40, 29))
    for x in range(0, W, 40):
        d.line([(x, 0), (x, H)], fill=(22, 40, 29))
    kind, _, name = pic.partition(":")
    if kind in ("title", "card"):
        icon = os.path.join(ROOT, "docs/img/icon.png")
        if os.path.exists(icon):
            ic = Image.open(icon).convert("RGBA").resize((300, 300))
            im.paste(ic, (W // 2 - 150, 170), ic)
        d.text((W // 2, 560), caption, font=font(96), fill=GOLD, anchor="mm")
        sub = {"title": "Where the project is going", "end": "VoxelSoftworks", "next": "2.6.0  ·  2.7.0  ·  2.8.0  ·  release 2027"}
        d.text((W // 2, 680), sub.get(name or kind, ""), font=font(48), fill=WHITE, anchor="mm")
        return im
    src = os.path.join(FAQ, name + ".jpg") if kind == "faq" else os.path.join(SHOTS, name + ".png")
    p = Image.open(src).convert("RGB")
    s = min((W - 160) / p.width, (H - 260) / p.height)
    p = p.resize((int(p.width * s), int(p.height * s)), Image.LANCZOS if kind == "faq" else Image.NEAREST)
    x, y = (W - p.width) // 2, 60 + (H - 260 - p.height) // 2
    d.rectangle([x - 8, y - 8, x + p.width + 8, y + p.height + 8], fill=(10, 20, 14))
    im.paste(p, (x, y))
    d.rectangle([0, H - 150, W, H], fill=(10, 20, 14))
    d.text((W // 2, H - 75), caption, font=font(64), fill=GOLD, anchor="mm")
    return im


def main():
    os.makedirs(WORK, exist_ok=True)
    parts = []
    for i, (pic, cap, text) in enumerate(SCENES):
        vo = os.path.join(WORK, f"vo{i:02d}.wav")
        if not os.path.exists(vo):
            subprocess.run(["piper", "-m", VOICE, "-f", vo, "--length_scale", "1.05", "--sentence_silence", "0.35"],
                           input=text.encode(), check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        with wave.open(vo) as w:
            dur = w.getnframes() / w.getframerate() + 1.0
        img = os.path.join(WORK, f"img{i:02d}.png")
        frame(pic, cap).save(img)
        clip = os.path.join(WORK, f"clip{i:02d}.mp4")
        n = int(dur * 30)
        # A slow zoom over the picture; the voice starts half a second in.
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-loop", "1", "-i", img, "-i", vo,
                        "-filter_complex",
                        f"[0:v]scale=2112:1188,zoompan=z='1+0.04*on/{n}':x='iw/2-(iw/zoom/2)':y='ih/2-(ih/zoom/2)':d={n}:s={W}x{H}:fps=30,"
                        f"fade=t=in:d=0.4,fade=t=out:st={dur - 0.4:.2f}:d=0.4[v];[1:a]adelay=500|500,apad,atrim=0:{dur:.2f},aresample=44100[a]",
                        "-map", "[v]", "-map", "[a]", "-t", f"{dur:.2f}", "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "24",
                        "-preset", "veryfast", "-c:a", "aac", "-ac", "2", "-b:a", "160k", clip], check=True)
        parts.append(clip)
        print("scene", i, f"{dur:.1f}s", flush=True)
    lst = os.path.join(WORK, "list.txt")
    with open(lst, "w") as f:
        f.writelines(f"file '{p}'\n" for p in parts)
    joined = os.path.join(WORK, "joined.mp4")
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-f", "concat", "-safe", "0", "-i", lst, "-c", "copy", joined], check=True)
    # The menu theme under the voice, quiet, fading out at the end.
    total = float(subprocess.check_output(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", joined]))
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", joined, "-stream_loop", "-1", "-i", MUSIC, "-filter_complex",
                    f"[1:a]volume=0.16,afade=t=out:st={total - 4:.2f}:d=4[m];[0:a][m]amix=inputs=2:duration=first:normalize=0[a]",
                    "-map", "0:v", "-map", "[a]", "-c:v", "copy", "-c:a", "aac", "-b:a", "160k", "-t", f"{total:.2f}", OUT], check=True)
    print("wrote", OUT, f"{total:.0f}s")


main()
