"""Run the editor's real transliterator; never silently drop a native lyric."""
from pathlib import Path
import re,subprocess
s=Path('web/editor.html').read_text()
a=s.index('function lyricText(')
fn=s[a:s.index('\n}',a)+2]
js="import assert from 'node:assert/strict';\n"+fn+'''
assert.equal(lyricText('Привет мир','RU'),'Privet mir'.toLowerCase());
assert.equal(lyricText('щёлк','RU'),'shchyolk');
assert.equal(lyricText('мой сыр','RU'),'moj syr');
assert.equal(lyricText('größer','DE'),'groesser');
assert.equal(lyricText('nǐ hǎo lǜ','ZH'),'ni hao lv');
assert.equal(lyricText('coração','PT'),'corasa~o');
assert.equal(lyricText('pytũ ñ','GN'),'pytu~ ny');
assert.equal(lyricText('été chat','FR'),'eitei chat');
assert.throws(()=>lyricText('你好','ZH'),/pinyin/);
assert.throws(()=>lyricText('щ'.repeat(16),'RU'),/63/);
assert.equal(lyricText('niño corazón'),'ninyo corazon');
assert.equal(lyricText('a'.repeat(100)).length,63); // legacy protocol bound
assert.equal(lyricText('a'.repeat(63),'EN').length,63);
console.log('voice web: transliteration, nasal/umlaut markers, UTF rejection and expanded-length bound passed');
'''
# (V10.7.33) the device has no English dictionary: the editor's phonemes (web/lyrics.js, web/lyrics_en.js), every
# language it converts, the device's 63-character bound kept, "-" melismas kept, unknown English words by the rules
js += """
import vm from 'node:vm';
import { readFileSync } from 'node:fs';
const ctx = {}; vm.createContext(ctx);
vm.runInContext(readFileSync('web/lyrics_en.js', 'utf8') + ';globalThis.LY_EN_DICT = LY_EN_DICT;', ctx);
vm.runInContext(readFileSync('web/lyrics.js', 'utf8') + ';globalThis.LYR = LYR;', ctx);
const L = ctx.LYR;
assert.equal(L.convert('EN', 'Hello darkness, my old friend').text, '/HAXLOUH DAARKNAXS MAAIH OUHLD FREHND');
assert.equal(L.convert('EN', "don't - stop").text, '/DOUHNT - STAAP');
assert.equal(L.convert('EN', 'zorblat').words.map(w => w.src).join(), 'rules');
assert.equal(L.convert('ES', 'niño corazón').text, '/NINYO KORASON');
assert.equal(L.convert('JA', 'konnichiwa').text, '/KONNNICHIWA');
assert.equal(L.convert('PT', 'não').text, '/NANNU.');
assert.equal(L.convert('IT', 'ciao bella').text, '/CHAO BELA');
for (const lang of L.langs) {
  const r = L.convert(lang, 'la la la la la la la la la la la la la la la la la la la la la la la');
  assert.ok(r.text.startsWith('/') && r.len === r.text.length, lang);
}
assert.ok(L.convert('EN', 'a '.repeat(40)).len > 63);   // (the editor refuses it: lyricPhText's bound)
console.log('voice web: phonemes (EN through the CMU dictionary, ES JA PT IT by rules), melismas, the length passed');
"""
out=Path('build/song-check');out.mkdir(parents=True,exist_ok=True)
(out/'voice-web.mjs').write_text(js)
subprocess.run(['node',str(out/'voice-web.mjs')],check=True)
