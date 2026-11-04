#!/usr/bin/env python3
"""Redraws art with baked-in Japanese from translation/images_ru.tsv into translation/build/assets/.
The engine picks those files up by name instead of the ISO's. Run with ~/tools/sakura-venv/bin/python."""
import math, os, struct, sys
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


OPT_ROWS = ['Скорость текста', 'Пропуск текста', 'Музыка', 'Эффекты', 'Голоса', 'Видео', 'Голос Сораты', 'Данные установки']
# (x, y, w) of each value label; keep in step with kOptRu in port/src/scenes_boot.cpp
OPT_VALS = [('Медл.', 0, 128, 40), ('Быстро', 44, 128, 40), ('Мин.', 88, 128, 32), ('Макс.', 124, 128, 36), ('Вкл', 164, 128, 40),
            ('Выкл', 208, 128, 40), ('/', 0, 144, 16), ('Прочит.', 0, 160, 48), ('Все', 56, 160, 40)]


def optiontext(text):
    """option_bg_text.txg: new 256x176 atlas. Rows 0-7 are the row labels (104x16), then the value labels."""
    out = Image.new('RGBA', (256, 176), (0, 0, 0, 0))
    for i, t in enumerate(OPT_ROWS + [v[0] for v in OPT_VALS]):
        if i < 8:
            x, y, w = 0, 16 * i, 104
        else:
            t, x, y, w = OPT_VALS[i - 8]
        l = label(t, 11, (0, 0, 0), (0, 0, 0), 0)
        assert l.width <= (w if i >= 8 else w - 6), (t, l.width, w)
        out.alpha_composite(l, (x + (w - l.width) // 2 if i >= 8 else x + 6, y + (16 - l.height) // 2))
    return out


def fit_label(text, size, fill, outline, spread, maxw):
    while True:
        l = label(text, size, fill, outline, spread)
        if l.width <= maxw or size <= 8:
            return l
        size -= 1


def menubutton(im, text):
    """menu_button.txg: eight 88x56 tiles with a blue header (text|x8, then the system envelope), and the envelope front."""
    names = text.split('|')
    blue = (107, 190, 216, 255)
    for k, t in enumerate(names[:8]):
        x0, y0 = 88 * (k & 1), 56 * (k // 2)
        im.paste(blue, (x0 + 5, y0 + 1, x0 + 84, y0 + 18))
        l = fit_label(t, 13, (255, 255, 255), (60, 140, 170), 0, 76)
        im.alpha_composite(l, (x0 + 5 + (79 - l.width) // 2, y0 + 1 + (17 - l.height) // 2 + 1))
    # envelope front at (176,120)-(288,240): the orange band is tilted about 30 degrees; replace the white lettering
    env = im.crop((176, 120, 288, 240))
    a = env.load()
    ang = math.radians(30)
    ux, uy = math.cos(ang), -math.sin(ang)          # along the band (rising to the right)
    cx, cy = 56.0, 64.0
    for y in range(120):
        for x in range(112):
            r, g, b, al = a[x, y]
            d = (x - cx) * -uy + (y - cy) * ux      # distance across the band
            if al > 200 and r > 235 and g > 190 and b > 120 and abs(d) < 9 and abs((x - cx) * ux + (y - cy) * uy) < 40:
                a[x, y] = (255, 137, 15, 255)
    l = fit_label(names[8], 13, (255, 255, 255), (230, 110, 0), 0, 62).rotate(30, expand=True, resample=Image.BICUBIC)
    env.alpha_composite(l, (int(cx - l.width / 2 + 2), int(cy - l.height / 2)))
    im.paste((0, 0, 0, 0), (176, 120, 288, 240))
    im.alpha_composite(env, (176, 120))
    return im


def fill(im, box, col):
    im.paste(col, box)


def simchoice(im, text):
    """action_choice_parts_on.txg: five 120x28 mode buttons (rows of 32), their 104x16 captions at y=160, six names at x=120.
    text = 未選択|仕様作成|ミーティング|お世話|お休み|six names."""
    t = text.split('|')
    for r in range(5):
        y = 32 * r
        has_key = r in (2, 4)
        x0, x1 = (5, 91) if has_key else (5, 115)
        if r == 0:
            fill(im, (0, 0, 120, 28), (221, 221, 221, 255)); col = (140, 140, 140); x0, x1 = 0, 120
        else:
            fill(im, (x0, y + 4, x1, y + 24), (255, 253, 197, 255)); col = (30, 30, 30)
        l = fit_label(t[r], 14, col, col, 0, x1 - x0 - 6)
        im.alpha_composite(l, (x0 + (x1 - x0 - l.width) // 2, y + (28 - l.height) // 2))
    for r in range(4):
        clear(im, (0, 160 + 16 * r, 104, 176 + 16 * r))
        l = fit_label(t[1 + r], 11, (0, 0, 0), (0, 0, 0), 0, 104)
        im.alpha_composite(l, (0, 160 + 16 * r + (16 - l.height) // 2))
    for r in range(6):
        clear(im, (120, 16 * r, 208, 16 * r + 16))
        l = fit_label(t[5 + r], 11, (0, 0, 0), (0, 0, 0), 0, 88)
        im.alpha_composite(l, (120, 16 * r + (16 - l.height) // 2))
    return im


def banner(im, box, lines, alpha=204):
    """Redraws a yellow warning plate with a red diagonal strip and red lettering on a white halo."""
    x0, y0, x1, y1 = box
    w, h = x1 - x0, y1 - y0
    k = 4
    m = Image.new('RGBA', (w * k, h * k), (0, 0, 0, 0))
    d = ImageDraw.Draw(m)
    red, yel = (250, 20, 12, alpha), (255, 240, 80, alpha)
    d.rounded_rectangle((0, 0, w * k - 1, h * k - 1), 5 * k, fill=red)
    d.rounded_rectangle((2 * k, 2 * k, w * k - 1 - 2 * k, h * k - 1 - 2 * k), 3 * k, fill=yel)
    st = Image.new('L', m.size, 0)
    ImageDraw.Draw(st).polygon([(0, h * k - 7 * k), (w * k, 0), (w * k, 4 * k), (0, h * k - 1)], fill=255)
    inner = Image.new('L', m.size, 0)
    ImageDraw.Draw(inner).rounded_rectangle((0, 0, w * k - 1, h * k - 1), 5 * k, fill=255)
    stripe = Image.new('RGBA', m.size, red); stripe.putalpha(Image.composite(st, inner, inner).point(lambda v: v).convert('L'))
    m.alpha_composite(Image.composite(stripe, Image.new('RGBA', m.size, (0, 0, 0, 0)), st.point(lambda v: 255 if v else 0)))
    m = m.resize((w, h), Image.BOX)
    gap = 0
    ls = [fit_label(t, 11, (240, 20, 20), (255, 255, 255), 1, w - 10) for t in lines]
    tot = sum(l.height for l in ls) - 2 * (len(ls) - 1)
    y = (h - tot) // 2
    for l in ls:
        m.alpha_composite(l, ((w - l.width) // 2, y)); y += l.height - 2
    clear(im, box)
    im.alpha_composite(m, (x0, y0))


def simcard(im, text):
    """action_choice_parts.txg: mood/health captions, absent plate, working pill, warning banners, day suffix.
    text = モチベ|体調|作業中|悩み発生中!|作業不可!|不在|日 ; ~ splits a banner line."""
    t = text.split('|')
    gray = (128, 128, 150)
    for x0 in (0, 40, 80):                         # mood tiles, caption above the tile
        clear(im, (x0, 0, x0 + 40, 11))
        l = fit_label(t[0], 10, gray, (255, 255, 255), 1, 40)
        im.alpha_composite(l, (x0 + (40 - l.width) // 2, 6 - l.height // 2))
    for x0 in (120, 160, 200):                     # health tiles
        clear(im, (x0, 0, x0 + 40, 11))
        l = fit_label(t[1], 10, gray, (255, 255, 255), 1, 40)
        im.alpha_composite(l, (x0 + (40 - l.width) // 2, 6 - l.height // 2))
    clear(im, (0, 136, 40, 146))                   # caption of the unknown-mood tile
    l = fit_label(t[0], 10, (230, 40, 20), (255, 255, 255), 1, 40)
    im.alpha_composite(l, ((40 - l.width) // 2 + 0, 141 - l.height // 2))
    pill = (242, 100, 20, 255)
    fill(im, (227, 43, 253, 53), im.getpixel((240, 43)))
    l = fit_label(t[2], 9, (255, 255, 255), (200, 70, 0), 0, 25)
    im.alpha_composite(l, (227 + (26 - l.width) // 2, 48 - l.height // 2))
    banner(im, (80, 136, 152, 168), t[3].split('~'))
    banner(im, (0, 176, 72, 208), t[4].split('~'))
    plate = im.getpixel((208, 120))
    fill(im, (202, 108, 215, 134), (236, 233, 231, 255))
    l = fit_label(t[5], 9, (60, 45, 30), (60, 45, 30), 0, 22).rotate(90, expand=True)
    im.alpha_composite(l, (208 - l.width // 2, 121 - l.height // 2))
    clear(im, (192, 72, 208, 88))
    l = fit_label(t[6], 10, (250, 20, 12), (255, 255, 255), 1, 16)
    im.alpha_composite(l, (192 + (16 - l.width) // 2, 80 - l.height // 2))
    return im


def deadline(im, text):
    """common_font.txg 256x128: 'あと'/'日' are replaced in place; four 96x16 lines for 企画書/α版/β版/マスター go
    into the free area at y=96 and y=112 (keep in step with kDeadRu in port/src/sim_ui.cpp).
    text = Ещё|дн.|four lines."""
    t = text.split('|')
    ink = (40, 30, 30)
    for (x0, y0, w, h), s in (((56, 56, 24, 16), t[0]), ((80, 56, 16, 16), t[1])):
        clear(im, (x0, y0, x0 + w, y0 + h))
        l = fit_label(s, 11, ink, ink, 0, w)
        im.alpha_composite(l, (x0 + (w - l.width) // 2, y0 + (h - l.height) // 2))
    for i, s in enumerate(t[2:6]):
        x0, y0 = 128 * (i // 2), 96 + 16 * (i % 2)
        l = fit_label(s, 12, ink, ink, 0, 96)
        im.alpha_composite(l, (x0 + (96 - l.width) // 2, y0 + (16 - l.height) // 2))
    return im


def calpills(im, text):
    """calendar_parts.txg: five tilted milestone tags in rects (x0,y0,x1,y1); redrawn whole at the original tilt."""
    rects = [(0, 0, 88, 40), (88, 0, 160, 40), (160, 0, 232, 40), (0, 40, 104, 80), (104, 40, 176, 80)]
    bodyw = [80, 62, 62, 92, 62]
    orange = (255, 86, 20)
    for (x0, y0, x1, y1), t, bw in zip(rects, text.split('|'), bodyw):
        clear(im, (x0, y0, x1, y1))
        k = 4
        h = 22
        pill = Image.new('RGBA', (bw * k, h * k), (0, 0, 0, 0))
        d = ImageDraw.Draw(pill)
        d.rounded_rectangle((0, 0, bw * k - 1, h * k - 1), 7 * k, fill=orange + (255,))
        d.rounded_rectangle((2 * k, 2 * k, bw * k - 1 - 2 * k, h * k - 1 - 2 * k), 5 * k, fill=(255, 255, 255, 255))
        pill = pill.resize((bw, h), Image.BOX)
        l = fit_label(t, 11, orange, orange, 0, bw - 10)
        pill.alpha_composite(l, ((bw - l.width) // 2, (h - l.height) // 2))
        pill = pill.rotate(6.5, expand=True, resample=Image.BICUBIC)
        im.alpha_composite(pill, (x0 + (x1 - x0 - pill.width) // 2, y0 + (y1 - y0 - pill.height) // 2))
    return im


def convdate(im, text):
    """conversation_date.txg (256x128): '.' replaces 月 at (96,0); weekdays 16x16 at x=144+16*(wd-1), Sunday at x=240.
    text = пн|вт|ср|чт|пт|сб|вс. White glyphs, tinted by the engine."""
    names = text.split('|')
    slots = [144 + 16 * i for i in range(6)] + [240]
    for x0, t in zip(slots, names):
        clear(im, (x0, 0, x0 + 16, 16))
        l = fit_label(t, 12, (255, 255, 255), (255, 255, 255), 0, 16)
        im.alpha_composite(l, (x0 + (16 - l.width) // 2, (16 - l.height) // 2))
    clear(im, (96, 0, 112, 16))
    d = ImageDraw.Draw(im)
    d.rectangle((103, 12, 104, 13), fill=(0, 0, 0, 255))
    return im


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
        elif kind == 'menubutton':
            im = menubutton(im, text)
        elif kind == 'simchoice':
            im = simchoice(im, text)
        elif kind == 'deadline':
            im = deadline(im, text)
        elif kind == 'calpills':
            im = calpills(im, text)
        elif kind == 'convdate':
            im = convdate(im, text)
        elif kind == 'simcard':
            im = simcard(im, text)
        elif kind == 'optiontext':
            im = optiontext(text)
        elif kind == 'loc':
            im = plate(im, text, 12, (28, 70, 123), 12)
        open(os.path.join(outdir, name), 'wb').write(txg(im))
        n += 1
    print(n, 'images')


main()
