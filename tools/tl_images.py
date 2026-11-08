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
    if len(names) > 9:                                 # DLC tile at (424,200)
        im.paste(blue, (429, 201, 508, 218))
        l = fit_label(names[9], 13, (255, 255, 255), (60, 140, 170), 0, 76)
        im.alpha_composite(l, (429 + (79 - l.width) // 2, 201 + (17 - l.height) // 2 + 1))
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


def mode_color(im, box):
    """Most common opaque colour in a rect: the flat background behind lettering."""
    from collections import Counter
    c = Counter(px for px in im.crop(box).getdata() if px[3] > 200)
    return c.most_common(1)[0][0] if c else (255, 255, 255, 255)


def erase(im, box, col=None, solid=False):
    """Repaints only the opaque pixels of a rect with the flat background colour, so a shaped plate keeps its outline."""
    col = col or mode_color(im, box)
    px = im.load()
    for y in range(box[1], box[3]):
        for x in range(box[0], box[2]):
            if px[x, y][3] > 0:
                px[x, y] = col[:3] + ((255 if solid else px[x, y][3]),)
    return col


def retext(im, box, text, size, fill, outline=None, spread=1, col=None, maxw=None, solid=False):
    """Erases the lettering in box, then centres new lettering there."""
    bg = erase(im, box, col, solid)
    if outline is None:
        outline = tuple(int(v * 0.5) for v in bg[:3])
    l = fit_label(text, size, fill, outline, spread, maxw or (box[2] - box[0]))
    im.alpha_composite(l, (box[0] + (box[2] - box[0] - l.width) // 2, box[1] + (box[3] - box[1] - l.height) // 2))
    return bg


def gamebadge(im, text):
    """action_parts_game.txg: hexagon 'ゲーム完成度UP' (0,0,48,54) and six 40x16 stat chips (see Toast::drawAt)."""
    t = text.split('|')
    cream = (255, 253, 215, 255)
    fill(im, (5, 11, 43, 44), cream)
    lines = t[0].split('~')
    ls = [fit_label(x, 10, (255, 110, 20), (255, 110, 20), 0, 34) for x in lines]
    ls[-1] = fit_label(lines[-1], 16, (255, 110, 20), (255, 110, 20), 0, 34)
    tot = sum(l.height for l in ls) - 2 * (len(ls) - 1)
    y = 27 - tot // 2
    for l in ls:
        im.alpha_composite(l, (24 - l.width // 2, y)); y += l.height - 2
    # chips: (x, y, tint) in the order of the labels in t[1:]
    chips = [(88, 0, (235, 235, 100)), (88, 16, (150, 185, 190)), (48, 16, (120, 200, 230)),
             (48, 32, (230, 170, 90)), (88, 32, (220, 120, 120)), (48, 0, (200, 150, 230))]
    for (x0, y0, tint), name in zip(chips, t[1:7]):
        clear(im, (x0, y0, x0 + 40, y0 + 16))
        k = 4
        bar = Image.new('RGBA', (40 * k, 14 * k), (0, 0, 0, 0))
        px = bar.load()
        for x in range(40 * k):
            e = min(x, 40 * k - 1 - x) / (7.0 * k)
            a = int(255 * min(1.0, 0.25 + 0.75 * e))
            for y in range(14 * k):
                px[x, y] = tint + (a,)
        bar = bar.resize((40, 14), Image.BOX)
        im.alpha_composite(bar, (x0, y0 + 1))
        l = fit_label(name, 10, (70, 70, 70), (255, 255, 255), 1, 38)
        im.alpha_composite(l, (x0 + (40 - l.width) // 2, y0 + (16 - l.height) // 2))
    return im


def backlog(im, text):
    """backlog_parts.txg: 'クラウド選択肢' (0,24,88,40) and '選択肢' (0,40,48,56) pills."""
    t = text.split('|')
    for (x0, y0, x1, y1), s in (((0, 24, 88, 40), t[0]), ((0, 40, 48, 56), t[1])):
        retext(im, (x0 + 4, y0 + 2, x1 - 4, y1 - 2), s, 10, (255, 255, 255), (255, 255, 255), 0, maxw=x1 - x0 - 12)
    return im


def saveparts(im, text):
    """saveload_parts.txg genre pills (rows of 16 at y=120.., text x 14..46) and saveload_window.txg tiles/headings.
    text = Канон|CG|Голос|Герои|FPS|Ритм|Нет|Жанр/близость|Маршруты (genre order: 王道 CG ボイス キャラ FPS リズム 未定)"""
    t = text.split('|')
    for i in range(7):
        y0 = 120 + 16 * i
        retext(im, (16, y0 + 2, 46, y0 + 14), t[i], 9, (255, 255, 255), None, 1, col=mode_color(im, (44, y0 + 3, 47, y0 + 9)))
    return im


def savewindow(im, text):
    t = text.split('|')
    tiles = [(0, 72), (0, 112), (0, 152), (48, 72), (48, 112), (48, 152), (0, 192)]   # 王道 CG ボイス キャラ FPS リズム 未定
    for (x0, y0), s in zip(tiles, t[:7]):
        retext(im, (x0 + 1, y0 + 2, x0 + 43, y0 + 15), s, 10, (255, 255, 255), None, 1)
    blue = (125, 190, 205)
    for (x0, x1), s in (((0, 80), t[7]), ((80, 208), t[8])):
        clear(im, (x0, 0, x1, 14))
        l = fit_label(s, 9, blue, blue, 0, x1 - x0 - 6)
        im.alpha_composite(l, (x0 + 3, 6 - l.height // 2))
    return im


def rainbow(text, size, palette, maxw):
    """Chunky multicolour lettering with a white halo (the 'friendship up' banner)."""
    while True:
        parts = [label(c, size, palette[i % len(palette)], (255, 255, 255), 2) if c != ' ' else None for i, c in enumerate(text)]
        adv = [(p.width - 4) if p else size // 3 for p in parts]
        if sum(adv) + 4 <= maxw or size <= 8:
            break
        size -= 1
    h = max(p.height for p in parts if p)
    out = Image.new('RGBA', (sum(adv) + 4, h), (0, 0, 0, 0))
    x = 0
    for p, a in zip(parts, adv):
        if p:
            out.alpha_composite(p, (x, 0))
        x += a
    return out


def friend(im, text):
    """conversation_friend.txg: two banners, (32,0,192,32) and (32,32,256,64)."""
    t = text.split('|')
    pal = [(70, 165, 225), (175, 105, 195), (140, 185, 40), (240, 85, 40), (250, 120, 160), (250, 170, 20)]
    for (x0, y0, x1, y1), s, sz in (((32, 0, 190, 32), t[0], 17), ((32, 32, 256, 64), t[1], 18)):
        clear(im, (x0, y0, x1, y1))
        l = rainbow(s, sz, pal, x1 - x0 - 4)
        im.alpha_composite(l, (x0 + (x1 - x0 - l.width) // 2, y0 + (y1 - y0 - l.height) // 2))
    return im


def infoparts(im, text):
    """information_parts.txg: six stat chips round the radar, tab, friendship label, health/mood bars.
    text = Шарм|Объём|Стабил.|Драйв|Эмоции|Красота|Способности|Близость Lv|Здоровье|Мотивация (chips: top, left-up, right-up, left-down, right-down, bottom)"""
    t = text.split('|')
    chips = [(46, 3, 81, 16), (3, 25, 36, 38), (91, 25, 124, 38), (3, 58, 36, 71), (91, 58, 124, 71), (46, 81, 81, 94)]
    for (x0, y0, x1, y1), s in zip(chips, t[:6]):
        retext(im, (x0 + 3, y0 + 2, x1 - 3, y1 - 2), s, 10, (70, 70, 80), (70, 70, 80), 0, maxw=x1 - x0 - 6)
    retext(im, (186, 2, 248, 16), t[6], 10, (110, 120, 120), (110, 120, 120), 0, solid=True)
    retext(im, (2, 105, 62, 121), t[7], 10, (255, 205, 225), (255, 205, 225), 0, solid=True)
    retext(im, (3, 172, 86, 186), t[8], 10, (140, 165, 235), (140, 165, 235), 0, maxw=76, solid=True)
    retext(im, (3, 188, 86, 202), t[9], 10, (140, 165, 235), (140, 165, 235), 0, maxw=76, solid=True)
    return im


def infonames(im, text):
    """information_font_name.txg: six name plates (rects as nm[] in InfoScene), romaji below stays."""
    t = text.split('|')
    rects = [(0, 0, 119, 32), (0, 64, 124, 96), (0, 32, 102, 64), (128, 0, 208, 32), (128, 32, 252, 64), (128, 64, 229, 96)]
    for r, s in zip(rects, t):
        cols = [px for px in im.crop(r).getdata() if px[3] > 200 and min(px[:3]) < 170]
        from collections import Counter
        col = Counter(cols).most_common(1)[0][0][:3] if cols else (60, 60, 60)
        clear(im, r)
        l = fit_label(s, 20, col, (255, 255, 255), 2, r[2] - r[0] - 2)
        im.alpha_composite(l, (r[0] + (r[2] - r[0] - l.width) // 2, r[1] + (r[3] - r[1] - l.height) // 2))
    return im


# name-list atlases: (x, y, cell width) of every 16 px row cell, in the order of the tsv text
CELLS = {
    'omake_menu_name.txg': [(0, 16 * i, 96) for i in range(5)],
    'omake_event_name.txg': [(0, 16 * i, 128) for i in range(6)],
    'omake_pose_name.txg': [(0, 16 * i, 80) for i in range(8)] + [(80, 0, 128)] + [(80, 16 * i, 80) for i in range(1, 8)]
                           + [(160, 16, 48), (160, 32, 48), (160, 48, 48), (160, 64, 96), (160, 80, 48), (160, 96, 48), (160, 112, 78)] + [(208, 16 * i, 48) for i in (1, 2, 3, 5, 6)],
    'omake_voice_ch_name.txg': [(0, 16 * i, 94) for i in range(9)] + [(0, 144, 132)] + [(0, 160, 94), (0, 176, 94)],
}


def rowlist(im, name, text):
    """Replaces dark lettering in fixed 16 px cells; each Russian text is shrunk to the old text's width."""
    cells = CELLS[name]
    t = text.split('|')
    assert len(t) == len(cells), (name, len(t), len(cells))
    ink = (20, 20, 20)
    for (x, y, w), s in zip(cells, t):
        box = (x, y, x + w, y + 16)
        bb = im.crop(box).split()[3].point(lambda v: 255 if v > 60 else 0).getbbox()
        if not bb or not s:
            continue
        ow = bb[2] - bb[0]
        clear(im, box)
        l = fit_label(s, 12, ink, ink, 0, max(ow, 24))
        im.alpha_composite(l, (x + bb[0], y + (16 - l.height) // 2))
    return im


def goldtext(text, size, maxw, top=(255, 238, 110), bot=(238, 168, 20), edge=(255, 255, 255)):
    """Big gold lettering with a vertical gradient, thin orange rim and a white halo (ranking banner)."""
    while True:
        f = face(size)
        l, t, r, b = f.getbbox(text)
        if (r - l) / SS <= maxw or size <= 10:
            break
        size -= 2
    pad = 6 * SS
    m = Image.new('L', (r - l + pad * 2, b - t + pad * 2), 0)
    ImageDraw.Draw(m).text((pad - l, pad - t), text, font=f, fill=255, stroke_width=max(1, size * SS // 25), stroke_fill=255)
    halo = m.filter(ImageFilter.MaxFilter(4 * SS + 1)).filter(ImageFilter.GaussianBlur(SS / 2))
    rim = m.filter(ImageFilter.MaxFilter(SS + 1))
    out = Image.new('RGBA', m.size, edge + (0,)); out.putalpha(halo)
    o2 = Image.new('RGBA', m.size, (214, 120, 10, 255)); o2.putalpha(rim); out.alpha_composite(o2)
    grad = Image.new('RGBA', m.size)
    gp = grad.load()
    for y in range(m.height):
        k = min(1.0, max(0.0, (y - pad) / max(1, b - t)))
        c = tuple(int(top[i] + (bot[i] - top[i]) * k) for i in range(3)) + (255,)
        for x in range(m.width):
            gp[x, y] = c
    grad.putalpha(m)
    out.alpha_composite(grad)
    return out.resize((out.width // SS, out.height // SS), Image.BOX)


def rankfont(im, text):
    """contest_ranking_font.txg 256x128: the engine draws only rect (0,0,240,64); the second line is rebuilt as well."""
    t = text.split('|')
    clear(im, (0, 0, 256, 128))
    for (y0, y1), s in (((0, 64), t[0]), ((64, 128), t[1])):
        l = goldtext(s, 56, 240)
        im.alpha_composite(l, ((256 - l.width) // 2, y0 + (y1 - y0 - l.height) // 2))
    return im


def dlc(im, name, text):
    t = text.split('|')
    ink = (30, 30, 30)
    if name == 'dlc_story.txg':               # 第N話, four 48x16 rows
        for i in range(4):
            clear(im, (0, 16 * i, 48, 16 * i + 16))
            l = fit_label(t[i], 12, ink, ink, 0, 46)
            im.alpha_composite(l, (2, 16 * i + (16 - l.height) // 2))
    elif name == 'dlc_parts.txg':             # store note at (72,0)-(256,48), four lines
        clear(im, (72, 0, 256, 64))
        for i in range(4):
            l = fit_label(t[i], 11, ink, ink, 0, 182)
            im.alpha_composite(l, (74, 12 * i + (12 - l.height) // 2 + 1))
    else:                                     # dlc_parts_title.txg: two-line logo left of the knickers
        clear(im, (0, 0, 196, 64)); clear(im, (190, 40, 256, 64))
        pal1 = [(70, 140, 225), (110, 80, 200)]
        l = rainbow(t[0], 13, pal1, 190)
        im.alpha_composite(l, (4, 8))
        pal2 = [(150, 80, 190), (70, 175, 90), (240, 130, 40), (235, 80, 160), (80, 150, 220)]
        l = rainbow(t[1], 19, pal2, 192)
        im.alpha_composite(l, (2, 28))
    return im


def chip(im, box, tint, name, size=10):
    """Horizontal-gradient stat chip (tint fading out at both ends) with grey lettering on a white halo."""
    x0, y0, x1, y1 = box
    w, h = x1 - x0, y1 - y0
    clear(im, (x0, y0 - 2, x1, y1 + 2))
    k = 4
    bar = Image.new('RGBA', (w * k, (h - 2) * k), (0, 0, 0, 0))
    px = bar.load()
    for x in range(w * k):
        e = min(x, w * k - 1 - x) / (7.0 * k)
        a = int(255 * min(1.0, 0.25 + 0.75 * e))
        for y in range((h - 2) * k):
            px[x, y] = tint + (a,)
    im.alpha_composite(bar.resize((w, h - 2), Image.BOX), (x0, y0 + 1))
    l = fit_label(name, size, (70, 70, 70), (255, 255, 255), 1, w - 2)
    im.alpha_composite(l, (x0 + (w - l.width) // 2, y0 + (h - l.height) // 2))


def psrts(im, text):
    """menu_psrts.txg: six chips round the status hexagon. text = Шарм|Объём|Стабил.|Драйв|Эмоции|Красота (top, l-up, r-up, l-down, r-down, bottom)"""
    t = text.split('|')
    spec = [((117, 4, 155, 16), (235, 235, 100)), ((73, 26, 110, 38), (200, 150, 230)), ((162, 26, 199, 38), (150, 185, 190)),
            ((73, 59, 110, 71), (120, 200, 230)), ((162, 59, 199, 71), (220, 120, 120)), ((117, 81, 155, 93), (230, 170, 90))]
    for (box, tint), name in zip(spec, t):
        chip(im, box, tint, name, 9)
    return im


def squareparts(im, text):
    """square_parts.txg state pills at x=208 (未着手 y0, 作業中 y16, LOCK y32 kept, 仕様 y48) and the 済 stamp (208,64,240,96)."""
    t = text.split('|')
    for (x0, y0, x1, y1), s in (((209, 2, 239, 14), t[0]), ((209, 18, 239, 30), t[1]), ((211, 50, 229, 62), t[3])):
        bg = mode_color(im, (x0, y0, x1, y1))
        retext(im, (x0 + 1, y0 + 1, x1 - 1, y1 - 1), s, 9, (255, 255, 255), (255, 255, 255), 0, solid=True)
    d = ImageDraw.Draw(im)
    d.ellipse((224 - 9, 78 - 9, 224 + 9, 78 + 9), fill=(0, 0, 0, 0))
    red = (225, 30, 30)
    l = fit_label(t[4], 11, red, red, 0, 17)
    im.alpha_composite(l, (224 - l.width // 2, 78 - l.height // 2))
    # day labels at (192,160) 64x16 each (T_1f1cd8), digits are drawn over them by the chart at x 18..50 / 22..54:
    # "全 . 日" total, "あと . 日" left, "完成" done. The "." stays; 全 has no counterpart.
    if len(t) > 8:
        ink, edge = (255, 255, 255), (50, 60, 80)

        def put(box, s, size, fill, outline):   # on transparency: clear, then centre the new word
            clear(im, box)
            l = fit_label(s, size, fill, outline, 1, box[2] - box[0])
            im.alpha_composite(l, (box[0] + (box[2] - box[0] - l.width) // 2, box[1] + (box[3] - box[1] - l.height) // 2))
        clear(im, (196, 162, 215, 176))
        put((238, 161, 256, 176), t[5], 9, ink, edge)
        put((192, 177, 216, 192), t[6], 9, ink, edge)
        put((243, 177, 256, 192), t[7], 9, ink, edge)
        put((192, 192, 256, 208), t[8], 11, red, (255, 255, 255))
    return im


def wrap(text, f, maxw):
    lines, cur = [], ''
    for w in text.split(' '):
        t = (cur + ' ' + w).strip()
        if f.getlength(t) / SS <= maxw or not cur:
            cur = t
        else:
            lines.append(cur); cur = w
    return lines + [cur]


def legal(im, text):
    """logo_dontcopy: dark 480x272 notice. text = heading|paragraph (paragraphs split by ~). Written as a plain TXG under the .txgtl name."""
    head, body = text.split('|')
    im = Image.new('RGBA', (480, 272), (31, 31, 31, 255))
    d = ImageDraw.Draw(im)
    f = ImageFont.truetype(FONT, 13 * SS)
    big = ImageFont.truetype(FONT, 15 * SS)
    big_im = Image.new('L', (480 * SS, 272 * SS), 0)
    dd = ImageDraw.Draw(big_im)
    w = dd.textlength(head, font=big)
    dd.text(((480 * SS - w) / 2, 62 * SS), head, font=big, fill=255)
    y = 100
    for para in body.split('~'):
        for ln in wrap(para, f, 420):
            dd.text((30 * SS, y * SS), ln, font=f, fill=255)
            y += 21
    mask = big_im.resize((480, 272), Image.LANCZOS)
    im.paste((255, 255, 255, 255), (0, 0), mask)
    return im


def actionwin(im, text):
    """action_window.txg: the 'absent' bear's plate (11,70)-(21,92); the word is turned 90 degrees like the original's column."""
    fill(im, (12, 71, 20, 91), (238, 236, 236, 255))
    l = fit_label(text, 8, (60, 45, 30), (60, 45, 30), 0, 18).rotate(90, expand=True)
    im.alpha_composite(l, (16 - l.width // 2, 81 - l.height // 2))
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


def tut07(out, orig, text):
    """BG_TUT07: the task list. The port does not draw this screen like the original, so the original picture keeps its
    layout and gets Russian text, painted at screen scale (the frame is enlarged to 480x272 and shrunk back). The page
    header and help bar come from the port's own task list (work/tut/s07.bmp).
    text = deadline title|"left"|days|5 names|badge "remaining"|badge days|chip|small chip 未|small chip 着手|panel line 1|panel line 2"""
    import tl_tutorial
    t = text.split('|')
    fx0, fy0, fx1, fy1 = tl_tutorial.FULL[1]
    scr = orig.crop((fx0, fy0, fx1, fy1)).resize((480, 272), Image.LANCZOS)
    port = Image.open(os.path.join(tl_tutorial.SHOTS, 's07.bmp')).convert('RGBA')
    scr.paste(port.crop((0, 0, 140, 33)), (0, 0))
    scr.paste(port.crop((0, 253, 480, 272)), (0, 253))

    def put(box, s, size, ink, bg, align='c', edge=None):
        fill(scr, box, bg)
        l = fit_label(s, size, ink, edge or bg[:3], 0 if edge is None else 1, box[2] - box[0])
        x = box[0] if align == 'l' else box[0] + (box[2] - box[0] - l.width) // 2
        scr.alpha_composite(l, (x, box[1] + (box[3] - box[1] - l.height) // 2))
    dark, grey = (48, 48, 48), (150, 145, 150)
    # deadline note: "企画書提出まで" / "あと 7 日"
    white = (255, 255, 252, 255)
    put((372, 3, 474, 17), t[0], 11, dark, white)
    put((396, 38, 418, 51), t[1], 10, dark, white)
    put((449, 38, 468, 51), t[2], 10, dark, white)
    # name plates, the first one selected
    for i in range(5):           # plates at y 50 + 30i, 22 px high; the last one is cut by the list's bottom
        y = 50 + 30 * i
        put((23, y + 3, 102, y + (19 if i < 4 else 12)), t[3 + i], 13, dark if i == 0 else grey, scr.getpixel((100, y + 8)), 'l')
    # "残タスク N 日分" badges: the number stays
    put((106, 46, 146, 58), t[8], 8, dark, scr.getpixel((108, 59)))
    put((125, 63, 144, 71), t[9], 8, dark, scr.getpixel((145, 66)))
    for i in range(3):
        y = 80 + 30 * i
        put((110, y + 1, 140, y + 9), t[8], 7, dark, scr.getpixel((111, y + 10)))
        put((124, y + 13, 139, y + 20), t[9], 7, dark, scr.getpixel((139, y + 12)))
    # task chip and the two small chips
    put((166, 53, 216, 77), t[10], 10, dark, scr.getpixel((168, 55)))
    put((180, 146, 201, 160), t[11], 8, (200, 90, 40), scr.getpixel((181, 147)))
    put((179, 175, 202, 190), t[12], 8, (40, 40, 200), scr.getpixel((180, 176)))
    # bottom panel
    pbg = scr.getpixel((250, 222))
    put((96, 213, 300, 230), t[13], 13, dark, pbg, 'l')
    put((96, 230, 300, 247), t[14], 13, dark, pbg, 'l')
    out.paste(scr.resize((fx1 - fx0, fy1 - fy0), Image.LANCZOS), (fx0, fy0))


def tut14(out, orig, text):
    """BG_TUT14, the drawing under the screen: two captions, the task card's "あと4.0日" and name, the red note.
    text = caption left|caption right|card prefix|card days|card name|note"""
    t = text.split('|')
    bg = (230, 250, 255, 255)

    def put(box, s, size, ink, back, edge=None, spread=0):
        fill(out, box, back)
        l = fit_label(s, size, ink, edge or back[:3], spread, box[2] - box[0])
        out.alpha_composite(l, (box[0] + (box[2] - box[0] - l.width) // 2, box[1] + (box[3] - box[1] - l.height) // 2))
    put((64, 95, 214, 114), t[0], 14, (30, 30, 30), bg)
    put((276, 95, 418, 114), t[1], 14, (30, 30, 30), bg)
    put((250, 198, 454, 215), t[5], 11, (226, 0, 18), bg)
    pink = out.getpixel((124, 152))
    put((120, 136, 178, 154), t[2] + ' ' + t[3], 12, (255, 255, 255), pink, (205, 95, 165), 1)
    put((97, 163, 178, 179), t[4], 10, (40, 40, 40), (255, 255, 255, 255))


def ru_of(jp):
    """The approved or draft Russian for a Japanese string, from translation/ru (keys only, no Japanese there)."""
    import glob, json
    from tl_common import key
    if not hasattr(ru_of, 'tab'):
        ru_of.tab = {}
        for f in glob.glob(os.path.join(ROOT, 'translation', 'ru', '*.jsonl')):
            for line in open(f, encoding='utf8'):
                e = json.loads(line)
                if e.get('st') != 'reject' and e.get('ru'):
                    ru_of.tab[e['k']] = e['ru']
    return ru_of.tab.get(key(jp), jp)


def tut16(out, orig, text):
    """BG_TUT16: the contest ranking list, which the port does not draw like this. Rows 41..46 get the Russian game
    titles, genres and team names from the team table (team_list.tmd records 47, 38, 45, 36, 41, 46), the stage tab
    its Russian name. Painted at screen scale like tut07."""
    import tl_tutorial
    tm = open(os.path.join(DATA, 'team_list.tmd'), 'rb').read()
    team = lambda i: [ru_of(tm[8 + i * 0x120 + o:8 + i * 0x120 + o + 64].split(b'\0')[0].decode('utf8')) for o in (0, 64, 128)]
    fx0, fy0, fx1, fy1 = tl_tutorial.FULL[1]
    scr = Image.new('RGBA', (480, 300))      # taller, so the cut last row can be painted past the bottom edge
    scr.paste(orig.crop((fx0, fy0, fx1, fy1)).resize((480, 272), Image.LANCZOS), (0, 0))

    def put(box, s, size, ink, bg, align='l', edge=None):
        if bg is not None:
            fill(scr, box, bg)
        l = fit_label(s, size, ink, edge or bg[:3], 0 if edge is None else 1, box[2] - box[0])
        x = box[0] if align == 'l' else box[0] + (box[2] - box[0] - l.width) // 2
        scr.alpha_composite(l, (x, box[1] + (box[3] - box[1] - l.height) // 2))
    # row 41 shows only the bottom of its team name under the header: draw it whole, then put the header back
    head = scr.crop((0, 0, 480, 37))
    put((120, 30, 262, 46), team(47)[0], 12, (20, 20, 20), scr.getpixel((260, 42)))
    scr.paste(head, (0, 0))
    # stage tab: wipe the word with the tab's own gradient (column x=170), then the Russian name
    col = scr.crop((170, 16, 171, 36))
    for x in range(84, 152):
        scr.paste(col, (x, 16))
    put((86, 18, 150, 34), text.split('|')[0], 13, (255, 255, 255), None, 'c', (225, 110, 10))
    for r, i in enumerate((38, 45, 36, 41, 46)):
        y = 60 + 50 * r
        name, title, genre = team(i)
        put((104, y + 1, 264, y + 18), title, 14, (255, 255, 255), scr.getpixel((104, y + 3)), 'l', (30, 50, 140))
        put((275, y + 5, 358, y + 15), genre, 9, (40, 70, 200), scr.getpixel((276, min(y + 10, 271))), 'c')
        if y + 34 < 272:
            put((118, y + 21, 290, y + 35), name, 12, (20, 20, 20), scr.getpixel((292, y + 28)))
    scr = scr.crop((0, 0, 480, 272))
    out.paste(scr.resize((fx1 - fx0, fy1 - fy0), Image.LANCZOS), (fx0, fy0))


TUT_PAINT = {7: tut07, 14: tut14, 16: tut16}


def main():
    outdir = os.path.join(BUILD, 'assets')
    os.makedirs(outdir, exist_ok=True)
    n = 0
    for line in open(os.path.join(ROOT, 'translation', 'images_ru.tsv'), encoding='utf8'):
        if line.startswith('#') or not line.strip():
            continue
        name, kind, text = line.rstrip('\n').split('\t')
        im = img.load(os.path.join(DATA, name)).convert('RGBA')
        if kind == 'tutorial':     # BG_TUTnn.cbg: rebuilt from port captures, written as CBG header + TXG
            import tl_tutorial
            k = int(name[6:8])
            paint = TUT_PAINT.get(k)
            try:
                im = tl_tutorial.build(k, im, paint and (lambda out, orig: paint(out, orig, text)))
            except FileNotFoundError as e:
                print('skip', name, e)
                continue
            open(os.path.join(outdir, name), 'wb').write(tl_tutorial.cbg(name, im, swizzle))
            n += 1
            continue
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
        elif kind == 'gamebadge':
            im = gamebadge(im, text)
        elif kind == 'backlog':
            im = backlog(im, text)
        elif kind == 'saveparts':
            im = saveparts(im, text)
        elif kind == 'savewindow':
            im = savewindow(im, text)
        elif kind == 'friend':
            im = friend(im, text)
        elif kind == 'infoparts':
            im = infoparts(im, text)
        elif kind == 'infonames':
            im = infonames(im, text)
        elif kind == 'rowlist':
            im = rowlist(im, name, text)
        elif kind == 'rankfont':
            im = rankfont(im, text)
        elif kind == 'dlc':
            im = dlc(im, name, text)
        elif kind == 'psrts':
            im = psrts(im, text)
        elif kind == 'squareparts':
            im = squareparts(im, text)
        elif kind == 'legal':
            im = legal(im, text)
        elif kind == 'actionwin':
            im = actionwin(im, text)
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
