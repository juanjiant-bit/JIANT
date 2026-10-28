// SPDX-License-Identifier: GPL-3.0-only
// Exercise the actual built emulator WASM through boot and physical-key input.
import fs from "node:fs";
import assert from "node:assert/strict";
import {encodeSampleBank,withSampleBank,hasSampleBank} from "./sample-bank.js";
const wasm=fs.readFileSync(process.argv[2]);
for(const resource of ["factory-pack3.json","factory-voices.json","factory-samples.json"]) {
const source=JSON.parse(fs.readFileSync(new URL("../"+resource,import.meta.url)));
const count=source.samples.length;
const bank=encodeSampleBank(source);
assert.equal(hasSampleBank({}),false);
const sectors=withSampleBank({},bank); assert.equal(hasSampleBank(sectors),true);
const {instance}=await WebAssembly.instantiate(wasm,{});
const ex=instance.exports;ex._initialize();ex.web_nor_erase();
const nor=new Uint8Array(ex.memory.buffer,ex.web_nor(),ex.web_nor_size());
for(const [off,bytes] of Object.entries(sectors))nor.set(bytes,+off);
ex.web_boot();assert.equal(ex.web_sample_count(),count);
function render(ms) {
  let peak=0;
  for(let n=Math.round(ms*44.1);n>0;n-=128) {
    const len=Math.min(128,n);ex.web_render(len);
    for(const v of new Float32Array(ex.memory.buffer,ex.web_out_l(),len))peak=Math.max(peak,Math.abs(v));
  }
  return peak;
}
render(4000);
assert.equal(ex.web_sample_kit(),0);
for(let slot=0;slot<count;slot++) {
  render(100);ex.web_keys(1<<(7+slot));
  const peak=render(150);
  assert.ok(peak>0.0001,"silent physical key for slot "+slot);
  ex.web_keys(0);render(800);
  console.log("emulator sample slot",slot+1,"peak",peak.toFixed(4));
}
console.log("PASS actual WASM: saved bank recognized, T1 KIT KEYS selected, all physical keys sound");


if(resource==="factory-pack3.json") {
  function btn(b){ex.web_buttons(1<<b);render(64);ex.web_buttons(0);render(440);}
  function key(k){ex.web_keys(1<<k);render(60);ex.web_keys(0);render(60);}
  btn(11);btn(10);
  for(let k=0;k<12;k++)key(7+k%4);btn(11);
  assert.ok(ex.web_playing());assert.ok(render(2900)>0.0001,"recorded sequence silent before SLOT sweep");
  for(let k=0;k<600;k++){ex.web_enc(3,k%12<6?1:-1);render(10);}
  btn(4);
  for(let k=0;k<600;k++){ex.web_enc(3,k%12<6?1:-1);render(10);}
  assert.ok(ex.web_playing());assert.ok(render(2900)>0.0001,"recorded sequence silent after SLOT sweep");
  console.log("PASS actual WASM: 1200 SLOT changes on HOME and EDIT with audible recorded sequence");
}
}
