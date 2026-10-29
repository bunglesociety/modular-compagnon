#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Alexis Arakis (Bungle Society)
"""Modular Compagnon (Modular One on the SMK-37): one HTML page that installs the firmware from Chrome (Web MIDI, SysEx), opened from the
disk (file://), nothing sent anywhere. The package is inside the page; the way back to the stock firmware takes the
unmodified official SMK-37 Elite V016 file, checked by its SHA-256 before anything is written. The install itself is
Felucca's web installer (web/fm1pkg.js, web/fm1ota.js: the same protocol as tools/fm1_install.py).

  tools/make_m1_installer.py PACKAGE.fwsc OUT.html
"""
import base64
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent / "web"
STOCK_ELITE016_SHA256 = "b2ee38abfef3ae922e4eecdb8a551e14345300ae6f270265feacd33b6ef99cb6"


def strip_module(src):
    src = re.sub(r"^export\s+", "", src, flags=re.M)
    return re.sub(r"^import .*?;\n", "", src, flags=re.M)


def product_of(raw):
    return "".join(chr((m - i - 1) & 0xFF) for i in range(20) if (m := raw[i * 0x30 + 0x2F]) != 0x7D)


PAGE = r"""<!doctype html>
<!-- SPDX-License-Identifier: GPL-3.0-only. Modular Compagnon (Alexis Arakis, Bungle Society), on Felucca by
     Leo Kuroshita (Hügelton Instruments). The package holds three JieLi AC79 SDK files (Apache-2.0). -->
<html lang="fr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Modular Compagnon</title>
<style>
  :root { --bg: #f6ecda; --ink: #26211d; --soft: #7a6d60; --acc: #e07a3a; --card: #fffaf1; --bad: #b8432f; }
  @media (prefers-color-scheme: dark) {
    :root:not([data-theme="light"]) { --bg: #1a1714; --ink: #f2e8d8; --soft: #a39684; --card: #24201c; }
  }
  :root[data-theme="dark"] { --bg: #1a1714; --ink: #f2e8d8; --soft: #a39684; --card: #24201c; }
  body { margin: 0; background: var(--bg); color: var(--ink); font: 17px/1.55 -apple-system, system-ui, sans-serif; }
  main { max-width: 580px; margin: 0 auto; padding: 44px 16px 64px; }
  h1 { font-size: 28px; margin: 0 0 4px; }
  .lead { color: var(--soft); margin: 0 0 28px; }
  .card { background: var(--card); border-radius: 14px; padding: 18px 20px; margin: 0 0 16px; }
  .card h2 { font-size: 17px; margin: 0 0 8px; }
  .card p { margin: 0 0 12px; }
  ol { margin: 0 0 12px; padding-left: 22px; }
  button { font-family: inherit; font-weight: 600; font-size: 16px; line-height: 1; background: var(--ink); color: var(--bg); border: 0;
           border-radius: 999px; padding: 14px 22px; cursor: pointer; }
  button:disabled { opacity: .35; cursor: default; }
  button:focus-visible { outline: 3px solid var(--acc); outline-offset: 3px; }
  .small { font-size: 14px; color: var(--soft); }
  progress { width: 100%; height: 6px; margin: 8px 0 0; accent-color: var(--acc); }
  #status { font-weight: 600; min-height: 1.5em; margin: 12px 0 0; }
  #status.bad { color: var(--bad); }
  pre { font-size: 12px; color: var(--soft); white-space: pre-wrap; max-height: 160px; overflow: auto; margin: 10px 0 0; }
  details summary { cursor: pointer; font-weight: 600; }
  input[type=file] { font: inherit; font-size: 14px; margin: 4px 0 12px; max-width: 100%; }
</style>
</head>
<body>
<main>
  <h1>Modular Compagnon</h1>
  <p class="lead">Le firmware du chat (Modular One sur le M-VAVE SMK-37 Elite). Cette page s'utilise dans Chrome, depuis ton ordinateur : rien n'est envoyé sur internet.</p>

  <div class="card">
    <h2>1. Vérifier le clavier</h2>
    <p>Branche le SMK-37 en USB directement sur l'ordinateur (pas de hub), ferme les autres applis MIDI (Logic, Ableton, l'appli M-VAVE...), puis :</p>
    <button id="check">Lire le clavier</button>
    <p id="who" class="small" style="margin:12px 0 0"></p>
  </div>

  <div class="card">
    <h2>2. Installer Modular Compagnon</h2>
    <p>Pendant l'écriture (environ 2 minutes) : ne débranche pas le câble, ne touche pas au clavier, ne ferme pas la page. Le clavier redémarre tout seul à la fin.</p>
    <button id="go" disabled>Installer Modular Compagnon</button>
  </div>

  <div class="card">
    <details>
      <summary>Revenir au firmware d'origine</summary>
      <p style="margin-top:12px">Choisis le fichier officiel <b>SMK-37_Elite_016.fwsc</b> (celui gardé dans ton dossier <i>_transfert_claude</i> sous le nom SMK-37_Elite_016_ORIGINE.fwsc, ou celui du site M-VAVE). La page vérifie que c'est bien le fichier officiel intact avant d'écrire quoi que ce soit.</p>
      <input id="stock-file" type="file" accept=".fwsc">
      <p><button id="stock-go" disabled>Remettre le firmware d'origine</button></p>
    </details>
  </div>

  <progress id="bar" max="100" value="0"></progress>
  <p id="status"></p>
  <pre id="log"></pre>
  <p class="small">Si l'écriture s'arrête en route (câble, veille de l'ordinateur...) : le clavier reste en mode mise à jour. Rebranche-le et appuie de nouveau sur le même bouton, l'écriture reprend.</p>
  <p class="small">Modular Compagnon, Alexis Arakis (Bungle Society), sur Felucca de Leo Kuroshita (Hügelton Instruments). GPL-3.0, sans garantie. Le paquet contient trois fichiers du SDK JieLi AC79 (Apache-2.0). M-VAVE et SMK-37 sont des marques de leurs propriétaires.</p>
</main>
<script>
/*LIB*/
const $ = (id) => document.getElementById(id);
const PKG_B64 = "/*PKG*/";
const PRODUCT = "/*PRODUCT*/";
const STOCK_SHA = "/*STOCKSHA*/";
const STOCK_PRODUCT = "SMK-37 Elite_016";
const pkgBytes = Uint8Array.from(atob(PKG_B64), (c) => c.charCodeAt(0));
const image = logicalImage(pkgBytes);
let stock = null, busy = false;

const MSG = {
  notfound: "Clavier introuvable. Vérifie le câble USB (un câble qui transmet les données), ferme les autres applis MIDI, puis réessaie.",
  model: "Ce n'est pas un SMK-37 Elite : rien n'a été écrit.",
  lost: "La connexion USB a été coupée. Rebranche le clavier et appuie de nouveau sur le même bouton.",
  stopped: "Le clavier s'est arrêté en route. Rebranche-le et appuie de nouveau sur le même bouton.",
  noloader: "Le clavier en mode mise à jour n'apparaît pas. Rebranche le câble et appuie de nouveau : il reste en mode mise à jour jusqu'à la fin.",
  noreturn: "Le clavier n'est pas revenu. Éteins-le et rallume-le.",
  mismatch: "Écrit, mais le clavier annonce une autre version : ",
  badreq: "Demande inattendue du clavier : ",
};
const say = (t, bad) => { $("status").textContent = t; $("status").className = bad ? "bad" : ""; };
const log = (t) => { const l = $("log"); l.textContent += t + "\n"; l.scrollTop = l.scrollHeight; };
const lock = (on) => { busy = on; $("go").disabled = on; $("check").disabled = on; $("stock-go").disabled = on || !stock; $("stock-file").disabled = on; };
window.addEventListener("beforeunload", (e) => { if (busy) { e.preventDefault(); e.returnValue = "Écriture en cours"; } });
$("go").disabled = false;

async function midi() {
  if (!navigator.requestMIDIAccess) { say("Ce navigateur ne sait pas faire : ouvre la page dans Chrome.", 1); return null; }
  try { return await navigator.requestMIDIAccess({ sysex: true }); }
  catch (e) { say("Chrome n'a pas donné l'accès MIDI. Recharge la page et accepte la demande (MIDI et SysEx).", 1); return null; }
}

function stepper(img) {
  return (k, a) => {
    log(`${k} ${a ?? ""}`);
    if (k === "start") say("Le clavier répond (" + a + "), on commence...");
    if (k === "verify") { say("Vérification..."); $("bar").value = Math.min(10, a / 8); }
    if (k === "loader") say("Passage en mode mise à jour...");
    if (k === "write") { say("Écriture... ne débranche pas"); $("bar").value = 10 + Math.min(88, (a * 512 / img.length) * 88); }
    if (k === "reboot") say("Redémarrage...");
    if (k === "done") { $("bar").value = 100; }
  };
}

async function run(img, product, doneText) {
  const m = await midi();
  if (!m) return;
  lock(true);
  $("bar").value = 0;
  try {
    const up = new Updater(m, log);
    const step = stepper(img);
    const ota = await up.find((id) => /^ota-/i.test(id.model));
    if (ota) {
      ota.link.close();
      log("mode mise à jour trouvé : reprise de l'écriture");
      say((await up.resume(img, step, product)) ? doneText : MSG.stopped, 0);
    } else {
      await up.install(img, product, step);
      say(doneText);
    }
  } catch (e) {
    say(e && e.code && MSG[e.code] ? MSG[e.code] + (e.code === "mismatch" || e.code === "badreq" ? (e.detail || e.message) : "") : "Erreur : " + (e && e.message), 1);
    log(String(e && e.stack || e));
  }
  lock(false);
}

$("check").addEventListener("click", async () => {
  const m = await midi();
  if (!m) return;
  lock(true);
  try {
    const up = new Updater(m, log);
    const dev = await up.find(null);
    if (!dev) { $("who").textContent = MSG.notfound; }
    else {
      dev.link.close();
      const id = dev.id.text, ota = /^ota-/i.test(dev.id.model);
      $("who").textContent = ota ? `${id} : le clavier est en mode mise à jour. Appuie sur « Installer Modular Compagnon » pour finir l'écriture.`
        : id === PRODUCT ? `${id} : Modular Compagnon est installé.`
        : id === STOCK_PRODUCT ? `${id} : le firmware d'origine (version 016). Prêt pour l'installation.`
        : sameModel(dev.id.model, PRODUCT.split("_")[0]) ? `${id} : un SMK-37 Elite. Prêt pour l'installation.`
        : `${id} : ce n'est pas un SMK-37 Elite, cette page n'écrira rien dessus.`;
    }
  } catch (e) { $("who").textContent = "Erreur : " + e.message; }
  lock(false);
});

$("go").addEventListener("click", () => {
  if (busy) return;
  if (!confirm("Installer Modular Compagnon sur le SMK-37 Elite ?\n\nLes réglages et presets du firmware d'origine seront perdus. Le retour à l'origine reste possible avec le fichier officiel.")) return;
  run(image, PRODUCT, "Terminé. Le chat est chez lui.");
});

$("stock-file").addEventListener("change", async () => {
  stock = null; lock(false);
  const f = $("stock-file").files[0];
  if (!f) return;
  const bytes = new Uint8Array(await f.arrayBuffer());
  if (await sha256hex(bytes) !== STOCK_SHA || productOf(bytes) !== STOCK_PRODUCT) {
    say("Ce fichier n'est pas le SMK-37_Elite_016.fwsc officiel intact : il ne sera pas installé.", 1);
    return;
  }
  stock = logicalImage(bytes);
  say("Fichier officiel SMK-37 Elite 016 vérifié.");
  lock(false);
});

$("stock-go").addEventListener("click", () => {
  if (busy || !stock) return;
  if (!confirm("Remettre le firmware d'origine (SMK-37 Elite 016) ?\n\nModular Compagnon, ses réglages et ses croquis ne seront plus utilisables.")) return;
  run(stock, STOCK_PRODUCT, "Terminé. Le SMK-37 est revenu à son firmware d'origine.");
});
</script>
</body>
</html>
"""


def main(pkg, out):
    raw = Path(pkg).read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"SMK-37 Elite_9\d\d", product) or b"FELUCCA-LOADER-1" not in raw:
        raise SystemExit(f"{pkg}: not a Modular One SMK-37 Elite package (identity {product!r})")
    lib = "\n".join(strip_module((HERE / f).read_text(encoding="utf-8")) for f in ("fm1pkg.js", "fm1ota.js"))
    html = PAGE
    for mark, val in (("/*LIB*/", lib), ("/*PKG*/", base64.b64encode(raw).decode()), ("/*PRODUCT*/", product),
                      ("/*STOCKSHA*/", STOCK_ELITE016_SHA256)):
        assert html.count(mark) == 1, mark
        html = html.replace(mark, val)
    Path(out).write_text(html, encoding="utf-8")
    print(f"installer: {out} ({len(html)} B, {product})")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(*sys.argv[1:3])
