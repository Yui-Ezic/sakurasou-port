"""Shared code for the translation tools. The hash, font metrics and wrap rule mirror port/src/tr.cpp and gfx.cpp."""
import json, os, re, struct, sys, unicodedata

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.path.join(ROOT, 'work', 'data')          # extracted DATA.CPK (tools/cpk.py); not in git
SRC = os.path.join(ROOT, 'work', 'tl', 'src')      # Japanese source per scene, from tl_extract.py; not in git
RU = os.path.join(ROOT, 'translation', 'ru')       # translations: key + Russian only, no Japanese
BUILD = os.path.join(ROOT, 'translation', 'build')
MSG_W = 396                                        # kTrMsgWidth
LIMITS = {'name': 152, 'choice': 236, 'cloud': 136}  # px per line
MAX_LINES = {'cloud': 3, 'choice': 1, 'name': 1, 'msg': 3}


def key(jp, ctx=''):
    h = 0xcbf29ce484222325
    for b in ctx.encode() + b'\0' + jp.encode():
        h = ((h ^ b) * 0x100000001b3) & 0xffffffffffffffff
    return f'{h:016x}'


class Font:
    def __init__(self, name='font_m'):
        self.d = open(os.path.join(DATA, name + '.fnt'), 'rb').read()
        self.sz = struct.unpack('<I', self.d[8:12])[0]
        self.idx = struct.unpack('<65536H', self.d[0x10:0x20010])
        self.cache = {}

    def has(self, cp):
        return cp == 0x20 or (cp < 0x10000 and self.idx[cp] != 0)

    def pixels(self, cp):
        sz = self.sz
        o = 0x20010 + self.idx[cp] * (sz * sz // 2)
        b = self.d[o:o + sz * sz // 2]
        return [[(b[y * (sz // 2) + x // 2] >> 4 if x & 1 else b[y * (sz // 2) + x // 2] & 15) for x in range(sz)]
                for y in range(sz)]

    def metrics(self, cp):
        """(lead, advance) as Font::propLead / propAdv"""
        if cp in self.cache:
            return self.cache[cp]
        if cp >= 0x2e80:
            r = (0, self.sz)
        else:
            cols = []
            if self.idx[cp]:
                p = self.pixels(cp)
                cols = [x for x in range(self.sz) if any(row[x] > 2 for row in p)]
            r = (min(cols), max(cols) - min(cols) + 2) if cols else (0, 4 if self.sz > 11 else 3)
        self.cache[cp] = r
        return r

    def width(self, s):
        return sum(self.metrics(ord(c))[1] for c in s)


TOKEN = re.compile(r'†[^‡]*‡|‡|▼')


def wrap(font, s, width=MSG_W):
    """Same greedy wrap as wrapMessage in tr.cpp. Returns the text with line breaks."""
    out, x, after, last = [], 0, 0, None
    i = 0
    while i < len(s):
        m = TOKEN.match(s, i)
        if m:
            out.append(m.group())
            i = m.end()
            continue
        c = s[i]
        i += 1
        if c == '\n':
            out.append(c)
            x = after = 0
            last = None
            continue
        a = font.metrics(ord(c))[1]
        if c == ' ':
            last = len(out)
            after = 0
        else:
            if x + a > width and last is not None:
                out[last] = '\n'
                x, last = after, None
            after += a
        out.append(c)
        x += a
    return ''.join(out)


def normalise(ru):
    """Fix what the font cannot show or the engine would misread."""
    ru = ru.replace('\\n', '\n').replace('—', '―').replace('–', '―').replace('...', '…').replace(' ', ' ')
    return re.sub(r' {2,}', ' ', ru).strip()


def jp_for_ai(jp):
    return jp.replace('▼', '').replace('\n', ' ').replace('　', ' ').strip()


def has_japanese(s):
    return any(unicodedata.name(c, '').startswith(('CJK', 'HIRAGANA', 'KATAKANA')) for c in s if ord(c) > 0x2e80)


def check(font, e, ru, glossary=()):
    """Returns (errors, warnings) for one translated entry."""
    err, warn = [], []
    if not ru.strip():
        return ['пусто'], []
    n = normalise(ru)
    if has_japanese(n):
        err.append('остался японский текст')
    bad = sorted({c for c in n if c != '\n' and not font.has(ord(c))})
    if bad:
        err.append('нет в шрифте: ' + ''.join(bad))
    jp = e['jp']
    if re.findall(r'†(\d+)‡', jp) != re.findall(r'†(\d+)‡', n) or jp.count('‡') != n.count('‡'):
        err.append('сломана ссылка TIPS (†номер‡текст‡)')
    kind = e['kind']
    if kind == 'msg':
        lines = wrap(font, n).split('\n')
        if len(lines) > 3:
            err.append(f'не помещается: {len(lines)} строки, максимум 3')
        elif len(lines) == 3:
            warn.append('3 строки (мелкий интервал), лучше 2')
    elif kind in LIMITS:
        lines = n.split('\n')
        if len(lines) > MAX_LINES[kind]:
            err.append(f'слишком много строк: {len(lines)}, максимум {MAX_LINES[kind]}')
        over = [l for l in lines if font.width(l) > LIMITS[kind]]
        if over:
            err.append(f'строка шире {LIMITS[kind]} px: {font.width(over[0])} px')
    src = jp_for_ai(jp)
    if len(src) >= 4 and len(n) > len(src) * 4:
        warn.append('намного длиннее оригинала')
    for term_jp, term_ru, stem in glossary:
        if term_jp in src and stem and stem.lower() not in n.lower():
            warn.append(f'глоссарий: {term_jp} → {term_ru}')
    if re.search(r'[A-Za-z]', n) and not re.search(r'[A-Za-z]', src):
        warn.append('латиница в переводе')
    if '"' in n:
        warn.append('прямые кавычки, нужны «»')
    return err, warn


def load_glossary():
    p = os.path.join(ROOT, 'translation', 'glossary_ru.tsv')
    out = []
    if os.path.exists(p):
        for line in open(p, encoding='utf8'):
            if line.startswith('#') or not line.strip():
                continue
            f = line.rstrip('\n').split('\t') + ['', '', '']
            out.append((f[0], f[1], f[2]))
    return out


def scene_src(name):
    return json.load(open(os.path.join(SRC, name + '.json'), encoding='utf8'))


def scenes():
    return sorted(f[:-5] for f in os.listdir(SRC) if f.endswith('.json'))


def load_ru(name):
    p = os.path.join(RU, name + '.jsonl')
    out = {}
    if os.path.exists(p):
        for line in open(p, encoding='utf8'):
            if line.strip():
                r = json.loads(line)
                out[r['k']] = r
    return out


def save_ru(name, rows):
    """Written in source order, one object per line, so git diffs show one line per changed translation."""
    order = [e['key'] for e in scene_src(name)]
    p = os.path.join(RU, name + '.jsonl')
    tmp = p + '.tmp'
    with open(tmp, 'w', encoding='utf8') as f:
        seen = set()
        for k in order:
            if k in rows and k not in seen:
                seen.add(k)
                f.write(json.dumps(rows[k], ensure_ascii=False, sort_keys=True) + '\n')
    os.replace(tmp, p)
