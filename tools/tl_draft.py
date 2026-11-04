#!/usr/bin/env python3
"""AI drafting: sends scenes to the local `claude` CLI and stores the Russian in translation/ru/<scene>.jsonl.
  tl_draft.py [--model M] [--tag T] SCENE...     --tag writes to translation/ru/ under the scene name; see --compare
Lines already translated are kept unless --force. Lines failing the automatic checks are sent back once."""
import argparse, json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tl_common import *
from tl_ai import ask

BATCH = 120
EFFORT = None      # --effort low|medium|high: thinking budget of the claude CLI
SCHEMA = {'type': 'object', 'required': ['lines'], 'properties': {'lines': {'type': 'array', 'items': {
    'type': 'object', 'required': ['k', 'ru'], 'properties': {'k': {'type': 'string'}, 'ru': {'type': 'string'}}}}}}


# --model auto: Opus where character voice carries the text (main story, heroine routes, character events),
# Sonnet for formulaic or low-stakes text. Both at --effort low unless told otherwise. Tune after reviewing samples.
SONNET_SCENES = {'TUTORIAL', 'RANK', 'BMSR', 'BNNM', 'BMSK', 'BOTH', 'BRTA', 'BCHI', 'BMYH', '_tables', '_ui', '_names'}


def route(scene):
    key = scene if scene.startswith('_') else scene.split('_')[1]
    return 'sonnet' if key in SONNET_SCENES else 'opus'


def glossary_notes():
    out = {}
    for l in open(os.path.join(ROOT, 'translation', 'glossary_ru.tsv'), encoding='utf8'):
        f = l.rstrip('\n').split('\t') + ['', '', '', '']
        if not l.startswith('#') and f[3]:
            out[f[0]] = f[3]
    return out


def voice_sheets():
    """speaker (Japanese) -> speech sheet text from translation/characters_ru.md"""
    p = os.path.join(ROOT, 'translation', 'characters_ru.md')
    out = {}
    if os.path.exists(p):
        for part in open(p, encoding='utf8').read().split('\n## ')[1:]:
            m = re.match(r'(.*?)\((.+?), реплик', part)
            if m:
                out[m.group(2)] = '## ' + part.strip()
    return out


def summaries():
    p = os.path.join(ROOT, 'translation', 'summaries_ru.jsonl')
    out = {}
    if os.path.exists(p):
        for l in open(p, encoding='utf8'):
            r = json.loads(l)
            out[r['scene']] = r['summary']
    return out


def system_prompt(entries, gloss, scene=None):
    text = open(os.path.join(ROOT, 'translation', 'style_ru.md'), encoding='utf8').read()
    blob = ' '.join(e['jp'] + ' ' + e.get('speaker', '') for e in entries)
    notes = glossary_notes()
    terms = [f'{j} → {r}' + (f' ({notes[j]})' if notes.get(j) else '') for j, r, _ in gloss if j in blob]
    out = text + '\n\n## Глоссарий этой сцены (обязателен)\n' + '\n'.join(terms) + '\n'
    sheets = voice_sheets()
    who = sorted({e['speaker'] for e in entries if e.get('speaker') in sheets})
    if who:
        out += '\n## Речевые карты говорящих (утверждены)\n' + '\n\n'.join(sheets[w] for w in who) + '\n'
    if scene:
        sm = summaries()
        order = [x for x in scenes() if not x.startswith('_')]
        i = order.index(scene) if scene in order else -1
        prev = order[i - 1] if i > 0 and order[i - 1][:8] == scene[:8] else None
        ctx = []
        if prev and prev in sm:
            ctx.append(f'Предыдущая сцена: {sm[prev]}')
        if scene in sm:
            ctx.append(f'Эта сцена: {sm[scene]}')
        if ctx:
            out += '\n## Содержание\n' + '\n'.join(ctx) + '\n'
    return out


