#!/usr/bin/env python3
"""Phase 2: context for the drafting pass, made by an AI read of the whole corpus.
  tl_context.py terms [--model M] [--limit N]   candidate terms -> translation/glossary_proposal.tsv (a person approves it)
  tl_context.py summaries [--model M]           one paragraph per scene -> translation/summaries_ru.jsonl
  tl_context.py voices [--model M]              speech sheet per speaker -> translation/characters_ru.md
Nothing here is used by the drafter until a person has approved it (see docs/TRANSLATION_PLAN.md)."""
import argparse, collections, json, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tl_common import *
from tl_ai import ask

PROPOSAL = os.path.join(ROOT, 'translation', 'glossary_proposal.tsv')
VOICES = os.path.join(ROOT, 'translation', 'characters_ru.md')
NAME_PARTS = '空太 美咲 乙羽 七海 龍之介 青山 神田 中野 椎名 赤坂 三鷹 千尋 千石 上井草 ましろ 仁 リタ 優子 沙織 姫宮 品川 虎次郎'.split()


def corpus():
    """All story lines in reading order: (scene, speaker, text)."""
    out = []
    for s in scenes():
        if s.startswith('_'):
            continue
        for e in scene_src(s):
            if e['kind'] == 'msg':
                out.append((s, e['speaker'], jp_for_ai(e['jp'])))
    return out


def mine(lines, gloss_jp):
    """Katakana words (3+ letters) and kanji compounds (2+) that recur across scenes, with sample lines."""
    count, scn, ex = collections.Counter(), collections.defaultdict(set), collections.defaultdict(list)
    for s, who, t in lines:
        for m in re.finditer(r'[ァ-ヶー・]{3,}|[一-龥々]{2,8}', t):
            w = m.group()
            count[w] += 1
            scn[w].add(s)
            if len(ex[w]) < 3 and len(t) <= 70 and all(x[1] != who for x in ex[w]):
                ex[w].append((t, who))
    cands = []
    for w, n in count.items():
        kata = bool(re.match('[ァ-ヶー・]', w))
        if n < (3 if kata else 8) or len(scn[w]) < 2:
            continue
        if any(p in w for p in NAME_PARTS) or any(w in g for g in gloss_jp):
            continue
        cands.append((w, n, len(scn[w]), ex[w]))
    cands.sort(key=lambda c: -c[1])
    return cands


TERMS_SCHEMA = {'type': 'object', 'required': ['terms'], 'properties': {'terms': {'type': 'array', 'items': {
    'type': 'object', 'required': ['id', 'keep'], 'properties': {
        'id': {'type': 'integer'}, 'keep': {'type': 'boolean'}, 'ru': {'type': 'string'}, 'stem': {'type': 'string'},
        'note': {'type': 'string'}}}}}}

TERMS_PROMPT = """Ты ведущий локализатор русского перевода визуальной новеллы «Sakurasou no Pet na Kanojo» (PSP). Ниже руководство
по стилю и действующий глоссарий. Тебе дают кандидатов в термины: слова из японского текста игры, которые часто повторяются.
Для каждого реши:
- keep=true, только если слово нужно перевести ОДИНАКОВО во всём тексте: имя собственное, место, организация, название игры
  или предмета в сюжете, профессиональный термин (разработка игр, конкурс, симуляция в игре), повторяющаяся шутка или фирменное
  слово персонажа, термин игровой механики. Включай и слова, у которых есть несколько заметно разных русских вариантов, если
  выбор влияет на сюжет или шутку.
- keep=false для обычных слов, перевод которых естественно меняется от контекста («大丈夫», «今日», «時間», «確認»).
- ru: русский перевод для keep=true, как он должен стоять в тексте (начальная форма). Система Поливанова для имён.
- stem: часть русского слова, общая для всех его падежных форм, строчными, не короче 4 знаков; пусто, если термин из нескольких
  слов или меняется сильно.
- note: до 90 знаков по-русски: что это и почему так, либо запрещённый вариант. Пусто, если и так ясно.
Верни все id из входа. Для keep=false остальные поля не нужны."""


