#!/usr/bin/env python3
"""Local review tool: python3 tools/tl_review.py, then open http://127.0.0.1:8765
Japanese and Russian side by side, a preview in the game's real font, Ctrl+Enter accepts and moves on.
Every save goes to translation/ru/ and rebuilds the pack, so F5 in the game picks it up."""
import http.server, json, os, struct, sys, threading, urllib.parse, zlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tl_common import *
from tl_build import build

FONT, GLOSS = Font(), load_glossary()
LOCK = threading.Lock()
HTML = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'tl_review.html')
CHARS = [chr(c) for c in list(range(0x20, 0x7f)) + list(range(0x410, 0x450)) + [0x401, 0x451, 0x2026, 0xab, 0xbb, 0x2015, 0x2116, 0x3001]]


def atlas_png():
    """Glyphs of the game font as a white-on-alpha PNG; zlib only, no image library."""
    sz, cols, rows = FONT.sz, 32, (len(CHARS) + 31) // 32
    w, h = cols * sz, rows * sz
    px = bytearray(w * h * 2)
    for n, c in enumerate(CHARS):
        if not FONT.idx[ord(c)]:
            continue
        ox, oy = (n % cols) * sz, (n // cols) * sz
        for y, row in enumerate(FONT.pixels(ord(c))):
            for x, v in enumerate(row):
                o = ((oy + y) * w + ox + x) * 2
                px[o], px[o + 1] = 255, v * 17
    raw = b''.join(b'\0' + bytes(px[y * w * 2:(y + 1) * w * 2]) for y in range(h))

    def chunk(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 4, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))


def metrics():
    return {'cell': FONT.sz, 'cols': 32, 'width': MSG_W, 'limits': LIMITS,
            'glyphs': {c: [i, *FONT.metrics(ord(c))] for i, c in enumerate(CHARS)}}


def flags():
    p = os.path.join(BUILD, 'feedback.tsv')
    return {l.split('\t')[0] for l in open(p, encoding='utf8')} if os.path.exists(p) else set()


def scene_list():
    out, fl = [], flags()
    for n in os.listdir(RU):
        if n.endswith('.jsonl'):
            s = n[:-6]
            rows = load_ru(s)
            src = scene_src(s)
            done = sum(1 for r in rows.values() if r['st'] == 'ok')
            todo = sum(1 for r in rows.values() if r['st'] in ('draft', 'fix'))
            out.append(dict(name=s, total=len(src), ok=done, todo=todo, flagged=sum(1 for k in rows if k in fl)))
    return sorted(out, key=lambda r: r['name'])


def scene_data(s):
    rows, fl, out = load_ru(s), flags(), []
    nm = load_ru('_names')
    names = {e['jp']: nm.get(e['key'], {}).get('ru', '') for e in scene_src('_names')}
    for e in scene_src(s):
        r = rows.get(e['key'], {})
        ru = r.get('ru', '')
        er, wa = check(FONT, e, ru, GLOSS) if ru else ([], [])
        out.append(dict(key=e['key'], kind=e['kind'], who=e['speaker'], whoRu=names.get(e['speaker'], ''), jp=e['jp'], ru=ru, st=r.get('st', ''), note=r.get('note', ''),
                        by=r.get('by', ''), errors=er, warnings=wa, flagged=e['key'] in fl, voice=e.get('voice', '')))
    return out


class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def send(self, body, ctype='application/json; charset=utf-8', code=200):
        if not isinstance(body, bytes):
            body = json.dumps(body, ensure_ascii=False).encode()
        self.send_response(code)
        self.send_header('Content-Type', ctype)
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        u = urllib.parse.urlparse(self.path)
        q = urllib.parse.parse_qs(u.query)
        if u.path == '/':
            self.send(open(HTML, 'rb').read(), 'text/html; charset=utf-8')
        elif u.path == '/font.png':
            self.send(atlas_png(), 'image/png')
        elif u.path == '/api/metrics':
            self.send(metrics())
        elif u.path == '/api/scenes':
            self.send(scene_list())
        elif u.path == '/api/scene':
            self.send(scene_data(q['name'][0]))
        else:
            self.send({'error': 'not found'}, code=404)

    def do_POST(self):
        d = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        src = {e['key']: e for e in scene_src(d['scene'])}
        e = src[d['key']]
        with LOCK:      # one writer at a time: the files are rewritten whole
            rows = load_ru(d['scene'])
            r = rows.get(d['key']) or dict(k=d['key'], by='human', note='', st='draft', ru='')
            if r['ru'] != d['ru']:
                r['by'] = 'human'
            r.update(ru=d['ru'], st=d['st'], note=d.get('note', ''))
            rows[d['key']] = r
            save_ru(d['scene'], rows)
            build(report=False)
        er, wa = check(FONT, e, d['ru'], GLOSS)
        self.send(dict(errors=er, warnings=wa))


if __name__ == '__main__':
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8765
    print(f'http://127.0.0.1:{port}')
    http.server.ThreadingHTTPServer(('127.0.0.1', port), H).serve_forever()
