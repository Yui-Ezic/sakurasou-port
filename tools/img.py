#!/usr/bin/env python3
"""TXG / XGTL / CBG / CSU decoders -> PIL images. run with ~/tools/sakura-venv/bin/python"""
import struct, sys
from PIL import Image
import numpy as np

def unswizzle(buf, wbytes, h):
    """PSP GE swizzle: 16-byte x 8-line blocks."""
    out = bytearray(len(buf))
    bw = wbytes // 16
    bh = h // 8
    src = 0
    for by in range(bh):
        for bx in range(bw):
            for line in range(8):
                dst = (by * 8 + line) * wbytes + bx * 16
                out[dst:dst+16] = buf[src:src+16]
                src += 16
    return bytes(out)

def txg(d, o=0, swz=True):
    tot, hs, ds, co, io, cs, ims, fmt, cfmt, w, h = struct.unpack('<11I', d[o+4:o+48])
    b = o + hs
    bpp = {4: 1, 0: 4, 1: 2}[fmt]
    if swz:
        d = d[:b + io] + unswizzle(d[b+io:b+io+w*h*bpp], w * bpp, h) + d[b+io+w*h*bpp:]
    if fmt == 4:
        pal = np.frombuffer(d, np.uint8, cs, b + co).reshape(-1, 4)
        idx = np.frombuffer(d, np.uint8, w * h, b + io)
        return Image.fromarray(pal[idx].reshape(h, w, 4), 'RGBA')
    if fmt == 0:
        return Image.fromarray(np.frombuffer(d, np.uint8, w*h*4, b + io).reshape(h, w, 4), 'RGBA')
    if fmt == 1:
        p = np.frombuffer(d, '<u2', w*h, b + io).astype(np.uint32)
        r = (p & 31) * 255 // 31; g = ((p >> 5) & 63) * 255 // 63; bl = ((p >> 11) & 31) * 255 // 31
        a = np.full_like(r, 255)
        return Image.fromarray(np.stack([r, g, bl, a], -1).astype(np.uint8).reshape(h, w, 4), 'RGBA')
    raise ValueError(fmt)

def xgtl(d, o=0):
    tot, w, h, tw, th, cols, rows = struct.unpack('<7I', d[o+4:o+32])
    first, step = struct.unpack('<II', d[o+32:o+40])
    im = Image.new('RGBA', (w, h))
    p = o + first
    for y in range(rows):
        for x in range(cols):
            im.paste(txg(d, p, True).crop((0, 0, tw, th)), (x * tw, y * th))
            p += step
    return im

def load(path):
    d = open(path, 'rb').read()
    m = d[:4]
    if m == b'TXG\0': return txg(d)
    if m == b'XGTL': return xgtl(d)
    if m == b'CBG\0': return xgtl(d, struct.unpack('<I', d[8:12])[0])
    if m == b'CSU\0': return xgtl(d, struct.unpack('<I', d[8:12])[0])
    raise ValueError(m)

if __name__ == '__main__':
    load(sys.argv[1]).save(sys.argv[2])
