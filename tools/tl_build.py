#!/usr/bin/env python3
"""Builds translation/build/ru.pack from translation/ru/*.jsonl and prints the automatic check report.
Rejected lines stay out of the pack, so the game shows the Japanese there."""
import glob, json, os, shutil, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tl_common import *


def build(report=True):
    font, gloss = Font(), load_glossary()
    pack, errs, warns, total, by_status = {}, [], [], 0, {}
    for p in sorted(glob.glob(os.path.join(RU, '*.jsonl'))):
        scene = os.path.basename(p)[:-6]
        src = {e['key']: e for e in scene_src(scene)}
        rows = load_ru(scene)
        total += len(src)
        for k, r in rows.items():
            e = src.get(k)
            if e is None:
                errs.append((scene, k, 'ключ устарел: нет такой строки в оригинале', r['ru']))
                continue
            by_status[r['st']] = by_status.get(r['st'], 0) + 1
            er, wa = check(font, e, r['ru'], gloss)
            errs += [(scene, k, m, r['ru']) for m in er]
            warns += [(scene, k, m, r['ru']) for m in wa]
            if r['st'] == 'reject' or er:
                continue
            v = normalise(r['ru'])
            if e['jp'].endswith('▼'):
                v += '▼'
            pack[k] = v.replace('\\', '\\\\').replace('\n', '\\n')
    os.makedirs(BUILD, exist_ok=True)
    tmp = os.path.join(BUILD, 'ru.pack.tmp')
    with open(tmp, 'w', encoding='utf8', newline='\n') as f:
        f.write('#SAKURA-TL 1\n')
        for k in sorted(pack):
            f.write(f'{k}\t{pack[k]}\n')
    os.replace(tmp, os.path.join(BUILD, 'ru.pack'))
    rel = os.path.join(os.path.dirname(BUILD), 'release')       # tracked copy: releases are built from it, without the ISO
    os.makedirs(rel, exist_ok=True)
    shutil.copyfile(os.path.join(BUILD, 'ru.pack'), os.path.join(rel, 'ru.pack'))
    if report:
        print(f'{len(pack)} строк в пакете из {total}; статусы: {by_status}; ошибок {len(errs)}, предупреждений {len(warns)}')
        for s, k, m, ru in errs[:40]:
            print(f'ОШИБКА {s} {k}: {m}\n    {ru}')
        for s, k, m, ru in warns[:40]:
            print(f'внимание {s}: {m}: {ru}')
    return not errs


def verify(dump):
    """Compares the wrap of the C++ engine (sakurasou --tl-dump FILE) with tl_common.wrap."""
    font, bad, n = Font(), 0, 0
    pack = dict(l.rstrip('\n').split('\t', 1) for l in open(os.path.join(BUILD, 'ru.pack'), encoding='utf8') if '\t' in l)
    for l in open(dump, encoding='utf8'):
        k, cpp = l.rstrip('\n').split('\t', 1)
        n += 1
        if wrap(font, pack[k].replace('\\n', '\n')).replace('\n', '\\n') != cpp:
            bad += 1
    print(f'{n} строк сверено с движком, расхождений {bad}')
    return not bad


if __name__ == '__main__':
    if len(sys.argv) > 2 and sys.argv[1] == '--verify':
        sys.exit(0 if verify(sys.argv[2]) else 1)
    sys.exit(0 if build() else 1)
