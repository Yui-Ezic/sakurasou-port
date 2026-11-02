#!/usr/bin/env python3
"""CRI CPK archive reader: @UTF table parsing, TOC/ITOC listing, CRILAYLA decompression."""
import struct, sys, os

class UTF:
    def __init__(self, data):
        if data[:4] != b'@UTF':
            data = bytes(self._dec(data))
        assert data[:4] == b'@UTF', data[:16]
        self.d = data
        tsize, rows_off, strs_off, data_off, name_off, ncols, rowsize, nrows = struct.unpack('>IIIIIHHI', data[4:32])
        base = 8
        self.rows_off, self.strs_off, self.data_off = rows_off + base, strs_off + base, data_off + base
        self.name = self._s(name_off)
        self.cols = []
        p = 32
        constpos = []
        for i in range(ncols):
            fl = data[p]; p += 1
            nm = self._s(struct.unpack('>I', data[p:p+4])[0]); p += 4
            st = fl >> 4; ty = fl & 15
            cv = None
            if st in (3, 7):
                cv, p = self._read(ty, p)
            self.cols.append((nm, st, ty, cv))
        self.rows = []
        rp = self.rows_off
        for r in range(nrows):
            row = {}
            q = rp
            for nm, st, ty, cv in self.cols:
                if st == 1: row[nm] = None
                elif st in (3, 7): row[nm] = cv
                elif st == 5:
                    v, q = self._read(ty, q); row[nm] = v
                else:
                    row[nm] = None
            self.rows.append(row)
            rp += rowsize

    @staticmethod
    def _dec(d):
        m, a = 0x655f, 0x4115
        out = bytearray(len(d))
        for i, b in enumerate(d):
            out[i] = b ^ (m & 0xff); m = (m * a) & 0xffff
        return out

    def _s(self, off):
        p = self.strs_off + off
        e = self.d.index(b'\0', p)
        return self.d[p:e].decode('cp932', 'replace')

    def _read(self, ty, p):
        d = self.d
        f = {0:'>B',1:'>b',2:'>H',3:'>h',4:'>I',5:'>i',6:'>Q',7:'>q',8:'>f'}
        if ty in f:
            s = struct.calcsize(f[ty])
            return struct.unpack(f[ty], d[p:p+s])[0], p+s
        if ty == 0xA:
            return self._s(struct.unpack('>I', d[p:p+4])[0]), p+4
        if ty == 0xB:
            o, s = struct.unpack('>II', d[p:p+8])
            return d[self.data_off+o:self.data_off+o+s], p+8
        raise ValueError(ty)

def crilayla(c):
    """Decompress CRILAYLA. Returns bytes."""
    assert c[:8] == b'CRILAYLA'
    usize, hdr = struct.unpack('<II', c[8:16])
    out = bytearray(usize + 0x100)
    out[:0x100] = c[0x10+hdr:0x10+hdr+0x100]
    pos = len(c) - 0x100  # not used
    bitp = 0x10 + hdr
    # bitstream read backwards from end of compressed block
    p = 0x10 + hdr - 1
    bits = 0; nb = 0
    def get(n):
        nonlocal p, bits, nb
        r = 0
        while n:
            if nb == 0:
                bits = c[p]; p -= 1; nb = 8
            t = min(n, nb)
            r = (r << t) | ((bits >> (nb - t)) & ((1 << t) - 1))
            nb -= t; n -= t
        return r
    w = usize + 0x100 - 1
    end = 0x100
    while w >= end:
        if get(1):
            off = get(13) + 3
            ln = 3
            levels = [2, 3, 5, 8]
            i = 0
            while True:
                b = levels[i]
                v = get(b)
                ln += v
                if v != (1 << b) - 1: break
                if i < 3: i += 1
            for _ in range(ln):
                out[w] = out[w + off]; w -= 1
                if w < end - 1: break
        else:
            out[w] = get(8); w -= 1
    return bytes(out)

class CPK:
    def __init__(self, path):
        self.f = open(path, 'rb')
        h = self.f.read(0x800)
        assert h[:4] == b'CPK '
        sz = struct.unpack('<I', h[8:12])[0]
        self.hdr = UTF(h[16:16+sz]).rows[0]
        self.files = []
        c = self.hdr
        self.align = c.get('Align') or 0x800
        if c.get('TocOffset'):
            t = self._tbl(c['TocOffset'])
            for r in t.rows:
                self.files.append(dict(dir=r.get('DirName') or '', name=r.get('FileName') or '',
                    size=r['FileSize'], ext=r.get('ExtractSize') or r['FileSize'],
                    off=r['FileOffset'] + min(c['TocOffset'], c.get('ContentOffset') or 1<<60), id=r.get('ID')))
        if c.get('ItocOffset') and not self.files:
            t = self._tbl(c['ItocOffset'])
            self.itoc = t
            print(t.cols, t.rows[:2], file=sys.stderr)

    def _tbl(self, off):
        self.f.seek(off)
        h = self.f.read(16)
        sz = struct.unpack('<I', h[8:12])[0]
        return UTF(self.f.read(sz))

    def read(self, fe):
        self.f.seek(fe['off'])
        d = self.f.read(fe['size'])
        if d[:8] == b'CRILAYLA':
            d = crilayla(d)
        return d

if __name__ == '__main__':
    c = CPK(sys.argv[1])
    if len(sys.argv) == 2:
        print({k: v for k, v in c.hdr.items() if not isinstance(v, bytes)}, file=sys.stderr)
        print(len(c.files), file=sys.stderr)
        for f in c.files:
            print(f"{f['off']:#x} {f['size']} {f['ext']} {f['id']} {f['dir']}/{f['name']}")
    else:
        out = sys.argv[2]
        for f in c.files:
            p = os.path.join(out, f['dir'], f['name'])
            os.makedirs(os.path.dirname(p), exist_ok=True)
            open(p, 'wb').write(c.read(f))
