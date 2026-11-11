/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* JIANT's host-test kit: the verdict line the tests print, the level and spectrum measures several of them share,
 * and a noise source. Header only, static functions (a test takes what it needs; built with -Wno-unused-function
 * or -w). Plain C: no firmware type, so it can come before or after hostsim.c.
 *   jt_verdict     one result line: a tag, the claim in a column, ok / FAIL, details in parentheses if given
 *   jt_rms_*       RMS of a span (int32 or double samples), or about its mean
 *   jt_peak_*      the largest magnitude of a span, and where it is
 *   jt_fft         an in-place radix-2 FFT, its twiddles computed per point or by a running rotation
 *   jt_hann_power  the power spectrum summed over half-overlapping Hann frames
 *   jt_peak_freq   the strongest spectral line between two frequencies, between bins
 *   jt_xs_noise    a xorshift32 white noise, scaled
 * The arithmetic of each is fixed as written (the order of the floating-point operations included): the tests'
 * printed figures and their demos depend on it to the last bit. */
#ifndef JIANT_TEST_UTIL_H
#define JIANT_TEST_UTIL_H
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ------------------------------------------------------------------------------------------------ verdicts --- */
/* `tag` (e.g. "voice: "), then the claim left-aligned in `col` columns, a space, "ok" or "FAIL"; when `fmt` is not
 * NULL its text follows in parentheses after two spaces; a newline. Returns 1 for a failure, to add to a count. */
static int jt_vverdict(const char *tag, int col, const char *claim, int pass, const char *fmt, va_list ap)
{
    printf("%s%-*s %s", tag, col, claim, pass ? "ok" : "FAIL");
    if (fmt != NULL) {
        fputs("  (", stdout);
        vprintf(fmt, ap);
        putchar(')');
    }
    putchar('\n');
    return pass ? 0 : 1;
}
static int jt_verdict(const char *tag, int col, const char *claim, int pass, const char *fmt, ...)
{
    va_list ap;
    int r;
    va_start(ap, fmt);
    r = jt_vverdict(tag, col, claim, pass, fmt, ap);
    va_end(ap);
    return r;
}

/* -------------------------------------------------------------------------------------------------- levels --- */
/* the root of the mean square over [a, b); 0 when the span is empty */
static double jt_rms_i32(const int32_t *x, uint32_t a, uint32_t b)
{
    const int32_t *p = x + a, *end = x + b;
    double acc = 0;
    while (p < end) {
        double v = *p++;
        acc += v * v;
    }
    return a < b ? sqrt(acc / (b - a)) : 0;
}
static double jt_rms_d(const double *x, uint32_t a, uint32_t b)
{
    const double *p = x + a, *end = x + b;
    double acc = 0;
    while (p < end) {
        acc += *p * *p;
        p++;
    }
    return a < b ? sqrt(acc / (b - a)) : 0;
}
/* the deviation about the mean of y[0, n) (two passes: the mean, then the squares about it); the mean to *mean */
static double jt_rms_ac(const double *y, uint32_t n, double *mean)
{
    double avg = 0, dev = 0;
    uint32_t k;
    for (k = 0; k < n; k++)
        avg += y[k];
    avg /= n;
    for (k = 0; k < n; k++) {
        double d = y[k] - avg;
        dev += d * d;
    }
    if (mean != NULL)
        *mean = avg;
    return sqrt(dev / n);
}
/* the first sample of [a, b) of the largest magnitude (a for an empty span), and that magnitude (0 then) */
static uint32_t jt_peak_at(const int32_t *x, uint32_t a, uint32_t b)
{
    uint32_t at = a, k;
    for (k = a + 1; k < b; k++)
        if (abs(x[k]) > abs(x[at]))
            at = k;
    return at;
}
static int32_t jt_peak_i32(const int32_t *x, uint32_t a, uint32_t b)
{
    return a < b ? abs(x[jt_peak_at(x, a, b)]) : 0;
}

