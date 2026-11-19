// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant). All rights reserved.
/* (V10.7.59) every section's pages and their four knob labels, for the names review (not run by CI) */
#define UI_TEST_NO_MAIN
#include "ui_test.c"
int main(void)
{
    static const char *const SN[] = {"HOME", "EDIT", "SEQ", "FX", "GLO", "ENV", "LFO", "SCL", "ARP", "SAVE"};
    uint32_t e, sec, i, k, n;
    for (e = 0; e < NENGINES; e++) {
        if (!eng_ok(e)) continue;
        ui_power_on();
        set_engine_of(TSEL, e);
        frame();
        for (sec = SEC_EDIT; sec < SEC_N; sec++) {
            uint8_t v[NAV_MAX];
            if (sec != SEC_EDIT && e) continue;
            n = nav_list(sec, v);
            for (i = 0; i < n; i++) {
                const nv_t *c = &nav_ch(sec)->e[v[i]];
                uint32_t pg = nv_page(sec, v[i]);
                int16_t *hv;
                printf("%s\t%s\t%s", sec == SEC_EDIT ? ENGINES[e]->name : "-", SN[sec], c->nm ? c->nm : c->t);
                for (k = 0; k < 4u; k++) {
                    const param_desc_t *d = pg < NPAGES ? page_desc(&PAGES[pg], k, &hv) : 0;
                    printf("\t%s", d && d->label ? d->label : "-");
                }
                printf("\n");
            }
        }
    }
    return 0;
}
