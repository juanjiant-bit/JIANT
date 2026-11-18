/* The emulator boots the same healthy two-core voice policy as the FM-1.
 * Its host mixer executes the two jobs sequentially, not on browser threads. */
#include "../web/emu/felucca_web.c"
#include <stdio.h>

static void render_ms(unsigned ms)
{
    for (unsigned n = 0; n < ms * 441u / 10u; n += 128) web_render(128);
}
static unsigned web_count(unsigned t)
{
    unsigned n = 0;
    for (unsigned i = 0; i < NVOICE; i++) n += trk[t].v[i].active && trk[t].v[i].stage != 4;
    return n;
}
int main(void)
{
    unsigned failed = 0;
    web_nor_erase(); web_boot(); render_ms(3000);
    printf("web boot: cores=%u headroom=%u cap=%u\n", mc_live() ? 2 : 1, voice_headroom, voice_cap());
    if (!mc_live() || !voice_headroom) failed++;
    for (unsigned tracks = 4; tracks <= 6; tracks += 2) {
        memset(trk, 0, sizeof trk); host_tracks_init();
        trks_put(tracks == 6); load_put(2); /* real TRACKS and FULL settings */
        for (unsigned t = 0; t < NTRK; t++) {
            host_preset(&trk[t], t ? ENGI_ANALOG : ENGI_DRUM, 0);
            trk[t].p[P_VOICE] = V_POLY; trk[t].p[P_SUS] = 127;
            trk[t].p[P_VCAP] = trk[t].p[P_DDST] = 0;
        }
        render_ms(100);
        for (unsigned l = 0; l < DV_NLANE; l++) trk_note_on(&trk[0], DRUM_LANE_NOTE[l], 110);
        static const unsigned notes[] = {48, 52, 55, 59};
        for (unsigned j = 0; j < 4; j++) trk_note_on(&trk[1], notes[j], 100);
        printf("web TRACKS=%u: kit=%u chord=%u cap=%u\n", tracks, web_count(0), web_count(1), voice_cap());
        if (trk_live != tracks || voice_cap() != (tracks == 6 ? 16 : 12) ||
            web_count(0) != 8 || web_count(1) != 4) failed++;
        render_ms(3);
        if (web_count(0) != 8 || web_count(1) != 4) failed++;
    }
    mc_host = 0; render_ms(100);
    if (voice_headroom || voice_cap() > 8) failed++;
    printf("web polyphony: %u failures\n", failed);
    return !!failed;
}
