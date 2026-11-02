#!/usr/bin/env python3
"""Redraws art with baked-in Japanese from translation/images_ru.tsv into translation/build/assets/.
The engine picks those files up by name instead of the ISO's. Run with ~/tools/sakura-venv/bin/python."""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import img
from tl_common import ROOT, DATA, BUILD
from PIL import Image, ImageDraw, ImageFilter, ImageFont

FONT = os.environ.get('TL_FONT', '/usr/share/fonts/Adwaita/AdwaitaSans-Regular.ttf')
SS = 4


def face(size, bold=True):
    f = ImageFont.truetype(FONT, size * SS)
    if bold:
        try:
            f.set_variation_by_axes([700, 0][:len(f.get_variation_axes())])
        except OSError:
            pass
    return f


def label(text, size, fill, outline, spread, width=None):
    """Text with a soft outline like the original art, antialiased by supersampling."""
    f = face(size)
    l, t, r, b = f.getbbox(text)
    pad = (spread + 2) * SS
    bold = max(1, size * SS // 20)      # the art uses heavy lettering; a stroke keeps it readable at 12 px
    im = Image.new('L', (r - l + pad * 2, (b - t) + pad * 2), 0)
    ImageDraw.Draw(im).text((pad - l, pad - t), text, font=f, fill=255, stroke_width=bold, stroke_fill=255)
    halo = im.filter(ImageFilter.MaxFilter(spread * 2 * SS + 1)).filter(ImageFilter.GaussianBlur(SS / 2))
    out = Image.new('RGBA', im.size, tuple(outline) + (0,))
    out.putalpha(halo)
    body = Image.new('RGBA', im.size, tuple(fill) + (255,))
    body.putalpha(im)
    out.alpha_composite(body)
    return out.resize((out.width // SS, out.height // SS), Image.BOX)


def clear(im, box):
    im.paste((0, 0, 0, 0), box)


def title_parts(im, labels):
    rows = [(0, 16), (16, 32), (48, 64), (64, 96), (32, 48), (96, 112)]
    for (y0, y1), t in zip(rows, labels):
        for x0, col in ((0, (232, 96, 0)), (80, (12, 66, 160))):
            clear(im, (x0, y0, x0 + 80, y1))
            l = label(t, 12, col, (255, 255, 255), 1)
            im.alpha_composite(l, (x0 + (78 - l.width) // 2, y0 + (y1 - y0 - l.height) // 2))
    return im


def start(im, text):
    clear(im, (0, 0, 256, 32))
    l = label(text, 20, (20, 62, 130), (255, 255, 255), 2)
    im.alpha_composite(l, ((184 - l.width) // 2, (32 - l.height) // 2))
    return im


def plate(im, text, size, ink, x0):
    """Rebuilds a torn paper strip at the width the Russian needs, keeping its right edge."""
    right = im.getbbox()[2]
    tail = im.crop((right - 8, 0, right, 32))
    col = im.crop((3, 0, 4, 32))
    l = label(text, size, ink, (255, 255, 255), 1)
    new_right = min(256, max(right if size < 16 else 0, x0 + l.width + 12))
    out = Image.new('RGBA', im.size, (0, 0, 0, 0))
    out.paste(col.resize((new_right - 8, 32), Image.NEAREST), (0, 0))
    out.paste(tail, (new_right - 8, 0))
    out.alpha_composite(l, (x0, (32 - l.height) // 2 + (1 if size < 16 else 0)))
    return out


def swizzle(px, wbytes, h):
    out = bytearray(len(px))
    src = 0
    for by in range(h // 8):
        for bx in range(wbytes // 16):
            for line in range(8):
                o = (by * 8 + line) * wbytes + bx * 16
                out[src:src + 16] = px[o:o + 16]
                src += 16
    return bytes(out)


def txg(im):
    w, h = im.size
    px = swizzle(im.tobytes(), w * 4, h)
    return b'TXG\0' + struct.pack('<11I', 48 + len(px), 48, len(px), 0, 0, 0, len(px), 0, 0, w, h) + px


def main():
    outdir = os.path.join(BUILD, 'assets')
    os.makedirs(outdir, exist_ok=True)
    n = 0
    for line in open(os.path.join(ROOT, 'translation', 'images_ru.tsv'), encoding='utf8'):
        if line.startswith('#') or not line.strip():
            continue
        name, kind, text = line.rstrip('\n').split('\t')
        im = img.txg(open(os.path.join(DATA, name), 'rb').read()).convert('RGBA')
        if kind == 'title':
            im = title_parts(im, text.split('|'))
        elif kind == 'start':
            im = start(im, text)
        elif kind == 'page':
            ink = im.getpixel((im.getbbox()[0] + 4, 2))
            im = plate(im, text, 20, (112, 20, 70) if ink[0] > ink[2] else (20, 62, 120), 9)
        elif kind == 'loc':
            im = plate(im, text, 12, (28, 70, 123), 12)
        open(os.path.join(outdir, name), 'wb').write(txg(im))
        n += 1
    print(n, 'images')


main()
