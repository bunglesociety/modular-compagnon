#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Alexis Arakis (Bungle Society)
"""Make the Modular Compagnon site (GitHub Pages: bunglesociety.github.io/modular-compagnon, or any https host):

  index.html                              the home: web/compagnon/block.html (the same block as the bunglesociety.com
                                          page) in a page of its own, the pictures and links made relative
  install/index.html                      web/compagnon/install.html with fm1pkg.js, fm1ota.js and the metadata
  img/                                    web/compagnon/img (tools/make_compagnon_img.py)
  firmware/modular-compagnon-VER.fwsc     the package (+ LICENSE, LICENSING.md, LICENSES/: the package holds three
                                          JieLi SDK files under Apache-2.0)
  .nojekyll                               (GitHub Pages: serve the files as they are)

  tools/make_compagnon_site.py build/modular-compagnon-smk37-elite.fwsc 0.1-beta OUT_DIR

Try it before publishing:  cd OUT_DIR && python3 -m http.server 8000  ->  http://localhost:8000
(Web MIDI needs https or localhost.)
"""
import re
import shutil
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from make_m1_installer import ROOT, WEB, check, page   # noqa: E402

SITE = "https://bunglesociety.github.io/modular-compagnon/"


def home():
    block = (WEB / "compagnon" / "block.html").read_text(encoding="utf-8")
    style = block[block.index("<style>") + 7:block.index("</style>")]
    style = style[:style.index("html, body, #siteWrapper")] + style[style.index(".mcw {"):]   # (the Squarespace page rules)
    body = block[block.index("</style>") + 8:].replace(SITE, "")                            # relative links
    return f"""<!doctype html>
<!-- SPDX-License-Identifier: GPL-3.0-only. Modular Compagnon (Alexis Arakis, Bungle Society): tools/make_compagnon_site.py -->
<html lang="fr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Modular Compagnon</title>
<meta name="description" content="Modular Compagnon: a free firmware for the M-VAVE SMK-37 Elite by Bungle Society. Sixteen Modular Town sounds, a tape recorder, and a little black cat who lives in the screen.">
<style>
html, body {{ margin: 0; background: #dccaa4; }}
{style}
.mcw {{ max-width: 1120px; margin: 0 auto; min-height: 100vh; }}
</style>
</head>
<body>
{body.strip()}
</body>
</html>
"""


def main(pkg, version, out):
    pkg, out = Path(pkg), Path(out)
    raw = pkg.read_bytes()
    product = check(raw, pkg)
    name = f"modular-compagnon-{re.sub(r'[^A-Za-z0-9.-]', '-', version)}.fwsc"
    inst, fw, img = out / "install", out / "firmware", out / "img"
    for d in (inst, fw, img):
        d.mkdir(parents=True, exist_ok=True)
    for old in fw.glob("*.fwsc"):                    # one package: the current one
        old.unlink()
    shutil.copy(pkg, fw / name)
    (inst / "index.html").write_text(page({"version": version, "product": product, "pkg": "../firmware/" + name},
                                          "../img/joue.gif",
                                          {"fr": "../img/commandes-fr.png", "en": "../img/controls-en.png"}),
                                     encoding="utf-8")
    for f in (WEB / "compagnon" / "img").iterdir():
        shutil.copy(f, img / f.name)
    (out / "index.html").write_text(home(), encoding="utf-8")
    (fw / "LICENSES").mkdir(exist_ok=True)
    for f in (ROOT / "LICENSES").glob("*.txt"):
        shutil.copy(f, fw / "LICENSES" / f.name)
    for doc in ("LICENSE", "LICENSING.md"):
        shutil.copy(ROOT / doc, fw / doc)
    (out / ".nojekyll").write_text("", encoding="utf-8")
    print(f"site: {out}: index.html, install/, img/ ({len(list(img.iterdir()))}), firmware/{name} ({len(raw)} B, {product})")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:4])
