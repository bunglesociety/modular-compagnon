# Modular Compagnon

[![License: GPL-3.0-only](https://img.shields.io/badge/license-GPL--3.0--only-blue.svg)](LICENSE)

![Modular Compagnon](web/compagnon/img/toilette.gif)

**FR** · Un firmware libre pour le **M-VAVE SMK-37 Elite**, par [Bungle Society](https://www.youtube.com/@bunglesociety).
Seize sons de Modular Town, un magnéto qui écoute toujours, et un petit chat noir qui vit dans l'écran.
Pas de banques, pas de sous-menus : une commande, un rôle.

**EN** · A free firmware for the **M-VAVE SMK-37 Elite**, by [Bungle Society](https://www.youtube.com/@bunglesociety).
Sixteen Modular Town sounds, a tape recorder that always listens, and a little black cat who lives in the screen.
No banks, no submenus: one control, one job.

- **Installer · Install:** [bunglesociety.github.io/modular-compagnon/install](https://bunglesociety.github.io/modular-compagnon/install/)
  (Chrome or Edge, USB cable, nothing else)
- **Page:** [bunglesociety.github.io/modular-compagnon](https://bunglesociety.github.io/modular-compagnon/)
- **Firmware:** `modular-compagnon-VERSION.fwsc` in the [Releases](https://github.com/bunglesociety/modular-compagnon/releases)
- Its big brother on the M-VAVE FM-1: [Modular One](https://bunglesociety.github.io/modular-one/)

## Les commandes · The controls

![Les commandes](web/compagnon/img/commandes-fr.png)

![The controls](web/compagnon/img/controls-en.png)

| | FR | EN |
|---|---|---|
| 16 pads | Les seize sons : Modular, Juno, Supersaw, PWM, Tape, FM, Souffle, Crépuscule · Flute, Brass, Strings, Choir, Rain, Deep, Pulse, Memory | The sixteen sounds |
| K1 | Volume | Volume |
| K2 – K8 | Cutoff, résonance, attaque, decay, sustain, release, detune | Cutoff, resonance, attack, decay, sustain, release, detune |
| F1 | Jour / nuit (le jour en haut) | Day / night (day at the top) |
| F2 | FM, l'hiver (la Montée sur Souffle et Crépuscule) | FM, the winter (the Rise on Souffle and Crépuscule) |
| F3 · F4 | Temps de l'écho (libre) · écho | Echo time (free) · echo |
| Knob bank · Fader bank | Onde 1 · onde 2 (la touche s'allume sur les sons qui en ont) | Wave 1 · wave 2 (the button lights up on the sounds that have them) |
| Oct − / + | L'octave | Octave |
| FX | Le vintage : pleurage de bande, chaleur, aigus adoucis | Vintage: tape wow, warmth, softened highs |
| Arp · Repeat · Scale · Chord | Arpège, note répétée, gamme, accord | Arpeggio, note repeat, scale, chord |
| Play · Rec · < > | Le magnéto : REC maintenu garde la dernière phrase jouée, PLAY la rejoue, < > choisissent, < et > maintenus effacent | The tape: REC held keeps the last phrase played, PLAY plays it back, < > choose, < and > held erase |
| Globe | Réglages : canal MIDI, vélocité, accordage, lumière des pads, veille, le caractère du chat | Settings: MIDI channel, velocity, tuning, pad light, sleep, the cat's temper |
| Molettes · Wheels | La modulation fait onduler le filtre ; le pitch est désactivé | Modulation waves the filter; pitch is off |

## Le chat · The cat

Il dort pendant l'ouverture (BUNGLE SOCIETY descend), puis se réveille. Il tourne la tête vers ce que tu touches, fait sa
toilette patte levée, ses griffes, des roulades, chasse, part en folie… La nuit (F1 en bas), il s'endort, même si tu joues,
jusqu'à ce que le jour revienne. Il vit sur l'herbe de Modular Town ; les étoiles sortent la nuit, la neige tombe l'hiver.

He sleeps during the opening (BUNGLE SOCIETY comes down), then wakes up. He turns his head to whatever you touch, washes
with a leg up, claws, rolls, hunts, gets the zoomies… At night (F1 down) he falls asleep, even while you play, until day
comes back. He lives on Modular Town grass; stars come out at night, snow falls in winter.

## Installer · Install

1. **FR** · Branche le SMK-37 Elite en USB, ouvre [l'installeur](https://bunglesociety.github.io/modular-compagnon/install/)
   dans Chrome ou Edge, clique *Installer* et autorise le MIDI. Ne débranche pas pendant l'écriture.
   **EN** · Plug the SMK-37 Elite in by USB, open [the installer](https://bunglesociety.github.io/modular-compagnon/install/)
   in Chrome or Edge, click *Install* and allow MIDI. Don't unplug while it writes.
2. **Retour d'origine · Back to stock:** the installer's last section takes M-VAVE's own *SMK-37 Elite version 016* file
   (checked by its SHA-256) and puts it back. Keep a copy of that file.

The installer only writes the firmware's own areas; the keyboard's boot, identity and calibration are never touched.
Installing is at your own risk. Only the **Elite** model is supported.

## Construire · Build

Built on [Felucca](https://github.com/hugelton/Felucca) 1.6.1 by Leo Kuroshita ([Hügelton Instruments](https://hugelton.com)),
its SMK-37 board layer. See [BUILDING.md](BUILDING.md) for the toolchain (the JieLi AC79 SDK is not in this tree), then:

```sh
BOARD=smk37 MODEL=elite ./build.sh          # -> build/modular-compagnon-smk37-elite.fwsc
tests/run_tests.sh
python3 tools/make_compagnon_img.py         # the screen pictures (web/compagnon/img)
python3 tools/make_controls_img.py          # the controls picture
python3 tools/make_m1_installer.py build/modular-compagnon-smk37-elite.fwsc Modular-Compagnon.html 0.1-beta   # one-file installer
python3 tools/make_compagnon_site.py build/modular-compagnon-smk37-elite.fwsc 0.1-beta site/                  # GitHub Pages
```

The Modular Compagnon code: `firmware/src/m1_*.c` (controls, screen, cat, tape, sketches), `firmware/src/eng_ville.c`
(the sounds), `tools/gen_cat.py` (the cat's sprites), `tools/gen_pixfont.py`, `web/compagnon/`.

## Licence · License

GPL-3.0-only, like Felucca: [LICENSE](LICENSE), [LICENSING.md](LICENSING.md), [LICENSES/](LICENSES/), [CREDITS.md](CREDITS.md).
The pixel font is [Silkscreen](https://github.com/googlefonts/silkscreen) by Jason Kottke (SIL OFL 1.1).
Felucca's own README: [README-FELUCCA.md](README-FELUCCA.md).

Not affiliated with M-VAVE. SMK-37 is their product name, used only to say which keyboard this runs on.

© 2026 Alexis Arakis · Bungle Society
