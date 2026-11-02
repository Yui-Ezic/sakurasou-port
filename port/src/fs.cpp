#include "fs.h"
#include <algorithm>
#include <cstring>
#include <algorithm>

AssetFS g_fs;

static uint32_t be32(const uint8_t* p) { return p[0]<<24 | p[1]<<16 | p[2]<<8 | p[3]; }
static uint16_t be16(const uint8_t* p) { return p[0]<<8 | p[1]; }
static uint64_t be64(const uint8_t* p) { return (uint64_t)be32(p)<<32 | be32(p+4); }
static uint32_t le32(const uint8_t* p) { return p[0] | p[1]<<8 | p[2]<<16 | (uint32_t)p[3]<<24; }

namespace {
struct Val { int type = -1; uint64_t u = 0; std::string s; };
using Row = std::unordered_map<std::string, Val>;

// CRI @UTF table
std::vector<Row> parseUtf(Bytes d) {
    std::vector<Row> rows;
    if (d.size() < 32) return rows;
    if (memcmp(d.data(), "@UTF", 4) != 0) {
        int m = 0x655f; const int a = 0x4115;
        for (auto& b : d) { b ^= m & 0xff; m = (m * a) & 0xffff; }
        if (memcmp(d.data(), "@UTF", 4) != 0) return rows;
    }
    const uint8_t* p0 = d.data();
    uint32_t rowsOff = be32(p0+8) + 8, strOff = be32(p0+12) + 8, dataOff = be32(p0+16) + 8;
    int ncols = be16(p0+24), rowW = be16(p0+26); uint32_t nrows = be32(p0+28);
    auto str = [&](uint32_t o) { return std::string((const char*)p0 + strOff + o); };
    auto rd = [&](int ty, const uint8_t*& p) {
        Val v; v.type = ty;
        switch (ty) {
        case 0: case 1: v.u = *p; p += 1; break;
        case 2: case 3: v.u = be16(p); p += 2; break;
        case 4: case 5: case 8: v.u = be32(p); p += 4; break;
        case 6: case 7: v.u = be64(p); p += 8; break;
        case 0xA: v.s = str(be32(p)); p += 4; break;
        case 0xB: p += 8; break;
        }
        return v;
    };
    struct Col { std::string n; int st, ty; Val c; };
    std::vector<Col> cols;
    const uint8_t* p = p0 + 32;
    for (int i = 0; i < ncols; i++) {
        int fl = *p++; std::string n = str(be32(p)); p += 4;
        Col c{n, fl >> 4, fl & 15, {}};
        if (c.st == 3 || c.st == 7) c.c = rd(c.ty, p);
        cols.push_back(std::move(c));
    }
    for (uint32_t r = 0; r < nrows; r++) {
        const uint8_t* q = p0 + rowsOff + r * rowW;
        Row row;
        for (auto& c : cols) {
            if (c.st == 3 || c.st == 7) row[c.n] = c.c;
            else if (c.st == 5) row[c.n] = rd(c.ty, q);
        }
        rows.push_back(std::move(row));
    }
    return rows;
}

Bytes crilayla(const Bytes& c) {
    uint32_t usize = le32(&c[8]), hdr = le32(&c[12]);
    Bytes out(usize + 0x100);
    memcpy(out.data(), &c[0x10 + hdr], 0x100);
    int64_t p = 0x10 + (int64_t)hdr - 1;
    uint32_t bits = 0; int nb = 0;
    auto get = [&](int n) {
        uint32_t r = 0;
        while (n) {
            if (!nb) { bits = c[p--]; nb = 8; }
            int t = std::min(n, nb);
            r = (r << t) | ((bits >> (nb - t)) & ((1u << t) - 1));
            nb -= t; n -= t;
        }
        return r;
    };
    int64_t w = (int64_t)usize + 0x100 - 1;
    static const int lv[4] = {2, 3, 5, 8};
    while (w >= 0x100) {
        if (get(1)) {
            int off = get(13) + 3, ln = 3, i = 0;
            for (;;) {
                int b = lv[i]; uint32_t v = get(b); ln += v;
                if (v != (1u << b) - 1) break;
                if (i < 3) i++;
            }
            for (int k = 0; k < ln && w >= 0x100; k++, w--) out[w] = out[w + off];
        } else out[w--] = (uint8_t)get(8);
    }
    return out;
}
}

