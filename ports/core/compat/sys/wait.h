/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* Windows: see ../unistd.h */
#ifndef JIANT_COMPAT_SYS_WAIT_H
#define JIANT_COMPAT_SYS_WAIT_H
static int waitpid(int pid, int *st, int opt) { (void)pid; (void)opt; if (st) *st = 0; return -1; }
#define WIFEXITED(s) 1
#define WEXITSTATUS(s) 0
#define WIFSIGNALED(s) 0
#define WTERMSIG(s) 0
#endif
