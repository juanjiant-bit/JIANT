/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <assert.h>

static project_t saved, decoded;
static project_store_t wire;
static dx_config_t expected[NTRK];
int main(void)
{
    uint32_t k;
    ui_power_on(); go_home(); frames(8);
    FILE *legacy=fopen("tests/fixtures/drum_shared_v46.func","rb");
    assert(legacy && fread(&wire,1,sizeof wire,legacy)==sizeof wire); fclose(legacy);
    assert(proj_import(&saved,&wire,sizeof wire) && !saved.kit_mask);
    assert(!project_restore_runtime(&saved));
    for(k=0;k<NTRK;k++) {
        assert(!memcmp(dx_cfg[k].lane,DX_KITS[3].lane,sizeof dx_cfg[k].lane));
        assert(dx_cfg[k].tone==0x24 && dx_cfg[k].hands==5 && dx_cfg[k].glue==7);
        assert(dx_cfg[k].mot[XM_RAND]==70 && dx_cfg[k].mute==DXM_LANE(2) && dx_cfg[k].snd[0]==0x9a);
    }
    puts("PASS real V10.7.46 project loads its shared kit on every track");
    ui_power_on(); go_home(); frames(8);
    for (k=0;k<NTRK;k++) set_engine_of(&trk[k],ENGI_ANALOG);
    for (k=0;k<4;k++) {
        song.sel=k; set_engine(ENGI_DRUM); apply_preset(k+1);
        dx_cfg[k].tone=(uint8_t)(k+1); dx_cfg[k].hands=(uint8_t)k;
        dx_cfg[k].glue=(uint8_t)(k+2); dx_cfg[k].mot[XM_DPTH]=(uint8_t)(k*10);
        assert(!memcmp(dx_cfg[k].lane,DX_KITS[k+1].lane,sizeof dx_cfg[k].lane));
    }
    memcpy(expected,dx_cfg,sizeof expected);
    song.sel=2; apply_preset(7);
    for (k=0;k<4;k++) if(k!=2) assert(!memcmp(&dx_cfg[k],&expected[k],sizeof expected[k]));
    memcpy(expected,dx_cfg,sizeof expected);
    puts("PASS four independent presets and edits");
    song.sel=3; power_sound_of(&trk[0],0);
    assert(!memcmp(dx_cfg[0].lane,DX_KITS[TRK_DEF[0][1]].lane,sizeof dx_cfg[0].lane));
    assert(!memcmp(&dx_cfg[3],&expected[3],DX_CONFIG_BYTES));
    dx_cfg[0]=expected[0];
    puts("PASS power-on kit targets its track, independent of selection");
    project_capture(&saved);
    assert(proj_pack(&wire,&saved));
    assert(proj_import(&decoded,&wire,sizeof wire));
    memset(dx_cfg,0,sizeof dx_cfg);
    assert(!project_restore_runtime(&decoded));
    for(k=0;k<4;k++) assert(!memcmp(&dx_cfg[k],&expected[k],sizeof expected[k]));
    puts("PASS independent kits survive project save/load");
    /* A damaged optional record must not become a valid but different kit. */
    project_store_t valid = wire;
    uint32_t dk = 0, sum;
    for (k = 0; k + 4 < PROJC_PFX_OFF; k++)
        if (wire.raw[k] == 'D' && wire.raw[k+1] == 'K' && wire.raw[k+2] == 1 && wire.raw[k+3] == saved.kit_mask) dk = k;
    assert(dk && saved.kit_mask);
    wire.raw[dk+2] = 2;
    sum = proj_hash(wire.raw, sizeof wire - 4); memcpy(wire.raw + sizeof wire - 4, &sum, 4);
    assert(!proj_import(&decoded,&wire,sizeof wire));
    wire = valid; wire.raw[dk+3] = 0x80;
    sum = proj_hash(wire.raw, sizeof wire - 4); memcpy(wire.raw + sizeof wire - 4, &sum, 4);
    assert(!proj_import(&decoded,&wire,sizeof wire));
    wire = valid; wire.raw[dk+4+__builtin_offsetof(dx_config_t, hands)] = 16;
    sum = proj_hash(wire.raw, sizeof wire - 4); memcpy(wire.raw + sizeof wire - 4, &sum, 4);
    assert(!proj_import(&decoded,&wire,sizeof wire));
    wire = valid;
    assert(proj_kits(&decoded,wire.raw,dk,dk+4+DX_CONFIG_BYTES-1) == UINT32_MAX);
    assert(proj_import(&decoded,&wire,sizeof wire));
    puts("PASS malformed and truncated kit records rejected");
    decoded.kit_mask=0;
    assert(proj_pack(&wire,&decoded));
    assert(proj_import(&saved,&wire,sizeof wire));
    assert(!saved.kit_mask);
    assert(!project_restore_runtime(&saved));
    for(k=1;k<NTRK;k++) assert(!memcmp(&dx_cfg[k],&dx_cfg[0],sizeof dx_cfg[0]));
    puts("PASS legacy shared kit copied to every track");
    /* Rendering must follow the voice's track, never the selected UI track. */
    for(k=0;k<4;k++) dx_cfg[k]=expected[k];
    uint64_t hashes[4]={0};
    for(k=0;k<4;k++) {
        int32_t a[CTL],b[CTL];dx_voice_t va={0},vb={0};
        va.trig=vb.trig=1;va.sv=vb.sv=100;
        song.sel=0;dx_run_fx(&dx_cfg[k],&dx_cfg[k].lane[0],&va,0,0,0,0,0,0,0,a,CTL,0);
        song.sel=3;dx_run_fx(&dx_cfg[k],&dx_cfg[k].lane[0],&vb,0,0,0,0,0,0,0,b,CTL,0);
        assert(!memcmp(a,b,sizeof a));
        for(uint32_t j=0;j<CTL;j++) hashes[k]=hashes[k]*1099511628211ull+(uint32_t)a[j];
    }
    assert(hashes[0]!=hashes[1] && hashes[1]!=hashes[2]);
    puts("PASS distinct drum audio independent of UI selection");
    /* Each track follows its own FROM section, including a kit edited live. */
    for(k=0;k<4;k++) { song.sel=k; set_engine(ENGI_DRUM); dx_cfg[k]=expected[k]; }
    project_capture(&saved); assert(proj_pack(&proj_slot[0],&saved));
    for(k=0;k<4;k++) dx_kit_load_of(&trk[k],k+5);
    dx_cfg[1].mute=DXM_LANE(2);
    project_capture(&decoded); assert(proj_pack(&proj_slot[1],&decoded));
    song_cur=0; chain.loop=0; chain.running=0; clip_mix=0;
    song_idx.tsrc[0][0]=2u << 3; /* T2 from B, the others from A. */
    assert(!song_stage(0,0)); sec_apply();
    dx_config_t from_b;
    proj_kit_get(&decoded,1,&from_b);
    assert(!memcmp(&dx_cfg[0],&expected[0],DX_CONFIG_BYTES));
    assert(!memcmp(&dx_cfg[1],&from_b,DX_CONFIG_BYTES));
    chain.running=1; assert(!song_stage(0,0)); sec_apply();
    dx_cfg[1].lane[0].a[DXP_PITCH]=31;
    song_idx.tsrc[0][1]=0;
    assert(!song_stage(0,1)); sec_apply();
    assert(snd_held == (1u<<1));
    assert(dx_cfg[1].lane[0].a[DXP_PITCH]==31 && dx_cfg[1].mute==expected[1].mute);
    assert(!memcmp(&dx_cfg[0],&expected[0],DX_CONFIG_BYTES));
    assert(!project_recall_sound(0,1) && !snd_held);
    assert(!memcmp(&dx_cfg[1],&expected[1],DX_CONFIG_BYTES));
    puts("PASS per-track section sources, held kit, mutes and recall");
    /* The fixed flash slot refuses a full project without touching the output. */
    for(k=0;k<NTRK;k++) for(uint32_t j=0;j<NSTEP;j++) {
        step_t *st=&saved.t[k].step[j];
        st->time=ST_NOTE; st->n=4; st->vel=90;
        for(uint32_t v=0;v<4;v++) st->note[v]=(uint8_t)(40+j+v);
    }
    memset(&wire,0xA5,sizeof wire); valid=wire;
    assert(!proj_pack(&wire,&saved) && proj_full && !memcmp(&wire,&valid,sizeof wire));
    puts("PASS full project fails without overwriting its saved data");
    return 0;
}
