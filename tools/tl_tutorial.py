"""Russian tutorial pictures BG_TUT01..17.cbg (shown by SCN_TUTORIAL_01..07).

Each picture is a shrunken game screen with red arrows and rings. The screen is rebuilt from the port's own Russian
rendering: tools/tl_tutorial.ctl drives a headless run that saves work/tut/sNN.bmp (tools/tl_tutorial_shots.sh), and
the capture is pasted into the original frame. Arrows are copied from the original picture through a mask of the
arrow sprite; rings are redrawn. Where the port does not draw a screen like the original (the task list, TUT07), or a
picture has drawn captions (TUT14), Russian text is painted over the original instead.

Used by tl_images.py (kind 'tutorial'); the output is the original 32-byte CBG header followed by a TXG, which the
engine reads like the ISO's CBG (gfx.cpp decodeAny), so the picture keeps its centre."""
import os
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHOTS = os.path.join(ROOT, 'work', 'tut')
SS = 4

FULL = ((0, 0, 480, 272), (84, 25, 412, 211))           # whole screen -> the 328x186 frame
COL = ((1, 108, 248, 252), (124, 46, 371, 190))         # left card column, 1:1 -> the frame of TUT09/10/13
ROW = ((1, 108, 248, 160), (124, 24, 371, 76))          # Mashiro's row, 1:1 -> TUT14's top frame

# rings: red stroke bounding box in picture pixels; arrows: bounding box of the red fill and the way it points
SPEC = {
    1: dict(shot='s01', rect=FULL, rings=[(311, 99, 402, 126)], arrows=[((408, 103, 443, 123), 'L')]),
    2: dict(shot='s02', rect=FULL),
    3: dict(shot='s02', rect=FULL, rings=[(231, 97, 284, 117)], arrows=[((190, 96, 225, 116), 'R')]),
    4: dict(shot='s04', rect=FULL, rings=[(325, 161, 379, 198)], arrows=[((385, 169, 420, 190), 'L')]),
    5: dict(shot='s05', rect=FULL, rings=[(328, 125, 364, 149)], arrows=[((370, 126, 405, 147), 'L')]),
    6: dict(shot='s06', rect=FULL, rings=[(203, 86, 239, 111)], arrows=[((161, 87, 196, 109), 'R')]),
    8: dict(shot='s08', rect=FULL, rings=[(111, 103, 185, 133)], arrows=[((70, 107, 105, 127), 'R')]),
    9: dict(shot='s09', rect=COL, rings=[(180, 49, 350, 96)], arrows=[((138, 62, 173, 82), 'R')]),
    10: dict(shot='s10', rect=COL, rings=[(180, 95, 350, 142)], arrows=[((138, 108, 173, 128), 'R')]),
    11: dict(shot='s11', rect=FULL, rings=[(92, 143, 183, 169)], arrows=[((51, 145, 86, 166), 'R')]),
    12: dict(shot='s12', rect=FULL, rings=[(92, 121, 183, 147)], arrows=[((51, 123, 86, 144), 'R')]),
    13: dict(shot='s13', rect=COL, rings=[(180, 141, 350, 188)], arrows=[((138, 154, 173, 174), 'R')]),
    14: dict(shot='s14', rect=ROW),
    15: dict(shot='s15', rect=FULL, rings=[(92, 165, 183, 191)], arrows=[((51, 167, 86, 188), 'R')]),
    16: dict(),                                          # painted over the original (tl_images.tut16)
    # TUT17: the Russian description ends in "ключ α" further right than "αキー": ring and arrow follow it
    17: dict(shot='s17', rect=FULL, rings=[(226, 86, 270, 112), (262, 160, 305, 186)], arrows=[((279, 163, 314, 185), 'L', (33, 0))]),
}

_ARROW = None


