/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * (V10.7.81) the news film of V10.7.66 .. V10.7.81 for tools/video/compose.py (1920 x 1080): GEN thrown with EXPLORE
 * three times (the pieces picked beforehand by tools/video/scan_explore.c: the least harsh, every track sounding), the
 * last kept and worked on with the new things: HARMONY's numerals, the FX layer's cells played, FOCUS in heat, DRUM-X's
 * MATH tables and DICE, the GLITCH FM and MATH LAB kits; then DUBSTEP (a MATH bass, its MORPH swept), CUMBIA villera,
 * the scope, MENU > PRESETS, the grid of it all and the outro. Writes audio.wav, screen.rgb, scenes.txt (EN),
 * scenes_es.txt, seps.txt (the throws: compose.py's die), beats.txt (the tempo changes with each throw).
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o build/video81/cap tools/video/capture_v81.c -lm
 *   build/video81/cap build/video81 && python3 tools/video/compose.py build/video81 */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

#define FPS 30u
#define SPLASH 4.0
#define END 100.0
static FILE *wav, *rgb, *cap, *ces, *sep, *bts;
static uint32_t nsamp, T_DRUM, T_BASS, T_LEAD;
static double next_beat, beat_s = 0.5;

static void caption(double t, const char *tag, const char *title, const char *sub, int kind,
                    const char *tag_es, const char *title_es, const char *sub_es)
{
    fprintf(cap, "%.4f\t%s\t%s\t%s\t%d\n", t, tag, title, sub, kind);
    fprintf(ces, "%.4f\t%s\t%s\t%s\t%d\n", t, tag_es, title_es, sub_es, kind);
}
static void enc_turn(uint32_t role, int32_t s) { host_enc[panel.enc[role]] += s * panel.dir[role]; }
static uint32_t genre_of(const char *name)
{
    uint32_t i;
    if (str_eq(name, "EXPLORE")) return GEN_EXPLORE;
    for (i = 0; i < NGENRE; i++) if (str_eq(GENRE[i].name, name)) return i + 1u;
    return GEN_EXPLORE;
}
static void roles(void)
{
    uint32_t k;
    T_DRUM = 0; T_BASS = gs.trk_of[RL_BASS] < NTRK ? gs.trk_of[RL_BASS] : 1u; T_LEAD = T_BASS;
    for (k = 0; k < NTRK; k++) if (trk[k].eng_req == ENGI_DRUM) { T_DRUM = k; break; }
    if (gs.trk_of[RL_LEAD] < NTRK) T_LEAD = gs.trk_of[RL_LEAD];
    else for (k = 0; k < NTRK; k++) if (k != T_DRUM && k != T_BASS && gs.act[k]) { T_LEAD = k; break; }
}
static void throw_gen(double t, const char *genre, uint32_t seed)
{
    gp.genre = (uint8_t)genre_of(genre); gp.seed = (uint16_t)seed; gp.energy = 70; gp.form = 0;
    gen_go();
    song.master_q12 = 4096;
    if (!song.playing) transport_req = 1;
    roles();
    ui.msg_t = 0; ui.msg[0] = ui.msg2[0] = 0;
    beat_s = 60.0 / (double)song.g[G_BPM];
    next_beat = t;
    fprintf(sep, "%.4f\n", t);
    fprintf(stderr, "%.1f s: %s  drums T%u bass T%u lead T%u  %d BPM\n", t, gp.last, T_DRUM + 1, T_BASS + 1, T_LEAD + 1, song.g[G_BPM]);
}
static void show_gen(void) { ui.menu = 0; confirm_open(CF_DICE_ALL, song.sel); gen_open(); ui.force = 1; }
static void page(uint32_t sel, const char *title) { ui.confirm = 0; ui.menu = 0; song.sel = (uint8_t)sel; go_title(title); ui.force = 1; }
static uint32_t label_k(const track_t *t, const char *l)   /* the E knob named l (FOCUS's parameter) */
{
    uint32_t k;
    for (k = 0; k < 8u; k++) { const param_desc_t *d = track_desc(t, P_E0 + k); if (d && d->label && str_eq(d->label, l)) return k; }
    return 8;
}
#define AT(x) (t0 < (x) && t >= (x))
static void script(double t0, double t)
{
    /* ---- GEN EXPLORE: three throws, the last kept */
    if (AT(4.5)) {
        show_gen(); throw_gen(4.5, "EXPLORE", 104);
        caption(4.5, "GEN", "EXPLORE", "a new piece every throw|seeds fused: never the same twice|every rule of the genres broken", 0,
                "GEN", "EXPLORE", "un tema nuevo en cada tirada|semillas fusionadas: nunca dos iguales|todas las reglas de los géneros rotas");
    }
    if (AT(8.5)) { show_gen(); throw_gen(8.5, "EXPLORE", 132); }
    if (AT(12.5)) { show_gen(); throw_gen(12.5, "EXPLORE", 50); }
    /* ---- HARMONY */
    if (AT(16.5)) {
        page(T_LEAD, "HARMONY");
        caption(16.5, "SCL", "HARMONY", "the progression in roman numerals|a chromatic palette, the chord playing lit|its own tab in SCL, CHORD kept", 0,
                "SCL", "HARMONY", "la progresión en números romanos|paleta cromática, el acorde que suena encendido|su propia pestaña en SCL, CHORD se queda");
    }
    /* ---- the FX layer: the cells, played */
    if (AT(17.2)) enc_turn(EN_K1, 1);
    if (AT(17.8)) enc_turn(EN_K1, 1);
    if (AT(23.0)) {
        page(T_LEAD, "FILTER"); go_home(); btn_down(B_FX);
        caption(23.0, "FX", "PUNCH-IN CELLS", "hold FX: every effect by its name|four macros on top, the mutes below|RPT · REVERSE · TAPE STOP · PITCH · STRETCH", 0,
                "FX", "CELDAS DE PUNCH-IN", "mantené FX: cada efecto con su nombre|cuatro macros arriba, los mutes abajo|RPT · REVERSE · TAPE STOP · PITCH · STRETCH");
    }
    if (AT(24.2)) key_down(2);  if (AT(25.6)) key_up(2);    /* RPT 1/16 */
    if (AT(26.2)) key_down(6);  if (AT(27.2)) key_up(6);    /* REVERSE */
    if (AT(27.8)) key_down(20); if (AT(28.8)) key_up(20);   /* PITCH UP */
    if (AT(29.2)) key_down(17); if (AT(30.4)) key_up(17);   /* TAPE STOP */
    if (AT(31.0)) btn_up(B_FX);
    /* ---- FOCUS in heat: the cutoff swept */
    if (AT(31.5)) {
        uint32_t q, tf = T_LEAD;
        for (q = 0; q < NTRK; q++) if (q != T_DRUM && label_k(&trk[q], "CUT") < 8u) { tf = q; break; }
        page(tf, "FOCUS"); ui.focus = (uint8_t)(label_k(&trk[tf], "CUT") & 7u);
        caption(31.5, "EDIT", "FOCUS IN HEAT", "one parameter at a time, large|the arc and E1..E8 in their values' heat|MORE now comes before PLAY", 0,
                "EDIT", "FOCUS TÉRMICO", "un parámetro a la vez, grande|el arco y E1..E8 con el calor de su valor|MORE ahora va antes de PLAY");
    }
    if (t >= 32.5 && t < 35.5 && ((uint32_t)(t * FPS) & 1u)) enc_turn(EN_K2, 1);
    if (t >= 35.5 && t < 38.5 && ((uint32_t)(t * FPS) & 1u)) enc_turn(EN_K2, -1);
    /* ---- DRUM-X: MATH tables, DICE */
    if (AT(39.5)) {
        dx_fml[3] = 7; dx_frt[3] = 5; dx_kit[3].mode = (uint8_t)((dx_kit[3].mode & ~3u) | DXW_FM);
        dx_ui_lane = 3; page(T_DRUM, "SOUND 3");
        caption(39.5, "DRUM-X", "MATH WAVETABLES", "a MATH formula as each sound's FM:|F01..F48, its table drawn on screen|MOD AMT and 24 RATIOs", 0,
                "DRUM-X", "WAVETABLES MATH", "una fórmula MATH como FM de cada sonido:|F01..F48, su tabla dibujada en pantalla|MOD AMT y 24 RATIOS");
    }
    if (AT(42.0)) { dx_fml[3] = 23; }
    if (AT(44.0)) {
        sound_dice_of(&trk[T_DRUM], 0);
        caption(44.0, "DRUM-X", "DICE", "the kit thrown: half its sounds|get a formula and a ratio of their own", 0,
                "DRUM-X", "DICE", "el kit tirado: la mitad de sus sonidos|recibe una fórmula y un ratio propios");
    }
    if (AT(47.5)) {
        page(T_DRUM, "PRESETS"); apply_preset(11);
        caption(47.5, "DRUM-X", "GLITCH FM · MATH LAB", "two new kits made of formulas|GLITCH FM: short, high ratios|MATH LAB: DRUM-X's kit, every sound a table", 0,
                "DRUM-X", "GLITCH FM · MATH LAB", "dos kits nuevos hechos de fórmulas|GLITCH FM: cortos, ratios altos|MATH LAB: el kit DRUM-X, cada sonido una tabla");
    }
    if (AT(51.0)) apply_preset(12);
    /* ---- DUBSTEP: the wobble, a MATH bass and its MORPH */
    if (AT(55.0)) {
        show_gen(); throw_gen(55.0, "DUBSTEP", 41);
        caption(55.0, "GEN", "DUBSTEP", "the wobble's rate moves every half bar|MATH basses for the extreme sounds|formula kits on the drums", 0,
                "GEN", "DUBSTEP", "el rate del wobble cambia cada medio compás|bajos MATH para los sonidos extremos|kits con fórmulas en la batería");
    }
    if (AT(60.0)) {
        if (trk_eng(&trk[T_BASS]) == ENGI_LOFI) trk[T_BASS].p[P_E0] = 30;
        page(T_BASS, "EDIT 2");
        caption(60.0, "MATH", "MORPH", "two formulas, ALGO and ALGO B|MORPH glides from one to the other|cheap on the CPU: sampled sparsely", 0,
                "MATH", "MORPH", "dos fórmulas, ALGO y ALGO B|MORPH va de una a la otra|liviano para la CPU");
    }
    if (t >= 61.0 && t < 66.0 && ((uint32_t)(t * FPS) % 3u) == 0u) enc_turn(EN_K3, 2);
    /* ---- CUMBIA villera */
    if (AT(67.0)) {
        show_gen(); throw_gen(67.0, "CUMBIA", 15);
        caption(67.0, "GEN", "CUMBIA VILLERA", "92..100 BPM, the keyboard on top|LATIN PERC: güiro, llamador, timbal|MATH, WEST and STRING more often", 0,
                "GEN", "CUMBIA VILLERA", "92..100 BPM, el teclado arriba|LATIN PERC: güiro, llamador, timbal|MATH, WEST y STRING más seguido");
    }
    if (AT(71.0)) page(T_DRUM, "SOUND");
    /* ---- the scope, MENU > PRESETS */
    if (AT(75.0)) {
        page(T_LEAD, "GLOBAL");
        caption(75.0, "GLO", "SCOPE", "the decorative bars are gone:|the output's oscilloscope instead", 0,
                "GLO", "OSCILOSCOPIO", "chau barras decorativas:|ahora el osciloscopio de la salida");
    }
    if (AT(80.0)) {
        ui.confirm = 0; hold(B_HOME); ui.menu_sel = MI_PRSCR; ui.force = 1;
        caption(80.0, "MENU", "PRESETS: SCROLL · MENU", "MENU > DISPLAY > PRESETS|SCROLL: the knob loads the next sound|MENU: it opens the list first", 0,
                "MENU", "PRESETS: SCROLL · MENU", "MENU > DISPLAY > PRESETS|SCROLL: la perilla carga el sonido siguiente|MENU: primero abre la lista");
    }
    if (AT(82.5)) { menu_put(MI_PRSCR, 1u); ui.force = 1; }
    /* ---- the grid, the outro */
    if (AT(85.0)) {
        menu_put(MI_PRSCR, 0u); ui.menu = 0; ui.confirm = 0; go_home(); ui.force = 1;
        caption(85.0, "NEXT", "SONG MODE,|REDESIGNED", "coming in the next version:|each track its sequences A B C D, switched live|the jam recorded as the song, LOCK to keep it", 0,
                "PRÓXIMAMENTE", "MODO CANCIÓN,|REDISEÑADO", "llega en la próxima versión:|cada pista con sus secuencias A B C D, en vivo|la jam se graba como canción, LOCK para fijarla");
    }
    if (AT(90.5)) {
        ui.menu = 0; go_home(); ui.force = 1;
        caption(90.5, "", "V10.7.81", "EXPLORE|HARMONY|PUNCH-IN CELLS|FOCUS IN HEAT|MATH WAVETABLES|GLITCH FM · MATH LAB|DUBSTEP|CUMBIA VILLERA", 2,
                "", "V10.7.81", "EXPLORE|HARMONY|CELDAS DE PUNCH-IN|FOCUS TÉRMICO|WAVETABLES MATH|GLITCH FM · MATH LAB|DUBSTEP|CUMBIA VILLERA");
    }
    if (AT(96.0)) caption(96.0, "", "", "", 0, "", "", "");
}

static void put_screen(void)
{
    static uint8_t buf[240u * 240u * 3u];
    uint32_t i;
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t c = swap16(host_screen[i]);
        buf[3u * i] = (uint8_t)((c >> 11) * 255u / 31u);
        buf[3u * i + 1u] = (uint8_t)(((c >> 5) & 63u) * 255u / 63u);
        buf[3u * i + 2u] = (uint8_t)((c & 31u) * 255u / 31u);
    }
    fwrite(buf, 1, sizeof buf, rgb);
}
static FILE *open_in(const char *dir, const char *name, const char *mode)
{
    char p[512];
    snprintf(p, sizeof p, "%s/%s", dir, name);
    return fopen(p, mode);
}
int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "build/video81";
    uint32_t f, total = (uint32_t)(FPS * END), nwav = (uint32_t)(FS * END), k;
    double tp = -1.0;
    wav = open_in(dir, "audio.wav", "wb"); rgb = open_in(dir, "screen.rgb", "wb");
    cap = open_in(dir, "scenes_en.txt", "w"); ces = open_in(dir, "scenes_es.txt", "w");
    sep = open_in(dir, "seps.txt", "w"); bts = open_in(dir, "beats.txt", "w");
    if (!wav || !rgb || !cap || !ces || !sep || !bts) { fprintf(stderr, "cannot write in %s\n", dir); return 1; }
    ui_power_on();
    settings.palette = UI_JIANT_INDEX; palette_set(UI_JIANT_INDEX);
    ui_rec_prefs |= PREF_SIX >> 8; rp_apply();
    go_home(); frames(60);
    wav_hdr(wav, nwav);
    fprintf(cap, "# %u fps, 120 BPM, 0 bars, splash %.4f s, end %.4f s, beat 0.500000 s\n", FPS, SPLASH, END);
    fprintf(ces, "# %u fps, 120 BPM, 0 bars, splash %.4f s, end %.4f s, beat 0.500000 s\n", FPS, SPLASH, END);
    next_beat = SPLASH;
    for (f = 0; f < total; f++) {
        double t = (double)f / FPS;
        uint32_t want = (uint32_t)((uint64_t)(f + 1u) * FS / FPS);
        script(tp, t); tp = t;
        while (t >= next_beat) { fprintf(bts, "%.4f\n", next_beat); next_beat += beat_s; }
        while (nsamp < want && nsamp < nwav) {
            int32_t o[2 * CTL];
            int16_t s[2 * CTL];
            uint32_t i, n = CTL;
            mix_block(o, CTL);
            for (i = 1; i < CTL; i += 2u)
                scope_buf[scope_w++ & (SCOPE_N - 1u)] = (int16_t)clamp(o[2u * i], -32768, 32767);
            if (nsamp + n > nwav) n = nwav - nsamp;
            for (i = 0; i < 2u * n; i++) s[i] = (int16_t)clamp(o[i], -32768, 32767);
            fwrite(s, 2, 2u * n, wav);
            nsamp += n;
        }
        host_ticks += 1000000u / FPS; fm1_ms += 1000u / FPS;
        if (t < SPLASH) {
            if (f == 0) { spl_anim = 1; draw_splash(); }
            splash_write((uint32_t)(t * 1000.0));
        } else {
            static int first = 1;
            if (first) { ui.force = 1; first = 0; }
            ui_input();
            song_poll();
            ui_draw();
        }
        for (k = 0; k < NTRK; k++) trk[k].peak = 0;
        put_screen();
    }
    fclose(wav); fclose(rgb); fclose(cap); fclose(ces); fclose(sep); fclose(bts);
    printf("%u frames, %u samples\n", total, nsamp);
    return 0;
}
