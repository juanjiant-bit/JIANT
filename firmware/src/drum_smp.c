/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (ports/, FELUCCA_HOST_POWER only) DRUM-X's sample layer: each lane's hit has, beside its synthesized sound, a sample
 * (a slot of smp_slots.c, or a slice of one: a kit), and SMORPH blends the two (0: the synth alone, exactly the FM-1's
 * DRUM-X; nothing of this runs). Included by eng_drum.c; on the FM-1 none of it exists, and the values below sit in
 * slots a DRUM track there never reads (so a project from here loads there as its synth kit).
 *
 * (V10.8.48) the values in slots a DRUM track never reads, on the FM-1 or here: WHEEL's ROTOR (P_WDUST, P_WRDEP), DRIFT
 * (P_DRIFT: voice.c track_render gives DRUM none) and MORPH (P_MORPH: ANALOG's and PHASE's). The SUS and LEVEL slots
 * hold 127 in every store before (DRUM-X's P_LRTRG is its KEY MAP now), so 127 there is each one's "as before":
 *   SMORPH (P_WDUST, stored 127 - v: 127 = 0 %)   0 synth .. 127 sample
 *   BLEND (P_DRIFT, 127 XFADE .. 123 RING)   XFADE the two crossfaded; S>B the synth's transient on the sample's body;
 *       B>S the sample's transient on the synth's body; FOLW the sample following the synth's amplitude; RING one by
 *       the other
 *   SOURCE (P_MORPH, 0..47: the slot 1..16 and its map)   1..16 SLOTS lane l the slot SLOT + l (a hit at velocity 100
 *       or more: SLOT + 8 + l if it holds a sample, the velocity layer), L1..L16 LANES the slot cut in 8, lane l its
 *       slice l, K1..K16 KEYS the slot cut in 16, the GM note 36 + n its slice n
 *   SLEVEL (P_WRDEP, 127 = 100 %)   the sample's level
 * The kit's TUNE (E1) and the lane's GM pitch tune the sample, DECAY (E3) below its middle shortens it, DRIVE (E7) and
 * the bus shape the sum as ever; a closed hat chokes the open one's sample as its synth. CHANCE, the lanes' ratchets
 * and TOUCH are the sequencer's, as for the synth */
#define P_DXSMORPH P_WDUST
#define P_DXSBLEND P_DRIFT
#define P_DXSSRC P_MORPH
#define P_DXSLEVEL P_WRDEP
enum { DXB_XFADE, DXB_SB, DXB_BS, DXB_FOLLOW, DXB_RING, DXB_N };
enum { DXM_SLOTS, DXM_LANES, DXM_KEYS };
static const char *const N_DXS_BLEND[129] = {[123] = "RING", "FOLW", "B>S", "S>B", "XFADE", 0};
static const char *const N_DXS_SRC[49] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16",
    "L1", "L2", "L3", "L4", "L5", "L6", "L7", "L8", "L9", "L10", "L11", "L12", "L13", "L14", "L15", "L16",
    "K1", "K2", "K3", "K4", "K5", "K6", "K7", "K8", "K9", "K10", "K11", "K12", "K13", "K14", "K15", "K16", 0};
static const param_desc_t DXS_DESC[4] = {
    {"SMORPH", F_PCTINV, 0, 127, 127, 0},
    {"BLEND", F_ENUM, 123, 127, 127, N_DXS_BLEND},
    {"SOURCE", F_ENUM, 0, 47, 0, N_DXS_SRC},
    {"SLEVEL", F_PCT, 0, 127, 127, 0},
};
static const param_desc_t *dxs_desc(uint32_t id)
{
    switch (id) {
    case P_DXSMORPH: return &DXS_DESC[0];
    case P_DXSBLEND: return &DXS_DESC[1];
    case P_DXSSRC: return &DXS_DESC[2];
    case P_DXSLEVEL: return &DXS_DESC[3];
    default: return 0;
    }
}
static inline int32_t dxs_morph(const track_t *t) { return 127 - clamp(t->p[P_DXSMORPH], 0, 127); }
static inline uint32_t dxs_blend(const track_t *t) { return (uint32_t)(127 - clamp(t->p[P_DXSBLEND], 123, 127)); }

typedef struct {
    double pos;
    float inc, env, peak, g, kd;          /* speed; the synth's follower, its peak; the sample's decay now, a sample */
    uint32_t a, b, ver, age;              /* the region [a, b); its slot's version; samples since the hit */
    uint8_t slot, on, lane, choke;
} dxs_v_t;
static dxs_v_t dxs_v[NPART][NVOICE];

static inline dxs_v_t *dxs_vs(const track_t *t, const voice_t *v)
{
    return &dxs_v[(uint32_t)(t - trk) % NPART][(uint32_t)(v - t->v) % NVOICE];
}
static int dxs_playing(const track_t *t, const voice_t *v)   /* the sample still to sound (drum_amp keeps the voice) */
{
    return dxs_morph(t) > 0 && dxs_vs(t, v)->on;
}

