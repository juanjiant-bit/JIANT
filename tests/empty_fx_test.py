#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
import re, subprocess, sys
hashes=[]
for mode in ('reference','sleep'):
    r=subprocess.run([sys.argv[1],mode],capture_output=True,text=True,check=True)
    print(r.stdout.strip())
    hashes.append(re.search(r'hash=([0-9a-f]+)',r.stdout)[1])
if hashes[0]!=hashes[1]: raise SystemExit('empty FX shortcut changed audio or state')
print('empty-fx: full audio and state match PASS')