/* ------------------------------------------------------------------------------------------------ spectrum --- */
/* the forward transform (e^-i) of re + i im, n points, n a power of 2, in place. `rotate` 0: each butterfly's twiddle
 * cos / sin of its own angle; 1: the stage's twiddles by a running complex rotation (fewer cos / sin calls, its
 * rounding builds up along a stage) */
static void jt_fft(double *re, double *im, uint32_t n, int rotate)
{
    uint32_t i, r, half;
    for (i = 0, r = 0; i < n; i++) {                    /* the bit-reversed order: r walks i reversed */
        uint32_t m = n >> 1;
        if (i < r) {
            double t = re[i];
            re[i] = re[r];
            re[r] = t;
            t = im[i];
            im[i] = im[r];
            im[r] = t;
        }
        while (r & m) {                                 /* r + 1, counting from the top bit down */
            r ^= m;
            m >>= 1;
        }
        r |= m;
    }
    for (half = 1; half < n; half <<= 1) {
        uint32_t span = half << 1, base, k;
        double ang = -2 * M_PI / span, sr = cos(ang), si = sin(ang);
        for (base = 0; base < n; base += span) {
            double *ar = re + base, *ai = im + base, cr = 1, ci = 0;
            for (k = 0; k < half; k++) {
                double wr, wi, tr, ti;
                if (rotate)
                    wr = cr, wi = ci;
                else
                    wr = cos(ang * k), wi = sin(ang * k);
                tr = ar[k + half] * wr - ai[k + half] * wi;
                ti = ar[k + half] * wi + ai[k + half] * wr;
                ar[k + half] = ar[k] - tr;
                ai[k + half] = ai[k] - ti;
                ar[k] += tr;
                ai[k] += ti;
                if (rotate) {
                    double t = cr * sr - ci * si;
                    ci = cr * si + ci * sr;
                    cr = t;
                }
            }
        }
    }
}
/* the Hann window's weight at point i of len */
static double jt_hann(uint32_t i, uint32_t len) { return 0.5 - 0.5 * cos(2 * M_PI * i / len); }
/* |X|^2 of y[0, n) summed over the Hann frames of len points that fit, a frame every len / 2, into ps[0, len / 2);
 * re, im: len points of scratch. Returns the frames taken (0: ps all 0) */
static uint32_t jt_hann_power(const double *y, uint32_t n, double *ps, uint32_t len, double *re, double *im)
{
    uint32_t start, k, frames = 0;
    for (k = 0; k < len / 2; k++)
        ps[k] = 0;
    for (start = 0; start + len <= n; start += len / 2) {
        for (k = 0; k < len; k++) {
            re[k] = y[start + k] * jt_hann(k, len);
            im[k] = 0;
        }
        jt_fft(re, im, len, 0);
        for (k = 0; k < len / 2; k++)
            ps[k] += re[k] * re[k] + im[k] * im[k];
        frames++;
    }
    return frames;
}
/* in a len-point power spectrum at fs Hz, the strongest bin from lo Hz up to under hi Hz (the first of equals), moved
 * by the vertex of the parabola through the logs of it and its two neighbours: its frequency in Hz */
static double jt_peak_freq(const double *ps, uint32_t len, double fs, double lo, double hi)
{
    uint32_t k = (uint32_t)(lo / fs * len), stop = (uint32_t)(hi / fs * len), top = k;
    double below, at, above, shift;
    for (; k < stop; k++)
        if (ps[k] > ps[top])
            top = k;
    below = log(ps[top - 1] + 1e-30);
    at = log(ps[top] + 1e-30);
    above = log(ps[top + 1] + 1e-30);
    shift = (below - above) * 0.5 / (below - 2 * at + above);
    return (top + shift) * fs / len;
}

/* --------------------------------------------------------------------------------------------------- noise --- */
/* the next value of a xorshift32 state (shifts 13, 17, 5; never 0 from a non-zero seed) as a signed sample times
 * amp / 2^31 */
static int32_t jt_xs_noise(uint32_t *state, int32_t amp)
{
    uint32_t s = *state;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    *state = s;
    return (int32_t)((int64_t)(int32_t)s * amp >> 31);
}
#endif
