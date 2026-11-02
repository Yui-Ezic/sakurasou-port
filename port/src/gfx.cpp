#include "gfx.h"
#include <algorithm>
#include <cstring>
#include <cstdio>

Gfx g_gfx; Font g_fontM, g_fontS;
static uint32_t le32(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }

static void unswizzle(const uint8_t* src, std::vector<uint8_t>& out, int wbytes, int h) {
    out.resize((size_t)wbytes * h);
    int bw = wbytes / 16, bh = h / 8;
    for (int by = 0; by < bh; by++)
        for (int bx = 0; bx < bw; bx++)
            for (int l = 0; l < 8; l++) {
                memcpy(&out[(size_t)(by * 8 + l) * wbytes + bx * 16], src, 16);
                src += 16;
            }
}

bool decodeTxg(const uint8_t* d, size_t n, Image& im) {
    if (n < 0x30 || memcmp(d, "TXG", 4)) return false;
    uint32_t hs = le32(d + 8), co = le32(d + 16), io = le32(d + 20), fmt = le32(d + 32);
    int w = le32(d + 40), h = le32(d + 44);
    const uint8_t* b = d + hs;
    int bpp = fmt == 4 ? 1 : fmt == 0 ? 4 : fmt == 1 ? 2 : 0;
    if (!bpp) { fprintf(stderr, "TXG: unknown format %u\n", fmt); return false; }
    std::vector<uint8_t> raw;
    unswizzle(b + io, raw, w * bpp, h);
    im.w = w; im.h = h; im.px.resize((size_t)w * h);
    if (fmt == 4) {
        const uint32_t* pal = (const uint32_t*)(b + co);
        for (size_t i = 0; i < im.px.size(); i++) im.px[i] = pal[raw[i]];
    } else if (fmt == 0) memcpy(im.px.data(), raw.data(), raw.size());
    else {
        const uint16_t* p = (const uint16_t*)raw.data();
        for (size_t i = 0; i < im.px.size(); i++) {
            uint32_t v = p[i];
            uint32_t r = (v & 31) * 255 / 31, g = ((v >> 5) & 63) * 255 / 63, bl = ((v >> 11) & 31) * 255 / 31;
            im.px[i] = r | g << 8 | bl << 16 | 0xff000000u;
        }
    }
    return true;
}

bool decodeXgtl(const uint8_t* d, size_t n, Image& im) {
    if (n < 0x30 || memcmp(d, "XGTL", 4)) return false;
    int w = le32(d + 8), h = le32(d + 12), tw = le32(d + 16), th = le32(d + 20), cols = le32(d + 24), rows = le32(d + 28);
    uint32_t first = le32(d + 32), step = le32(d + 36);
    im.w = w; im.h = h; im.px.assign((size_t)w * h, 0);
    size_t p = first;
    for (int y = 0; y < rows; y++)
        for (int x = 0; x < cols; x++, p += step) {
            Image t;
            if (p + step > n || !decodeTxg(d + p, step, t)) return false;
            for (int j = 0; j < th && y * th + j < h; j++) {
                int cw = std::min(tw, w - x * tw);
                if (cw > 0) memcpy(&im.px[(size_t)(y * th + j) * w + x * tw], &t.px[(size_t)j * t.w], cw * 4);
            }
        }
    return true;
}

bool decodeAny(const Bytes& d, Image& im, int* cx, int* cy) {
    if (d.size() < 16) return false;
    const uint8_t* p = d.data();
    if (!memcmp(p, "TXG", 4)) return decodeTxg(p, d.size(), im);
    if (!memcmp(p, "XGTL", 4)) return decodeXgtl(p, d.size(), im);
    if (!memcmp(p, "CBG", 4) || !memcmp(p, "CSU", 4)) {
        uint32_t o = le32(p + 8);
        if (!memcmp(p, "CBG", 4)) { if (cx) *cx = le32(p + 16); if (cy) *cy = le32(p + 20); }
        return decodeXgtl(p + o, d.size() - o, im);
    }
    return false;
}