def cmd_terms(a):
    lines = corpus()
    gloss = load_glossary()
    cands = mine(lines, [g[0] for g in gloss])
    # TIPS titles are the game's own vocabulary; the text of a tip tells what the word is
    tips = [e for e in scene_src('_tables') if 'tip title' in e['hint']]
    tipbody = {}
    for e in scene_src('_tables'):
        if 'tip text' in e['hint']:
            tipbody[e['jp'][:6]] = e['jp'].replace('\n', '')
    seen = {c[0] for c in cands}
    for e in tips:
        if e['jp'] not in seen and not any(g[0] == e['jp'] for g in gloss):
            cands.append((e['jp'], 0, 0, [('(заголовок справки TIPS)', '')]))
    if a.limit:
        cands = cands[:a.limit]
    print(f'{len(cands)} candidates', flush=True)
    system = open(os.path.join(ROOT, 'translation', 'style_ru.md'), encoding='utf8').read() + \
        '\n\n## Действующий глоссарий\n' + '\n'.join(f'{j} → {r}' for j, r, _ in gloss) + '\n\n' + TERMS_PROMPT
    rows, spent = [], 0
    for i in range(0, len(cands), 60):
        part = cands[i:i + 60]
        items = [dict(id=i + k, jp=w, uses=n, scenes=sc, examples=[dict(who=who, text=t) for t, who in ex[:2]])
                 for k, (w, n, sc, ex) in enumerate(part)]
        out, c = ask(a.model, system, 'Кандидаты:\n' + json.dumps(items, ensure_ascii=False), TERMS_SCHEMA)
        spent += c
        got = {t['id']: t for t in out['terms']}
        for k, (w, n, sc, ex) in enumerate(part):
            t = got.get(i + k)
            if t and t.get('keep') and t.get('ru'):
                rows.append((w, t['ru'].strip(), (t.get('stem') or '').strip(), (t.get('note') or '').strip(), n))
        print(f'  {min(i + 60, len(cands))}/{len(cands)} kept {len(rows)} ${spent:.2f}', flush=True)
    with open(PROPOSAL, 'w', encoding='utf8') as f:
        f.write('# ПРЕДЛОЖЕНИЕ, не утверждено. Формат как у glossary_ru.tsv; последний столбец: сколько раз слово встречается.\n'
                '# Утверждённые строки переносятся в glossary_ru.tsv. Исправляй русский столбец прямо здесь.\n'
                '# Примечания написаны ИИ по двум-трём примерам из текста и могут ошибаться в фактах; проверяй их.\n'
                '# Примечания написаны ИИ по двум-трём примерам из текста и могут ошибаться в фактах; проверяй их.\n')
        for w, ru, stem, note, n in rows:
            f.write(f'{w}\t{ru}\t{stem}\t{note}\t{n}\n')
    print(f'{len(rows)} terms -> {PROPOSAL}, ${spent:.2f}')


SUFFIXES = 'くん 君 さん 先輩 先生 ちゃん 様 殿 っち 嬢 氏 ちん'.split()
VOICE_SCHEMA = {'type': 'object', 'required': ['gender', 'voice', 'address', 'tics'], 'properties': {
    'gender': {'type': 'string'}, 'voice': {'type': 'string'}, 'formality': {'type': 'string'},
    'address': {'type': 'array', 'items': {'type': 'object', 'required': ['to', 'ru'], 'properties': {
        'to': {'type': 'string'}, 'jp': {'type': 'string'}, 'ru': {'type': 'string'}}}},
    'tics': {'type': 'array', 'items': {'type': 'object', 'required': ['jp', 'ru'], 'properties': {
        'jp': {'type': 'string'}, 'ru': {'type': 'string'}, 'note': {'type': 'string'}}}},
    'avoid': {'type': 'string'}}}

VOICE_PROMPT = """Ты ведущий локализатор русского перевода визуальной новеллы «Sakurasou no Pet na Kanojo». Ниже реплики одного
персонажа (выборка по всей игре, по порядку), статистика обращений и руководство по стилю. Составь для переводчиков КРАТКУЮ
речевую карту этого персонажа по-русски.
- gender: «м» или «ж» (по тексту и по руководству).
- voice: 2-4 предложения: как он говорит (темп, длина фраз, регистр, юмор, эмоции) и как это передать по-русски.
- formality: ты или вы по отношению к основным собеседникам, кому и как.
- address: как он называет других и как это писать по-русски (кто, японское обращение, русский вариант; суффиксы по руководству).
  Используй статистику обращений; не выдумывай обращений, которых нет в данных.
- tics: до 8 повторяющихся оборотов, словечек, концовок фраз, междометий: японский оборот, русский эквивалент, пометка.
  Только то, что видно в выборке.
- avoid: чего в речи персонажа быть не должно (необязательно).
Опирайся только на выборку. Не пересказывай сюжет."""


def address_stats(lines, speaker):
    """How the speaker's lines name other people: counts of NAME+suffix and bare names."""
    c = collections.Counter()
    for s, who, t in lines:
        if who != speaker:
            continue
        for m in re.finditer('(' + '|'.join(NAME_PARTS + ['お前', 'あんた', 'あなた', 'キミ', '君']) + ')(' + '|'.join(SUFFIXES) + ')?', t):
            c[m.group()] += 1
    return c.most_common(25)


