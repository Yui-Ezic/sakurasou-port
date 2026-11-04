#!/usr/bin/env python3
"""AI edit pass (phase 4). An editor model reads a whole scene (Japanese + current Russian) and proposes changes only.
  tl_edit.py SCENE...            propose: writes translation/edits/<scene>.jsonl  (k, old, ru, why); the pack is untouched
  tl_edit.py --all               every story scene, then _names, _ui, _tables are skipped (formulaic, fit-limited)
  tl_edit.py --apply [SCENE...]  apply proposals that pass tl_common.check; sets by=edit and note=why, status stays draft
Proposals already written are kept unless --force, so an interrupted run resumes."""
import argparse, json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tl_common import *
from tl_ai import ask
from tl_draft import system_prompt, item, route

CHUNK = 150
OVERLAP = 6
EDITS = os.path.join(ROOT, 'translation', 'edits')
SCHEMA = {'type': 'object', 'required': ['changes'], 'properties': {'changes': {'type': 'array', 'items': {
    'type': 'object', 'required': ['k', 'ru', 'why'],
    'properties': {'k': {'type': 'string'}, 'ru': {'type': 'string'}, 'why': {'type': 'string'}}}}}}
RULES = ('Ты редактор русского перевода визуальной новеллы. Ниже японский оригинал и текущий русский перевод целой сцены. '
         'Читай сцену как единый текст и предлагай правки ТОЛЬКО там, где есть реальная проблема: смысл искажён или потерян, '
         'неверный род или обращение, голос не совпадает с речевой картой, несогласованность с соседними репликами или глоссарием, '
         'неестественный подстрочник, потерянная шутка, строка длиннее лимита. Не правь ради вкуса, не переписывай рабочие строки. '
         'Не добавляй фактов, не меняй ключи и маркеры †N‡. В сообщениях без переносов строк. '
         'Верни JSON {"changes":[{"k":ключ,"ru":новый перевод,"why":причина в 5-10 словах}]}. Нет проблем: пустой список.')


def path(scene):
    return os.path.join(EDITS, scene + '.jsonl')


def load(scene):
    return [json.loads(l) for l in open(path(scene), encoding='utf8') if l.strip()] if os.path.exists(path(scene)) else []


def propose(scene, model, force):
    if os.path.exists(path(scene)) and not force:
        return None, 0
    src, ru = scene_src(scene), load_ru(scene)
    src = [e for e in src if e['key'] in ru]
    seen, out, spent = set(), [], 0
    for i in range(0, len(src), CHUNK):
        part = src[i:i + CHUNK]
        ctx = src[max(0, i - OVERLAP):i]
        rows = [dict(item(e), ru=ru[e['key']]['ru'].replace('\n', '\\n')) for e in part]
        prompt = (RULES + f'\nСцена {scene}.\n'
                  + (f'Для контекста, предыдущие реплики (не править):\n{json.dumps([dict(who=e["speaker"], ru=ru[e["key"]]["ru"]) for e in ctx], ensure_ascii=False)}\n' if ctx else '')
                  + 'Строки для проверки:\n' + json.dumps(rows, ensure_ascii=False))
        so, c = ask(model, system_prompt(part, load_glossary(), scene), prompt, SCHEMA, effort='low')
        spent += c
        byk = {e['key']: e for e in part}
        for ch in so['changes']:
            k = ch['k']
            new = ch['ru'].replace('\\n', '\n')
            if k in byk and k not in seen and normalise(new) != normalise(ru[k]['ru']):
                seen.add(k)
                out.append(dict(k=k, old=ru[k]['ru'], ru=new, why=ch['why']))
    os.makedirs(EDITS, exist_ok=True)
    with open(path(scene), 'w', encoding='utf8') as f:
        for r in out:
            f.write(json.dumps(r, ensure_ascii=False) + '\n')
    return len(out), spent


def apply(scene, font, gloss):
    props, rows = load(scene), load_ru(scene)
    src = {e['key']: e for e in scene_src(scene)}
    ok = bad = 0
    for p in props:
        r = rows.get(p['k'])
        if not r or r['ru'] != p['old'] or r['st'] != 'draft':     # changed since; a human edit wins
            bad += 1
            continue
        errs, _ = check(font, src[p['k']], p['ru'], gloss)
        if errs:
            bad += 1
            continue
        r.update(ru=p['ru'], by='edit', note=p['why'])
        ok += 1
    if ok:
        save_ru(scene, rows)
    return ok, bad


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('scenes', nargs='*')
    ap.add_argument('--all', action='store_true')
    ap.add_argument('--apply', action='store_true')
    ap.add_argument('--force', action='store_true')
    ap.add_argument('--model', default='auto')
    a = ap.parse_args()
    todo = a.scenes or [s for s in scenes() if not s.startswith('_')]
    if not (a.scenes or a.all or a.apply):
        sys.exit('name scenes, or pass --all')
    if a.apply:
        font, gloss = Font(), load_glossary()
        t = [0, 0]
        for s in todo:
            if os.path.exists(path(s)):
                ok, bad = apply(s, font, gloss)
                t[0] += ok
                t[1] += bad
        print(f'applied {t[0]}, skipped {t[1]}')
        sys.exit()
    total = 0
    for s in todo:
        try:
            n, c = propose(s, route(s) if a.model == 'auto' else a.model, a.force)
        except (Exception, SystemExit) as ex:
            print(f'{s}: FAILED {ex}', flush=True)
            continue
        total += c
        if n is not None:
            print(f'{s}: {n} proposals, ${c:.4f}, total ${total:.2f}', flush=True)