bool Gfx::init(bool headless, int scale, bool fullscreen) {
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) { fprintf(stderr, "SDL video: %s\n", SDL_GetError()); return false; }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    Uint32 fl = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    if (headless) fl |= SDL_WINDOW_HIDDEN;
    win = SDL_CreateWindow("さくら荘のペットな彼女", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W * scale, H * scale, fl);
    if (!win) { fprintf(stderr, "window: %s\n", SDL_GetError()); return false; }
    r = SDL_CreateRenderer(win, -1, (headless ? SDL_RENDERER_SOFTWARE : SDL_RENDERER_ACCELERATED) | SDL_RENDERER_TARGETTEXTURE);
    if (!r) r = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE | SDL_RENDERER_TARGETTEXTURE);
    if (!r) { fprintf(stderr, "renderer: %s\n", SDL_GetError()); return false; }
    canvas = SDL_CreateTexture(r, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET, W, H);
    if (fullscreen) toggleFullscreen();
    return canvas != nullptr;
}
void Gfx::shutdown() { cache.clear(); if (r) SDL_DestroyRenderer(r); if (win) SDL_DestroyWindow(win); r = nullptr; win = nullptr; }
void Gfx::toggleFullscreen() { fs = !fs; SDL_SetWindowFullscreen(win, fs ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0); }

TexP Gfx::fromImage(const Image& im) {
    auto t = std::make_shared<Tex>();
    t->t = SDL_CreateTexture(r, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, im.w, im.h);
    if (!t->t) return nullptr;
    SDL_UpdateTexture(t->t, nullptr, im.px.data(), im.w * 4);
    SDL_SetTextureBlendMode(t->t, SDL_BLENDMODE_BLEND);
    t->w = im.w; t->h = im.h;
    return t;
}

TexP Gfx::load(const std::string& name) {
    if (auto i = cache.find(name); i != cache.end()) if (auto p = i->second.lock()) return p;
    Bytes d = g_fs.read(name);
    Image im; int cx = 0, cy = 0;
    if (d.empty() || !decodeAny(d, im, &cx, &cy)) { fprintf(stderr, "missing image %s\n", name.c_str()); return nullptr; }
    auto t = fromImage(im);
    if (t) { t->cx = cx; t->cy = cy; cache[name] = t; }
    return t;
}

void Gfx::beginFrame() { SDL_SetRenderTarget(r, canvas); }
SDL_Rect Gfx::dstRect() const {
    int ww, wh; SDL_GetRendererOutputSize(r, &ww, &wh);
    double s = std::min((double)ww / W, (double)wh / H);
    int dw = (int)(W * s), dh = (int)(H * s);
    return {(ww - dw) / 2, (wh - dh) / 2, dw, dh};
}
void Gfx::endFrame() {
    SDL_SetRenderTarget(r, nullptr);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);
    SDL_Rect d = dstRect();
    SDL_RenderCopy(r, canvas, nullptr, &d);
    SDL_RenderPresent(r);
}
void Gfx::clear(Color c) { SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a); SDL_RenderClear(r); }
void Gfx::draw(const Tex& t, float x, float y, float a, float sx, float sy, Color c, SDL_BlendMode bm) {
    drawSub(t, {0, 0, t.w, t.h}, {x, y, t.w * sx, t.h * sy}, a, c, bm);
}
void Gfx::drawSub(const Tex& t, SDL_Rect s, SDL_FRect d, float a, Color c, SDL_BlendMode bm) {
    SDL_SetTextureBlendMode(t.t, bm);
    SDL_SetTextureAlphaMod(t.t, (Uint8)std::clamp(a * c.a, 0.f, 255.f));
    SDL_SetTextureColorMod(t.t, c.r, c.g, c.b);
    SDL_RenderCopyF(r, t.t, &s, &d);
}
void Gfx::fillRect(float x, float y, float w, float h, Color c) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_FRect f{x, y, w, h}; SDL_RenderFillRectF(r, &f);
}
void Gfx::geometry(const Tex* t, const SDL_Vertex* v, int nv, const int* idx, int ni, SDL_BlendMode bm) {
    if (t) SDL_SetTextureBlendMode(t->t, bm);
    SDL_RenderGeometry(r, t ? t->t : nullptr, v, nv, idx, ni);
}
SDL_Texture* Gfx::makeTarget(int w, int h) {
    SDL_Texture* t = SDL_CreateTexture(r, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET, w, h);
    if (t) SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    return t;
}
void Gfx::setTarget(SDL_Texture* t) { SDL_SetRenderTarget(r, t ? t : canvas); }
bool Gfx::screenshot(const std::string& path) {
    SDL_SetRenderTarget(r, canvas);
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_ABGR8888);
    SDL_RenderReadPixels(r, nullptr, SDL_PIXELFORMAT_ABGR8888, s->pixels, s->pitch);
    int ok = SDL_SaveBMP(s, path.c_str());
    SDL_FreeSurface(s);
    return ok == 0;
}
bool Gfx::readCanvas(Image& o) {
    o.w = W; o.h = H; o.px.resize(W * H);
    SDL_Texture* cur = SDL_GetRenderTarget(r);
    SDL_SetRenderTarget(r, canvas);
    int e = SDL_RenderReadPixels(r, nullptr, SDL_PIXELFORMAT_ABGR8888, o.px.data(), W * 4);
    SDL_SetRenderTarget(r, cur);
    return e == 0;
}
void Gfx::toCanvas(int wx, int wy, int& cx, int& cy) {
    int ww, wh, pw, ph; SDL_GetWindowSize(win, &ww, &wh); SDL_GetRendererOutputSize(r, &pw, &ph);
    SDL_Rect d = dstRect();
    double px = wx * (double)pw / ww, py = wy * (double)ph / wh;
    cx = (int)((px - d.x) * W / d.w); cy = (int)((py - d.y) * H / d.h);
}

