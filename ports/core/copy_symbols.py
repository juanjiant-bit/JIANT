#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
"""copy_symbols.h from the core's object (jiant_copies.h): every external symbol jiant_core.o defines, renamed per copy.
  copy_symbols.py <nm> <jiant_core.o> [--check]     (ports/core/build.sh symbols | test)
--check: 1 when the committed header misses a symbol the object defines (a new global in the firmware)."""
import re
import subprocess
import sys
from pathlib import Path

HEADER = Path(__file__).with_name("copy_symbols.h")


def symbols(nm, obj):
    out = subprocess.run([nm, "-g", "--defined-only", obj], check=True, capture_output=True, text=True).stdout
    names = set()
    for line in out.splitlines():
        f = line.split()
        if len(f) >= 2:
            names.add(f[-1])
    if "_jiant_render" in names and "jiant_render" not in names:          # Mach-O: a leading underscore
        names = {n[1:] for n in names if n.startswith("_")}
    ident = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
    reserved = re.compile(r"^(__|_[A-Z])")                                  # the C library's own
    return sorted(n for n in names if ident.match(n) and not reserved.match(n))


def committed():
    if not HEADER.exists():
        return []
    return re.findall(r"^#define (\w+) JC_SYM\(", HEADER.read_text(), re.M)


def main():
    nm, obj = sys.argv[1], sys.argv[2]
    names = symbols(nm, obj)
    if "--check" in sys.argv:
        missing = sorted(set(names) - set(committed()))
        if missing:
            print("copy_symbols.h misses " + " ".join(missing) + ": run ports/core/build.sh symbols")
            return 1
        print("copy_symbols.h: %d symbols, up to date" % len(names))
        return 0
    lines = ["/* SPDX-License-Identifier: GPL-3.0-only",
             " * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */",
             "/* Made by ports/core/copy_symbols.py (ports/core/build.sh symbols): every external symbol of jiant_core.c,",
             " * renamed per copy by jiant_copy.c (jiant_copies.h). Not edited by hand. */"]
    lines += ["#define %s JC_SYM(%s)" % (n, n) for n in names]
    HEADER.write_text("\n".join(lines) + "\n")
    print("copy_symbols.h: %d symbols" % len(names))
    return 0


if __name__ == "__main__":
    sys.exit(main())
