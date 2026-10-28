import assert from 'node:assert/strict';
import fs from 'node:fs';
import {ensureStockSamples,sampleList,sampleInfo} from './fm1samples.js';
import {encodeSampleBank} from './emu/sample-bank.js';
import {bkCrc,bkU32} from './fm1backup.js';
const bank=encodeSampleBank(JSON.parse(fs.readFileSync(new URL('./factory-pack3.json',import.meta.url))));
const str=s=>[...s].map(c=>c.charCodeAt(0)).concat(0);
const info=(engine='SAMPLE')=>[...str('JIANT v0.9.9-par14-beta4'),14,100,30,64,90,...Array.from({length:14},(_,i)=>str(i===13?engine:'ENGINE')).flat()];
const list=n=>[15,96,...Array.from({length:15},(_,i)=>[i<n?1:0,...str(i<n?'SAMPLE':''),16,0,0,80]).flat()];
assert.equal(sampleInfo(info()).sample,true);
assert.equal(sampleInfo(info('ADD')).sample,false);
assert.equal(sampleList(list(9)).filter(s=>s.used).length,9);
async function device({occupied=0,engine='SAMPLE',badBegin=false,brokenList=false,wrongCount=false}={}){
 const writes=[],calls=[];let ended=false;
 const result=await ensureStockSamples(async ([cmd,a])=>{
  calls.push(cmd);
  if(cmd===1)return info(engine);
  if(cmd===81)return brokenList?[15,96]:list(ended?(wrongCount?8:9):occupied);
  if(cmd===78)return [badBegin?1:0];
  if(cmd===79){const off=a[0]|a[1]<<7|a[2]<<14;assert.equal(off,writes.length*64);assert.equal(a.length,131);writes.push(Uint8Array.from({length:64},(_,i)=>a[3+i*2]|a[4+i*2]<<4));return [0];}
  if(cmd===80){assert.deepEqual(a,bkU32(bkCrc(bank)));ended=true;return [0,9];}
  throw Error('Unexpected command '+cmd);
 },bank);
 return {result,writes,calls};
}
let d=await device();assert.deepEqual(d.result,{status:'loaded',count:9});assert.equal(d.writes.length,96);assert.deepEqual(Uint8Array.from(d.writes.flatMap(x=>[...x])),bank);
d=await device({occupied:1});assert.equal(d.result.status,'preserved');assert.deepEqual(d.calls,[1,81]);
d=await device({engine:'ADD'});assert.equal(d.result.status,'unsupported');assert.deepEqual(d.calls,[1]);
await assert.rejects(device({brokenList:true}),/Invalid|Incomplete/);
await assert.rejects(device({badBegin:true}),/refused/);
await assert.rejects(device({wrongCount:true}),/verification/);
console.log('PASS stock samples: nine-slot bank, 96 verified chunks, occupied bank preserved, ADD skipped, malformed/refused/failed verification rejected');
