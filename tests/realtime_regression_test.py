#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exercise production mailbox submission under a pending USB job."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]


def function(path, start, end):
    source = (root / path).read_text()
    return source[source.index(start):source.index(end, source.index(start))]


submit = function('firmware/hal/fm1_multicore.h', 'static inline int fm1_mc_run(',
                  'static inline int fm1_mc_done(')
code = r'''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#define FM1_C1_SYNC() ((void)0)
#define FM1_C1_CHECK(fn,arg,job) ((uint32_t)(uintptr_t)(fn) ^ (arg) ^ (job) * 0x9E3779B1u)
static uint32_t fm1_mc_on, fm1_mc_lost;
static struct { uint32_t count; } fm1_core1_fault;
static struct { uint32_t job, done, arg, check; void (*fn)(uint32_t); } fm1_mc_box;
static void fm1_mc_hold(void) { fm1_mc_on = 0; }
static void resample(uint32_t a) { (void)a; }
static void mix(uint32_t a) { (void)a; }
@SUBMIT@
int main(void) {
    fm1_mc_on = 1;
    assert(fm1_mc_run(resample, 128));
    uint32_t check = fm1_mc_box.check;
    /* Core 1 has not read the USB job yet; a late audio IRQ wants the mixer. */
    assert(!fm1_mc_run(mix, 64));
    assert(fm1_mc_box.job == 1 && fm1_mc_box.arg == 128);
    assert(fm1_mc_box.fn == resample && fm1_mc_box.check == check);
    fm1_mc_box.done = 1;
    assert(fm1_mc_run(mix, 64) && fm1_mc_box.job == 2);
    fm1_mc_box.done = 2;
    fm1_core1_fault.count = 1;
    assert(!fm1_mc_run(mix, 64) && !fm1_mc_on && fm1_mc_lost == 3);
    fm1_core1_fault.count = 0; fm1_mc_on = 1;
    fm1_mc_box.job = fm1_mc_box.done = UINT32_MAX;
    assert(fm1_mc_run(mix, 64) && fm1_mc_box.job == 0);
    puts("realtime: mailbox ownership/fault/wrap PASS");
}
'''.replace('@SUBMIT@', submit)
with tempfile.TemporaryDirectory() as tmp:
    source, binary = Path(tmp) / 'test.c', Path(tmp) / 'test'
    source.write_text(code)
    subprocess.run(['cc', '-O2', '-Wall', '-Wextra', '-fsanitize=undefined',
                    '-fno-sanitize-recover=all', str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