def item(e):
    d = {'k': e['key'], 'jp': jp_for_ai(e['jp']) if e['kind'] == 'msg' else e['jp'].replace('\n', '\\n')}
    if e['kind'] != 'msg':
        d['kind'] = e['kind']
    if e['speaker']:
        d['who'] = e['speaker']
    return d


def call(model, system, prompt):
    so, cost = ask(model, system, prompt, SCHEMA, effort=EFFORT)
    return so['lines'], cost


def draft(scene, model, force, font, gloss):
    src = scene_src(scene)
    have = {} if force else load_ru(scene)
    todo = [e for e in src if e['key'] not in have]
    spent = 0
    for i in range(0, len(todo), BATCH):
        part = todo[i:i + BATCH]
        byk = {e['key']: e for e in part}
        before = [dict(who=e['speaker'], ru=have[e['key']]['ru']) for e in src[:src.index(part[0])][-8:] if e['key'] in have]
        prompt = (f'Сцена {scene}. Переведи все строки по порядку, это единый диалог.\n'
                  + (f'Предыдущие реплики уже переведены:\n{json.dumps(before, ensure_ascii=False)}\n' if before else '')
                  + 'Верни JSON {"lines":[{"k":ключ,"ru":перевод}]}, все ключи без пропусков.\n'
                  + json.dumps([item(e) for e in part], ensure_ascii=False))
        lines, c = call(model, system_prompt(part, gloss, scene), prompt)
        spent += c
        got = {l['k']: l['ru'] for l in lines if l['k'] in byk}
        fix = []
        for e in part:
            ru = got.get(e['key'], '')
            errs, _ = check(font, e, ru)
            if errs:
                fix.append((e, ru, errs))
        if fix:
            p = 'Исправь эти строки, ошибки указаны в поле err. Верни JSON {"lines":[{"k":ключ,"ru":перевод}]}.\n' + json.dumps(
                [dict(item(e), ru=ru, err=errs) for e, ru, errs in fix], ensure_ascii=False)
            lines, c = call(model, system_prompt([f[0] for f in fix], gloss, scene), p)
            spent += c
            got.update({l['k']: l['ru'] for l in lines if l['k'] in byk})
        for e in part:
            if got.get(e['key']):
                have[e['key']] = dict(k=e['key'], ru=got[e['key']].replace('\\n', '\n'), st='draft', by=model, note='')
        save_ru(scene, have)
    return len(todo), spent


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('scenes', nargs='*')
    ap.add_argument('--all', action='store_true', help='every scene in reading order: common route, heroine routes, events, names, interface, tables')
    ap.add_argument('--model', default='auto', help='opus, sonnet, or auto (routes by scene type)')
    ap.add_argument('--effort', choices=['low', 'medium', 'high', 'xhigh', 'max'], help='thinking effort; low saves most tokens')
    ap.add_argument('--force', action='store_true')
    a = ap.parse_args()
    EFFORT = a.effort or 'low'
    font, gloss = Font(), load_glossary()
    todo = a.scenes
    if a.all:
        prio = ['MAIN', 'TUTORIAL', 'LIFE', 'SMSR', 'SMSK', 'SNNM', 'SJIN', 'SRYU', 'SOTH', 'YMSR']
        rank = lambda n: (prio.index(n.split('_')[1]) if n.split('_')[1] in prio else len(prio), n)
        story = sorted((n for n in scenes() if not n.startswith('_')), key=rank)
        todo = story + ['_names', '_ui', '_tables']
    total = 0
    for s in todo:
        try:
            n, c = draft(s, route(s) if a.model == 'auto' else a.model, a.force, font, gloss)
        except (Exception, SystemExit) as ex:       # one failed call must not stop an overnight run; rerun resumes
            print(f'{s}: FAILED {ex}', flush=True)
            continue
        total += c
        if n:
            print(f'{s}: {n} lines, ${c:.4f}, total ${total:.2f}', flush=True)
