// SPDX-License-Identifier: GPL-3.0-only
// Stock samples are web data, not part of the firmware image.
import {BackupConnection, bkCrc, bkU32} from './fm1backup.js';
export function sampleInfo(a) {
  let at=a.indexOf(0); if(at<0) throw new Error('Invalid firmware INFO');
  const version=String.fromCharCode(...a.slice(0,at));at++;
  const n=a[at];at+=5;const engines=[];
  if(!Number.isInteger(n)||n>32)throw new Error('Invalid engine list');
  for(let i=0;i<n;i++){const end=a.indexOf(0,at);if(end<at)throw new Error('Incomplete engine list');engines.push(String.fromCharCode(...a.slice(at,end)));at=end+1;}
  return {version,sample:/^(JIANT|FELUCCA) /i.test(version)&&engines[13]==='SAMPLE'};
}
export function sampleList(a) {
  let p=2;const slots=[];
  if(a[0]!==15||a[1]*64!==6144)throw new Error('Unexpected sample bank response');
  for(let k=0;k<15;k++) {
    const used=a[p++];if(used!==0&&used!==1)throw new Error('Invalid sample slot');
    const end=a.indexOf(0,p);if(end<0||end-p>9)throw new Error('Invalid sample name');
    const name=String.fromCharCode(...a.slice(p,end));p=end+1;
    if(p+4>a.length)throw new Error('Incomplete sample list');
    const n=a[p]|a[p+1]<<7|a[p+2]<<14,rate=a[p+3]*100;p+=4;slots.push({used:!!used,name,n,rate});
  }
  if(p!==a.length)throw new Error('Trailing sample list data');return slots;
}
export async function ensureStockSamples(request, bytes) {
  if(!(bytes instanceof Uint8Array)||bytes.length!==6144)throw new Error('Invalid stock sample bank');
  const info=sampleInfo(await request([1,[]],{timeout:1500,retries:1}));
  if(!info.sample)return {status:'unsupported'};
  if(sampleList(await request([81,[]],{timeout:2000,retries:1})).some(s=>s.used))return {status:'preserved'};
  const check=(a)=>{if(a.length<1||a[0]!==0)throw new Error('Sample transfer refused: '+(a[0]??'no reply'));};
  check(await request([78,[]],{timeout:3000,retries:0}));
  for(let off=0;off<bytes.length;off+=64) {
    const nib=[...bytes.subarray(off,off+64)].flatMap(x=>[x&15,x>>4]);
    check(await request([79,[off&127,(off>>7)&127,(off>>14)&127,...nib]],{timeout:2000,retries:0}));
  }
  const end=await request([80,bkU32(bkCrc(bytes))],{timeout:8000,retries:0});check(end);
  const slots=sampleList(await request([81,[]],{timeout:2000,retries:1}));
  if(slots.filter(s=>s.used).length!==new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength).getUint16(6,true))throw new Error('Sample verification failed');
  return {status:'loaded',count:slots.filter(s=>s.used).length};
}
export async function provisionStockSamples(up, bytes) {
  const ports=up.installedPorts;
  if(!ports?.input||!ports?.output)throw new Error('The installed FM-1 MIDI port is unavailable');
  const connection=new BackupConnection(ports.input,ports.output);
  try{return await ensureStockSamples((r,o)=>connection.request(r,o),bytes);}finally{connection.close();}
}
