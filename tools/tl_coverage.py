#!/usr/bin/env python3
"""Compares the game's log of untranslated strings (translation/build/untranslated.tsv, written while playing with --tl)
with the extracted source. Strings found in the source are just not translated yet. Strings not found are extraction
gaps: built at run time, split into pieces, or loaded from somewhere the extractor does not know. Phase 1 is done when
a playthrough leaves no gaps."""
import json, os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tl_common import *

# Every scene the game can push (ids as in createScene), shown for a few hundred frames with some button presses.
SCENES = [0x14, 0xf, 0x10, 0x11, 0x12, 0x13, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x24,
          0x25, 0x26, 0x7, 0x8, 0x9, 0xa, 0xc, 0xd, 0xe]
# PSP masks: down, right, circle, up, left, triangle, R, L, cross. A sweep every 12 frames walks cursors, pages and tabs.
_SEQ = [0x40, 0x40, 0x20, 0x2000, 0x10, 0x80, 0x1000, 0x200, 0x100, 0x2000, 0x40, 0x2000, 0x4000, 0x4000]
KEYS = ','.join(f'{60 + 12 * i}:{_SEQ[i % len(_SEQ)]:#x}' for i in range(60))


def run():
    """--run: clears the log, then shows every scene with --tl, so the log lists what the pack does not cover."""
    log = os.path.join(BUILD, 'untranslated.tsv')
    if os.path.exists(log):
        os.remove(log)
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    for sc in SCENES:
        subprocess.run([os.path.join(ROOT, 'port', 'build', 'sakurasou'), '--tl', BUILD, '--headless', '--scene', hex(sc),
                        '--frames', '900', '--keys', KEYS], env=env, capture_output=True, timeout=300)


if '--run' in sys.argv:
    run()

known = {}
for s in scenes():
    for e in scene_src(s):
        if e['kind'] != 'msg':
            known[e['key']] = s
            for line in e['jp'].split('\n'):     # the engine also looks up single lines of multi-line texts
                known.setdefault(key(line), s)
blob = '\n'.join(e['jp'] for sc in scenes() for e in scene_src(sc) if e['kind'] != 'msg')   # pieces of a split long text are explained too
done = set()
for s in scenes():
    done |= {k for k, r in load_ru(s).items() if r.get('st') != 'reject'}
log = os.path.join(BUILD, 'untranslated.tsv')
gaps, todo, seen = [], 0, set()
if not os.path.exists(log):      # the engine creates the log on the first untranslated string
    print('no untranslated string was shown: log is empty')
    sys.exit(0)
for l in open(log, encoding='utf8', errors='replace'):
    kind, _, jp = l.rstrip('\n').partition('\t')
    jp = jp.replace('\\n', '\n')
    if kind != 'ui' or jp in seen:
        continue
    seen.add(jp)
    k = key(jp)
    if k in known or jp in blob:
        todo += 1
    else:
        gaps.append(jp)
print(f'{len(seen)} Japanese strings reached the screen without translation: {todo} are in the source, {len(gaps)} are gaps')
for g in gaps:
    print('GAP', repr(g))
