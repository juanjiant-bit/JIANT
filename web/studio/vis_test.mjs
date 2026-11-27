/* VIS's recipe against docs/VISGEN.md's vectors (the JIANT-BYTE repository's, made by its tests/visgen_test.c) */
globalThis.document = { createElement: () => ({ getContext: () => ({}) }) };
const { recipe, VI_NAMES, VD_NAMES } = await import("./visgen.js");
const V = [
  [421, 4, 70, 0x11, 0, "ORBIT", 1, 2, 2, 2, 67, 86, 1, ["LIFE - MIRROR 78", "KEY - MIRROR 78", "LIFE - MIRROR 78", "RADAR - MIRROR 78"]],
  [421, 4, 70, 0x11, 1, "RADAR", 1, 2, 2, 1, 70, 86, 5, ["KEY - TWIST 46", "LIFE - TWIST 46", "ORBIT - TWIST 46", "ORBIT - TWIST 46 SCAN 69"]],
  [422, 4, 70, 0x11, 0, "SPLASH", 1, 2, 2, 1, 75, 86, 1, ["KEY - POLAR 86", "KEY RADAR POLAR 86", "KEY ORBIT POLAR 86", "KEY HARMONY POLAR 86"]],
  [0, 27, 50, 0x00, 0, "HARMONY", 1, 1, 2, 2, 62, 74, 3, ["KEY - TWIST 72", "LFO - TWIST 72", "LIFE - TWIST 72", "KEY - TWIST 72"]],
  [9999, 0, 95, 0x90, 0, "LFO", 0, 2, 0, 1, 94, 100, 0, ["TUNNEL CHORUS SCAN 47 TWIST 61", "RADAR CHORUS SCAN 47 TWIST 61", "TUNNEL CHORUS SCAN 47 TWIST 61", "RADAR CHORUS SCAN 47 TWIST 61"]],
  [1234, 13, 85, 0x15, 0, "KEY", 3, 2, 0, 4, 76, 95, 0, ["RADAR HARMONY FOLD 85 TWIST 47", "RADAR HARMONY TWIST 47 POLAR 53", "RADAR ORBIT TWIST 47 POLAR 53", "LIFE ORBIT TWIST 47 POLAR 53"]],
  [77, 22, 30, 0xA0, 0, "LIFE", 4, 0, 1, 2, 72, 62, 4, ["CHORUS HIVE MELT 55", "KEY HIVE MELT 55", "CHORUS HIVE MELT 55", "CHORUS TUNNEL MELT 55"]],
  [5000, 7, 65, 0x30, 3, "RADAR", 2, 1, 1, 3, 66, 83, 3, ["HIVE LFO MIRROR 71", "LIFE LFO MIRROR 71", "LIFE HIVE MIRROR 71", "LFO HIVE MIRROR 71"]],
  [1, 26, 40, 0x48, 0, "CLAY", 5, 1, 0, 1, 91, 68, 6, ["LFO - SCAN 49", "LFO - SCAN 49 POLAR 91", "LIFE - SCAN 49 POLAR 91", "LIFE HARMONY SCAN 49 POLAR 91"]],
  [2097151, 18, 10, 0xFF, 255, "MATH", 4, 0, 2, 2, 78, 50, 5, ["HIVE FUNC MELT 81", "ORBIT FUNC MELT 81", "KEY FUNC MELT 81", "CHORUS FUNC MELT 81"]],
  [600, 11, 72, 0x5B, 2, "KEY", 3, 2, 0, 4, 93, 87, 5, ["MATH - FOLD 88 MELT 90", "MATH - MELT 90 SCAN 91", "MATH RADAR MELT 90 SCAN 91", "MATH ORBIT MELT 90 SCAN 91"]],
  [31337, 24, 55, 0x31, 0, "RADAR", 2, 1, 1, 1, 71, 77, 5, ["HIVE ORBIT MIRROR 71", "LFO ORBIT MIRROR 71", "HIVE ORBIT MIRROR 71", "HIVE KEY MIRROR 71"]],
];
let failed = 0;
for (const [seed, g, e, m, rec, content, fam, mood, blend, pal, mix, react, lis, secs] of V) {
  const R = recipe(seed, g, e, m, rec);
  const got = [0, 1, 2, 3].map((s) => [VI_NAMES[R.skel[s]], R.over[s] === 255 ? "-" : VI_NAMES[R.over[s]], ...[0, 1].filter((i) => R.def[s][i] !== 255).map((i) => `${VD_NAMES[R.def[s][i]]} ${R.amt[s][i]}`)].join(" "));
  const ok = VI_NAMES[R.content] === content && R.fam === fam && R.mood === mood && R.blend === blend && R.pal === pal && R.mix === mix && R.react === react && R.lis === lis && got.every((x, i) => x === secs[i]);
  if (!ok) { failed++; console.log("FAIL", seed, g, e, m, rec, VI_NAMES[R.content], R.fam, R.mood, R.blend, R.pal, R.mix, R.react, R.lis, got); }
}
console.log(failed ? `VIS TESTS FAILED (${failed})` : `vis: the recipe matches VISGEN.md's ${V.length} vectors`); process.exit(failed ? 1 : 0);
