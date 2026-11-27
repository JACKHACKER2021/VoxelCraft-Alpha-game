#!/usr/bin/env python3
"""2.8.0: the world shaders as files of the VC language (assets/vc/shaders), taken from the code."""
import os, re, sys
root = sys.argv[1] if len(sys.argv) > 1 else '.'
eng = os.path.join(root, 'app/src/main/cpp/engine')
out = os.path.join(root, 'app/src/main/assets/vc/shaders')
os.makedirs(out, exist_ok=True)
def raw(path, name):
    s = open(path).read()
    m = re.search(r'const char\* ' + name + r' = R"\((.*?)\)";', s, re.S)
    return m.group(1)
for f, (src, name) in {'world.vsh': ('Renderer.cpp', 'kWorldVs'), 'classic_world.fsh': ('Shaders.cpp', 'kClassicWorldFs'),
                       'vibrant_world.fsh': ('Shaders.cpp', 'kVibrantWorldFs')}.items():
    open(os.path.join(out, f), 'w').write(raw(os.path.join(eng, src), name))
    print(f)
