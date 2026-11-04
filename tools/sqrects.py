#!/usr/bin/env python3
"""Prints the pixel rects of an SQ table from port/src/sim_sprites.h: sqrects.py T_1ebd9c W H"""
import re, sys
s = open('port/src/sim_sprites.h').read()
name, W, H = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
m = re.search(name + r'\[\] = \{(.*?)\};', s, re.S)
for i, q in enumerate(re.findall(r'\{([^{}]*)\}', m.group(1))):
    v = [float(x.rstrip('f')) for x in q.split(',')]
    print(i, (round(v[0] * W), round(v[1] * H), round(v[2] * W), round(v[3] * H)), 'quad', v[4:])
