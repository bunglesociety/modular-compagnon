#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Alexis Arakis (Bungle Society)
"""Modular Compagnon FM-1: the controls picture (web/compagnon/img/commandes-fm1-fr.png, controls-fm1-en.png): the
M-VAVE FM-1 seen from above, drawn like the SMK-37's (tools/make_controls_img.py: old paper, sepia ink, the pixel
letters), every control with its job in Modular Compagnon FM-1 (firmware/src/m1_ui_fm1.c, web/compagnon-fm1).
The cat in the screen is the firmware's own screen (web/compagnon/img/joue.gif, its first frame).

  tools/make_controls_fm1_img.py"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
IMG = ROOT / "web" / "compagnon" / "img"
FONT = ROOT / "assets" / "fonts" / "ui" / "silkscreen-latin-400-normal.woff"

PAPER, GRAIN, INK, DIM = (222, 200, 160), (212, 188, 148), (60, 42, 28), (125, 104, 80)
BODY, ORANGE, CREAM, KEYW = (92, 86, 82), (204, 98, 54), (240, 232, 214), (244, 238, 226)
LBL, OFFB = (240, 226, 196), (130, 96, 80)

T = {
    "fr": dict(file="commandes-fm1-fr.png", title="MODULAR COMPAGNON FM-1  ·  LES COMMANDES",
               left=[("MASTER", "VOLUME"), ("SELECT", "JOUR/NUIT"), ("PRESETS", "LES 16 SONS"), ("ALGORITHM", "L'HIVER")],
               oct="OCTAVE", knobs="K1 A K4 : CE QUE DIT LA PAGE",
               b1=[("FX", "ECHO"), ("SEL", "COMPAGNON"), ("ENV", "ENVELOPPE"), ("LFO", "MOD"), ("EDIT", "ONDES"), ("GLO", "REGLAGES")],
               b2=[("HOME", "FILTRE"), ("SAVE", "GARDER"), ("ARP", "ARPEGE"), ("SEQ", "LISTE"), ("PLAY", "MAGNETO"), ("REC", "CROQUIS")],
               keys="27 TOUCHES : LES NOTES",
               notes=["PRESETS : LES 16 SONS DE MODULAR TOWN  ·  SAVE : GARDE TON REGLAGE DU SON (MAINTENU 2 S : L'ORIGINAL)",
                      "HOME : CUTOFF, RESO, DETUNE, ECHO  ·  ENV : ATTAQUE, DECAY, SUSTAIN, RELEASE",
                      "EDIT : ONDE 1, ONDE 2, GLIDE, VOIX  ·  LFO : MOD, VIBRATO, TREMOLO, VITESSE  ·  FX : TEMPS, ECHO, BANDE, CHALEUR",
                      "ARP : UN APPUI = ON / OFF  ·  MAINTENU : K1 LE MODE, K2 LA VITESSE, K3 LE TEMPO (SUIT LE DAW TOUT SEUL)",
                      "PLAY : LE MAGNETO (OU OCT- ET OCT+ MAINTENUS 1 S)  ·  REC MAINTENU : GARDE TA PHRASE  ·  SEQ MAINTENU : LA LISTE",
                      "SEL : DU TEMPS AVEC LE COMPAGNON  ·  GLO : LES REGLAGES (K1 LA LIGNE, K2 LA VALEUR)"]),
    "en": dict(file="controls-fm1-en.png", title="MODULAR COMPAGNON FM-1  ·  THE CONTROLS",
               left=[("MASTER", "VOLUME"), ("SELECT", "DAY/NIGHT"), ("PRESETS", "THE 16 SOUNDS"), ("ALGORITHM", "WINTER")],
               oct="OCTAVE", knobs="K1 TO K4: WHAT THE PAGE SAYS",
               b1=[("FX", "ECHO"), ("SEL", "COMPANION"), ("ENV", "ENVELOPE"), ("LFO", "MOD"), ("EDIT", "WAVES"), ("GLO", "SETTINGS")],
               b2=[("HOME", "FILTER"), ("SAVE", "KEEP"), ("ARP", "ARPEGGIO"), ("SEQ", "LIST"), ("PLAY", "TAPE"), ("REC", "SKETCH")],
               keys="27 KEYS: THE NOTES",
               notes=["PRESETS: THE 16 MODULAR TOWN SOUNDS  ·  SAVE: KEEPS YOUR TAKE ON THE SOUND (HELD 2 S: THE ORIGINAL)",
                      "HOME: CUTOFF, RESO, DETUNE, ECHO  ·  ENV: ATTACK, DECAY, SUSTAIN, RELEASE",
                      "EDIT: WAVE 1, WAVE 2, GLIDE, VOICE  ·  LFO: MOD, VIBRATO, TREMOLO, SPEED  ·  FX: TIME, ECHO, TAPE, WARMTH",
                      "ARP: A TAP = ON / OFF  ·  HELD: K1 THE MODE, K2 THE SPEED, K3 THE TEMPO (FOLLOWS A DAW BY ITSELF)",
                      "PLAY: THE TAPE (OR OCT- AND OCT+ HELD 1 S)  ·  REC HELD: KEEPS YOUR PHRASE  ·  SEQ HELD: THE LIST",
                      "SEL: TIME WITH THE COMPANION  ·  GLO: THE SETTINGS (K1 THE LINE, K2 THE VALUE)"]),
}


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
    k = lambda v: int(v * S)
    im = Image.new("RGB", (W * S, H * S), PAPER)
    d = ImageDraw.Draw(im)
    for y in range(0, H * S, 14):                           # the paper's grain
        for x in range((y // 14) % 2 * 7, W * S, 14):
            d.rectangle([x, y, x + 1, y + 1], fill=GRAIN)
    f12, f16, f28 = font(12 * S), font(16 * S), font(28 * S)
    text(d, (k(W / 2), k(46)), t["title"], f28, INK)
    # the body
    X0, Y0, X1, Y1 = 230, 130, 1370, 690
    d.rounded_rectangle([k(X0 + 8), k(Y0 + 8), k(X1 + 8), k(Y1 + 8)], k(34), fill=(190, 170, 134))
    d.rounded_rectangle([k(X0), k(Y0), k(X1), k(Y1)], k(34), fill=BODY)
    # the four knobs on the left: the name above, the job below
    for i, (name, job) in enumerate(t["left"]):
        cx, cy = 318 + (i % 2) * 112, 205 + (i // 2) * 108
        text(d, (k(cx), k(cy - 42)), name, fit(d, name, k(104), (k(12), k(10))), LBL)
        d.ellipse([k(cx - 24), k(cy - 24), k(cx + 24), k(cy + 24)], fill=ORANGE)
        d.line([k(cx), k(cy - 21), k(cx), k(cy - 4)], fill=CREAM, width=k(4))
        text(d, (k(cx), k(cy + 38)), job, fit(d, job, k(108), (k(12), k(10), k(8))), CREAM)
    # OCT- OCT+
    for i, s in enumerate(("OCT-", "OCT+")):
        cx, cy = 318 + i * 112, 418
        d.rounded_rectangle([k(cx - 44), k(cy - 18), k(cx + 44), k(cy + 18)], k(6), fill=ORANGE)
        text(d, (k(cx), k(cy)), s, f16, CREAM)
    text(d, (k(374), k(452)), t["oct"], f12, CREAM)
    # the screen: the cat
    sx, sy, sw = 500, 175, 200
    d.rounded_rectangle([k(sx - 14), k(sy - 14), k(sx + sw + 14), k(sy + sw + 14)], k(18), fill=(22, 20, 18))
    cat = Image.open(IMG / "joue.gif").convert("RGB").resize((k(sw), k(sw)), Image.NEAREST)
    im.paste(cat, (k(sx), k(sy)))
    # K1..K4
    for i in range(4):
        cx = 820 + i * 140
        d.ellipse([k(cx - 26), k(179), k(cx + 26), k(231)], fill=ORANGE)
        d.line([k(cx), k(182), k(cx), k(202)], fill=CREAM, width=k(4))
        text(d, (k(cx), k(160)), f"K{i + 1}", f12, LBL)
        d.line([k(cx), k(118), k(cx), k(148)], fill=DIM, width=k(2))
    d.line([k(820), k(118), k(1240), k(118)], fill=DIM, width=k(2))
    text(d, (k(1030), k(100)), t["knobs"], f16, INK)
    # the button block: the FM-1's name, its job below
    d.rounded_rectangle([k(745), k(268), k(1315), k(446)], k(14), fill=(78, 72, 68))
    for row, names in enumerate((t["b1"], t["b2"])):
        for i, (name, job) in enumerate(names):
            cx, cy = 800 + i * 92, 312 + row * 90
            d.rounded_rectangle([k(cx - 40), k(cy - 34), k(cx + 40), k(cy + 34)], k(8), fill=ORANGE)
            text(d, (k(cx), k(cy - 11)), name, f16, CREAM)
            text(d, (k(cx), k(cy + 15)), job, fit(d, job, k(74), (k(10), k(8))), LBL)
    # the keys: 11 on top (3, 2, 3, 2, 1), 16 below
    kx0, kx1 = 300, 1300
    step = (kx1 - kx0) / 16
    d.rounded_rectangle([k(kx0 - 26), k(486), k(kx1 + 26), k(672)], k(18), fill=(70, 65, 62))
    top = (0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14.4)
    for p in top:
        cx = kx0 + (p + 0.5) * step + step * 0.25
        d.rounded_rectangle([k(cx - 22), k(500), k(cx + 22), k(568)], k(22), fill=KEYW)
    for i in range(16):
        cx = kx0 + (i + 0.5) * step
        d.rounded_rectangle([k(cx - 24), k(584), k(cx + 24), k(660)], k(24), fill=KEYW)
    text(d, (k(800), k(476)), t["keys"], f12, LBL)
    # the notes below
    y = 724
    for i, s in enumerate(t["notes"]):
        text(d, (k(90), k(y + i * 28)), "·  " + s, fit(d, "·  " + s, k(1440), (k(16), k(14), k(12))), INK, anchor="lm")
    return im.resize((W, H), Image.LANCZOS)


def main():
    for lang in T:
        out = IMG / T[lang]["file"]
        draw(lang).quantize(48, dither=Image.Dither.NONE).save(out, optimize=True)
        print(f"controls: {out}")


if __name__ == "__main__":
    main()