def cmd_voices(a):
    lines = corpus()
    by = collections.defaultdict(list)
    for s, who, t in lines:
        if who and len(t) >= 6:
            by[who].append((s, t))
    total = collections.Counter(who for _, who, _ in lines)
    style = open(os.path.join(ROOT, 'translation', 'style_ru.md'), encoding='utf8').read()
    gloss = '\n'.join(f'{j} → {r}' for j, r, _ in load_glossary())
    system = style + '\n\n## Глоссарий\n' + gloss + '\n\n' + VOICE_PROMPT
    names = {j: r for j, r, _ in load_glossary()}
    out, spent = [], 0
    speakers = [w for w, n in total.most_common() if n >= (a.min_lines or 50) and w]
    for who in speakers:
        ls = by[who]
        step = max(1, len(ls) // 90)
        sample = [t for _, t in ls[::step]][:90]
        prompt = (f'Персонаж: {who} ({names.get(who, "")}), всего реплик: {total[who]}.\n'
                  f'Статистика обращений: {json.dumps(address_stats(lines, who), ensure_ascii=False)}\n'
                  'Реплики:\n' + '\n'.join(sample))
        sheet, c = ask(a.model, system, prompt, VOICE_SCHEMA)
        spent += c
        out.append((who, total[who], sheet))
        print(f'  {who}: ${spent:.2f}', flush=True)
    with open(VOICES, 'w', encoding='utf8') as f:
        f.write('# Речевые карты персонажей (ПРЕДЛОЖЕНИЕ, не утверждено)\n\n'
                'Составлены ИИ по выборке реплик (до 90 на персонажа) и статистике обращений. Человек правит и утверждает до черновика.\n')
        for who, n, sh in out:
            f.write(f'\n## {names.get(who, who)} ({who}, реплик: {n})\n\n')
            f.write(f"- Род: {sh['gender']}. Обращения на «ты»/«вы»: {sh.get('formality', '')}\n- Речь: {sh['voice']}\n")
            if sh['address']:
                f.write('- Обращения: ' + '; '.join(f"{x['to']}: {x.get('jp', '')} → {x['ru']}" for x in sh['address']) + '\n')
            for t in sh['tics']:
                f.write(f"- Оборот: {t['jp']} → {t['ru']}" + (f" ({t['note']})" if t.get('note') else '') + '\n')
            if sh.get('avoid'):
                f.write(f"- Избегать: {sh['avoid']}\n")
    print(f'{len(out)} sheets -> {VOICES}, ${spent:.2f}')


SUMMARIES = os.path.join(ROOT, 'translation', 'summaries_ru.jsonl')
SUM_SCHEMA = {'type': 'object', 'required': ['scenes'], 'properties': {'scenes': {'type': 'array', 'items': {
    'type': 'object', 'required': ['scene', 'summary'], 'properties': {'scene': {'type': 'string'}, 'summary': {'type': 'string'}}}}}}
SUM_PROMPT = """Ты помощник переводчика визуальной новеллы «Sakurasou no Pet na Kanojo». Для каждой сцены напиши по-русски краткое
содержание в 1-3 предложения: кто участвует, где и когда (если видно), что происходит и чем кончается, какой тон. Это справка
для переводчика, который переведёт сцену по частям: упомяни всё, что влияет на выбор слов (кто кому что обещал, шутки, кто с кем
в каких отношениях, чья реплика к кому обращена). Имена бери из глоссария. Не выдумывай того, чего нет в тексте.
Верни по записи на каждую сцену, ключ scene копируй точно."""


def cmd_summaries(a):
    gloss = '\n'.join(f'{j} → {r}' for j, r, _ in load_glossary())
    system = open(os.path.join(ROOT, 'translation', 'style_ru.md'), encoding='utf8').read() + '\n\n## Глоссарий\n' + gloss + '\n\n' + SUM_PROMPT
    done = {}
    if os.path.exists(SUMMARIES) and not a.force:
        for l in open(SUMMARIES, encoding='utf8'):
            r = json.loads(l); done[r['scene']] = r['summary']
    texts = {}
    for s in scenes():
        if s.startswith('_') or s in done:
            continue
        texts[s] = '\n'.join((f'{e["speaker"]}: ' if e['speaker'] else '') + jp_for_ai(e['jp']) for e in scene_src(s) if e['kind'] == 'msg')
    todo = [s for s in texts if texts[s].strip()]
    batches, cur, size = [], [], 0
    for s in todo:
        t = texts[s][:9000]
        if cur and size + len(t) > 28000:
            batches.append(cur); cur, size = [], 0
        cur.append((s, t)); size += len(t)
    if cur:
        batches.append(cur)
    spent = 0
    for i, b in enumerate(batches):
        prompt = '\n\n'.join(f'=== {s} ===\n{t}' for s, t in b)
        out, c = ask(a.model, system, prompt, SUM_SCHEMA)
        spent += c
        for r in out['scenes']:
            if r['scene'] in texts:
                done[r['scene']] = r['summary'].strip()
        with open(SUMMARIES, 'w', encoding='utf8') as f:
            for s in sorted(done):
                f.write(json.dumps(dict(scene=s, summary=done[s]), ensure_ascii=False) + '\n')
        print(f'  batch {i + 1}/{len(batches)}: {len(done)} scenes, ${spent:.2f}', flush=True)
    print(f'{len(done)} summaries -> {SUMMARIES}')


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('cmd', choices=['terms', 'voices', 'summaries'])
    ap.add_argument('--model', default='opus')
    ap.add_argument('--force', action='store_true')
    ap.add_argument('--limit', type=int, default=0)
    ap.add_argument('--min-lines', type=int, default=0, help='voices: speakers with at least this many lines (default 50)')
    a = ap.parse_args()
    {'terms': cmd_terms, 'voices': cmd_voices, 'summaries': cmd_summaries}[a.cmd](a)
