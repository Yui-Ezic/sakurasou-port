#!/usr/bin/env python3
"""AI drafting: sends scenes to the local `claude` CLI and stores the Russian in translation/ru/<scene>.jsonl.
  tl_draft.py [--model M] [--tag T] SCENE...     --tag writes to translation/ru/ under the scene name; see --compare
Lines already translated are kept unless --force. Lines failing the automatic checks are sent back once."""
import argparse, datetime, json, os, re, subprocess, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tl_common import *

BATCH = 120
SCHEMA = {'type': 'object', 'required': ['lines'], 'properties': {'lines': {'type': 'array', 'items': {
    'type': 'object', 'required': ['k', 'ru'], 'properties': {'k': {'type': 'string'}, 'ru': {'type': 'string'}}}}}}


def system_prompt(entries, gloss):
    text = open(os.path.join(ROOT, 'translation', 'style_ru.md'), encoding='utf8').read()
    blob = ' '.join(e['jp'] + ' ' + e.get('speaker', '') for e in entries)
    terms = [f'{j} → {r}' for j, r, _ in gloss if j in blob]
    return text + '\n\n## Глоссарий этой сцены (обязателен)\n' + '\n'.join(terms) + '\n'


def item(e):
    d = {'k': e['key'], 'jp': jp_for_ai(e['jp']) if e['kind'] == 'msg' else e['jp'].replace('\n', '\\n')}
    if e['kind'] != 'msg':
        d['kind'] = e['kind']
    if e['speaker']:
        d['who'] = e['speaker']
    return d


def call(model, system, prompt):
    with tempfile.TemporaryDirectory() as d:      # empty cwd: no project context in the request
        sp = os.path.join(d, 'system.md')
        open(sp, 'w', encoding='utf8').write(system)
        r = subprocess.run(['claude', '-p', '--model', model, '--output-format', 'json', '--tools', '',
                            '--system-prompt-file', sp, '--setting-sources', '', '--no-session-persistence',
                            '--json-schema', json.dumps(SCHEMA)],
                           input=prompt, capture_output=True, text=True, cwd=d, timeout=1200)
    if r.returncode:
        raise SystemExit(f'claude failed: {r.stdout[:400]} {r.stderr[:400]}')
    o = json.loads(r.stdout)
    os.makedirs(os.path.join(ROOT, 'work', 'tl'), exist_ok=True)
    with open(os.path.join(ROOT, 'work', 'tl', 'usage.jsonl'), 'a') as f:
        f.write(json.dumps(dict(model=model, cost=o.get('total_cost_usd'), usage=o.get('usage'), ms=o.get('duration_ms'),
                                time=datetime.datetime.now().isoformat())) + '\n')
    so = o.get('structured_output')
    if so is None:
        m = re.search(r'\{.*\}', o.get('result', ''), re.S)
        so = json.loads(m.group())
    return so['lines'], o.get('total_cost_usd') or 0


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
        lines, c = call(model, system_prompt(part, gloss), prompt)
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
            lines, c = call(model, system_prompt([f[0] for f in fix], gloss), p)
            spent += c
            got.update({l['k']: l['ru'] for l in lines if l['k'] in byk})
        for e in part:
            if got.get(e['key']):
                have[e['key']] = dict(k=e['key'], ru=got[e['key']].replace('\\n', '\n'), st='draft', by=model, note='')
        save_ru(scene, have)
    return len(todo), spent


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('scenes', nargs='+')
    ap.add_argument('--model', default='sonnet')
    ap.add_argument('--force', action='store_true')
    a = ap.parse_args()
    font, gloss = Font(), load_glossary()
    for s in a.scenes:
        n, c = draft(s, a.model, a.force, font, gloss)
        print(f'{s}: {n} lines, ${c:.4f}', flush=True)