def arrow_mask():
    """The arrow sprite's footprint (fill, white and red outline, shadow) relative to its red fill box, taken from the
    clean right-pointing arrow on TUT14's plain background. Returned with its mirror image so that both directions and
    the shadow side are covered."""
    global _ARROW
    if _ARROW is None:
        import img
        a = np.array(img.load(os.path.join(ROOT, 'work', 'data', 'BG_TUT14.cbg')).convert('RGB')).astype(int)
        x0, y0, x1, y1 = 220, 142, 255, 164
        m = 6
        c = a[y0 - m:y1 + m, x0 - m:x1 + m]
        bg = np.array([230, 250, 255])
        on = np.abs(c - bg).sum(axis=2) > 24
        h, w = on.shape
        seen = np.zeros_like(on)
        st = [(m + (y1 - y0) // 2, m + (x1 - x0) // 2)]
        while st:
            y, x = st.pop()
            if 0 <= y < h and 0 <= x < w and on[y, x] and not seen[y, x]:
                seen[y, x] = True
                st += [(y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)]
        mk = Image.fromarray((seen * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(3))
        _ARROW = (np.array(mk) > 0, m)
    return _ARROW


def arrow_sprite(d):
    """The clean TUT14 arrow as RGBA (its footprint as alpha), pointing 'R' or 'L'; for arrows that have to move."""
    import img
    mk, m = arrow_mask()
    a = img.load(os.path.join(ROOT, 'work', 'data', 'BG_TUT14.cbg')).convert('RGBA')
    x0, y0 = 220 - m, 142 - m
    sp = a.crop((x0, y0, x0 + mk.shape[1], y0 + mk.shape[0]))
    sp.putalpha(Image.fromarray((mk * 255).astype(np.uint8)))
    return sp if d == 'R' else sp.transpose(Image.FLIP_LEFT_RIGHT)


def paste_arrows(out, orig, arrows):
    """(fill box, direction) copies the original arrow in place; a third item (dx, dy) draws it moved instead."""
    mk, m = arrow_mask()
    for a in arrows:
        (x0, y0, x1, y1), d = a[:2]
        if len(a) > 2:
            sp = arrow_sprite(d)
            out.alpha_composite(sp, (x0 - m + a[2][0], y0 - m + a[2][1]))
            continue
        both = mk | mk[:, ::-1]
        h, w = both.shape
        ox = x0 - m + ((x1 - x0) - (w - 2 * m)) // 2      # centred on this arrow's fill
        oy = y0 - m + ((y1 - y0) - (h - 2 * m)) // 2
        src = np.array(orig)
        dst = np.array(out)
        ys, xs = np.nonzero(both)
        ys = ys + oy; xs = xs + ox
        ok = (ys >= 0) & (ys < dst.shape[0]) & (xs >= 0) & (xs < dst.shape[1])
        dst[ys[ok], xs[ok]] = src[ys[ok], xs[ok]]
        out.paste(Image.fromarray(dst))


def draw_rings(out, rings):
    W, H = out.size
    lay = Image.new('RGBA', (W * SS, H * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(lay)
    for x0, y0, x1, y1 in rings:
        b = [v * SS for v in (x0, y0, x1, y1)]
        g = lambda e, dx=0, dy=0: (b[0] - e + dx, b[1] - e + dy, b[2] + e + dx, b[3] + e + dy)
        d.ellipse(g(SS, SS, SS * 3 // 2), outline=(40, 40, 60, 110), width=4 * SS)   # soft shadow below right
    lay = lay.filter(ImageFilter.GaussianBlur(SS * 0.6))
    d = ImageDraw.Draw(lay)
    for x0, y0, x1, y1 in rings:
        b = [v * SS for v in (x0, y0, x1, y1)]
        d.ellipse((b[0] - SS, b[1] - SS, b[2] + SS, b[3] + SS), outline=(255, 255, 255, 255), width=4 * SS)
        d.ellipse(b, outline=(226, 0, 18, 255), width=2 * SS)
    out.alpha_composite(lay.resize((W, H), Image.LANCZOS))


def screen(out, orig, shot, rect):
    (sx0, sy0, sx1, sy1), (tx0, ty0, tx1, ty1) = rect
    cap = Image.open(os.path.join(SHOTS, shot + '.bmp')).convert('RGBA').crop((sx0, sy0, sx1, sy1))
    out.paste(cap.resize((tx1 - tx0, ty1 - ty0), Image.LANCZOS), (tx0, ty0))
    # the original's 1 px frame line
    for box in ((tx0, ty0, tx1, ty0 + 1), (tx0, ty1 - 1, tx1, ty1), (tx0, ty0, tx0 + 1, ty1), (tx1 - 1, ty0, tx1, ty1)):
        out.paste(orig.crop(box), box[:2])


def build(n, orig, painter=None):
    """orig: the ISO picture as RGBA. painter(out, orig) paints extra Russian text (TUT07, TUT14)."""
    s = SPEC.get(n, {})
    out = orig.copy()
    if s.get('shot'):
        if not os.path.exists(os.path.join(SHOTS, s['shot'] + '.bmp')):
            raise FileNotFoundError(f"{s['shot']}.bmp: run tools/tl_tutorial_shots.sh first")
        screen(out, orig, s['shot'], s['rect'])
    if painter:
        painter(out, orig)
    draw_rings(out, s.get('rings', []))
    paste_arrows(out, orig, s.get('arrows', []))      # the arrows lie over the rings
    return out


def txg565(im, swizzle):
    """A 16-bit RGB565 TXG (format 1), the ISO pictures' own depth: half the size of RGBA, no visible loss."""
    import struct
    a = np.array(im.convert('RGB')).astype(np.uint32)
    v = ((a[..., 0] * 31 + 127) // 255) | ((a[..., 1] * 63 + 127) // 255) << 5 | ((a[..., 2] * 31 + 127) // 255) << 11
    h, w = v.shape
    px = swizzle(v.astype('<u2').tobytes(), w * 2, h)
    return b'TXG\0' + struct.pack('<11I', 48 + len(px), 48, len(px), 0, 0, 0, len(px), 1, 3, w, h) + px


def cbg(name, im, swizzle):
    """The ISO file's CBG header (keeps the 248,144 centre) around a TXG body."""
    from tl_common import DATA
    head = open(os.path.join(DATA, name), 'rb').read(32)
    return head + txg565(im, swizzle)
