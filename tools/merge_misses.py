#!/usr/bin/env python3
"""Append [[extra_func]] proposals from recomp_master_misses_AWAE.toml.frag to game.toml (dedup by addr)."""
import re, sys
toml = 'variants/wario_land_4/game.toml'
frag = sys.argv[1] if len(sys.argv) > 1 else 'recomp_master_misses_AWAE.toml.frag'
have = set(int(a, 16) for a in re.findall(r'^addr\s*=\s*(0x[0-9A-Fa-f]+)', open(toml).read(), re.M))
add = []
for a, m in re.findall(r'\[\[extra_func\]\]\s*addr\s*=\s*(0x[0-9A-Fa-f]+)\s*mode\s*=\s*"(\w+)"', open(frag).read()):
    if int(a, 16) not in have:
        have.add(int(a, 16)); add.append((a, m))
with open(toml, 'a') as f:
    for a, m in add:
        f.write(f'\n[[extra_func]]\naddr = {a}\nmode = "{m}"\nnote = "self-heal miss"\n')
print(f'added {len(add)}')
