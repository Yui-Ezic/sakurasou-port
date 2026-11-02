#!/usr/bin/env python3
"""Dumps every translatable string to work/tl/src/<scene>.json, with speaker and order.
Keys are hashes of the Japanese, so translations in git do not contain the original text."""
import glob, json, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ssbdis as D
from tl_common import ROOT, DATA, SRC, key

SKIP = re.compile(r'^(script_|SCR_)')   # debug, test and trial scripts: never reached in the retail flow
UI_FILES = 'adv common menus menus_misc menus_omake menus_saveload scenes_boot scenes_sim sim_action sim_calendar sim_chart sim_choice sim_contest sim_tasklist'.split()


def is_text(s):
    return bool(re.search(r'[\u3000-\u9fff\uff00-\uffef]', s)) and '未定義' not in s and 'デバッグ' not in s


def scene(path):
    d, w = D.load(path)
    code = D.walk(w)
    st, out, speaker, hint, names = [], [], '', '', set()
    seen = set()

    def add(kind, jp, ctx='', **kw):
        k = key(jp, ctx)
        if k in seen:
            return
        seen.add(k)
        out.append(dict(key=k, kind=kind, jp=jp, speaker=speaker, hint=hint, **kw))

    for pc in sorted(code):
        op, a = code[pc]
        if op == 2:
            st.append(a[0])
        elif op == 3:
            st.append(D.s(d, a[0]))
        elif op in (4, 5):
            st.append(None)
        elif op == 14:
            argc, nid = a
            args = st[-argc:] if argc else []
            st = []
            sa = lambda i: args[i] if i < len(args) and isinstance(args[i], str) else ''
            if nid == 129:
                speaker = sa(0)
                if speaker:
                    names.add(speaker)
            elif nid in (161, 179) and sa(2 if nid == 161 else 0):
                names.add(sa(2 if nid == 161 else 0))
            elif nid == 128:
                speaker = ''
            elif nid == 125:
                hint = sa(0)
            elif nid == 126 and sa(1):
                add('msg', sa(1), str(args[2]), id=args[2], voice=sa(0))
            elif nid in (135, 136):
                for line in sa(0 if nid == 135 else 1).split('\n'):
                    if line:
                        add('choice', line)
            elif nid == 140 and sa(1):
                add('cloud', sa(1))
        elif op == 13:      # cloud text goes through a script function, not straight to native 140
            for v in st:
                if isinstance(v, str) and is_text(v):
                    add('cloud', v)
            st = []
        elif op == 15:      # character-name lookup functions return the name
            for v in st:
                if isinstance(v, str) and is_text(v):
                    names.add(v)
            st = []
        elif op in (8, 9, 0, 6):
            st = []
    return out, names


def literals():
    out, seen = [], set()
    for f in UI_FILES:
        s = open(os.path.join(ROOT, 'port', 'src', f + '.cpp'), encoding='utf8').read()
        s = re.sub(r'//.*', '', s)
        for m in re.finditer(r'"((?:[^"\\\n]|\\.)*)"', s):
            t = m.group(1).replace('\\n', '\n').replace('\\"', '"')
            if re.search(r'[　-鿿＀-￯]', t) and t not in seen and len(t) > 1 and '�' not in t:
                seen.add(t)
                out.append(dict(key=key(t), kind='ui', jp=t, speaker='', hint=f))
    return out


def main():
    os.makedirs(SRC, exist_ok=True)
    names, total = set(), 0
    found = {}
    for p in sorted(glob.glob(os.path.join(DATA, '*.ssb'))):
        n = os.path.basename(p)[:-4]
        if SKIP.match(n):
            continue
        found[n], nm = scene(p)
        names |= nm
    for n, e in found.items():      # the wrapper also receives the speaker, so it is a name, not cloud text
        e = [x for x in e if not (x['kind'] == 'cloud' and x['jp'] in names)]
        total += len(e)
        json.dump(e, open(os.path.join(SRC, n + '.json'), 'w', encoding='utf8'), ensure_ascii=False, indent=0)
    json.dump([dict(key=key(t), kind='name', jp=t, speaker='', hint='') for t in sorted(names)],
              open(os.path.join(SRC, '_names.json'), 'w', encoding='utf8'), ensure_ascii=False, indent=0)
    ui = literals()
    json.dump(ui, open(os.path.join(SRC, '_ui.json'), 'w', encoding='utf8'), ensure_ascii=False, indent=0)
    print(f'{total} script strings, {len(names)} names, {len(ui)} interface strings')


main()
