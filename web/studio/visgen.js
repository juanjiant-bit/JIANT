/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* JIANT Studio's VIS: JIANT BYTE's visual generator (the JIANT-BYTE repository's docs/VISGEN.md) drawn big, from BYTE over
 * USB (VIS_SONG 98, VIS_TICK 99), the web emulator (jiantEmu.vis(), or for JIANT FM its generator and stems) or any MIDI
 * device. recipe() is vis_recipe.c to the bit (vis_test.mjs checks VISGEN.md's vectors); the drawing is the approved
 * mock-up's (docs/visgen-mock.html: the ingredients, the skeletons, the deformations, the heat cells), fed by Live,
 * the state any source fills. */
// @ts-nocheck
const TAU=Math.PI*2;
const NN=['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];
function rng(seed){let a=seed>>>0||1;return()=>{a=a+0x6D2B79F5|0;let t=Math.imul(a^a>>>15,1|a);t=t+Math.imul(t^t>>>7,61|t)^t;return((t^t>>>14)>>>0)/4294967296}}
function hash(...v){let h=2166136261;for(const x of v){h^=x>>>0;h=Math.imul(h,16777619);h^=h>>>13}return h>>>0}
const hh=(i,s)=>((Math.sin(i*12.9898+s*78.233)*43758.5453)%1+1)%1;
const pickW=(r,it)=>{let s=0;for(const e of it)s+=e[1];let x=r()*s;for(const e of it){if((x-=e[1])<0)return e[0]}return it[it.length-1][0]};
const clamp=(v,a,b)=>v<a?a:v>b?b:v,lerp=(a,b,t)=>a+(b-a)*t,wrap=v=>((v+1)%2+2)%2-1;

const MODES={min:[0,2,3,5,7,8,10],dor:[0,2,3,5,7,9,10],phr:[0,1,3,5,7,8,10],lyd:[0,2,4,6,7,9,11],maj:[0,2,4,5,7,9,11],harm:[0,2,3,5,7,8,11]};
/* ---------- paletas ---------- */
function mkLut(st){const out=[];for(let i=0;i<256;i++){const v=i/255,f=v*(st.length-1),k=Math.min(st.length-2,f|0),u=f-k,a=st[k],b=st[k+1];
 let c=a.map((x,j)=>x+(b[j]-x)*u);if(v<.12)c=c.map(x=>x*v/.12);out.push(`rgb(${c.map(Math.round).join()})`)}return out}
const RAMP={HR:[[57,40,172],[131,89,255],[255,64,74],[255,117,24],[255,190,90],[255,242,205],[255,255,255]],
 MAG:[[40,10,70],[164,93,255],[255,56,180],[255,137,41],[255,230,160],[255,255,255]],
 ICE:[[10,20,90],[41,60,255],[58,208,255],[160,240,255],[255,255,255]],
 AMB:[[60,15,0],[160,50,0],[255,120,20],[255,200,60],[255,245,190],[255,255,255]],
 TRK:[[57,40,172],[164,93,255],[255,56,180],[255,56,57],[255,137,41],[255,210,57],[255,255,255]]};
const LUT={};for(const k in RAMP)LUT[k]=mkLut(RAMP[k]);
const HUD={wire:'#3a54ff',dim:'#1d2440',wh:'#e8ecf4',ye:'#eef25a',or:'#ff8929',mg:'#ff38b4',red:'#ff3839',cy:'#3ad0ff',vi:'#a45dff',gr:'#7cff9a'};
const PALS=[{lut:'HR',alt:'MAG',col:HUD},{lut:'ICE',alt:'HR',col:HUD},{lut:'TRK',alt:'HR',col:{...HUD,wire:'#398dff'}},
 {lut:'ICE',alt:'MAG',col:{wire:'#293cff',dim:'#141c44',wh:'#f2fbff',ye:'#3ad0ff',or:'#9fe8ff',mg:'#a45dff',red:'#5d7bff',cy:'#e8ecf4',vi:'#7a8cff',gr:'#3ad0ff'}},
 {lut:'AMB',alt:'HR',col:{wire:'#8a3a10',dim:'#2a1406',wh:'#fff3d0',ye:'#ffd239',or:'#ff8929',mg:'#ff5a2a',red:'#ff3839',cy:'#ffe7a0',vi:'#ff8929',gr:'#ffd239'}}];
const ROLES=['ye','or','mg','red','cy','vi','gr'],TRKC=['#ffd239','#ff8929','#ff3839','#ff38b4','#a45dff','#398dff'];
function colOf(c,P,rot,lut){if(typeof c==='number')return lut[clamp(Math.round(c/16)*16,0,255)];if(typeof c==='object')return TRKC[(c.trk+rot)%6];
 const i=ROLES.indexOf(c);if(i>=0&&rot)c=ROLES[(i+rot)%ROLES.length];return P.col[c]}
const intOf=c=>typeof c==='number'?c/256:typeof c==='object'?1:({wire:.5,dim:.2,wh:.85})[c]??1;

/* ---------- ingredientes: geometría en un plano [-1,1] (y arriba) ---------- */
const L=(o,p,col,a=1,w=1)=>o.s.push({p,col,a,w}),Ce=(o,x,y,sz,col,a=1)=>o.c.push({x,y,sz,col,a}),Gl=(o,x,y,t,col)=>o.g.push({x,y,t,col});
function lfo(sh,u){u=((u%1)+1)%1;switch(sh){case 0:return Math.sin(u*TAU);case 1:return 1-4*Math.abs(u-.5);case 2:return 1-2*u;case 3:return u<.5?1:-1;default:return hh(Math.floor(u*8),7)*2-1}}
const ING={
 LIFE(C,o){const S=C.S;for(let i=0;i<8;i++){const x=-1+i*.25;L(o,[x,-.97,x,.97],'wire',.75)}for(const y of[-.66,-.33,0,.33,.66])L(o,[-1,y,1,y],'wire',.75);
  const P=[];for(let i=0;i<6;i++){const x=wrap(hh(i,C.R.seed)*2-1+C.t*(.03+.02*(i%3))),y=(hh(i+7,C.R.seed)-.5)*1.4;P.push([x,y]);Ce(o,x,y,4+S.pads[i]*7*C.react,{trk:i})}
  for(let i=0;i<6;i++){const a=P[i],b=P[(i+1)%6];if(Math.abs(a[0]-b[0])<.9)L(o,[a[0],a[1],b[0],b[1]],'ye',.7)}},
 KEY(C,o){const S=C.S;L(o,[-1,0,1,0],'wh',.9);L(o,[-1,.5,1,.5],'wire',.5);L(o,[-1,-.5,1,-.5],'wire',.5);
  for(let i=0;i<7;i++){const n=(S.root+S.mode[i])%12,x=-1+(i+.5)*2/7,on=i===S.chordDeg;Ce(o,x,0,on?11+S.kick*5*C.react:8,on?'ye':40+n*200/11);Gl(o,x,0,NN[n],on?'#eef25a':'#e8ecf4')}},
 RADAR(C,o){const S=C.S;for(let k=0;k<8;k++){const x=-1+k*.25;L(o,[x,-1,x,1],'wire',.35)}L(o,[-1,-.5,1,-.5],'wire',.35);L(o,[-1,.3,1,.3],'wire',.3);
  const p=[];for(let k=0;k<8;k++){const e=S.pads[k];for(let j=0;j<4;j++){const u=j/4;p.push(-1+(k+u)*.25,(.18+.05*Math.sin(C.t*3+k)+e*C.react*.85*Math.sin(u*Math.PI)**.6)*2-1)}}p.push(1,p[1]);L(o,p,'wh',1,1.5);
  for(const r of S.rings){const age=(S.time-r)/1000,rr=1-age/1.1;if(rr>0)L(o,[-1,rr*2-1,1,rr*2-1],250-age*170,1-age*.6,1.5)}
  for(let k=0;k<8;k++){const e=S.pads[k];Ce(o,-1+(k+.5)*.25,.9,3+e*6,e>.3?'ye':'dim')}},
 ORBIT(C,o){const S=C.S,[a,b]=C.R.lis,ph=C.t*.4+S.bass*C.react,A=.82*(.85+.25*S.level*C.react);
  for(const[col,dp,sc]of[['red',0,1],['wire',Math.PI/2,.9]]){const p=[];for(let i=0;i<=160;i++){const q=i/160*TAU;p.push(Math.sin(a*q+ph+dp)*A*sc,Math.sin(b*q)*A*sc)}L(o,p,col,1,1.5)}},
 TUNNEL(C,o){const S=C.S,cs=['red','ye','wire'];for(let k=0;k<4;k++){const z=(k/4+C.t*.12*(1+S.level))%1;L(o,[-1,z*2-1,1,z*2-1],cs[k%3],(1-z)*.75,1.5)}
  for(const e of S.echoes){const age=(S.time-e)/1000,z=age/1.6;if(z<1)L(o,[-1,z*2-1,1,z*2-1],cs[(e/97|0)%3],(1-z)*C.react+.1,2)}},
 CHORUS(C,o){const S=C.S,t=C.t;[['red',1.5,0],['wire',2,2.1],['wh',2.5,4.2]].forEach(([c,k,p0])=>{const p=[];for(let i=0;i<=60;i++){const x=-.05+i/60*1.05;p.push(x,.45*Math.sin(Math.PI*x*k+t*1.6+p0)*(.7+.4*S.level*C.react))}L(o,p,c,.9,1.2)});
  const p=[];for(let i=0;i<=80;i++){const q=i/80*TAU;p.push(-.55+.38*Math.sin(q*2+t*.9),.6*Math.sin(q*3))}L(o,p,'red',.9,1.2)},
 HIVE(C,o){const S=C.S;[.6,.2,-.2,-.6].forEach((y0,i)=>{const e=S.hive[i],pk=-.4+hh(i,C.R.seed)*.8,p=[];for(let j=0;j<=40;j++){const x=-1+j/20,tri=x<pk?(x+1)/(pk+1):(1-x)/(1-pk);
  p.push(x,y0+e*.22*C.react*tri*Math.sin(C.t*31+i*2))}L(o,p,'wh',.5+e*.5,1)})},
 CLAY(C,o){const S=C.S,R=C.R;[.62,.2,-.22,-.64].forEach((y0,i)=>{L(o,[-1,y0,1,y0],'dim',.6);const p=[],sp=S.beat*(i+1)*.5;
  for(let j=0;j<=60;j++){const x=-1+j/30,u=(x+1)*3+sp,k=Math.floor(u),f=u-k,v0=R.clay[i][k&15],v1=R.clay[i][(k+1)&15],fr=i===1?f:Math.min(1,f*6);p.push(x,y0+(v0+(v1-v0)*fr)*.16*(1+S.bass*C.react*.6))}
  L(o,p,i===1?'or':'red',1,1.5);Ce(o,-.95,y0+.1,6,i===1?'ye':'wire')})},
 LFO(C,o){const S=C.S;L(o,[-1,0,1,0],'dim',.6);const p=[];for(let j=0;j<=80;j++){const x=-1+j/40;p.push(x,.55*lfo(C.R.lfoShape,(x+1)*1.25-S.beat*.25)*(.6+.5*S.bass*C.react))}L(o,p,'ye',1,1.5)},
 HARM(C,o){const S=C.S;for(let i=0;i<4;i++){const cx=-.72+i*.48,on=i===S.chord,w=.19,h=.3;L(o,[cx-w,-h,cx+w,-h,cx+w,h,cx-w,h,cx-w,-h],on?'ye':'wire',on?1:.5,on?2:1);
  if(on)for(let k=-2;k<=2;k++)L(o,[cx-w*.7,k*.1,cx+w*.7,k*.1],200,.35+.5*S.kick*C.react);Gl(o,cx,-.02,S.chordNames[i],on?'#eef25a':'#9aa3b5')}},
 PART(C,o){const S=C.S,cur=S.step%16;[[.55,'mg'],[0,'cy'],[-.55,'gr']].forEach(([y0,c],r)=>{L(o,[-1,y0-.18,1,y0-.18],'dim',.4);
  for(let i=0;i<16;i++){const age=(cur-i+16)%16,a=Math.max(.15,1-age/16),x=-1+(i+.5)/8;let hit=0,yy=y0;
   if(r===0)hit=S.drumAt(i);else if(r===1){hit=S.bassM[i];yy=y0+S.bassN[i]*.025}else{hit=S.arpM[i];yy=y0+S.arpN[i]*.04}
   if(hit)Ce(o,x,yy,i===cur?9:6,c,a)}})},
 MATH(C,o){const S=C.S,R=C.R,fl=S.bar*7+(S.step>>2);for(let j=0;j<8;j++)for(let i=0;i<4;i++){let b=(R.being[j]>>i)&1;if(((fl*13+j*5+i*3)&7)===0&&S.kick>.3)b^=1;
  for(const ii of[i,7-i]){const x=-.6+ii*.17,y=.6-j*.17;if(b)Ce(o,x,y,8+S.kick*C.react*3,(j===2&&i===1)?'wh':'vi');else Ce(o,x,y,2,'dim',.6)}}},
 SIG(C,o){const S=C.S,R=C.R,N=90,prog=Math.min(1,(S.time-S.secT)/(S.barMs*2)),n=Math.max(2,prog*N|0);let px=null,py=0;
  for(let i=0;i<n;i++){const s=i/N,x=-.85+1.7*s+.11*Math.sin(s*TAU*R.sig[0]+R.sig[1]),y=.32*Math.sin(s*TAU*R.sig[2]+R.sig[3])+.16*Math.cos(s*TAU*R.sig[0]+R.sig[1]);
   if(px!==null)L(o,[px,py,x,y],24+i*220/N|0,1,2.2);px=x;py=y}if(prog<1)Ce(o,px,py,5,'wh')}};

/* ---------- esqueletos ---------- */
function skelOf(ing,content){if(content==='TUNNEL'&&CLOSED.has(ing))return{t:'TUN',shape:ing};
 return{LIFE:{t:'ESF',tilt:.45,spd:.45,R:.8},KEY:{t:'ESF',tilt:.3,spd:.22,R:.78},RADAR:{t:'POL'},ORBIT:{t:'LIS'},TUNNEL:{t:'TUN',shape:'RECT'},
  CHORUS:{t:'OND',w:'CHO'},LFO:{t:'OND',w:'LFO'},HIVE:{t:'CUE'},CLAY:{t:'ESC'},MATH:{t:'CEL'}}[ing]}
const nativeOf=ing=>['LIFE','KEY','RADAR','TUNNEL'].includes(ing)?skelOf(ing,''):{t:'FLAT'};
function shape(sh,s,C){const q=s*TAU;if(sh==='RECT'){const u=s*4,k=u|0,f=u-k;return[[-1+2*f,1],[1,1-2*f],[1-2*f,-1],[-1,-1+2*f]][k&3]}
 if(sh==='ORBIT'){const[a,b]=C.R.lis;return[Math.sin(a*q+C.t*.3),Math.sin(b*q)]}
 if(sh==='RADAR'){const r=.7+.3*C.S.pads[(s*8|0)&7]*C.react;return[Math.sin(q)*r,Math.cos(q)*r]}
 return[Math.cos(q),Math.sin(q)*(.6+.4*Math.sin(C.t*.7))]}
function carrier(sp,x,y,C){switch(sp.t){
 case'ESF':{const lon=x*Math.PI+C.t*sp.spd,lat=y*1.5,cl=Math.cos(lat),cx=cl*Math.sin(lon),cy=Math.sin(lat),cz=cl*Math.cos(lon),ct=Math.cos(sp.tilt),st=Math.sin(sp.tilt),R=sp.R*C.sc;
  return[cx*R,(cy*ct-cz*st)*R,(cy*st+cz*ct+1)/2]}
 case'POL':{const a=(x+1)*Math.PI+C.t*.05,r=(y+1)/2*.92*C.sc;return[Math.sin(a)*r,Math.cos(a)*r,1]}
 case'LIS':{const[a,b]=C.R.lis,q=(x+1)/2*TAU,ph=C.t*.25+C.S.bass*.6*C.react,dx=a*Math.cos(a*q+ph),dy=b*Math.cos(b*q),l=Math.hypot(dx,dy)||1,A=.78*C.sc;
  return[Math.sin(a*q+ph)*A*C.asp*.95-dy/l*y*.16,Math.sin(b*q)*A+dx/l*y*.16,1]}
 case'TUN':{const z=clamp((y+1)/2,0,1),k=1/(1+z*2.4),[sx,sy]=shape(sp.shape,(((x+1)/2)%1+1)%1,C),dr=1-k;
  return[(sx*.92*C.asp*k+Math.sin(C.t*.3)*.2*dr)*C.sc,(sy*.9*k+Math.cos(C.t*.23)*.12*dr)*C.sc,k]}
 default:return[x*C.asp*.92,y*.9,1]}}
function apply(sp,x,y,C,nat){if(sp.t==='OND'||sp.t==='CUE'||sp.t==='ESC'||sp.t==='CEL'){const p=carrier(nat,x,y,C),X=p[0]/C.asp,S=C.S,t=C.t;
  if(sp.t==='OND'&&sp.w==='CHO')p[1]+=.16*(Math.sin(X*5+t*1.7)+Math.sin(X*7.3-t*1.1)+Math.sin(X*3.1+t*.6))/3*(1+S.level*C.react*1.5);
  else if(sp.t==='OND')p[1]+=.24*lfo(C.R.lfoShape,(X+1)*1.2-S.beat*.25)*(.6+.6*S.bass*C.react);
  else if(sp.t==='CUE'){const i=clamp(Math.floor((p[1]+1)*2),0,3);p[1]+=(.05+S.hive[i]*.3*C.react)*Math.sin((X+1)/2*Math.PI)*Math.sin(t*22+i)}
  else if(sp.t==='ESC'){const u=(X+1)*4+S.beat*.5,k=Math.floor(u),f=Math.min(1,(u-k)*6),v=lerp(C.R.clay[0][k&15],C.R.clay[0][(k+1)&15],f);p[1]+=v*.2*(1+S.bass*C.react*.5)}
  else{p[0]=Math.round(p[0]*7)/7;p[1]=Math.round(p[1]*7)/7}
  return p}return carrier(sp,x,y,C)}
function deform(p,D,C){const tw=D.TWIST||0,fo=D.FOLD||0,po=D.POLAR||0;if(!(tw>.02||fo>.1||po>.02))return p;
 if(fo>.1){const f=lerp(1.1,.28,Math.min(1,fo))+.08*Math.sin(C.t*.4)+C.S.kick*C.react*.06,fy=f*(.9+.2*Math.sin(C.t*.27));p=[p[0],p[1],p[2]];
  for(let k=0;k<3;k++){if(p[0]>f)p[0]=2*f-p[0];else if(p[0]<-f)p[0]=-2*f-p[0];if(fo>.55){if(p[1]>fy)p[1]=2*fy-p[1];else if(p[1]<-fy)p[1]=-2*fy-p[1]}}
  if(!(tw>.02||po>.02))return p}
 let r=Math.hypot(p[0],p[1]),a=Math.atan2(p[1],p[0]);
 if(tw>.02)a+=tw*(1.8*(1-r)*Math.sin(C.t*.37)+C.S.kick*C.react*.6*(1-r));
 if(po>.02){r=Math.pow(Math.min(r,1.4),1-.45*po*Math.sin(C.t*.5));a+=po*.3*Math.sin(r*9-C.t*2)}
 return[r*Math.cos(a),r*Math.sin(a),p[2]]}

/* ---------- el motor ---------- */
const mk=(w,h)=>{const c=document.createElement('canvas');c.width=w;c.height=h;return c};
class Vis{
 constructor(W,H){this.W=W;this.H=H;this.out=mk(W,H);this.o=this.out.getContext('2d');this.comp=mk(W,H);this.c=this.comp.getContext('2d');
  this.cols=W/6|0;this.rows=H/6|0;this.sm=mk(this.cols,this.rows);this.s=this.sm.getContext('2d',{willReadFrequently:true});this.heat=new Float32Array(this.cols*this.rows);
  this.c.fillStyle='#000';this.c.fillRect(0,0,W,H);this.sec=0;this.prev=0;this.mT=-9}
 set(song,rec){this.song=song;this.rec=rec;this.heat.fill(0);this.sec=0;this.prev=0;this.mT=-9;this.c.fillStyle='#000';this.c.fillRect(0,0,this.W,this.H)}
 frame(now,K={}){const S=this.song.update(now),R=this.rec,t=S.time/1000,W=this.W,H=this.H;
  if(S.sec!==this.sec){this.prev=this.sec;this.sec=S.sec;this.mT=t}
  const e0=clamp((t-this.mT)/1.4,0,1),ee=e0*e0*(3-2*e0),cur=R.secs[this.sec],old=R.secs[this.prev];
  const react=K.react??R.react,mix=K.mix??R.mix,dk=(K.def??.5)*2,P=PALS[K.pal>0?K.pal-1:R.pal],rot=react>.15?S.chord:0;
  const lutK=(rot&1)?P.alt:P.lut,lut=LUT[lutK],slut=LUT[P.lut];
  const C={t,S,R,asp:W/H,react,sc:1+react*(S.kick*.08+S.bass*.1)},D={};
  for(const k of['TWIST','FOLD','MIRROR','POLAR','MELT','SCAN'])D[k]=lerp(old.D[k]||0,cur.D[k]||0,ee)*dk;
  const Rp=H/2*.96,cx=W/2,cy=H/2,nat=nativeOf(R.content),fN=skelOf(cur.skel,R.content),fO=skelOf(old.skel,R.content),morph=ee<1&&old.skel!==cur.skel;
  const mapC=(x,y)=>{const n=carrier(nat,x,y,C);let f=apply(fN,x,y,C,nat);if(morph){const g=apply(fO,x,y,C,nat);f=[lerp(g[0],f[0],ee),lerp(g[1],f[1],ee),lerp(g[2],f[2],ee)]}
   return deform([lerp(n[0],f[0],mix),lerp(n[1],f[1],mix),lerp(n[2],f[2],mix)],D,C)};
  const mapN=ing=>{const sp=nativeOf(ing);return(x,y)=>deform(carrier(sp,x,y,C),D,C)};
  const tHeat=R.blend!=='SUMA'?'heat':'col',tOther=R.blend==='TERM'?'heat':'col';
  const layers=[[R.content,mapC,1,tHeat]],a1=.2+.6*(1-mix);
  layers.push([cur.skel,mapN(cur.skel),a1*(morph?ee:1),tOther]);if(morph)layers.push([old.skel,mapN(old.skel),a1*(1-ee),tOther]);
  if(cur.over)layers.push([cur.over,mapN(cur.over),.7*(old.over===cur.over?1:ee),tOther]);if(old.over&&old.over!==cur.over)layers.push([old.over,mapN(old.over),.7*(1-ee),tOther]);
  const B=new Map(),cells=[],gl=[];
  for(const[ing,map,la,tg]of layers){if(la<.02)continue;const o={s:[],c:[],g:[]};ING[ing](C,o);
   for(const st of o.s){const p=st.p,heat=tg==='heat',css=heat?'#fff':colOf(st.col,P,rot,slut),base=st.a*la*(heat?intOf(st.col):1);let px=null,py=0;
    for(let i=0;i+1<p.length;i+=2){const x1=p[i],y1=p[i+1];if(i===0){const q=map(x1,y1);px=cx+q[0]*Rp;py=cy-q[1]*Rp;continue}
     const x0=p[i-2],y0=p[i-1],n=Math.max(1,Math.ceil(Math.hypot(x1-x0,y1-y0)/.07));
     for(let j=1;j<=n;j++){const q=map(lerp(x0,x1,j/n),lerp(y0,y1,j/n)),X=cx+q[0]*Rp,Y=cy-q[1]*Rp,al=base*(q[2]<.45?.3:1),aq=Math.round(clamp(al,0,1)*4)/4;
      if(aq>0){const key=tg+css+aq+st.w;let b=B.get(key);if(!b){b={tg,css,a:aq,w:st.w,sg:[]};B.set(key,b)}b.sg.push(px,py,X,Y)}px=X;py=Y}}}
   for(const c of o.c){const q=map(c.x,c.y);if(q[2]<.3&&ing!==R.content)continue;cells.push({X:cx+q[0]*Rp,Y:cy-q[1]*Rp,sz:Math.max(2,c.sz*(.55+.45*q[2])),css:colOf(c.col,P,rot,slut),a:c.a*la*(q[2]<.45?.4:1),tg,v:intOf(c.col)})}
   for(const g of o.g){const q=map(g.x,g.y);if(q[2]>.5&&la>.3)gl.push({X:cx+q[0]*Rp,Y:cy-q[1]*Rp,t:g.t,col:g.col})}}
  const mir=D.MIRROR>.1?(D.MIRROR>.6?2:1):0;
  const flush=(g,tg,col,am,lw,tr)=>{const passes=[[1,0,0,1,0,0]];if(mir)passes.push([-1,0,0,1,W,0]);if(mir>1)passes.push([1,0,0,-1,0,H]);
   for(const m of passes){g.setTransform(m[0]*tr,0,0,m[3]*tr,m[4]*tr,m[5]*tr);for(const b of B.values()){if(b.tg!==tg)continue;g.globalAlpha=b.a*am;g.strokeStyle=col||b.css;g.lineWidth=lw?lw(b.w):b.w;
    g.beginPath();const s=b.sg;for(let i=0;i<s.length;i+=4){g.moveTo(s[i],s[i+1]);g.lineTo(s[i+2],s[i+3])}g.stroke()}
    for(const c of cells){if(c.tg!==tg||col===null)continue;if(col){g.globalAlpha=c.a*c.v;g.fillStyle=col;g.fillRect(c.X-c.sz/2,c.Y-c.sz/2,c.sz,c.sz)}}}
   g.setTransform(1,0,0,1,0,0);g.globalAlpha=1};
  /* calor: líneas a una grilla de celdas de 6 px con memoria */
  const s=this.s;s.globalCompositeOperation='source-over';s.fillStyle='#000';s.fillRect(0,0,this.cols,this.rows);s.globalCompositeOperation='lighter';flush(s,'heat','#fff',.8,()=>4,1/6);
  const id=s.getImageData(0,0,this.cols,this.rows).data,H2=this.heat,gain=.55+S.bass*react*.35+S.level*.15+(S.time-S.chordT<250?.25*react:0);
  const c=this.c;c.globalCompositeOperation='source-over';c.globalAlpha=1;c.fillStyle=R.blend==='SUMA'?'rgba(0,0,0,.42)':'rgba(0,0,0,.6)';c.fillRect(0,0,W,H);
  for(let i=0;i<H2.length;i++){const h=Math.max(H2[i]*R.decay,id[i*4]/255*gain);H2[i]=Math.min(h,1.25);if(h>.05){c.fillStyle=lut[Math.min(255,h*h*230|0)];c.fillRect((i%this.cols)*6,(i/this.cols|0)*6,5,5)}}
  c.globalCompositeOperation='lighter';flush(c,'heat',lut[215],.45,()=>1,1);flush(c,'col',undefined,1,null,1);
  for(const p of[0,1,2].slice(0,mir+1)){for(const q of cells){const X=p===1?W-q.X:q.X,Y=p===2?H-q.Y:q.Y;c.globalAlpha=Math.min(1,q.a);c.fillStyle=q.css;c.fillRect(Math.round(X-q.sz/2),Math.round(Y-q.sz/2),Math.round(q.sz),Math.round(q.sz))}}
  c.globalAlpha=1;c.globalCompositeOperation='source-over';
  /* post: barrido por filas, derretido por columnas (en el equipo: al copiar el canvas) */
  const o=this.o;o.globalCompositeOperation='source-over';o.globalAlpha=1;
  if(D.SCAN>.05){o.fillStyle='#000';o.fillRect(0,0,W,H);for(let y=0;y<H;y+=3){const b=y/3|0,hit=(((b*7)^(S.step*11))&31)<4;
   const dx=Math.sin(b*.7+t*3)*D.SCAN*2.5+(hit?(S.kick*react+.3)*D.SCAN*30*(((b^S.step)&1)?1:-1):0);o.drawImage(this.comp,0,y,W,3,Math.round(dx),y,W,3)}}
  else o.drawImage(this.comp,0,0);
  if(D.MELT>.05){o.globalCompositeOperation='lighter';o.globalAlpha=.45*Math.min(1,D.MELT);for(let x=0;x<W;x+=3){const dy=(hh(x/3,R.seed%97)*18+5)*D.MELT*(.6+S.bass*react)+Math.sin(t*1.3+x*.05)*3*D.MELT;
   o.drawImage(this.comp,x,0,3,H,x,Math.round(dy),3,H)}o.globalCompositeOperation='source-over';o.globalAlpha=1}
  o.font='600 8px "IBM Plex Mono",monospace';o.textAlign='center';for(const g of gl){o.fillStyle='#000';o.fillText(g.t,g.X+1,g.Y-6);o.fillStyle=g.col;o.fillText(g.t,g.X,g.Y-7)}o.textAlign='left';
  return S}}


/* ---------- the recipe: firmware/src/vis_recipe.c, unsigned 32-bit as there ---------- */
export const VI_NAMES = ["LIFE", "KEY", "HARMONY", "LFO", "SPLASH", "RADAR", "ORBIT", "CLAY", "TUNNEL", "CHORUS", "HIVE", "FUNC", "MATH"];
export const VD_NAMES = ["TWIST", "FOLD", "MIRROR", "POLAR", "MELT", "SCAN"];
export const VB_NAMES = ["THERM", "SUM", "MIX"];
export const VP_NAMES = ["AUTO", "THERM", "HUD", "TRACKS", "ICE", "AMBER"];
const V_FAM = [0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 0, 3, 3, 3, 4, 3, 1, 0, 4, 4, 5, 5, 4, 5, 2, 4, 5];
const V_W = [12, 10, 8, 7, 5, 30, 30, 30, 30, 30, 30, 30, 30], V_ENG = [0, 0, 0, 0, 0, 4, 0, 6, 7, 7, 5, 2, 1], SKEL_OK = 0x17EB;
const V_FSK = [[10, 0, 0, 0, 0, 30, 0, 10, 30, 0, 0, 0, 0], [30, 20, 0, 0, 0, 0, 20, 0, 0, 10, 0, 0, 0], [10, 0, 0, 10, 0, 30, 0, 0, 0, 0, 30, 0, 0],
  [0, 0, 0, 0, 0, 20, 10, 20, 0, 0, 0, 0, 30], [0, 30, 0, 0, 0, 0, 20, 0, 10, 20, 10, 0, 0], [0, 0, 0, 0, 0, 20, 0, 10, 30, 0, 0, 0, 0]];
const V_FBL = [[3, 0, 1], [1, 1, 3], [0, 3, 1], [2, 2, 0], [0, 1, 3], [3, 0, 1]];
const V_FDF = [[2, 0, 0, 1, 0, 3], [2, 0, 2, 2, 0, 0], [1, 2, 3, 0, 0, 0], [1, 3, 0, 0, 0, 3], [0, 0, 2, 2, 3, 0], [2, 0, 0, 0, 2, 2]];
const V_FPAL = [[1, 5, 0], [1, 2, 0], [3, 1, 0], [4, 1, 0], [4, 5, 2], [5, 1, 0]];
export function recipe(seed, genre, energy, mask, rec) {
  const NONE = 255, N = 13, u = (v) => v >>> 0;
  let h = 2166136261;
  for (const v of [u(seed) & 0x1FFFFF, u(genre), u(energy), u(mask) & 255, u(rec) & 255]) { h = Math.imul(u(h ^ v), 16777619) >>> 0; h = u(h ^ (h >>> 13)); }
  let x = h || 1;
  const rnd = () => { x = u(x ^ (x << 13)); x = u(x ^ (x >>> 17)); x = u(x ^ (x << 5)); return x; };
  const pick = (w) => { let s = 0; for (const v of w) s += v; if (!s) return NONE; let r = rnd() % s; for (let i = 0; i < w.length; i++) { if (r < w[i]) return i; r -= w[i]; } return NONE; };
  energy = Math.min(u(energy), 100); genre = u(genre);
  const fam = genre < 27 ? V_FAM[genre] : 1, R = { fam, mood: energy < 40 ? 0 : energy < 70 ? 1 : 2, skel: [], over: [], def: [[NONE, NONE]], amt: [[0, 0]] };
  const pool = V_W.map((w, i) => (i < 5 || ((mask >>> V_ENG[i]) & 1) ? w : 0));
  R.content = pick(pool);
  const sk = pool.map((w, i) => (i !== R.content && ((SKEL_OK >>> i) & 1) ? w * (V_FSK[fam][i] || 4) : 0));
  let ov = pool.map((w, i) => (i !== R.content && i !== 4 ? w : 0));
  R.skel[0] = pick(sk); R.over[0] = NONE;
  if (rnd() % 220 < 66 + energy) { const k = ov[R.skel[0]]; ov[R.skel[0]] = 0; R.over[0] = pick(ov); ov[R.skel[0]] = k; }
  R.blend = pick(V_FBL[fam].slice());
  const w = V_FDF[fam].slice(); if (((mask >>> 3) & 1) && w[4] < 3) w[4] = 3;
  const nd = energy > 70 ? 2 : 1;
  for (let i = 0; i < nd; i++) { const k = pick(w); if (k === NONE) break; w[k] = 0; R.def[0][i] = k; R.amt[0][i] = 45 + rnd() % 50; }
  R.pal = V_FPAL[fam][rnd() % (V_FPAL[fam][2] ? 3 : 2)];
  R.mix = 62 + rnd() % 34;
  R.react = Math.min(100, 45 + Math.floor(energy * 10 / 17));
  R.lis = rnd() % 7;
  for (let s = 1; s < 4; s++) {
    const r = rnd() % 100;
    R.skel[s] = R.skel[s - 1]; R.over[s] = R.over[s - 1]; R.def[s] = R.def[s - 1].slice(); R.amt[s] = R.amt[s - 1].slice();
    if (r < 45) {
      const o = R.over[s] < N ? R.over[s] : R.skel[s], ko = sk[o], k = sk[R.skel[s]];
      sk[R.skel[s]] = 0; sk[o] = 0;
      const i = pick(sk);
      sk[o] = ko; sk[R.skel[s - 1]] = k;
      if (i !== NONE) R.skel[s] = i;
    } else if (r < 75) {
      const f = []; for (let i = 0; i < 6; i++) if (i !== R.def[s][0] && i !== R.def[s][1]) f.push(i);
      const k = f[rnd() % f.length];
      if (R.def[s][1] !== NONE) { R.def[s][0] = R.def[s][1]; R.amt[s][0] = R.amt[s][1]; }
      const i = R.def[s][0] === NONE ? 0 : 1;
      R.def[s][i] = k; R.amt[s][i] = 50 + rnd() % 45;
    } else {
      ov = pool.map((w2, i) => (i !== R.content && i !== 4 && i !== R.over[s - 1] && i !== R.skel[s] ? w2 : 0));
      R.over[s] = pick(ov);
    }
  }
  return R;
}
/* the recipe in the mock-up's terms (its ingredient names, deformations by name 0..1, its palette index); what the
   mock draws freely (the LFO's shape, CLAY's steps, MATH's being, the signature) from the same seed */
const MOCK_ING = ["LIFE", "KEY", "HARM", "LFO", "SIG", "RADAR", "ORBIT", "CLAY", "TUNNEL", "CHORUS", "HIVE", "PART", "MATH"];
export function forDrawing(seed, genre, energy, mask, rec) {
  const C = recipe(seed, genre, energy, mask, rec), r = rng(hash(seed, genre, energy, mask, rec, 7)), LIS = [[1, 2], [2, 3], [3, 4], [3, 2], [1, 3], [3, 5], [2, 5]];
  const blend = ["TERM", "SUMA", "MEZCLA"][C.blend] || "MEZCLA";
  const R = { seed, gi: genre, energy, vis: rec, content: MOCK_ING[C.content] || "LIFE", blend, pal: Math.max(0, C.pal - 1), mix: C.mix / 100, react: C.react / 100,
    decay: blend === "TERM" ? .87 : .8, lis: LIS[C.lis], fold: 3 + (r() * 4 | 0), lfoShape: r() * 5 | 0,
    clay: [0, 1, 2, 3].map(() => [...Array(16)].map(() => r() * 2 - 1)), being: [...Array(8)].map(() => r() * 16 | 0), sig: [5 + (r() * 4 | 0), r() * TAU, 1 + (r() * 3 | 0), r() * TAU], c: C };
  R.being[2] |= 2;
  R.secs = [0, 1, 2, 3].map((s) => { const D = {}; for (let i = 0; i < 2; i++) if (C.def[s][i] !== 255) D[VD_NAMES[C.def[s][i]]] = C.amt[s][i] / 100;
    return { skel: MOCK_ING[C.skel[s]] || "LIFE", over: C.over[s] !== 255 ? MOCK_ING[C.over[s]] : null, D }; });
  return R;
}
/* ---------- the live state the drawing reads (the mock-up's Song, filled by a source) ---------- */
const QUAL = [[0, 4, 7], [0, 3, 7], [0, 3, 6], [0, 4, 8], [0, 5, 7]], QNAME = ["", "-", "°", "+", "s"];
const SCALE = [[0, 2, 4, 5, 7, 9, 11], [0, 2, 3, 5, 7, 8, 10], [0, 1, 3, 5, 6, 8, 10], [0, 2, 4, 6, 8, 10, 11], [0, 2, 4, 5, 7, 9, 10]];
export class Live {
  constructor() {
    this.t0 = null; this.time = 0; this.lastT = 0; this.kick = this.snare = this.hat = this.bass = this.level = 0; this.pads = new Float32Array(8); this.hive = new Float32Array(4);
    this.rings = []; this.echoes = []; this.secT = 0; this.chordT = 0; this.step = 0; this.bar = 0; this.sec = 0; this.beat = 0; this.chord = 0; this.chordDeg = 0;
    this.root = 0; this.mode = SCALE[0]; this.stepMs = 125; this.barMs = 2000; this.bpm = 120; this.chordNames = ["C", "F", "G", "A-"]; this.chords = [];
    this.levels = new Float32Array(6); this.lastStep = -1; this.stepAt = 0; this.hist = new Uint8Array(16); this.bassM = new Array(16).fill(0); this.bassN = new Array(16).fill(0);
    this.arpM = new Array(16).fill(0); this.arpN = new Array(16).fill(0); this.playing = false;
  }
  /* a tick: { levels[6] 0..1, hits (bit 0 KICK, 1 SNARE, 2 HAT, 3 PERC), root, quality, step, section, playing } */
  tick(t) {
    const now = this.time;
    if (t.levels) for (let i = 0; i < 6; i++) this.levels[i] = t.levels[i] || 0;
    const h = t.hits | 0;
    if (h & 1) { this.kick = 1; this.pads[0] = 1; this.rings.push(now); this.echoes.push(now); }
    if (h & 2) { this.snare = 1; this.pads[1] = this.pads[2] = 1; }
    if (h & 4) { this.hat = 1; this.pads[3] = this.pads[4] = 1; }
    if (h & 8) { for (let k = 5; k < 8; k++) this.pads[k] = Math.max(this.pads[k], .8); }
    for (let i = 0; i < 4; i++) this.hive[i] = Math.max(this.hive[i], this.levels[i + 1] || 0);
    this.bass = Math.max(this.bass, this.levels[1] || 0);
    if (t.step != null && t.step !== this.lastStep) {
      if (this.lastStep >= 0) { const d = now - this.stepAt, n = ((t.step - this.lastStep) + 64) % 64 || 1; if (d > 20 && d < 2000) this.stepMs += (d / n - this.stepMs) * .2; }
      this.lastStep = t.step; this.stepAt = now; this.step = t.step; const s = t.step & 15;
      this.hist[s] = h & 15 ? 1 : 0; this.bassM[s] = (this.levels[1] || 0) > .3 ? 1 : 0; this.bassN[s] = (t.root || 0) % 7; this.arpM[s] = (this.levels[3] || this.levels[4] || 0) > .3 ? 1 : 0; this.arpN[s] = (s * 3 + (t.root || 0)) % 7;
      if (s === 0) this.chordT = now;
      this.barMs = this.stepMs * 16; this.bpm = Math.round(60000 / this.stepMs / 4);
    }
    if (t.section != null && t.section !== this.sec) { this.sec = t.section & 3; this.secT = now; }
    if (t.root != null) {
      const q = Math.min(4, t.quality | 0), name = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"][t.root % 12] + QNAME[q];
      if (name !== this.chordNames[this.chord]) {
        let i = this.chordNames.indexOf(name);
        if (i < 0) { this.chordNames = [...this.chordNames.slice(1), name]; i = 3; }
        this.chord = i; this.chordT = now;
      }
      this.root = t.root % 12; this.mode = SCALE[q] || SCALE[0]; this.chordDeg = 0;
    }
    this.playing = t.playing ?? true;
  }
  drumAt(i) { return this.hist[i & 15]; }
  update(now) {
    if (this.t0 === null) this.t0 = now;
    const tt = now - this.t0, dt = Math.min(100, Math.max(0, tt - this.lastT)); this.lastT = tt; this.time = tt;
    const k = (x) => Math.exp(-dt / x);
    this.kick *= k(130); this.snare *= k(160); this.hat *= k(70); this.bass *= k(280);
    for (let i = 0; i < 8; i++) this.pads[i] *= k(200); for (let i = 0; i < 4; i++) this.hive[i] *= k(450);
    const lv = this.levels.reduce((a, b) => Math.max(a, b), 0), tg = Math.min(1, this.kick * .45 + this.bass * .35 + lv * .4);
    this.level += (tg - this.level) * (1 - k(160));
    this.rings = this.rings.filter((x) => tt - x < 1200); this.echoes = this.echoes.filter((x) => tt - x < 1700);
    if (this.playing && tt - this.stepAt > this.stepMs * 1.5) { /* no step from the source: run on its own tempo */ }
    this.bar = this.step >> 4; this.beat = tt / (this.stepMs * 4);
    return this;
  }
}
export { Vis, PALS };
