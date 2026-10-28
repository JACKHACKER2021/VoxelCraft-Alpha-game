#!/usr/bin/env python3
"""Renders the story trailer on every core: the frames are split into parts, each part is
encoded by its own process, then the parts are joined and the mixed audio is added."""
import os
import subprocess
import sys
from multiprocessing import Process

ROOT, MUSIC = sys.argv[1], sys.argv[2]
PARTS = int(sys.argv[3]) if len(sys.argv) > 3 else os.cpu_count()
OUTDIR = os.path.join(ROOT, "build/trailer10")


def part(i, n):
    sys.argv = [sys.argv[0], ROOT, MUSIC]
    import trailer10 as t
    k = t.k
    frames = int(k.total * k.FPS)
    a, b = frames * i // n, frames * (i + 1) // n
    out = os.path.join(OUTDIR, "part_%02d.mp4" % i)
    enc = subprocess.Popen(["ffmpeg", "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", "%dx%d" % (k.W, k.H),
                            "-r", str(k.FPS), "-i", "-", "-c:v", "libx264", "-crf", "20", "-preset", "veryfast", "-pix_fmt", "yuv420p", out],
                           stdin=subprocess.PIPE)
    for fi in range(a, b):
        enc.stdin.write(k.frame_at(fi / k.FPS).tobytes())
        if (fi - a) % (k.FPS * 30) == 0:
            print("part", i, "frame", fi - a, "/", b - a, flush=True)
    enc.stdin.close()
    enc.wait()
    print("part", i, "done", flush=True)


if __name__ == "__main__":
    # The audio first (music, narration), for the current edit.
    sys.argv = [sys.argv[0], ROOT, MUSIC]
    import trailer10 as t0
    t0.k.audio_track(os.path.join(OUTDIR, "mix.wav"))
    print("audio mixed", flush=True)
    ps = [Process(target=part, args=(i, PARTS)) for i in range(PARTS)]
    for p in ps:
        p.start()
    for p in ps:
        p.join()
    lst = os.path.join(OUTDIR, "parts.txt")
    with open(lst, "w") as f:
        for i in range(PARTS):
            f.write("file 'part_%02d.mp4'\n" % i)
    out = os.path.join(ROOT, "dist/VoxelCraft-story-en.mp4")
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-f", "concat", "-safe", "0", "-i", lst, "-i", os.path.join(OUTDIR, "mix.wav"),
                    "-map", "0:v", "-map", "1:a", "-c:v", "copy", "-c:a", "aac", "-b:a", "192k", "-shortest", "-movflags", "+faststart", out],
                   check=True)
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", out, "-vf", "scale=1280:720", "-c:v", "libx264", "-crf", "24", "-preset", "veryfast",
                    "-c:a", "copy", "-movflags", "+faststart", os.path.join(ROOT, "dist/VoxelCraft-story-en-720p.mp4")], check=True)
    print("wrote", out, flush=True)
