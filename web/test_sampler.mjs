// SPDX-License-Identifier: GPL-3.0-only
import fs from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
const html=fs.readFileSync(new URL('editor.html',import.meta.url),'utf8');
const E=vm.runInNewContext(html.slice(html.indexOf('/*PROTO-BEGIN*/'),html.indexOf('/*PROTO-END*/'))+';({smpBank,openEditorMidi,makeMockDevice,parse,req,CMD,Link,frame,unframe})', {setTimeout,clearTimeout,setInterval,clearInterval,console});
const stock=JSON.parse(fs.readFileSync(new URL('factory-samples.json',import.meta.url),'utf8'));
const built=E.smpBank.build(stock.samples.map(s=>({...s,pcm:Int16Array.from(s.pcm)})));
assert.equal(built.count,12);assert.ok(built.used<=6144);
assert.equal(256+stock.samples.reduce((n,s)=>n+Math.ceil(s.pcm.length/2),0),built.used);
assert.equal(new Set(stock.samples.map(s=>s.name)).size,12);
for(const s of stock.samples) {assert.ok(Math.max(...s.pcm)>0);assert.ok(Math.min(...s.pcm)<0);}
fs.mkdirSync('build/host',{recursive:true});fs.writeFileSync('build/host/factory-samples.bin',built.bytes);
const mock=E.makeMockDevice({auto:false});
const input=[...mock.access.inputs.values()][0],output=[...mock.access.outputs.values()][0];
input.name=output.name='M-VAVE FM-1';let inputOpen=false,outputOpen=false;
input.open=async()=>{inputOpen=true};output.open=async()=>{outputOpen=true};
input.close=async()=>{inputOpen=false};output.close=async()=>{outputOpen=false};
const send=output.send;output.send=d=>{assert.ok(inputOpen && outputOpen,'both MIDI ports opened before INFO');send(d)};
const dead={id:'old',name:'JIANT OLD',state:'connected',open:async()=>{throw Error('port busy')},close:async()=>{}};
mock.access.inputs=new Map([[dead.id,dead],[input.id,input]]);
const d=await E.openEditorMidi(mock.access);assert.equal(d.input,input);assert.equal(d.output,output);
assert.match(d.info.version,/JIANT|FELUCCA/);
assert.equal(await E.smpBank.send((r,o)=>d.link.request(r,o),built.bytes),0);
const list=await E.smpBank.list((r,o)=>d.link.request(r,o));assert.equal(list.slots.filter(s=>s.used).length,12);
assert.equal(list.slots[0].name,'KICK');assert.equal(list.slots[11].name,'CHORD');d.link.close();
input.name=output.name='USB MIDI';
const manual=await E.openEditorMidi(mock.access,{}, {input:input.id,output:output.id});assert.equal(manual.input,input);manual.link.close();
let calls=0;const gone=new E.Link(()=>{calls++;gone.close()});
await assert.rejects(()=>E.smpBank.send((r,o)=>gone.request(r,o),built.bytes),/closed/);assert.equal(calls,1);
for(const rate of [2000,3000,4000,6000,8000,11025,16000]) {
 const x=Float32Array.from({length:800},(_,i)=>Math.sin(2*Math.PI*200*i/8000));
 const y=E.smpBank.resample(x,8000,rate);assert.equal(y.length,Math.ceil(800*rate/8000));assert.ok([...y].every(Number.isFinite));
}
console.log(`sampler: stock ${built.count} slots, ${built.used}/6144 B; port fallback/open, session cancellation, send/list CRC and low-rate conversion PASS`);
