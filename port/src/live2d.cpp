#include "live2d.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace l2d {
namespace {

constexpr float EPS = 0.0001f;
constexpr int kMaxDims = 6;

// ---- moc object stream ----
struct Obj { virtual ~Obj() = default; };
using OP = std::shared_ptr<Obj>;
struct OStr : Obj { std::string s; };
struct OList : Obj { std::vector<OP> v; };
struct OInts : Obj { std::vector<int> v; };
struct OFloats : Obj { std::vector<float> v; };
struct OPivots : Obj { ParamPivots p; };
struct OPivotMgr : Obj { PivotMgr p; };
struct OAffine : Obj { Affine a; };
struct ODeformer : Obj { Deformer d; };
struct OMesh : Obj { Mesh m; };
struct OParamDef : Obj { float lo = 0, hi = 0, def = 0; std::string id; };
struct OParamSet : Obj { OP list; };
struct OPart : Obj { bool locked = false, visible = false; std::string id; OP deformers, meshes; };
struct OModel : Obj { OP params, parts; int w = 0, h = 0; };

struct Reader {
    const uint8_t* p; const uint8_t* e;
    int ver = 0, bitPos = 0; uint8_t bitByte = 0;
    std::vector<OP> objects;

    void need(size_t n) const { if ((size_t)(e - p) < n) throw std::runtime_error("moc truncated"); }
    uint8_t u8() { need(1); return *p++; }
    int num() {
        int v = 0;
        for (int i = 0; i < 4; i++) { uint8_t b = u8(); v = v << 7 | (b & 0x7f); if (!(b & 0x80)) break; }
        return v;
    }
    uint32_t u32be() { need(4); uint32_t v = (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; p += 4; return v; }
    int i32() { bitPos = 0; return (int)u32be(); }
    float f32() { bitPos = 0; uint32_t v = u32be(); float f; memcpy(&f, &v, 4); return f; }
    double f64() {
        bitPos = 0; uint64_t v = (uint64_t)u32be() << 32; v |= u32be();
        double d; memcpy(&d, &v, 8); return d;
    }
    bool bit() {
        if (bitPos == 0 || bitPos == 8) { bitByte = u8(); bitPos = 0; }
        return bitByte >> (7 - bitPos++) & 1;
    }
    std::string bytes() { int n = num(); need(n); std::string s((const char*)p, n); p += n; return s; }
    std::vector<int> intArray() { int n = num(); std::vector<int> v(n); for (auto& x : v) x = (int)u32be(); return v; }
    std::vector<float> floatArray() {
        int n = num(); std::vector<float> v(n);
        for (auto& x : v) { uint32_t u = u32be(); memcpy(&x, &u, 4); }
        return v;
    }

    OP object(int flags = 0) {
        bitPos = 0;
        int cid = num();
        if (cid == 0x21) {
            int i = i32();
            if (i < 0 || i >= (int)objects.size()) throw std::runtime_error("moc: bad reference");
            return objects[i];
        }
        OP o = build(cid, flags);
        objects.push_back(o);
        return o;
    }
    template <class T> std::shared_ptr<T> as(const OP& o) { return std::dynamic_pointer_cast<T>(o); }
    std::string idOf(const OP& o) { auto s = as<OStr>(o); return s ? s->s : std::string(); }

    PivotMgr pivotMgr() { auto m = as<OPivotMgr>(object()); return m ? m->p : PivotMgr{}; }
    std::vector<std::vector<float>> floatLists(const OP& o) {
        std::vector<std::vector<float>> r;
        if (auto l = as<OList>(o)) for (auto& e : l->v) { auto f = as<OFloats>(e); r.push_back(f ? f->v : std::vector<float>{}); }
        return r;
    }

    OP build(int cid, int flags) {
        switch (cid) {
        case 0: return nullptr;
        case 1: case 0x32: case 0x33: case 0x3c: case 0x86: { auto o = std::make_shared<OStr>(); o->s = bytes(); return o; }
        case 0x0f: { auto o = std::make_shared<OList>(); int n = num(); for (int i = 0; i < n; i++) o->v.push_back(object()); return o; }
        case 0x19: {
            auto o = std::make_shared<OInts>(); o->v = intArray();
            if (flags & 1) for (auto& x : o->v) x &= 0xffff;
            return o;
        }
        case 0x1a: { int n = num(); auto o = std::make_shared<OFloats>(); o->v.resize(n); for (auto& x : o->v) x = (float)f64(); return o; }
        case 0x1b: { auto o = std::make_shared<OFloats>(); o->v = floatArray(); return o; }
        case 0x41: {
            auto o = std::make_shared<ODeformer>(); auto& d = o->d; d.type = 2;
            d.id = idOf(object()); d.target = idOf(object());
            d.rows = i32(); d.cols = i32(); d.piv = pivotMgr(); d.keys = floatLists(object());
            return o;
        }
        case 0x42: {
            auto o = std::make_shared<OPivotMgr>();
            if (auto l = as<OList>(object())) for (auto& e : l->v) if (auto q = as<OPivots>(e)) o->p.push_back(q->p);
            return o;
        }
        case 0x43: {
            auto o = std::make_shared<OPivots>();
            o->p.id = idOf(object()); o->p.count = i32();
            if (auto f = as<OFloats>(object())) o->p.pivots = f->v;
            return o;
        }
        case 0x44: {
            auto o = std::make_shared<ODeformer>(); auto& d = o->d; d.type = 1;
            d.id = idOf(object()); d.target = idOf(object()); d.piv = pivotMgr();
            if (auto l = as<OList>(object())) for (auto& e : l->v) { auto a = as<OAffine>(e); d.akeys.push_back(a ? a->a : Affine{}); }
            return o;
        }
        case 0x45: {
            auto o = std::make_shared<OAffine>(); auto& a = o->a;
            a.ox = f32(); a.oy = f32(); a.sx = f32(); a.sy = f32(); a.rot = f32();
            return o;
        }
        case 0x46: {
            auto o = std::make_shared<OMesh>(); auto& m = o->m;
            m.id = idOf(object()); m.target = idOf(object()); m.piv = pivotMgr();
            m.order = i32();
            m.orderKeys = intArray(); m.opacityKeys = floatArray();
            m.texNo = i32(); m.pointCount = i32(); m.triCount = i32();
            if (auto a = as<OInts>(object(1))) m.indices = a->v;
            m.keys = floatLists(object());
            if (auto a = as<OFloats>(object())) m.uv = a->v;
            if (ver >= 8) {
                int fl = i32();
                if (fl & 1) i32();
                m.blend = (fl & 0x1e) >> 1;
            }
            return o;
        }
        case 0x83: {
            auto o = std::make_shared<OParamDef>();
            o->lo = f32(); o->hi = f32(); o->def = f32(); o->id = idOf(object());
            return o;
        }
        case 0x85: {
            auto o = std::make_shared<OPart>();
            o->locked = bit(); o->visible = bit();
            o->id = idOf(object()); o->deformers = object(); o->meshes = object();
            return o;
        }
        case 0x88: {
            auto o = std::make_shared<OModel>();
            o->params = object(); o->parts = object(); o->w = i32(); o->h = i32();
            return o;
        }
        case 0x89: { auto o = std::make_shared<OParamSet>(); o->list = object(); return o; }
        }
        char b[48]; snprintf(b, sizeof b, "moc: unknown class 0x%x", cid);
        throw std::runtime_error(b);
    }
};

// ---- key selection (6.2) ----
struct KeySel {
    bool outside = false;
    int dims = 0, n = 1;
    float t[kMaxDims];
    int key[1 << kMaxDims];
    float w[1 << kMaxDims];
};

void selectKeys(PivotMgr& piv, const std::vector<float>& val, KeySel& s) {
    s.outside = false;
    for (auto& pp : piv) {
        float v = pp.param >= 0 && pp.param < (int)val.size() ? val[pp.param] : 0;
        int n = pp.count, index; float t = 0;
        const float* p = pp.pivots.data();
        if (n < 1 || (int)pp.pivots.size() < n) index = -1;
        else if (n == 1) { index = 0; if (v <= p[0] - EPS || p[0] + EPS <= v) s.outside = true; }
        else if (v < p[0] - EPS) { index = 0; s.outside = true; }
        else if (v < p[0] + EPS) index = 0;
        else {
            index = -1;
            for (int i = 1; i < n; i++)
                if (v < p[i] + EPS) {
                    if (v <= p[i] - EPS) { index = i - 1; t = (v - p[i - 1]) / (p[i] - p[i - 1]); }
                    else index = i;
                    break;
                }
            if (index < 0) { index = n - 1; s.outside = true; }
        }
        pp.index = index; pp.t = t;
    }
    int base = 0, mul = 1, dimMul[kMaxDims];
    s.dims = 0;
    for (auto& pp : piv) {
        base += pp.index * mul;
        if (pp.t != 0 && s.dims < kMaxDims) { dimMul[s.dims] = mul; s.t[s.dims] = pp.t; s.dims++; }
        mul *= pp.count;
    }
    s.n = 1 << s.dims;
    for (int k = 0; k < s.n; k++) {
        int idx = base; float w = 1;
        for (int j = 0; j < s.dims; j++) {
            bool on = k >> j & 1;
            if (on) idx += dimMul[j];
            w *= on ? s.t[j] : 1 - s.t[j];
        }
        s.key[k] = idx; s.w[k] = w;
    }
}

int lerpI(int a, int b, float t) { return (int)((float)a + (float)(b - a) * t); }

int interpInt(const std::vector<int>& k, const KeySel& s) {
    auto at = [&](int i) { return s.key[i] >= 0 && s.key[i] < (int)k.size() ? k[s.key[i]] : 0; };
    if (s.dims >= 5) {
        float r = 0;
        for (int i = 0; i < s.n; i++) r += s.w[i] * (float)at(i);
        return (int)r;
    }
    int v[1 << kMaxDims] = {};
    for (int i = 0; i < s.n; i++) v[i] = at(i);
    for (int j = 0, n = s.n; j < s.dims; j++, n /= 2)
        for (int i = 0; i < n / 2; i++) v[i] = lerpI(v[i * 2], v[i * 2 + 1], s.t[j]);
    return v[0];
}

float interpFloat(const std::vector<float>& k, const KeySel& s) {
    float r = 0;
    for (int i = 0; i < s.n; i++) if (s.key[i] >= 0 && s.key[i] < (int)k.size()) r += s.w[i] * k[s.key[i]];
    return r;
}

void interpPoints(const std::vector<std::vector<float>>& keys, const KeySel& s, std::vector<float>& out) {
    if (keys.empty()) return;
    out.assign(keys[0].size(), 0.f);
    for (int i = 0; i < s.n; i++) {
        if (s.key[i] < 0 || s.key[i] >= (int)keys.size()) continue;
        const auto& k = keys[s.key[i]];
        float w = s.w[i];
        size_t c = std::min(k.size(), out.size());
        for (size_t j = 0; j < c; j++) out[j] += w * k[j];
    }
}

// ---- deformer point transform ----
void warpTransform(const Deformer& d, const float* src, float* dst, int n) {
    const std::vector<float>& g = d.base ? d.grid : d.world;
    int W = d.cols, H = d.rows, total = (int)g.size() / 2;
    auto at = [&](int k, int c) { return k < total ? g[k * 2 + c] : 0.f; };
    for (int i = 0; i < n; i++) {
        float u = std::clamp(src[i * 2], 0.f, 1.f), v = std::clamp(src[i * 2 + 1], 0.f, 1.f);
        float x = u * W, y = v * H;
        int ix = (int)x, iy = (int)y;
        float fx = x - ix, fy = y - iy;
        int k = ix + iy * (W + 1);
        for (int c = 0; c < 2; c++) {
            float A = at(k, c), R = at(k + 1, c), D = at(k + W + 1, c);
            if (fx + fy < 1) dst[i * 2 + c] = A + fx * (R - A) + fy * (D - A);
            else {
                float Q = at(k + W + 2, c);
                dst[i * 2 + c] = Q + (1 - fx) * (D - Q) + (1 - fy) * (R - Q);
            }
        }
    }
}

void rotTransform(const Deformer& d, const float* src, float* dst, int n) {
    const Affine& e = d.base ? d.a : d.wa;
    float r = 0.017453292f * e.rot, c = cosf(r), s = sinf(r);
    float m00 = c * e.sx, m01 = -s * e.sy, m10 = s * e.sx, m11 = c * e.sy;
    for (int i = 0; i < n; i++) {
        float x = src[i * 2], y = src[i * 2 + 1];
        dst[i * 2] = m00 * x + m01 * y + e.ox;
        dst[i * 2 + 1] = m10 * x + m11 * y + e.oy;
    }
}

void transform(const Deformer& d, const float* src, float* dst, int n) {
    if (d.type == 2) warpTransform(d, src, dst, n); else rotTransform(d, src, dst, n);
}

// 6.5. The original leaves the output unset when every probe collapses; that gives zero here.
void directionThroughParent(const Deformer& par, const float o[2], const float dir[2], float out[2]) {
    float p0[2]; transform(par, o, p0, 1);
    float f = 1;
    out[0] = out[1] = 0;
    for (int i = 0; i < 10; i++) {
        float q[2] = {o[0] + f * dir[0], o[1] + f * dir[1]}, r[2];
        transform(par, q, r, 1);
        r[0] -= p0[0]; r[1] -= p0[1];
        if (r[0] != 0 || r[1] != 0) { out[0] = r[0]; out[1] = r[1]; return; }
        q[0] = o[0] - f * dir[0]; q[1] = o[1] - f * dir[1];
        transform(par, q, r, 1);
        r[0] -= p0[0]; r[1] -= p0[1];
        if (r[0] != 0 || r[1] != 0) { out[0] = r[0]; out[1] = r[1]; return; }
        f *= 0.1f;
    }
}

}  // namespace

// ---- construction (6.1) ----
int Model::addParam(const std::string& id, float lo, float hi, float def) {
    int i = (int)val.size();
    val.push_back(def); saved.push_back(def); mn.push_back(lo); mx.push_back(hi);
    ids[id] = i;      // later duplicates win: the original searches from the end
    return i;
}

int Model::paramIndex(const std::string& name) {
    auto i = ids.find(name);
    return i != ids.end() ? i->second : addParam(name, -1e6f, 1e6f, 0);
}

void Model::bind(PivotMgr& p) { for (auto& pp : p) pp.param = paramIndex(pp.id); }

bool Model::parse(const uint8_t* d, size_t n) {
    if (n < 8 || memcmp(d, "moc", 3)) return false;
    try {
        Reader r{d + 4, d + n};
        r.ver = d[3];
        auto root = r.as<OModel>(r.object());
        if (!root) return false;
        if (r.u32be() != 0x88888888u) { fprintf(stderr, "moc: missing end sentinel\n"); return false; }
        canvasW = root->w; canvasH = root->h;

        std::vector<Deformer> all;
        auto partList = r.as<OList>(root->parts);
        if (partList) for (auto& po : partList->v) {
            auto p = r.as<OPart>(po);
            if (!p) continue;
            parts.push_back({p->id, p->visible ? 1.f : 0.f});
            partIdx[p->id] = (int)parts.size() - 1;
            if (auto l = r.as<OList>(p->deformers)) for (auto& e : l->v) if (auto o = r.as<ODeformer>(e)) all.push_back(o->d);
            if (auto l = r.as<OList>(p->meshes)) for (auto& e : l->v) if (auto o = r.as<OMesh>(e)) {
                meshes.push_back(o->m);
                meshes.back().part = (int)parts.size() - 1;
            }
        }

        // Repeat passes over the unplaced deformers until a pass places none; leftovers are dropped.
        std::unordered_map<std::string, int> placed;
        std::vector<char> used(all.size(), 0);
        for (bool again = true; again;) {
            again = false;
            for (size_t i = 0; i < all.size(); i++) {
                if (used[i]) continue;
                auto& t = all[i].target;
                bool base = t.empty() || t == "DST_BASE";
                if (!base && !placed.count(t)) continue;
                used[i] = 1; again = true;
                all[i].base = base;
                all[i].parent = base ? -1 : placed[t];
                placed[all[i].id] = (int)deformers.size();
                deformers.push_back(std::move(all[i]));
            }
        }
        for (auto& m : meshes) {
            m.base = m.target.empty() || m.target == "DST_BASE";
            if (!m.base) { auto i = placed.find(m.target); m.parent = i != placed.end() ? i->second : -1; }
            m.local.assign(m.pointCount * 2, 0.f);
        }

        if (auto ps = r.as<OParamSet>(root->params))
            if (auto l = r.as<OList>(ps->list))
                for (auto& e : l->v) if (auto q = r.as<OParamDef>(e)) addParam(q->id, q->lo, q->hi, q->def);
        for (auto& x : deformers) {
            bind(x.piv);
            if (x.type == 2) {
                int pts = (x.rows + 1) * (x.cols + 1);
                x.grid.assign(pts * 2, 0.f);
                if (!x.base) x.world.assign(pts * 2, 0.f);
            }
        }
        for (auto& m : meshes) bind(m.piv);
    } catch (const std::exception& e) {
        fprintf(stderr, "%s\n", e.what());
        return false;
    }
    return true;
}

// ---- parameters and parts ----
void Model::setParam(int i, float v) {
    if (i < 0 || i >= (int)val.size()) return;
    if (std::isnan(v)) v = 0;
    val[i] = std::clamp(v, mn[i], mx[i]);
}
void Model::addWeighted(const std::string& n, float v, float w) {
    int i = paramIndex(n);
    setParam(i, value(i) * (1 - w) + v * w);
}
void Model::loadParam() { size_t c = std::min(val.size(), saved.size()); std::copy_n(saved.begin(), c, val.begin()); }
void Model::saveParam() { saved = val; }

float Model::partOpacity(const std::string& name) const {
    auto i = partIdx.find(name);
    return i != partIdx.end() ? parts[i->second].opacity : 0;
}
void Model::setPartOpacity(const std::string& name, float v) {
    auto i = partIdx.find(name);
    if (i != partIdx.end()) parts[i->second].opacity = v;
}

// ---- evaluation (6.3 - 6.5) ----
void Model::update() {
    KeySel s;
    for (auto& d : deformers) {
        selectKeys(d.piv, val, s);
        d.outside = s.outside;
        const Deformer* par = d.parent >= 0 ? &deformers[d.parent] : nullptr;
        d.ready = true;
        if (d.type == 2) {
            interpPoints(d.keys, s, d.grid);
            if (!d.base) {
                if (!par || !par->ready || par->outside) d.ready = false;
                else transform(*par, d.grid.data(), d.world.data(), (int)d.grid.size() / 2);
            }
        } else {
            Affine a{0, 0, 0, 0, 0};
            for (int i = 0; i < s.n; i++) {
                if (s.key[i] < 0 || s.key[i] >= (int)d.akeys.size()) continue;
                const Affine& k = d.akeys[s.key[i]]; float w = s.w[i];
                a.ox += w * k.ox; a.oy += w * k.oy; a.sx += w * k.sx; a.sy += w * k.sy; a.rot += w * k.rot;
            }
            d.a = a;
            if (d.base) continue;
            if (!par) { d.ready = false; continue; }
            float step = par->type == 1 ? -10.0f : -0.1f;
            float o[2] = {a.ox, a.oy}, dir0[2] = {0, step}, v[2], w[2];
            directionThroughParent(*par, o, dir0, v);
            double angle = atan2((double)dir0[1], (double)dir0[0]) - atan2((double)v[1], (double)v[0]);
            while (angle > M_PI) angle -= 2 * M_PI;
            while (angle < -M_PI) angle += 2 * M_PI;
            transform(*par, o, w, 1);
            d.wa = a;
            d.wa.ox = w[0]; d.wa.oy = w[1];
            d.wa.rot = a.rot - 57.29578f * (float)angle;
            d.ready = par->ready && !par->outside;
        }
    }
    for (auto& m : meshes) {
        selectKeys(m.piv, val, s);
        m.outside = s.outside;
        if (m.outside) continue;
        m.order = interpInt(m.orderKeys, s);
        m.opacity = interpFloat(m.opacityKeys, s);
        interpPoints(m.keys, s, m.local);
        if (m.base) { m.final_ = m.local; continue; }
        const Deformer* par = m.parent >= 0 ? &deformers[m.parent] : nullptr;
        if (!par || par->outside) { m.available = false; continue; }
        m.final_.resize(m.local.size());
        transform(*par, m.local.data(), m.final_.data(), (int)m.local.size() / 2);
        m.available = true;
    }
}

const std::vector<int>& Model::drawOrder() {
    order_.resize(meshes.size());
    for (size_t i = 0; i < order_.size(); i++) order_[i] = (int)i;
    std::stable_sort(order_.begin(), order_.end(), [&](int a, int b) { return meshes[a].order < meshes[b].order; });
    return order_;
}

}  // namespace l2d
