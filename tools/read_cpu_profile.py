#!/usr/bin/env python3
"""Read named Callgrind dumps without treating host instructions as device CPU.
Usage: python3 tools/read_cpu_profile.py 'build/system-cpu/cg.out*' --units 16384
For ui_cpu_profile use --units 60 (frames); kit fixture uses 65536 samples.
Peak sections in cpu_vg are whole blocks, not samples, and are kept unscaled.
"""
import argparse
import glob
import json
import re


def sections(pattern, units):
    result = {}
    for filename in glob.glob(pattern):
        with open(filename, errors="replace") as stream:
            dump = stream.read()
        name = re.search(r"^desc: Trigger: Client Request: (\S+)", dump, re.M)
        total = re.search(r"^(?:totals|summary): (\d+)", dump, re.M)
        if name and total:
            count = int(total[1])
            result[name[1]] = {"instructions": count,
                               "instructions_per_unit": count / (1 if name[1].startswith("peak/") else units)}
    return dict(sorted(result.items()))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pattern")
    parser.add_argument("--units", type=int, required=True)
    args = parser.parse_args()
    if args.units < 1:
        parser.error("units must be positive")
    print(json.dumps(sections(args.pattern, args.units), indent=2))
