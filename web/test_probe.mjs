// SPDX-License-Identifier: GPL-3.0-only
// fm1probe.js: what each firmware answers, and what the install pages then allow.  node web/test_probe.mjs
import { classify, verdict, verCmp, readInfo, devLabel } from "./fm1probe.js";
let fails = 0;
const eq = (a, b, what) => { if (JSON.stringify(a) !== JSON.stringify(b)) { fails++; console.log(`FAIL ${what}: ${JSON.stringify(a)} != ${JSON.stringify(b)}`); } };
const id = (text) => { const m = /^([^_]+)_(\d+)$/.exec(text); return { text, model: m[1], version: +m[2] }; };
const C = (t, info) => { const c = classify(id(t), info); return [c.family, c.version]; };
eq(C("FM-1_016", null), ["stock", "016"], "M-VAVE");
eq(C("FM-1_901", "FELUCCA KOI 0.1"), ["koi", "0.1"], "Koi");
eq(C("FM-1_903", "FELUCCA M1 0.3 BETA"), ["one", "0.3"], "Modular One 0.3");
eq(C("FM-1_900", "FELUCCA M1 0.1 BETA"), ["one", "0.1"], "Modular One 0.1");
eq(C("FM-1_902", "FELUCCA 0.2-BETA"), ["one", "0.2"], "Modular One 0.2");
eq(C("FM-1_900", "FELUCCA v1.6.1"), ["felucca", "1.6.1"], "Felucca");
eq(C("SMK-37 Elite_910", "FELUCCA v1.0"), ["compagnon", "1.0"], "Compagnon 1.0");
eq(C("SMK-37 Elite_900", "FELUCCA v1.0"), ["compagnon", "1.0"], "Compagnon 1.0 as released (_900)");
eq(C("SMK-37 Elite_911", "FELUCCA MC 1.1"), ["compagnon", "1.1"], "Compagnon 1.1");
eq(C("SMK-37 Pro_900", "FELUCCA v1.6.1"), ["felucca", "1.6.1"], "Felucca SMK");
eq(C("ota-FM-1_900", null), ["ota", ""], "loader");
eq(C("FM-1_900", null), ["unknown", "0.0"], "no INFO, 9xx");
eq(verCmp("0.10", "0.9"), 1, "0.10 > 0.9");
const koi = { family: "koi", version: "0.1", product: "FM-1_901" }, one = { family: "one", version: "0.3", product: "FM-1_903" };
const V = (t, info, pkg) => verdict({ id: id(t), ...classify(id(t), info) }, pkg).key;
eq(V("FM-1_016", null, koi), "fresh", "stock -> koi");
eq(V("FM-1_903", "FELUCCA M1 0.3 BETA", koi), "replace", "one -> koi");
eq(V("FM-1_901", "FELUCCA KOI 0.1", koi), "same", "koi again");
eq(V("FM-1_902", "FELUCCA 0.2-BETA", one), "update", "one 0.2 -> 0.3");
eq(V("FM-1_904", "FELUCCA M1 0.4 BETA", one), "older", "one 0.4 -> 0.3");
eq(V("SMK-37 Elite_910", "FELUCCA v1.0", koi), "otherModel", "an SMK: no Koi");
eq(verdict({ id: id("SMK-37 Elite_910"), ...classify(id("SMK-37 Elite_910"), "FELUCCA v1.0") }, koi).ok, false, "an SMK: refused");
eq(V("ota-FM-1_900", null, koi), "ota", "in update mode");
eq(devLabel(classify(id("FM-1_016"), null), "fr"), "le firmware d'origine M-VAVE (016)", "label");
/* readInfo on a fake link: the INFO reply, an unrelated SysEx before it */
const fake = (replies) => ({ q: [], drain() {}, send() { this.q.push(...replies); return true; }, async read() { return this.q.shift() || null; } });
const enc = (s) => [...s].map((c) => c.charCodeAt(0));
eq(await readInfo(fake([[0xF0, 0x7D, 0x46, 0x4C, 0x30, 1, 0xF7], [0xF0, 0x7D, 0x46, 0x4C, 1, ...enc("FELUCCA KOI 0.1"), 0, 13, 69, 0xF7]])), "FELUCCA KOI 0.1", "readInfo");
eq(await readInfo(fake([]), 50), null, "readInfo: no answer");
console.log(fails ? `test_probe: ${fails} FAILED` : "test_probe: ok");
process.exit(fails ? 1 : 0);
