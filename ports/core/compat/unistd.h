/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* Windows: the POSIX calls the host tests' main uses (tests/hostsim.c), never called by the ports; on the include path
 * only for Windows builds (ports/juce-plugin/CMakeLists.txt) */
#ifndef JIANT_COMPAT_UNISTD_H
#define JIANT_COMPAT_UNISTD_H
#include <stdio.h>
typedef int pid_t;
static int pipe(int fd[2]) { (void)fd; return -1; }
static pid_t fork(void) { return -1; }
static int close(int fd) { (void)fd; return -1; }
static long read(int fd, void *b, unsigned long n) { (void)fd; (void)b; (void)n; return -1; }
static long write(int fd, const void *b, unsigned long n) { (void)fd; (void)b; (void)n; return -1; }
static void _exit(int code) { (void)code; for (;;) {} }
#endif
