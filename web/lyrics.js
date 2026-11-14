// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
//
// (JIANT V10.7.33) VOICE's lyrics as phonemes, for the web editor. The device lost its English dictionary: the editor
// turns the words a user types into the phoneme codes the device sings (firmware/src/sing.c sy_phon: a lyric that
// starts with "/", each phoneme a code of one or two letters, "." where a syllable starts when the device would put it
// elsewhere, sy_phauto). The user sees the codes and can edit them before they are sent (63 characters at most).
//   ES  the device's own Spanish rules (sing.c sy_es_tok, sy_syl; tools/gen_lyrics.py es_tokens, syllabify): the same
//       phonemes as the device reads the letters, and "ü" as w (pingüino), which the device's letters cannot say
//   EN  the CMU Pronouncing Dictionary (web/lyrics_en.js: 121710 words, BSD-2-Clause), syllabified as
//       tools/gen_lyrics.py en_phones; a word it lacks by the device's letter rules (sing.c sy_en_rules)
//   JA  romaji as the device reads it (sing.c sy_ja): a mora a syllable
//   PT  Brazilian Portuguese by rules (approximate: nasal vowels, lh nh ch x j, final e o as i u, ti di as chi ji,
//       r / rr, s between vowels z, l at a syllable's end w)
//   IT  Italian by rules (approximate: c g soft before e i, ch gh, gli gn sc, z as ts, doubled consonants once), sung
//       with the device's LANG ES (the codes are all Spanish's)
// The tables (PH, CODE, CM) are firmware/src/sing.c's (tools/gen_lyrics.py: web/test_web.mjs checks them against
// build/gen/felucca_lyrics.h). A plain script: LYR in the page, and in Node's vm (web/test_web.mjs).
var LYR = (function () {
  "use strict";
  const PH = ["SIL", "A", "E", "I", "O", "U", "UJ", "IH", "EH", "AE", "AA", "AO", "UH", "AH", "ER", "AX", "Y", "W", "L",
              "RT", "RR", "RA", "M", "N", "NY", "NG", "NM", "P", "T", "K", "B", "D", "G", "F", "S", "SH", "H", "X", "TH",
              "V", "Z", "ZH", "DH", "CH", "TS", "J"];
  const NAME = ["_", "a", "e", "i", "o", "u", "u", "ih", "eh", "ae", "aa", "ao", "uh", "ah", "er", "ax", "y", "w", "l",
                "r", "rr", "r", "m", "n", "ny", "ng", "N", "p", "t", "k", "b", "d", "g", "f", "s", "sh", "h", "x", "th",
                "v", "z", "zh", "dh", "ch", "ts", "j"];
  const CODE = ["#", "A", "E", "I", "O", "U", "U", "IH", "EH", "AE", "AA", "AO", "UH", "AH", "ER", "AX", "Y", "W", "L",
                "R", "RR", "R", "M", "N", "NY", "NG", "NN", "P", "T", "K", "B", "D", "G", "F", "S", "SH", "H", "X", "TH",
                "V", "Z", "ZH", "DH", "CH", "TS", "J"];
  const CM = [20, 31, 31, 31, 31, 31, 0, 18, 26, 18, 18, 26, 18, 18, 18, 18, 31, 31, 31, 31, 25, 0, 31, 31, 25, 18, 28, 31,
              31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 18, 31, 29, 31];
  const P = Object.fromEntries(PH.map((n, i) => [n, i]));
  const MASK = { ES: 1, EN: 2, JA: 4, PT: 8, OT: 16, IT: 1 };          // (IT: sung as ES)
  const isV = c => c >= 1 && c <= 15;                                // the vowels (PHON's PC_VOW)
  const PLO = new Set([P.P, P.T, P.K, P.B, P.D, P.G]);
  const sing = l => (l === "IT" ? "ES" : l);                         // the device's LANG for a converter's language

  /* ------------------------------------------------------------------------ the codes (sing.c sy_phon) --- */
  function cl(a, b) {                                                // sing.c ph_cl
    if (a === P.SIL) return true;
    if (isV(a) || a === P.Y || a === P.W) return false;
    if (b === P.Y || b === P.W) return true;
    return (b === P.L || b === P.RT || b === P.RA) && (PLO.has(a) || a === P.F) && !(a === P.D && b === P.L);
  }
  function auto(lang, ph) {                                          // sing.c sy_phauto: the syllables' starts
    const st = new Set();
    let e = 0, first = true;
    for (let k = 0; k < ph.length; k++) {
      const c = ph[k];
      if (!(isV(c) || (lang === "JA" && c === P.NM))) continue;
      if (lang !== "JA" && k && isV(ph[k - 1]) && [P.IH, P.UH, P.I, P.U].includes(c)) { e = k + 1; continue; }
      let s = k - e;
      if (first) s = 0;
      else if (s <= 1) s = k - s;
      else if (cl(ph[k - 2], ph[k - 1])) s = s >= 3 && ph[k - 3] === P.SIL ? k - 3 : k - 2;
      else s = k - 1;
      st.add(s); e = k + 1; first = false;
    }
    if (ph.length) st.add(0);
    return st;
  }
  /* a word of codes (no "/") -> [[phoneme, syllable start]] as the device reads it */
  function parse(lang, text) {
    lang = sing(lang);
    const mask = MASK[lang] || MASK.OT, w = String(text).toUpperCase(), out = [], dots = w.includes(".");
    let i = 0, st = true;
    while (i < w.length) {
      if (w[i] === ".") { st = true; i++; continue; }
      let best = -1, bl = 0;
      for (let c = 0; c < PH.length; c++)
        if ((CM[c] & mask) && CODE[c].length > bl && w.startsWith(CODE[c], i)) { best = c; bl = CODE[c].length; }
      if (best < 0) { i++; continue; }
      if (best === P.U && lang === "JA") best = P.UJ;
      if (best === P.RT && lang === "EN") best = P.RA;
      out.push([best, dots && st]);
      st = false; i += bl;
    }
    if (!dots) { const s = auto(lang, out.map(x => x[0])); out.forEach((x, k) => { x[1] = s.has(k); }); }
    if (out.length) out[0][1] = true;
    return out;
  }
  const same = (a, b) => a.length === b.length && a.every((x, k) => x[0] === b[k][0] && !!x[1] === !!b[k][1]);
  /* [[phoneme, syllable start]] -> the shortest code text that reads back as it (gen_lyrics.py ph_text), null none */
  function text(lang, ph) {
    lang = sing(lang);
    const want = ph.map(([c, s], k) => [c === P.UJ && lang !== "JA" ? P.U : c === P.RA && lang !== "EN" ? P.RT : c, !!s || k === 0]);
    const plain = want.map(([c]) => CODE[c]).join("");
    if (same(parse(lang, plain), want)) return plain;
    let dotted = want.map(([c, s], k) => (s && k ? "." : "") + CODE[c]).join("");
    if (!dotted.includes(".")) dotted += ".";                        // (one syllable the device would split)
    return same(parse(lang, dotted), want) ? dotted : null;
  }
  const show = ph => ph.map(([c, s]) => (s ? "|" : "") + NAME[c]).join(" ");   // "|k o |r a" (tests/voice_test.c g2p_str)

  /* --------------------------------------------------------------- syllables (sing.c sy_syl) --- */
  const TC = 0, TS = 1, TW = 2;
  function esCl(a, b) {
    return [P.P, P.B, P.F, P.K, P.G, P.T, P.D].includes(a) && (b === P.L || b === P.RT) && !(a === P.D && b === P.L);
  }
  function syl(tok) {                                                // [[code, kind]] -> [[code, start]]
    const groups = [];
    let i = 0;
    while (i < tok.length) {
      if (tok[i][1] === TC) { i++; continue; }
      let j = i + 1;
      while (j < tok.length && tok[j][1] !== TC && !(tok[j][1] === TS && tok[j - 1][1] === TS)) j++;
      groups.push([i, j]); i = j;
    }
    if (!groups.length) return [];
    const out = tok.map(t => [t[0], false]), st = new Set([0]);
    groups.forEach(([a, b], g) => {
      let nu = a;
      while (nu < b && tok[nu][1] !== TS) nu++;
      if (nu === b) nu = b - 1;
      for (let k = a; k < nu; k++) out[k][0] = tok[k][0] === P.I ? P.Y : P.W;
      if (g) {
        const pa = groups[g - 1][1], c = a - pa;
        if (!c) st.add(a);
        else if (c === 1) st.add(pa);
        else if (c === 2) st.add(esCl(tok[pa][0], tok[pa + 1][0]) ? pa : pa + 1);
        else st.add(esCl(tok[a - 2][0], tok[a - 1][0]) ? a - 2 : a - 1);
      }
    });
    out.forEach((x, k) => { x[1] = st.has(k); });
    return out;
  }
  const vow = c => "aeiou".includes(c) && c !== "";

  /* -------------------------------------------------------------- Spanish (sing.c sy_es_tok) --- */
  function esTok(w) {
    const t = [];
    for (let i = 0; i < w.length && t.length + 2 <= 32; i++) {
      const c = w[i], nx = w[i + 1] || "", nx2 = w[i + 2] || "";
      let code = -1, kind = TC;
      switch (c) {
        case "a": code = P.A; kind = TS; break;
        case "e": code = P.E; kind = TS; break;
        case "o": code = P.O; kind = TS; break;
        case "i": code = P.I; kind = TW; break;
        case "u": case "ü": code = P.U; kind = TW; break;       // (ü: the editor's, güe güi; the device has no ü)
        case "y": if (vow(nx)) code = P.Y; else { code = P.I; kind = TW; } break;
        case "c": if (nx === "h") { code = P.CH; i++; } else code = nx === "e" || nx === "i" ? P.S : P.K; break;
        case "q": code = P.K; if (nx === "u") i++; break;
        case "g":
          if (nx === "e" || nx === "i") code = P.X;
          else { code = P.G; if (nx === "u" && (nx2 === "e" || nx2 === "i")) i++; }
          break;
        case "j": code = P.X; break;
        case "x": t.push([P.K, TC]); code = P.S; break;
        case "z": case "s": code = P.S; break;
        case "v": case "b": code = P.B; break;
        case "d": code = P.D; break;
        case "f": code = P.F; break;
        case "k": code = P.K; break;
        case "p": code = P.P; break;
        case "t": code = P.T; break;
        case "m": code = P.M; break;
        case "w": code = P.W; break;
        case "l": if (nx === "l") { code = P.Y; i++; } else code = P.L; break;
        case "n": if (nx === "y" && vow(nx2)) { code = P.NY; i++; } else code = P.N; break;
        case "r":
          if (nx === "r") { code = P.RR; i++; }
          else code = !t.length || [P.N, P.L, P.S].includes(t[t.length - 1][0]) ? P.RR : P.RT;
          break;
        default: break;                                              // h, the rest: silent
      }
      if (code >= 0) t.push([code, kind]);
    }
    return t;
  }

  /* --------------------------------------------------- English letter rules (sing.c sy_en_rules) --- */
  const R = (s, when, adv, a, b) => ({ s, when, adv, a, b });
  const EN_R = [
    R("y", "start", 1, [P.Y, TC]), R("y", "nxv", 1, [P.Y, TC]), R("y", "last", 1, [P.I, TS]), R("y", "", 1, [P.IH, TS]),
    R("ee", "", 2, [P.I, TS]), R("ea", "", 2, [P.I, TS]), R("oo", "", 2, [P.U, TS]),
    R("ou", "", 2, [P.AA, TS], [P.UH, TW]), R("ow", "", 2, [P.AA, TS], [P.UH, TW]),
    R("ai", "", 2, [P.E, TS], [P.IH, TW]), R("ay", "", 2, [P.E, TS], [P.IH, TW]), R("oa", "", 2, [P.O, TS], [P.UH, TW]),
    R("oi", "", 2, [P.AO, TS], [P.IH, TW]), R("oy", "", 2, [P.AO, TS], [P.IH, TW]),
    R("au", "", 2, [P.AO, TS]), R("aw", "", 2, [P.AO, TS]), R("igh", "", 3, [P.AA, TS], [P.IH, TW]),
    R("a", "", 1, [P.AE, TS]), R("e", "", 1, [P.EH, TS]), R("i", "", 1, [P.IH, TS]), R("o", "", 1, [P.AA, TS]),
    R("u", "", 1, [P.AH, TS]),
    R("th", "", 2, [P.TH, TC]), R("sh", "", 2, [P.SH, TC]), R("ch", "", 2, [P.CH, TC]), R("ph", "", 2, [P.F, TC]),
    R("wh", "", 2, [P.W, TC]), R("ck", "", 2, [P.K, TC]), R("qu", "", 2, [P.K, TC], [P.W, TC]), R("kn", "start", 1),
    R("wr", "", 1), R("gh", "", 2), R("ng", "nxc", 2, [P.NG, TC]),
    R("ce", "", 1, [P.S, TC]), R("ci", "", 1, [P.S, TC]), R("cy", "", 1, [P.S, TC]), R("h", "nxv", 1, [P.H, TC]),
    R("ll", "", 1), R("s", "vsv", 1, [P.Z, TC]), R("x", "", 1, [P.K, TC], [P.S, TC]),
  ];
  const EN_1 = { b: P.B, c: P.K, d: P.D, f: P.F, g: P.G, j: P.J, k: P.K, l: P.L, m: P.M, n: P.N, p: P.P, r: P.RA, s: P.S,
                 t: P.T, v: P.V, w: P.W, z: P.Z };
  function enTok(w) {
    const n = w.length, t = [];
    let magic = -1, i = 0;
    if (n >= 3 && w[n - 1] === "e" && !vow(w[n - 2]) && vow(w[n - 3])) magic = n - 3;
    while (i < n) {
      const c = w[i];
      let o = [], adv = 1;
      if (i === n - 1 && c === "e" && n > 2) break;
      if (i === magic) {
        o = { a: [[P.E, TS], [P.IH, TW]], e: [[P.I, TS]], i: [[P.AA, TS], [P.IH, TW]], o: [[P.O, TS], [P.UH, TW]] }[c] || [[P.U, TS]];
      } else {
        const r = EN_R.find(e => {
          if (!w.startsWith(e.s, i)) return false;
          const nx = e.s.length < 2 || e.s[0] === "n" ? i + e.s.length : i + 1;
          switch (e.when) {
            case "start": return i === 0;
            case "nxv": return nx < n && vow(w[nx]);
            case "nxc": return !(nx < n && vow(w[nx]));
            case "last": return i === n - 1;
            case "vsv": return i > 0 && vow(w[i - 1]) && i + 1 < n && vow(w[i + 1]);
            default: return true;
          }
        });
        if (r) { o = [r.a, r.b].filter(Boolean); adv = r.adv; }
        else if (EN_1[c] !== undefined) o = [[EN_1[c], TC]];
      }
      for (const x of o) if (t.length < 32) t.push(x);
      i += adv;
    }
    return t;
  }

  /* ------------------------------------------------------------------- Japanese (sing.c sy_ja) --- */
  function ja(w) {
    const JV = [P.A, P.I, P.UJ, P.E, P.O], V = c => "aiueo".indexOf(c === undefined ? "#" : c);
    const JO = [P.K, P.B, P.K, P.D, P.K, P.F, P.G, P.H, P.K, P.J, P.K, P.RT, P.M, P.N, P.K, P.P, P.K, P.RT, P.S, P.T, P.K,
                P.V, P.W, P.K, P.Y, P.Z];
    const out = [], put = (c, s) => { if (out.length < 32) out.push([c, !!s]); };
    const n = w.length;
    let i = 0, k = 0, gem = false, lastv = P.A;
    while (i < n) {
      const c = w[i], nx = w[i + 1], nx2 = w[i + 2];
      if (V(c) >= 0) {
        if (gem) put(P.SIL, 1);
        lastv = c === "u" && lastv === P.O && k ? P.O : JV[V(c)];
        put(lastv, !gem); gem = false; k++; i++;
      } else if (c === "-") {
        put(lastv, 1); k++; i++;
      } else if (c === "n" && V(nx) < 0 && nx !== "y") {
        put(P.NM, 1); k++;
        i += (nx === "n" || nx === "'") && V(nx2) < 0 ? 2 : 1;
      } else if ((c === nx && c >= "a" && c <= "z") || (c === "t" && nx === "c")) {
        gem = true; i++;
      } else if (!(c >= "a" && c <= "z")) {
        i++;
      } else {
        let j = i, pal = false;
        let on = JO[c.charCodeAt(0) - 97];
        if ((c === "s" || c === "c") && nx === "h") on = c === "s" ? P.SH : P.CH;
        else if (c === "t" && nx === "s") on = P.TS;
        while (j < n && V(w[j]) < 0 && w[j] !== "-") { if (j > i && w[j] === "y") pal = true; j++; }
        const vv = j < n ? V(w[j]) : -1, v = vv >= 0 ? JV[vv] : P.UJ;
        if (on === P.S && vv === 1) on = P.SH;
        else if (on === P.T && vv === 1) on = P.CH;
        else if (on === P.T && vv === 2) on = P.TS;
        else if (on === P.H && vv === 2) on = P.F;
        else if ((on === P.Z || on === P.D) && vv === 1) on = P.J;
        else if (on === P.D && vv === 2) on = P.Z;
        if (on === P.SH || on === P.CH || on === P.J) pal = false;
        const mo = [];
        if (gem) mo.push(P.SIL);
        if (!(on === P.W && vv === 4)) mo.push(on);
        if (pal) mo.push(P.Y);
        mo.push(lastv = v);
        mo.forEach((x, q) => put(x, !q));
        gem = false; k++;
        i = vv >= 0 ? j + 1 : j;
      }
    }
    return out;
  }

  /* --------------------------------------------------- Portuguese and Italian (approximate rules) --- */
  function ptTok(raw) {                                              // raw: lower case, its accents kept
    const w = raw.normalize("NFC"), t = [], n = w.length;
    const isv = c => "aeiouáéíóúâêôãõà".includes(c) && c !== "";
    const base = c => ({ á: "a", à: "a", â: "a", ã: "a", é: "e", ê: "e", í: "i", ó: "o", ô: "o", õ: "o", ú: "u" }[c] || c);
    for (let i = 0; i < n; i++) {
      const c = w[i], nx = w[i + 1] || "", nx2 = w[i + 2] || "", end = i === n - 1 || (i === n - 2 && w[n - 1] === "s");
      const pv = i > 0 && isv(w[i - 1]);
      if (isv(c)) {
        const b = base(c);
        let code = { a: P.A, e: c === "é" ? P.EH : P.E, i: P.I, o: c === "ó" ? P.AO : P.O, u: P.U }[b];
        if (end && c === "e") code = P.I;                            // (final e, o: i, u)
        if (end && c === "o") code = P.U;
        t.push([code, b === "i" || b === "u" || code === P.I && end || code === P.U && end ? TW : TS]);
        if (c === "ã" || c === "õ" || ((nx === "m" || nx === "n") && !isv(nx2) && nx2 !== "h")) {
          t.push([P.NM, TW]);                                        // (a nasal vowel: its n after it, in its syllable)
          if (nx === "m" || nx === "n") i++;
        }
        continue;
      }
      switch (c) {
        case "l": if (nx === "h") { t.push([P.Y, TC]); i++; } else t.push([isv(nx) ? P.L : P.W, TC]); break;
        case "n": if (nx === "h") { t.push([P.NY, TC]); i++; } else t.push([P.N, TC]); break;
        case "c": if (nx === "h") { t.push([P.SH, TC]); i++; } else t.push([nx === "e" || nx === "i" ? P.S : P.K, TC]); break;
        case "ç": t.push([P.S, TC]); break;
        case "q": t.push([P.K, TC]); if (nx === "u" && (nx2 === "e" || nx2 === "i")) i++; break;
        case "g":
          if (nx === "e" || nx === "i") t.push([P.ZH, TC]);
          else { t.push([P.G, TC]); if (nx === "u" && (nx2 === "e" || nx2 === "i")) i++; }
          break;
        case "j": t.push([P.ZH, TC]); break;
        case "x": t.push([P.SH, TC]); break;
        case "s":
          if (nx === "s") { t.push([P.S, TC]); i++; }
          else t.push([pv && isv(nx) ? P.Z : P.S, TC]);
          break;
        case "z": t.push([end ? P.S : P.Z, TC]); break;
        case "r":
          if (nx === "r") { t.push([P.X, TC]); i++; }
          else t.push([i === 0 || ["n", "l", "s"].includes(w[i - 1]) ? P.X : P.RT, TC]);
          break;
        case "t": t.push([(nx === "i" || (nx === "e" && i + 2 >= n)) ? P.CH : P.T, TC]); break;
        case "d": t.push([(nx === "i" || (nx === "e" && i + 2 >= n)) ? P.J : P.D, TC]); break;
        case "h": break;
        default: {
          const m = { b: P.B, f: P.F, k: P.K, m: P.M, p: P.P, v: P.V, w: P.W, y: P.I }[c];
          if (m !== undefined) t.push([m, c === "y" ? TW : TC]);
        }
      }
    }
    return t;
  }
  function itTok(raw) {
    const w = raw.normalize("NFD").replace(/[̀-ͯ]/g, ""), t = [], n = w.length;
    for (let i = 0; i < n; i++) {
      const c = w[i], nx = w[i + 1] || "", nx2 = w[i + 2] || "", soft = nx === "e" || nx === "i";
      if (c === nx && !vow(c) && c !== "r") continue;                // (a doubled consonant: once)
      switch (c) {
        case "a": t.push([P.A, TS]); break;
        case "e": t.push([P.E, TS]); break;
        case "o": t.push([P.O, TS]); break;
        case "i": t.push([P.I, TW]); break;
        case "u": t.push([P.U, TW]); break;
        case "c":
          if (nx === "h") { t.push([P.K, TC]); i++; }
          else if (soft) { t.push([P.CH, TC]); if (nx === "i" && vow(nx2)) i++; }
          else t.push([P.K, TC]);
          break;
        case "g":
          if (nx === "h") { t.push([P.G, TC]); i++; }
          else if (nx === "l" && nx2 === "i") { t.push([P.Y, TC]); i += vow(w[i + 3] || "") ? 2 : 1; }
          else if (nx === "n") { t.push([P.NY, TC]); i++; }
          else if (soft) { t.push([P.J, TC]); if (nx === "i" && vow(nx2)) i++; }
          else t.push([P.G, TC]);
          break;
        case "s":
          if (nx === "c" && (nx2 === "e" || nx2 === "i")) { t.push([P.SH, TC]); i++; if (nx2 === "i" && vow(w[i + 2] || "")) i++; }
          else t.push([i > 0 && vow(w[i - 1]) && vow(nx) ? P.Z : P.S, TC]);
          break;
        case "q": t.push([P.K, TC]); break;
        case "z": t.push([P.TS, TC]); break;
        case "r": if (nx === "r") { t.push([P.RR, TC]); i++; } else t.push([P.RT, TC]); break;
        case "j": t.push([P.Y, TC]); break;
        case "h": break;
        default: {
          const m = { b: P.B, d: P.D, f: P.F, k: P.K, l: P.L, m: P.M, n: P.N, p: P.P, t: P.T, v: P.V, w: P.W, x: P.K, y: P.I }[c];
          if (m !== undefined) t.push([m, c === "y" ? TW : TC]);
        }
      }
    }
    return t;
  }

  /* ------------------------------------------------------------------------------- English (CMU) --- */
  let dict = null;
  function enDict() {
    if (dict) return dict;
    const src = typeof LY_EN_DICT === "string" ? LY_EN_DICT : null;
    if (!src) return null;
    dict = new Map();
    let prev = "";
    for (const ln of src.split("\n")) {
      const sp = ln.indexOf(" "), keep = parseInt(ln[0], 36), w = prev.slice(0, keep) + ln.slice(1, sp);
      dict.set(w, ln.slice(sp + 1));
      prev = w;
    }
    return dict;
  }

  /* -------------------------------------------------------------------------------- a word, a text --- */
  const strip = s => s.normalize("NFD").replace(/[̀-ͯ]/g, "");
  /* one word (letters; "_" joins) in a language -> {ph: [[phoneme, start]], src: "dict" | "rules"} */
  function word(lang, w) {
    const parts = String(w).split("_").filter(Boolean);
    if (parts.length > 1) {
      const ph = [], srcs = new Set();
      for (const p of parts) { const r = word(lang, p); ph.push(...r.ph); srcs.add(r.src); }
      return { ph: ph.slice(0, 32), src: srcs.has("rules") ? "rules" : [...srcs][0] };
    }
    const lw = String(w).toLowerCase();
    switch (lang) {
      case "EN": {
        const k = strip(lw).replace(/[^a-z]/g, ""), d = enDict(), code = d && d.get(k);
        if (code) return { ph: parse("EN", code), src: "dict" };
        return { ph: syl(enTok(k)), src: "rules" };
      }
      case "JA": return { ph: ja(strip(lw).replace(/[^a-z'-]/g, "")), src: "rules" };
      case "PT": return { ph: syl(ptTok(lw.replace(/[^a-záéíóúâêôãõàç]/g, ""))), src: "rules" };
      case "IT": return { ph: syl(itTok(lw.replace(/[^a-zàèéìòóù]/g, ""))), src: "rules" };
      default: {                                                     // ES: ñ as ny, ü said (güe, pingüino)
        const k = lw.replace(/ñ/g, "ny").replace(/ü/g, "\u0001").normalize("NFD").replace(/[̀-ͯ]/g, "")
          .replace(/[^a-z\u0001]/g, "").replace(/\u0001/g, "ü");
        return { ph: syl(esTok(k)), src: "rules" };
      }
    }
  }
  /* a typed lyric -> {text: "/..." (the device's), words: [{w, code, src}], len}; "-" alone holds a vowel (a melisma) */
  function convert(lang, s) {
    const toks = String(s ?? "").replace(/[’‘]/g, "'").split(/[^\p{L}_'\-]+/u).flatMap(t => t.split(/(?<=\S)(?=-)|(?<=-)(?=\S)/))
      .filter(t => t && t !== "'");
    const words = [];
    for (const t of toks) {
      if (/^-+$/.test(t)) { for (const _ of t) words.push({ w: "-", code: "-", src: "" }); continue; }
      const r = word(lang, t);
      if (!r.ph.length) continue;
      const code = text(lang, r.ph);
      if (code) words.push({ w: t, code, src: r.src });
    }
    const body = words.map(x => x.code).join(" ");
    return { text: body ? "/" + body : "", words, len: body ? body.length + 1 : 0 };
  }
  /* the letters as the device reads them (ES, EN's letter rules, JA; web/test_web.mjs checks) */
  const letters = (lang, w) => (lang === "JA" ? ja(w) : syl(lang === "EN" ? enTok(w) : esTok(w)));
  return { PH, NAME, CODE, CM, MASK, parse, text, auto, show, word, convert, enDict, sing, letters,
           langs: ["ES", "EN", "JA", "PT", "IT"] };
})();
if (typeof module !== "undefined") module.exports = LYR;
