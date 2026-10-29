#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Alexis Arakis (Bungle Society)
"""Modular Compagnon: the site's pictures from tests/m1_shots.c's scenes (the firmware's own screen code).
  tools/make_compagnon_img.py SHOTS_DIR OUT_DIR
GIFs at 12.5 frames a second (every other 40 ms picture), 240 x 240 as the screen is (the pages scale them up
pixelated); PNG stills where a still says it."""
import sys
from pathlib import Path
from PIL import Image

GIFS = {"ouverture": None, "toilette": None, "joue": None, "nuit": (0, 260), "neige": None, "folie": (60, 216)}
STILLS = {"reglage": -1, "magneto": -6, "globe": 20}


def frames(d):
    return sorted(Path(d).glob("f*.ppm"))


def main(src, out):
    out = Path(out); out.mkdir(parents=True, exist_ok=True)
    for name, cut in GIFS.items():
        fs = frames(Path(src) / name)
        if cut: fs = fs[cut[0]:cut[1]]
        ims = [Image.open(f).convert("RGB") for f in fs[::2]]
        pal = [im.quantize(colors=64, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE) for im in ims]
        pal[0].save(out / f"{name}.gif", save_all=True, append_images=pal[1:], duration=80, loop=0, optimize=True)
        print(f"img: {name}.gif {len(pal)} frames {(out / f'{name}.gif').stat().st_size // 1024} KB")
    for name, i in STILLS.items():
        fs = frames(Path(src) / name)
        Image.open(fs[i]).convert("RGB").save(out / f"{name}.png", optimize=True)
        print(f"img: {name}.png")


if __name__ == "__main__":
    main(*sys.argv[1:3])
