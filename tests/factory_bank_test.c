/* SPDX-License-Identifier: GPL-3.0-only */
#define main hostsim_main
#include "hostsim.c"
#undef main
#define FELUCCA_FLASH 0
static uint32_t st_crc32_add(uint32_t c,const void *p,uint32_t n)
{
 const uint8_t *b=p;while(n--){uint32_t k;c^=*b++;for(k=0;k<8;k++)c=(c>>1)^(0xEDB88320u & (0u-(c&1u)));}return c;
}
static uint32_t st_crc32(const void *p,uint32_t n){return ~st_crc32_add(0xffffffffu,p,n);}
#include "../firmware/src/sample_bank.c"
int main(int argc,char **argv)
{
 uint8_t bytes[SMP_BANK]; uint32_t i,k;FILE *f=argc>1?fopen(argv[1],"rb"):0;
 if(!f || fread(bytes,1,sizeof bytes,f)!=sizeof bytes || fgetc(f)!=EOF)return 1;fclose(f);
 host_tracks_init();if(smp_begin())return 2;
 for(i=0;i<sizeof bytes;i+=64)if(smp_write(i,bytes+i,64))return 3;
 if(smp_end(st_crc32(bytes,sizeof bytes)) || smp_ix.count!=12)return 4;
 for(k=0;k<12;k++) {
  int64_t energy=0;int32_t out[CTL];
  if(smp_ix.c0[k]==255u || smp_bank.h.e[k].rate<1000 || smp_bank.h.e[k].rate>16000)return 5;
  memset(trk,0,sizeof trk);host_tracks_init();host_preset(&trk[0],ENGI_ADD,1);
  trk[0].p[P_E0]=0;trk[0].p[P_ATK]=0;trk[0].p[P_SUS]=127;
  trk_note_on(&trk[0],60+k,100);
  for(i=0;i<300;i++){uint32_t j;track_render(&trk[0],out,CTL);for(j=0;j<CTL;j++)energy+=out[j]<0?-(int64_t)out[j]:out[j];}
  if(energy<1000){fprintf(stderr,"silent slot %u\n",k);return 6;}
 }
 printf("factory-bank: 12 slots transferred, CRC checked and played by the real C SAMPLE engine PASS\n");return 0;
}