bool Cpk::open(FILE* f, uint64_t b) {
    fp = f; base = b;
    auto table = [&](uint64_t off) {
        uint8_t h[16];
        fseeko(fp, base + off, SEEK_SET);
        if (fread(h, 1, 16, fp) != 16) return std::vector<Row>{};
        uint32_t sz = le32(h + 8);
        Bytes t(sz);
        if (fread(t.data(), 1, sz, fp) != sz) return std::vector<Row>{};
        return parseUtf(std::move(t));
    };
    auto hdr = table(0);
    if (hdr.empty()) return false;
    uint64_t toc = hdr[0]["TocOffset"].u;
    auto rows = table(toc);
    for (auto& r : rows) {
        Ent e;
        e.off = r["FileOffset"].u + toc;
        e.size = (uint32_t)r["FileSize"].u;
        e.ext = (uint32_t)r["ExtractSize"].u;
        idx[r["FileName"].s] = e;
    }
    return !idx.empty();
}

bool Cpk::locate(const std::string& n, uint64_t& off, uint64_t& size) const {
    auto i = idx.find(n);
    if (i == idx.end()) return false;
    off = base + i->second.off; size = i->second.size;
    return true;
}

Bytes Cpk::read(const std::string& n) {
    auto i = idx.find(n);
    if (i == idx.end()) return {};
    Bytes d(i->second.size);
    fseeko(fp, base + i->second.off, SEEK_SET);
    if (fread(d.data(), 1, d.size(), fp) != d.size()) return {};
    if (d.size() > 16 && memcmp(d.data(), "CRILAYLA", 8) == 0) return crilayla(d);
    return d;
}

std::vector<std::string> Cpk::names() const {
    std::vector<std::string> v;
    for (auto& k : idx) v.push_back(k.first);
    return v;
}

// minimal ISO9660 lookup of a path
static bool isoFind(FILE* f, const char* path, uint64_t& off, uint64_t& size) {
    uint8_t pvd[2048];
    fseeko(f, 16 * 2048, SEEK_SET);
    if (fread(pvd, 1, 2048, f) != 2048 || memcmp(pvd + 1, "CD001", 5)) return false;
    uint32_t lba = le32(pvd + 156 + 2), len = le32(pvd + 156 + 10);
    std::string p = path;
    size_t pos = 0;
    while (pos < p.size()) {
        size_t e = p.find('/', pos);
        if (e == std::string::npos) e = p.size();
        std::string part = p.substr(pos, e - pos);
        pos = e + 1;
        Bytes dir(len);
        fseeko(f, (uint64_t)lba * 2048, SEEK_SET);
        if (fread(dir.data(), 1, len, f) != len) return false;
        bool found = false;
        for (uint32_t i = 0; i < len;) {
            int rl = dir[i];
            if (!rl) { i = (i / 2048 + 1) * 2048; continue; }
            int nl = dir[i + 32];
            std::string n((char*)&dir[i + 33], nl);
            if (auto s = n.find(';'); s != std::string::npos) n.resize(s);
            if (n == part) { lba = le32(&dir[i + 2]); len = le32(&dir[i + 10]); found = true; break; }
            i += rl;
        }
        if (!found) return false;
    }
    off = (uint64_t)lba * 2048; size = len;
    return true;
}

bool AssetFS::mount(const std::string& path) {
    fp = fopen(path.c_str(), "rb");
    if (!fp) { error = "cannot open " + path; return false; }
    uint64_t o1, o2, s;
    if (!isoFind(fp, "PSP_GAME/USRDIR/DATA.CPK", o1, s) || !isoFind(fp, "PSP_GAME/USRDIR/STRM.CPK", o2, s)) {
        error = "not the expected game image"; return false;
    }
    if (!data.open(fp, o1) || !strm.open(fp, o2)) { error = "bad CPK"; return false; }
    return true;
}
static bool readFile(const std::string& p, Bytes& out) {
    FILE* f = fopen(p.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    out.resize(n > 0 ? n : 0);
    size_t got = n > 0 ? fread(out.data(), 1, n, f) : 0;
    fclose(f);
    return (long)got == n;
}
Bytes AssetFS::read(const std::string& n) {
    Bytes o;
    if (!overrideDir.empty() && readFile(overrideDir + "/" + n, o)) return o;
    return data.contains(n) ? data.read(n) : strm.read(n);
}
bool AssetFS::exists(const std::string& n) const { return data.contains(n) || strm.contains(n); }
std::vector<std::string> AssetFS::names() const {
    auto a = data.names(); auto b = strm.names(); a.insert(a.end(), b.begin(), b.end()); return a;
}
