#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
import re, sys
# (JIANT 0.5) the README's presentation image (docs/jiant-hero.png) from the UI renders (tests/ui_render.c -> build/ui_new):
#   python3 tools/gen_readme_hero.py docs/jiant-hero.png    (after tests/run_tests.sh; needs PIL)
from PIL import Image, ImageDraw, ImageFont
R='/home/user/JIANT/'
FB=R+'assets/fonts/ChakraPetch-Bold.ttf'; FS=R+'assets/fonts/ChakraPetch-SemiBold.ttf'; FM=R+'assets/fonts/ChakraPetch-Medium.ttf'
H=[(0,210,255),(90,60,255),(180,50,230),(240,40,50),(255,122,0),(255,215,0),(255,255,255)]
def heat(v):
    v=max(0,min(1,v))*6; k=min(5,int(v)); f=v-k
    return tuple(int(H[k][j]+(H[k+1][j]-H[k][j])*f) for j in range(3))
BG=(0,0,0); TXT=(244,241,234); MID=(160,154,150); DIM=(80,76,84); SURF=(16,12,22)
W,Hh=1600,1520
im=Image.new('RGB',(W,Hh),BG); d=ImageDraw.Draw(im)
# signature from the firmware table
src=open(R+'firmware/src/ui_signature.h').read()
nums=[int(x) for x in re.findall(r'\b\d+\b', src.split('{',1)[1])]
pts=[(nums[i],nums[i+1]) for i in range(0,len(nums)-1,2)]
segs=[];L=0
for i in range(len(pts)-1):
    if pts[i+1][1]&128: continue
    a=(pts[i][0],pts[i][1]&127); b=(pts[i+1][0],pts[i+1][1]&127)
    l=max(abs(b[0]-a[0]),abs(b[1]-a[1])); segs.append((a,b,L)); L+=l
sc=2.6; ox,oy=70,40
for a,b,acc in segs:
    c=heat(0.08+0.86*acc/L)
    d.line([(ox+a[0]*sc,oy+a[1]*sc),(ox+b[0]*sc,oy+b[1]*sc)],fill=c,width=5)
# title block
d.text((690,70),"JIANT FM",font=ImageFont.truetype(FB,128),fill=TXT)
d.text((696,215),"bio-synthetic operating system for the M-VAVE FM-1",font=ImageFont.truetype(FM,30),fill=MID)
d.text((696,258),"firmware v0.5  ·  síntesis · secuencia · mutación · performance",font=ImageFont.truetype(FM,24),fill=DIM)
for i in range(W-140):
    d.line([(70+i,330),(70+i,335)],fill=heat(i/(W-140)))
# big screens
def scr(name,x,y,s):
    s_im=Image.open(R+f'build/ui_new/JIANT/{name}.png').convert('RGB').resize((240*s,240*s),Image.NEAREST)
    d.rectangle([x-3,y-3,x+240*s+2,y+240*s+2],outline=(40,34,48),width=2)
    im.paste(s_im,(x,y))
cap=ImageFont.truetype(FS,22)
y0=370
big=[('home','HOME · el ecosistema'),('edit_analog','ANALOG · su ser'),('drumx','DRUM-X · la colonia')]
for i,(n,c) in enumerate(big):
    x=70+i*500; scr(n,x,y0,2); d.text((x,y0+492),c,font=cap,fill=heat(0.15+0.3*i))
# small strip
y1=y0+545
small=[('lfo','LFO'),('dist','DIST'),('dly','DLY'),('master','MASTER'),('roll_acid','STEP'),('global','GLOBAL')]
for i,(n,c) in enumerate(small):
    x=70+i*245; s_im=Image.open(R+f'build/ui_new/JIANT/{n}.png').convert('RGB').resize((216,216),Image.LANCZOS)
    im.paste(s_im,(x,y1)); d.text((x,y1+222),c,font=ImageFont.truetype(FS,18),fill=MID)
# feature cards
y2=y1+275
cards=[("VISIÓN TÉRMICA","El color es intensidad: frío cian,\ncaliente amarillo y blanco."),
       ("SERES","Cada motor es un organismo que se\ndeforma con sus perillas y el audio."),
       ("FILTER EN TODO","LP · HP · BP · COMB afinado a la\nnota, en los ocho motores."),
       ("DADOS","SELECT sortea el sonido; las macros\ncaen solo sobre lo que suena."),
       ("DRUM-X","Batería sintetizada: morph entre A y B,\nFOLD y FM, sin samples."),
       ("MASTER NIVELADO","CLIP con carácter y un nivelador\nsiempre activo: volumen coherente.")]
ft=ImageFont.truetype(FS,27); fd=ImageFont.truetype(FM,20)
for i,(t,txt) in enumerate(cards):
    cx=70+(i%3)*500; cy=y2+(i//3)*125
    d.rectangle([cx,cy,cx+5,cy+92],fill=heat(i/5))
    d.text((cx+22,cy-2),t,font=ft,fill=TXT)
    d.multiline_text((cx+22,cy+38),txt,font=fd,fill=MID,spacing=6)
d.text((70,Hh-50),"GPL-3.0  ·  fork de Felucca (Hügelton Instruments)  ·  song mode de SLOOP  ·  juanjiant-bit.github.io/JIANT",font=ImageFont.truetype(FM,20),fill=DIM)
im.save(sys.argv[1]); print(im.size)