/* a hit: the lane's sample from its slot (or slice), at the lane's pitch */
static void dxs_note_on(track_t *t, voice_t *v, uint32_t lane, const drum_lane_t *L)
{
    dxs_v_t *D = dxs_vs(t, v);
    uint32_t src = (uint32_t)clamp(t->p[P_DXSSRC], 0, 47), base = src % 16u, map = src / 16u;
    uint32_t slot = base, k, ns = 0, sl = 0;
    const smp_obj_t *o;
    int32_t semis = 0;
    D->on = 0;
    if (dxs_morph(t) <= 0)
        return;
    if (lane == DV_HATC)                              /* the closed hat chokes the open one's sample too */
        for (k = 0; k < NVOICE; k++)
            if (dxs_v[(uint32_t)(t - trk) % NPART][k].on && dxs_v[(uint32_t)(t - trk) % NPART][k].lane == DV_HATO)
                dxs_v[(uint32_t)(t - trk) % NPART][k].choke = 1;
    if (map == DXM_SLOTS) {
        slot = (base + lane) % SMP_NSLOT;
        if (v->vel >= 100u && smp_get((base + 8u + lane) % SMP_NSLOT))
            slot = (base + 8u + lane) % SMP_NSLOT;    /* the velocity layer */
        semis = L->st;
    } else if (map == DXM_LANES) {
        ns = 8u;
        sl = lane;
        semis = L->st;
    } else {
        ns = 16u;
        sl = (uint32_t)clamp((int32_t)v->note - 36, 0, 15);
    }
    o = smp_get(slot);
    if (!o)
        return;
    D->slot = (uint8_t)slot;
    D->ver = smp_slot[slot].ver;
    D->a = 0;
    D->b = o->frames;
    if (ns) {
        uint32_t len = o->frames / ns;
        D->a = sl * len;
        D->b = D->a + (len ? len : 1u);
    }
    D->pos = (double)D->a;
    D->inc = powf(2.0f, ((float)semis * 16.0f + (float)(t->p[P_E1] - 64) * 3.0f + (float)t->pfx_pit) / 192.0f);
    D->env = D->peak = 0.0f;
    D->g = 1.0f;
    {   /* DECAY below its middle: the sample shortened, tau 10 ms .. 1 s (the middle and above: as it is) */
        int32_t d = clamp(t->p[P_E3], 0, 127);
        D->kd = d >= 64 ? 1.0f : expf(-1.0f / (0.01f * powf(100.0f, (float)d / 64.0f) * (float)FS));
    }
    D->age = 0;
    D->lane = (uint8_t)lane;
    D->choke = 0;
    D->on = 1;
}

/* the layer into the hit's block y (the synth, dx_run's), by SMORPH and BLEND */
static void dxs_layer(track_t *t, voice_t *v, int32_t *y, uint32_t n)
{
    dxs_v_t *D = dxs_vs(t, v);
    const smp_obj_t *o = smp_get(D->slot);
    float mo = (float)dxs_morph(t) / 127.0f, lvl = (float)clamp(t->p[P_DXSLEVEL], 0, 127) / 127.0f;
    uint32_t bl = dxs_blend(t), i;
    float vg = 0.5f + 0.5f * (float)v->vel / 127.0f;  /* (voice.c takes the velocity after: a little more here) */
    if (!D->on || !o || D->ver != smp_slot[D->slot].ver)
        D->on = 0;
    for (i = 0; i < n; i++) {
        float s = 0.0f, x = (float)y[i], w, r;
        if (D->on) {
            if (D->pos >= (double)D->b || D->g < 0.0005f) {
                D->on = 0;
            } else {
                s = 0.5f * (smp_read(o, 0, D->pos) + smp_read(o, 1, D->pos)) * 0.75f * lvl * D->g * vg;
                D->pos += (double)D->inc;
                D->g *= D->kd;
                if (D->choke)
                    D->g *= 0.995f;
            }
        }
        w = expf(-(float)D->age / (0.006f * (float)FS));   /* the transient's window: ~6 ms */
        if (D->age < 0x7FFFFFFFu)
            D->age++;
        switch (bl) {
        case DXB_SB: r = x * (w + (1.0f - w) * (1.0f - mo)) + s * (1.0f - w) * mo; break;
        case DXB_BS: r = x * (1.0f - w * mo) + s * w * mo; break;
        case DXB_FOLLOW: {
            float a = fabsf(x);
            D->env += (a - D->env) * (a > D->env ? 0.2f : 0.002f);
            D->peak = D->env > D->peak ? D->env : D->peak;
            r = x * (1.0f - mo) + s * (D->peak > 1.0f ? D->env / D->peak : 0.0f) * mo;
            break;
        }
        case DXB_RING: r = x * (1.0f - mo) + x * s * (2.0f / 16384.0f) * mo; break;
        default: r = x * (1.0f - mo) + s * mo; break;
        }
        y[i] = (int32_t)smp_finite(r > 200000.0f ? 200000.0f : r < -200000.0f ? -200000.0f : r);
    }
}
