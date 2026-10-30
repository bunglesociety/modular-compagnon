#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Alexis Arakis (Bungle Society)
"""Modular Compagnon: the controls picture (web/compagnon/img/commandes-fr.png, controls-en.png): the SMK-37 Elite seen
from above, drawn in the screen's style (old paper, sepia ink, the pixel letters), every control with its job.
The cat in the screen is a picture of the firmware's own screen (web/compagnon/img/joue.gif, its first frame).

  tools/make_controls_img.py"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
IMG = ROOT / "web" / "compagnon" / "img"
FONT = ROOT / "assets" / "fonts" / "ui" / "silkscreen-latin-400-normal.woff"

PAPER, GRAIN, INK, DIM = (222, 200, 160), (212, 188, 148), (60, 42, 28), (125, 104, 80)
BODY, BODY2, ORANGE, CREAM, KEYW, KEYB = (92, 86, 82), (78, 72, 68), (204, 98, 54), (240, 232, 214), (244, 238, 226), (204, 98, 54)
LBL = (240, 226, 196)

T = {
    "fr": dict(file="commandes-fr.png", title="MODULAR COMPAGNON  ·  LES COMMANDES",
               pitch="PITCH", pitch2="(OFF)", mod="MOD", mod2="FILTRE",
               knobs=["VOLUME", "CUTOFF", "RESO", "ATTAQUE", "DECAY", "SUSTAIN", "RELEASE", "DETUNE"],
               faders=["JOUR/NUIT", "FM / HIVER", "TEMPS", "ECHO"],
               small=["ONDE 1", "ONDE 2", "", "", "OCT-", "OCT+"],
               top=["ARP", "", "", "LA 440", "GLOBE", ""], bot=["", "", "VINTAGE", "", "", ""],
               keys="37 TOUCHES : LES NOTES",
               notes=["PADS : LES 16 SONS  ·  FX : LE SON VINTAGE (BANDE, CHALEUR)  ·  CHORD : UN LA 440 POUR S'ACCORDER",
                      "ARP : UN APPUI = ON / OFF  ·  MAINTENU : K2 LE MODE, K3 LA VITESSE, K4 LE TEMPO (SUIT LE DAW TOUT SEUL)",
                      "GLOBE : LES REGLAGES (K2 A K8) : CANAL MIDI, VELOCITE, ACCORDAGE, PADS, VEILLE, HUMEUR, COMPAGNON",
                      "COMPAGNONS : LE CHAT, LE RENARD, LE ROBOT, LE POISSON (TOUT L'ECRAN EST SON AQUARIUM)",
                      "ONDE 1 / 2 : LA TOUCHE S'ALLUME SUR LES SONS QUI ONT DES ONDES"]),
    "en": dict(file="controls-en.png", title="MODULAR COMPAGNON  ·  THE CONTROLS",
               pitch="PITCH", pitch2="(OFF)", mod="MOD", mod2="FILTER",
               knobs=["VOLUME", "CUTOFF", "RESO", "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "DETUNE"],
               faders=["DAY/NIGHT", "FM / WINTER", "TIME", "ECHO"],
               small=["WAVE 1", "WAVE 2", "", "", "OCT-", "OCT+"],
               top=["ARP", "", "", "A 440", "GLOBE", ""], bot=["", "", "VINTAGE", "", "", ""],
               keys="37 KEYS: THE NOTES",
               notes=["PADS: THE 16 SOUNDS  ·  FX: THE VINTAGE SOUND (TAPE, WARMTH)  ·  CHORD: AN A 440 TO TUNE TO",
                      "ARP: A TAP = ON / OFF  ·  HELD: K2 ITS MODE, K3 ITS SPEED, K4 THE TEMPO (FOLLOWS A DAW BY ITSELF)",
                      "GLOBE: THE SETTINGS (K2 TO K8): MIDI CHANNEL, VELOCITY, TUNING, PADS, SLEEP, MOOD, COMPANION",
                      "COMPANIONS: THE CAT, THE FOX, THE ROBOT, THE FISH (THE WHOLE SCREEN IS HIS AQUARIUM)",
                      "WAVE 1 / 2: THE BUTTON LIGHTS UP ON THE SOUNDS THAT HAVE WAVES"]),
}
PADS = ["MODULAR", "JUNO", "SUPERSAW", "PWM", "TAPE", "FM", "SOUFFLE", "CREPUSCULE",
        "FLUTE", "BRASS", "STRINGS", "CHOIR", "RAIN", "DEEP", "PULSE", "MEMORY"]


def font(px):
    return ImageFont.truetype(str(FONT), px)


def text(d, xy, s, f, fill, anchor="mm"):
    d.text(xy, s, font=f, fill=fill, anchor=anchor)


def fit(d, s, width, sizes):
    """the biggest letters that fit"""
    for px in sizes:
        f = font(px)
        if d.textlength(s, font=f) <= width:
            return f
    return f


def draw(lang):
    t = T[lang]
    W, H = 1600, 900
    S = 2                                                   # drawn at 2x, scaled down at the end (crisp letters)
    im = Image.new("RGB", (W * S, H * S), PAPER)
    d = ImageDraw.Draw(im)
    for y in range(0, H * S, 14):                           # the paper's grain
        for x in range((y // 14) % 2 * 7, W * S, 14):
            d.rectangle([x, y, x + 1, y + 1], fill=GRAIN)
    f8, f12, f16, f20, f28 = font(8 * S), font(12 * S), font(16 * S), font(20 * S), font(28 * S)
    k = lambda v: int(v * S)
    text(d, (k(W / 2), k(46)), t["title"], f28, INK)
    # the body
    X0, Y0, X1, Y1 = 90, 150, 1530, 700
    d.rounded_rectangle([k(X0 + 8), k(Y0 + 8), k(X1 + 8), k(Y1 + 8)], k(26), fill=(190, 170, 134))
    d.rounded_rectangle([k(X0), k(Y0), k(X1), k(Y1)], k(26), fill=BODY)
    # wheels
    for i, (x, a, b, col) in enumerate(((140, t["pitch"], t["pitch2"], DIM), (232, t["mod"], t["mod2"], INK))):
        d.rounded_rectangle([k(x - 30), k(240), k(x + 30), k(385)], k(14), fill=ORANGE if i else (150, 110, 92))
        text(d, (k(x), k(205)), a, f16, LBL)
        text(d, (k(x), k(130)), b, f16, col)
        d.line([k(x), k(142), k(x), k(196)], fill=col, width=k(2))
    # the screen: the cat
    sx, sy, sw = 290, 190, 150
    d.rounded_rectangle([k(sx - 10), k(sy - 10), k(sx + sw + 10), k(sy + sw + 10)], k(12), fill=(22, 20, 18))
    cat = Image.open(IMG / "joue.gif").convert("RGB").resize((k(sw), k(sw)), Image.NEAREST)
    im.paste(cat, (k(sx), k(sy)))
    # the six buttons under the screen
    for i, s in enumerate(t["small"]):
        cx, cy = 318 + (i % 2) * 96, 395 + (i // 2) * 52
        d.rounded_rectangle([k(cx - 44), k(cy - 20), k(cx + 44), k(cy + 20)], k(6), fill=ORANGE if s else (130, 96, 80))
        if s:
            text(d, (k(cx), k(cy)), s, f12 if len(s) > 4 else f16, CREAM)
    # knobs
    for i, s in enumerate(t["knobs"]):
        cx = 530 + i * 82
        d.ellipse([k(cx - 26), k(215), k(cx + 26), k(267)], fill=ORANGE)
        d.line([k(cx), k(218), k(cx), k(238)], fill=CREAM, width=k(4))
        text(d, (k(cx), k(195)), f"K{i + 1}", f12, LBL)
        text(d, (k(cx), k(118 if i % 2 else 140)), s, f16, INK)
        d.line([k(cx), k(150 if i % 2 else 152), k(cx), k(184)], fill=DIM, width=k(2))
    # pads
    for i, s in enumerate(PADS):
        cx, cy = 530 + (i % 8) * 82, 315 + (i // 8) * 82
        d.rounded_rectangle([k(cx - 37), k(cy - 33), k(cx + 37), k(cy + 33)], k(6), fill=CREAM)
        text(d, (k(cx), k(cy)), s, fit(d, s, k(66), (k(12), k(10), k(8))), INK)
    # faders
    for i, s in enumerate(t["faders"]):
        cx = 1232 + i * 80
        d.rectangle([k(cx - 3), k(215), k(cx + 3), k(380)], fill=(30, 28, 26))
        d.rounded_rectangle([k(cx - 24), k(250), k(cx + 24), k(282)], k(4), fill=ORANGE)
        text(d, (k(cx), k(195)), f"F{i + 1}", f12, LBL)
        text(d, (k(cx), k(118 if i % 2 else 140)), s, f16, INK)
        d.line([k(cx), k(150 if i % 2 else 152), k(cx), k(184)], fill=DIM, width=k(2))
    # the button block
    for row, names in enumerate((t["top"], t["bot"])):
        for i, s in enumerate(names):
            cx, cy = 1218 + i * 56, 420 + row * 48
            used = bool(s)
            d.rounded_rectangle([k(cx - 26), k(cy - 19), k(cx + 26), k(cy + 19)], k(5), fill=ORANGE if used else (130, 96, 80))
            if used:
                text(d, (k(cx), k(cy)), s, fit(d, s, k(48), (k(12), k(10), k(8))), CREAM)
    # keys
    kx0, kx1, ky0, ky1 = 120, 1500, 520, 690
    nw = 22
    ww = (kx1 - kx0) / nw
    for i in range(nw):
        d.rectangle([k(kx0 + i * ww + 1), k(ky0), k(kx0 + (i + 1) * ww - 1), k(ky1)], fill=KEYW)
    black = {0, 1, 3, 4, 5}
    for i in range(nw - 1):
        if i % 7 in black:
            bx = kx0 + (i + 1) * ww
            d.rectangle([k(bx - ww * 0.32), k(ky0), k(bx + ww * 0.32), k(ky0 + 100)], fill=KEYB)
    text(d, (k(810), k(505)), t["keys"], f12, LBL)
    # the notes below
    y = 735
    for i, s in enumerate(t["notes"]):
        text(d, (k(90), k(y + i * 30)), "·  " + s, fit(d, "·  " + s, k(1440), (k(16), k(14), k(12))), INK, anchor="lm")
    return im.resize((W, H), Image.LANCZOS)


def main():
    for lang in T:
        out = IMG / T[lang]["file"]
        draw(lang).quantize(48, dither=Image.Dither.NONE).save(out, optimize=True)
        print(f"controls: {out}")


if __name__ == "__main__":
    main()
