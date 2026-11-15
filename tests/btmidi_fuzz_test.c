/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.7.39) Fuzz of every path from a Bluetooth link to the synth, on the host, the real sources compiled in (as
 * tests/midi_control_test.c), meant to run under -fsanitize=address,undefined (tests/run_tests.sh builds it so when the
 * compiler has them). A crash report ("connecting my BT keyboard") had no keyboard named: this feeds what any central
 * could send.
 *
 * Built twice:
 *   -DBTF_STACK=1  the stack's side (firmware/bt/ble: ble_host.c's L2CAP reassembly, ble_att.c's ATT server with the
 *                  BLE-MIDI write path, ble_midi.c's decoder and encoder, ble_scan.c's AD parser and device table):
 *                  link-layer fragments of random and adversarial L2CAP frames (short, long, cut, a continuation
 *                  without a start, every ATT opcode with every length, MTU exchanges, reads past the end, CCCD writes),
 *                  BLE-MIDI packets of every shape (no header, header only, timestamps alone, running status over
 *                  packets, SysEx cut and over packets, real time inside messages, unknown status octets), connects and
 *                  disconnects between them; the decoder's state stays in range, every event it emits is a well-formed
 *                  USB-MIDI packet, the encoder packs MIDI OUT's ring without a hang; advertisements with long,
 *                  unterminated, non-ASCII names into the scan table: the names stay terminated, Latin-1.
 *   (default)      the app's side: those packets decoded, taken by bt_midi_accept and queued as bt.c bt_midi_rx queues
 *                  them (USB's ring, source 1), then events_block / the voices / the UI's PROGRAM CHANGE poll, the real
 *                  dispatch (seq.c midi_event, midi_control.c, midi_clock.c, the matrix, MIDI LEARN): floods of
 *                  aftertouch and poly pressure, bend, NRPN / RPN sequences on every id, PROGRAM CHANGE 0..127 on every
 *                  channel, CC 0..127, notes on every channel with ROUT, KEY SPLIT and MIDI BASE CH changing under them,
 *                  the TRS parser fed the same bytes at once (two devices), panics and lost links (the ring's overflow)
 *                  with notes held; the ownership tables stay consistent and every note is let go after a full release. */
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static uint32_t fz_seed = 0x2545F491u;
static uint32_t rnd(void)
{
    fz_seed ^= fz_seed << 13;
    fz_seed ^= fz_seed >> 17;
    fz_seed ^= fz_seed << 5;
    return fz_seed;
}
static uint32_t rn(uint32_t n) { return n ? rnd() % n : 0u; }

static int fz_fails;
static void fz_check(const char *what, int ok)
{
    printf("%-100s %s\n", what, ok ? "ok" : "FAIL");
    fz_fails += !ok;
}
static void fz_seed_env(void)                            /* FZ_SEED=n: another stream (the default: the same every run) */
{
    const char *e = getenv("FZ_SEED");
    if (e && strtoul(e, 0, 0))
        fz_seed = (uint32_t)strtoul(e, 0, 0);
}
static void fz_hang(int s)
{
    (void)s;
    static const char m[] = "btmidi_fuzz: HANG (no progress in 120 s)\n";
    (void)!write(2, m, sizeof m - 1u);
    _exit(3);
}

/* a BLE-MIDI packet of n octets: adversarial shapes */
static uint32_t fz_packet(uint8_t *p, uint32_t cap)
{
    static const uint8_t ST[] = {0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5,
                                 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF};
    uint32_t n = 0, kind = rn(8), m, i;
    if (kind == 0) {                                     /* noise of any length */
        m = rn(cap + 1u);
        for (i = 0; i < m; i++)
            p[n++] = (uint8_t)rnd();
        return n;
    }
    p[n++] = (uint8_t)(rn(4) ? 0x80u | rn(64) : rnd());  /* (mostly a valid header) */
    m = 1u + rn(cap - 1u);
    while (n < m) {
        uint32_t w = rn(16);
        if (w < 3)
            p[n++] = (uint8_t)(0x80u | rn(128));         /* a timestamp octet alone (or a status after one) */
        else if (w < 9) {                                /* a message, its status a channel's or any */
            uint8_t st = (uint8_t)(rn(3) ? (0x80u + 16u * rn(7)) | rn(16) : ST[rn(sizeof ST)]);
            if (n + 2u > m)
                break;
            p[n++] = (uint8_t)(0x80u | rn(128));
            p[n++] = st;
            for (i = rn(4); i && n < m; i--)
                p[n++] = (uint8_t)(rn(8) ? rn(128) : 0x80u | rn(128));   /* data, sometimes a real time / timestamp */
        } else if (w < 12)
            p[n++] = (uint8_t)rn(128);                   /* data: running status, SysEx data */
        else if (w < 14) {
            p[n++] = (uint8_t)(0x80u | rn(128));
            if (n < m)
                p[n++] = (uint8_t)(0xF8u + rn(8));       /* real time inside anything */
        } else if (n + 2u <= m) {
            p[n++] = (uint8_t)(0x80u | rn(128));
            p[n++] = (uint8_t)(rn(2) ? 0xF0u : 0xF7u);   /* SysEx start / end */
        }
    }
    return n;
}

/* a USB-MIDI event the decoder may emit: its CIN agrees with its octets */
static int fz_event_ok(uint32_t pkt)
{
    uint32_t cin = pkt & 15u, a = pkt >> 8 & 0xFFu, b = pkt >> 16 & 0xFFu, c = pkt >> 24;
    if (pkt & 0xF0u)
        return 0;                                        /* (cable 0) */
    if (cin >= 8u && cin <= 0xEu)
        return a >> 4 == cin && !(b & 0x80u) && !(c & 0x80u) && (cin != 0xCu && cin != 0xDu ? 1 : !c);
    if (cin == 0xFu)
        return a >= 0xF8u && !b && !c;
    if (cin == 2u || cin == 3u)
        return (a == 0xF1u || a == 0xF2u || a == 0xF3u) && !(b & 0x80u) && !(c & 0x80u);
    if (cin == 4u)
        return (a == 0xF0u || !(a & 0x80u)) && !(b & 0x80u) && !(c & 0x80u);
    if (cin >= 5u && cin <= 7u) {
        uint32_t k = cin - 4u, last = pkt >> (8u * k) & 0xFFu;
        return (cin == 5u && a == 0xF6u && !b && !c) || (last == 0xF7u && (k < 3u ? !(pkt >> (8u + 8u * k)) : 1));
    }
    return 0;
}

#if BTF_STACK
/* ------------------------------------------------------------------------------------- the stack's side */
#define BLE_API static
#define BLE_DIAG 0
#define BLE_LL_MAX_OCTETS 27u                            /* (as firmware/bt/bt_seg.c) */
#define BLE_CENTRAL 0                                    /* (the scan parser is included by itself below) */
#include "../firmware/bt/ble/ble_cfg.h"
#include "../firmware/bt/ble/ble_util.h"
#include "../firmware/bt/ble/ble_diag.h"
#include "../firmware/bt/ble/ble_midi.c"
#include "../firmware/bt/bt_midi.h"

/* the link layer and SMP as ble_host.c / ble_att.c see them */
static int fz_conn = 1, fz_enc;
static uint32_t fz_tx_room = 4096u, fz_sent, fz_events, fz_bad_events;
static uint8_t fz_last_tx[600];
static int ble_ll_send(uint16_t cid, const uint8_t *p, uint16_t n)
{
    (void)cid;
    if (n > BLE_ATT_MTU_MAX + 4u) {                      /* (bigger than any ATT PDU of ours: a bug) */
        fprintf(stderr, "send of %u octets\n", n);
        abort();
    }
    memcpy(fz_last_tx, p, n);
    fz_sent++;
    return 1;
}
static int ble_ll_connected(void) { return fz_conn; }
static int ble_ll_encrypted(void) { return fz_enc; }
static uint32_t ble_ll_tx_room(void) { return fz_tx_room; }
static uint16_t ble_ll_interval(void) { return 9; }
static uint16_t ble_ll_timeout(void) { return 200; }
static int ble_ll_conn_update(uint16_t a, uint16_t b, uint16_t c) { (void)a, (void)b, (void)c; return 1; }
static void ble_ll_disconnect(uint8_t r) { (void)r; }
static void ble_smp_rx(const uint8_t *p, uint16_t n) { (void)p, (void)n; }
static void ble_smp_connected(void) {}
static void ble_smp_reset(void) {}
/* the app's side as bt_seg.c has it: accepted events counted, MIDI OUT from a ring the test fills */
static struct bt_oq bt_oq;
static void ble_app_midi_in(uint32_t pkt, uint16_t ts, uint16_t last)
{
    (void)ts, (void)last;
    fz_events++;
    if (!fz_event_ok(pkt)) {
        if (fz_bad_events++ < 4u)
            fprintf(stderr, "decoder emitted %08x\n", pkt);
    }
    (void)bt_midi_accept(pkt);
}
static uint32_t fz_ms;
static int ble_app_midi_peek(uint32_t *pkt, uint32_t *t)
{
    uint32_t due;
    if (!bt_oq_peek(&bt_oq, pkt, &due))
        return 0;
    *t = fz_ms;
    return 1;
}
static void ble_app_midi_pop(void) { bt_oq_pop(&bt_oq); }
static void ble_app_state(void) { bt_oq.on = (uint8_t)fz_conn; }
#include "../firmware/bt/ble/ble_att.c"
/* ble_host.c's L2CAP reassembly and LE signalling (as it is; the connection-parameter requests answered here) */
#include "../firmware/bt/ble/ble_host.c"
#undef BLE_CENTRAL
#define BLE_CENTRAL 3
#include "../firmware/bt/ble/ble_scan.c"

static void fz_sink(void *ctx, uint32_t pkt, uint16_t ts)
{
    (void)ctx, (void)ts;
    ble_app_midi_in(pkt, 0, 0);
}
static void fz_frag(const uint8_t *f, uint32_t n)        /* an L2CAP frame as 27-octet LL fragments (start, continue) */
{
    uint32_t o = 0;
    do {
        uint32_t k = n - o > 27u ? 27u : n - o;
        if (rn(64) == 0)
            k = rn(28);                                  /* (a fragment cut short) */
        ble_host_rx(f + o, (uint8_t)k, o == 0);
        o += k ? k : 1u;
    } while (o < n);
}
static void fz_att(const uint8_t *a, uint32_t n)          /* an ATT PDU in its L2CAP frame */
{
    uint8_t f[4 + 600];
    uint32_t len = n, lie = rn(32) == 0;
    if (lie)
        len = rn(700);                                   /* (a frame length that lies) */
    ble_wr16(f, len);
    ble_wr16(f + 2, rn(16) ? L2CAP_CID_ATT : rn(8));
    memcpy(f + 4, a, n);
    fz_frag(f, 4u + n);
}

static void stack_fuzz(void)
{
    static struct ble_midi_dec D;
    static struct ble_scan_tab T;
    uint8_t p[600], a[600];
    uint32_t it, i, bad_state = 0, bad_name = 0, oq_stuck = 0;
    ble_att_reset();
    for (it = 0; it < 2000000u; it++) {
        uint32_t n = fz_packet(p, 1u + rn(it & 1u ? 20u : 250u)), w = rn(32);
        fz_ms += rn(20);
        /* the decoder by itself */
        ble_midi_dec(&D, p, (uint16_t)n, fz_sink, 0);
        if (D.sx_n > 3u || D.got > 1u || D.need > 2u)
            bad_state++;
        /* the same packet as a central writes it (Write Command / Request to the MIDI I/O, any handle sometimes) */
        a[0] = rn(4) ? 0x52 : 0x12;
        ble_wr16(a + 1, rn(8) ? 14u : rn(20));
        memcpy(a + 3, p, n);
        fz_att(a, 3u + n);
        if (w == 0) {                                    /* any ATT PDU */
            uint32_t m = rn(40);
            for (i = 0; i < m; i++)
                a[i] = (uint8_t)rnd();
            if (rn(2))
                a[0] = (uint8_t)(rn(0x20));
            fz_att(a, m);
        } else if (w == 1) {                             /* MTU exchange */
            a[0] = 0x02;
            ble_wr16(a + 1, rnd());
            fz_att(a, 3);
        } else if (w == 2) {                             /* the MIDI CCCD on / off */
            a[0] = 0x12;
            ble_wr16(a + 1, 15);
            a[3] = (uint8_t)rn(4);
            a[4] = 0;
            fz_att(a, 5);
        } else if (w == 3) {                             /* a disconnect and a reconnect */
            fz_conn = 0;
            ble_app_state();
            ble_att_reset();
            fz_conn = 1;
        } else if (w == 4) {                             /* a raw LL payload (any LLID start bit, any length) */
            uint32_t m = rn(28);
            for (i = 0; i < m; i++)
                a[i] = (uint8_t)rnd();
            ble_host_rx(a, (uint8_t)m, (uint8_t)rn(2));
        } else if (w == 5) {                             /* an advertisement (and a scan response) into the table */
            uint32_t l = 8u + rn(30), k = 8;
            a[0] = (uint8_t)(rn(2) ? 0 : 4);
            a[1] = (uint8_t)(l - 2u);
            for (i = 2; i < 8; i++)
                a[i] = (uint8_t)rn(4);                   /* (few addresses: updates of the same entries) */
            while (k < l) {
                uint32_t len = 1u + rn(l - k);
                a[k] = (uint8_t)(rn(8) ? len - 1u : rnd());
                if (k + 1u < l)
                    a[k + 1u] = (uint8_t)(rn(2) ? 0x09u : rn(3) ? 0x08u : rn(2) ? 0x07u : rnd());
                for (i = k + 2u; i < k + len && i < l; i++)
                    a[i] = (uint8_t)(rn(3) ? rnd() : rn(2) ? 0xC3u : 0x20u + rn(95));
                k += len;
            }
            ble_scan_add(&T, a, rn(16) ? l : rn(40), (uint16_t)rnd(), fz_ms);
            if (rn(64) == 0)
                ble_scan_age(&T, fz_ms + rn(10000));
            for (i = 0; i < BLE_SCAN_N; i++) {
                uint32_t k2;
                if (!T.e[i].used)
                    continue;
                for (k2 = 0; k2 <= BLE_NAME_MAX && T.e[i].name[k2]; k2++)
                    if ((uint8_t)T.e[i].name[k2] < 0x20u || ((uint8_t)T.e[i].name[k2] >= 0x7Fu && (uint8_t)T.e[i].name[k2] < 0xA0u))
                        bad_name++;
                if (k2 > BLE_NAME_MAX)
                    bad_name++;
                (void)ble_scan_bars(&T, i);
            }
        }
        /* MIDI OUT: the ring filled with anything, then packed as each connection event packs it */
        for (i = rn(8); i; i--) {
            uint32_t e = rnd();
            if (rn(2))
                e = (e & 0x7F7F0000u) | (0x80u + rn(0x80)) << 8 | (0x8u + rn(8));
            bt_oq_put(&bt_oq, e);
        }
        fz_tx_room = rn(8) ? 4096u : rn(300);
        {
            uint32_t r0 = bt_oq.r, k;
            for (k = 0; k < 4u; k++)
                ble_att_event();
            if (bt_oq.on && fz_tx_room >= batt.mtu + BLE_ATT_RESERVE && bt_oq.w != bt_oq.r && bt_oq.r == r0)
                oq_stuck++;
        }
        if (it % 1000u == 0)
            ble_midi_dec_reset(&D);
    }
    fz_check("stack: 2000000 packets, L2CAP / ATT / BLE-MIDI / scan: no crash, no out-of-bounds (ASan / UBSan)", 1);
    fz_check("stack: the decoder's state in range (SysEx octets <= 3, data octets <= 2)", !bad_state);
    fz_check("stack: every event the decoder gives the app a well-formed USB-MIDI packet", fz_events > 10000u && !fz_bad_events);
    fz_check("stack: the scan table's names terminated, printable Latin-1, at most 16", !bad_name);
    fz_check("stack: MIDI OUT's ring drains while a central listens (no stall with room to send)", !oq_stuck);
    printf("  (%u events to the app, %u PDUs sent)\n", fz_events, fz_sent);
}

int main(void)
{
    signal(SIGALRM, fz_hang);
    alarm(120);
    fz_seed_env();
    stack_fuzz();
    return fz_fails ? 1 : 0;
}

#else
/* ------------------------------------------------------------------------------------- the app's side */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#undef BLE_API
#define BLE_API static
#include "../firmware/bt/ble/ble_midi.c"
#include "../firmware/bt/bt_midi.h"

static uint32_t fz_gated, fz_ovf;
static int fz_any_gate(void)
{
    uint32_t i, v;
    for (i = 0; i < NTRK; i++)
        for (v = 0; v < NVOICE; v++)
            if (trk[i].v[v].active && trk[i].v[v].gate && !trk[i].p[P_AMODE] && !trk_drone(&trk[i]))
                return 1;
    return 0;
}

/* bt.c bt_midi_rx as the firmware has it (the host builds no FELUCCA_BT: the same lines, the interrupts aside) */
static void fz_bt_midi_rx(uint32_t pkt)
{
    uint32_t *last = &midi_in_q[(mi_w - 1u) % MQ];
    if (!pkt)
        midi_in_overflow = 1;
    else if (mi_w != mi_r && bt_midi_same(pkt, *last))
        *last = pkt;
    else
        midi_enqueue(pkt, 1u);
}
static void fz_rx(void *ctx, uint32_t pkt, uint16_t ts)   /* bt_seg.c ble_app_midi_in -> bt.c bt_midi_rx */
{
    (void)ctx, (void)ts;
    if (!fz_event_ok(pkt))
        fz_fails += 1000;
    if ((pkt = bt_midi_accept(pkt)) != 0u)
        fz_bt_midi_rx(pkt);
}
static void fz_link_lost(void) { fz_bt_midi_rx(0); }      /* bt_seg.c ble_app_state: the link gone */

/* (V10.7.39) what the fuzz found by reasoning, kept as cases: a central's 244-octet packet (ATT MTU 247) of running
 * status holds 120 .. 240 messages, more than the ring's 64; a link lost with notes held */
static int fz_cases(void)
{
    static struct ble_midi_dec D;
    uint8_t p[244];
    uint32_t n, i;
    int bad = 0, ok;
    ui_power_on();
    midi_in_overflow = 1;
    events_block(CTL);
    song.g[G_ROUTE] = 0;
    ble_midi_dec_reset(&D);
    n = 0;
    p[n++] = 0x80, p[n++] = 0x80;
    p[n++] = 0x90, p[n++] = 60, p[n++] = 100, p[n++] = 64, p[n++] = 100, p[n++] = 67, p[n++] = 100;   /* a chord */
    ble_midi_dec(&D, p, (uint16_t)n, fz_rx, 0);
    events_block(CTL);
    ok = midi_owners[0] == 3u;
    n = 0;
    p[n++] = 0x80, p[n++] = 0x81, p[n++] = 0xA0;            /* poly pressure on the chord by running status */
    while (n + 2u <= sizeof p)
        p[n++] = (uint8_t)(60u + 4u * (n % 3u)), p[n++] = (uint8_t)(n & 127u);
    ble_midi_dec(&D, p, (uint16_t)n, fz_rx, 0);
    events_block(CTL);
    ok &= midi_owners[0] == 3u;
    bad += check("BT: a packet of 120 poly pressures (running status) under a held chord: the chord held", ok);
    n = 0;
    p[n++] = 0x80, p[n++] = 0x81, p[n++] = 0xD0;            /* 240 channel pressures in one packet */
    while (n < sizeof p)
        p[n] = (uint8_t)(n & 127u), n++;
    ble_midi_dec(&D, p, (uint16_t)n, fz_rx, 0);
    events_block(CTL);
    ok = midi_owners[0] == 3u && trk[0].at == ((sizeof p - 1u) & 127u);
    bad += check("BT: 240 channel pressures in one packet: the chord held, the last value taken", ok);
    n = 0;
    p[n++] = 0x80, p[n++] = 0x81, p[n++] = 0xE0;            /* 120 bends, the last the centre */
    while (n + 2u <= sizeof p - 2u)
        p[n++] = (uint8_t)rn(128), p[n++] = (uint8_t)rn(128);
    p[n++] = 0, p[n++] = 64;
    ble_midi_dec(&D, p, (uint16_t)n, fz_rx, 0);
    events_block(CTL);
    ok = midi_owners[0] == 3u && midi_bend_target[0] == 0;
    n = 0;
    p[n++] = 0x80, p[n++] = 0x81, p[n++] = 0xB0;            /* the wheel, 120 values */
    while (n + 2u <= sizeof p)
        p[n++] = 1, p[n++] = (uint8_t)(n & 127u);
    ble_midi_dec(&D, p, (uint16_t)n, fz_rx, 0);
    events_block(CTL);
    ok &= midi_owners[0] == 3u && trk[0].mw == ((n - 1u) & 127u);
    bad += check("BT: 120 bends, 120 wheel CCs in a packet each: the chord held, the last values taken", ok);
    n = 0;
    p[n++] = 0x80;                                          /* notes and pressure interleaved: nothing lost */
    for (i = 0; i < 24u && n + 8u <= sizeof p; i++) {
        p[n++] = 0x81, p[n++] = 0xD0, p[n++] = (uint8_t)i;
        p[n++] = 0x81, p[n++] = 0x90, p[n++] = (uint8_t)(70u + i), p[n++] = 90;
    }
    ble_midi_dec(&D, p, (uint16_t)n, fz_rx, 0);
    events_block(CTL);
    ok = midi_owners[0] == 3u + 24u;
    bad += check("BT: 24 notes each after a pressure in one packet: every note queued, none dropped", ok);
    fz_link_lost();
    events_block(CTL);
    for (i = 0; i < 64u; i++) {
        int32_t o[CTL];
        events_block(CTL);
        track_render(&trk[0], o, CTL);
    }
    ok = !midi_owners[0] && !fz_any_gate();
    bad += check("BT: the link lost with 27 notes held: every one let go, no voice gated", ok);
    return bad;
}

/* the ownership tables agree: midi_owners[t] = the notes held for track t on every channel (and the pedal's) */
static int fz_owners_ok(void)
{
    uint32_t ch, note, t, n[NTRK] = {0};
    for (ch = 0; ch < 16u; ch++)
        for (note = 0; note < 128u; note++) {
            uint32_t id = mn_get(ch, note) & 7u;
            if (id > NTRK)
                return 0;
            if (id)
                n[id - 1u]++;
        }
    for (t = 0; t < NTRK; t++)
        if (n[t] != midi_owners[t])
            return 0;
    return midi_map.n <= 128u;
}

/* a message stream of musical and hostile messages, as one BLE-MIDI packet (several messages, running status) */
static uint32_t fz_msgs(uint8_t *p, uint32_t cap, uint32_t base)
{
    uint32_t n = 0, last = 0;
    p[n++] = 0x80;
    while (n + 6u < cap && rn(8)) {
        uint32_t ch = rn(4) ? (base + rn(8)) & 15u : rn(16), w = rn(20), st, d1 = rn(128), d2 = rn(128);
        if (w < 6)
            st = 0x90u | ch, d1 = 36u + rn(48);
        else if (w < 9)
            st = 0x80u | ch, d1 = 36u + rn(48);
        else if (w < 11)
            st = 0xA0u | ch;                            /* poly pressure floods */
        else if (w < 13)
            st = 0xD0u | ch;                            /* channel pressure */
        else if (w < 14)
            st = 0xE0u | ch;
        else if (w < 15)
            st = 0xC0u | ch;
        else {
            static const uint8_t CC[] = {0, 1, 6, 7, 10, 11, 16, 23, 32, 38, 64, 71, 74, 98, 99, 100, 101, 102, 120, 121, 123, 127};
            st = 0xB0u | ch;
            d1 = rn(3) ? CC[rn(sizeof CC)] : rn(128);
        }
        p[n++] = (uint8_t)(0x80u | rn(128));
        if (st != last || rn(2))
            p[n++] = (uint8_t)st;
        if (rn(32) == 0)                                 /* a clock inside the message */
            p[n++] = 0x80u | 5u, p[n++] = 0xF8;
        p[n++] = (uint8_t)d1;
        if ((st & 0xE0u) != 0xC0u)
            p[n++] = (uint8_t)d2;
        last = st;
    }
    return n;
}

static void fz_release_all(void)                          /* every channel: the pedal up, every note off */
{
    uint32_t ch, note;
    for (ch = 0; ch < 16u; ch++) {
        midi_enqueue(0xBu | (0xB0u | ch) << 8 | 64u << 16, 1u);
        events_block(CTL);
        for (note = 0; note < 128u; note++) {
            midi_enqueue(0x8u | (0x80u | ch) << 8 | note << 16, 1u);
            if ((note & 31u) == 31u)
                events_block(CTL);
        }
        events_block(CTL);
    }
}

int main(void)
{
    static struct ble_midi_dec D;
    uint8_t p[300];
    int32_t out[CTL];
    uint32_t it, bad_owner = 0, rounds = 0, held_after = 0, n, k;
    signal(SIGALRM, fz_hang);
    alarm(600);
    fz_seed_env();
    fz_fails += fz_cases();
    ui_power_on();
    song.playing = 0;
    for (it = 0; it < 300000u; it++) {
        uint32_t w = rn(64), base = 0;
        base = mi_base;
        if (w < 40)
            n = fz_msgs(p, sizeof p, base);
        else
            n = fz_packet(p, 1u + rn(w & 1u ? 20u : 120u));
        ble_midi_dec(&D, p, (uint16_t)n, fz_rx, 0);
        if (rn(3) == 0) {                                /* a second device on the TRS jack: the same bytes at once */
            for (k = 1; k < n; k++)
                um_byte(p[k]);
        }
        if (w == 50)
            song.g[G_ROUTE] = (int16_t)rn(2);           /* ROUT CH1-6 / SEL under held notes */
        else if (w == 51)
            midi_base_set = mi_base = (uint8_t)rn(11);  /* MIDI BASE CH under held notes (as ui_input.c sets it) */
        else if (w == 52)
            song.sel = (uint8_t)rn(NTRK);
        else if (w == 53 && rn(8) == 0)
            panic_all = 1;
        else if (w == 54 && rn(4) == 0)
            fz_link_lost();                              /* a link lost with notes held */
        else if (w == 55) {
            ble_midi_dec_reset(&D);                      /* a disconnect: the next connection's decoder from zero */
            memset(&um, 0, sizeof um);
        }
#if FELUCCA_SPLIT
        else if (w == 56) {
            split_cfg[0] = (uint8_t)rn(2);              /* KEY SPLIT on / off, its channel, cut, tracks, octaves */
            split_cfg[6] = (uint8_t)rn(17);
            split_cfg[7] = (uint8_t)rn(128);
            split_cfg[2] = (uint8_t)rn(NTRK), split_cfg[3] = (uint8_t)rn(NTRK);
            split_cfg[4] = (uint8_t)(rn(7) - 3u), split_cfg[5] = (uint8_t)(rn(7) - 3u);
        }
#endif
        events_block(CTL);
        if (rn(4) == 0)
            midi_pc_poll();                              /* (the UI's: PROGRAM CHANGE and channel 16's) */
        if (rn(16) == 0)
            for (k = 0; k < NTRK; k++)
                track_render(&trk[k], out, CTL);
        if (!fz_owners_ok())
            bad_owner++;
        fz_gated += (uint32_t)fz_any_gate();
        fz_ovf += midi_in_overflow != 0;
        if (it % 2000u == 1999u) {                       /* every so often everything let go: nothing hangs */
            ble_midi_dec_reset(&D);
            memset(&um, 0, sizeof um);
            fz_release_all();
            for (k = 0; k < NTRK; k++)
                if (midi_owners[k])
                    held_after++;
            rounds++;
        }
    }
    fz_check("app: 300000 packets through bt_midi_accept, the ring, events_block, the voices: no crash (ASan / UBSan)", 1);
    fz_check("app: every decoded event well formed", fz_fails < 1000);
    fz_check("app: the MIDI ownership tables consistent after every block", !bad_owner);
    fz_check("app: after every note off and pedal up on every channel no MIDI owner is left", rounds && !held_after);
    printf("  (%u events queued, %u blocks with a voice gated, ring overflows %u)\n", (unsigned)mi_w, fz_gated, fz_ovf);
    return fz_fails ? 1 : 0;
}
#endif