uint32_t utf8Next(const char*& p) {
    uint8_t c = *p++;
    if (c < 0x80) return c;
    int n = c >= 0xf0 ? 3 : c >= 0xe0 ? 2 : 1;
    uint32_t cp = c & (0x3f >> n);
    while (n-- && *p) cp = cp << 6 | (*p++ & 0x3f);
    return cp;
}

bool Font::load(const std::string& a) {
    data = g_fs.read(a);
    if (data.size() < 0x20010 || memcmp(data.data(), "FNT", 4)) return false;
    sz = le32(&data[8]);
    // Build a white-with-alpha atlas of all cells so glyphs draw as ordinary textures.
    int glyphs = (int)((data.size() - 0x20010) / (sz * sz / 2));
    int rows = glyphs / atlasCols + 1;
    Image im; im.w = atlasCols * sz; im.h = rows * sz; im.px.assign((size_t)im.w * im.h, 0x00ffffffu);
    int bytes = sz * sz / 2;
    for (int g = 0; g < glyphs; g++) {
        const uint8_t* gp = &data[0x20010 + (size_t)g * bytes];
        int ox = (g % atlasCols) * sz, oy = (g / atlasCols) * sz;
        for (int i = 0; i < sz * sz; i++) {
            int v = (gp[i / 2] >> ((i & 1) * 4)) & 15;
            im.px[(size_t)(oy + i / sz) * im.w + ox + i % sz] = 0x00ffffffu | (uint32_t)(v * 17) << 24;
        }
    }
    tex = g_gfx.fromImage(im);
    return tex != nullptr;
}
const uint8_t* Font::glyph(uint32_t cp) const {
    if (cp > 0xffff) return nullptr;
    uint16_t i; memcpy(&i, &data[0x10 + cp * 2], 2);
    return i ? &data[0x20010] + (size_t)i * (sz * sz / 2) : nullptr;
}
void Font::drawGlyph(uint32_t cp, float x, float y, Color c, float s) {
    if (cp > 0xffff) return;
    uint16_t i; memcpy(&i, &data[0x10 + cp * 2], 2);
    if (!i && cp != 0x20) return;
    g_gfx.drawSub(*tex, {(i % atlasCols) * sz, (i / atlasCols) * sz, sz, sz}, {x, y, sz * s, sz * s}, 1, c);
}
void Font::drawText(const std::string& t, float x, float y, Color c, int pitch) {
    if (!pitch) pitch = sz;
    const char* p = t.c_str(); float ox = x;
    while (*p) {
        uint32_t cp = utf8Next(p);
        if (cp == '\n') { x = ox; y += sz + 2; continue; }
        drawGlyph(cp, x, y, c); x += pitch;
    }
}
