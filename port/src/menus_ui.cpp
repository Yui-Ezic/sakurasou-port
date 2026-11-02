#include "menus_ui.h"
#include "tr.h"

namespace mu {
float textWidth(const std::string& text, float adv) {
    const std::string& s = tr(text); bool prop = isCyrillic(s);
    float w = 0, best = 0; const char* p = s.c_str();
    while (*p) { uint32_t c = utf8Next(p); if (c == '\n') { best = std::max(best, w); w = 0; continue; } w += prop ? g_fontM.propAdv(c) : c < 0x80 ? adv / 2 : adv; }
    return std::max(best, w);
}
int textLines(const std::string& s) { int n = s.empty() ? 0 : 1; for (char c : s) if (c == '\n') n++; return n; }
void text(Font& f, const std::string& raw, float x, float yTop, Color c, float adv, float lineH, float alpha) {
    const std::string& s = tr(raw); bool prop = isCyrillic(s);
    c.a = (uint8_t)(c.a * alpha);
    const char* p = s.c_str(); float px = 240 + x, py = 136 - yTop;
    while (*p) {
        uint32_t cp = utf8Next(p);
        if (cp == '\n') { px = 240 + x; py += lineH; continue; }
        if (prop) { f.drawGlyph(cp, px - f.propLead(cp), py, c); px += f.propAdv(cp); continue; }
        f.drawGlyph(cp, px, py, c); px += cp < 0x80 ? adv / 2 : adv;
    }
}
void textCentered(Font& f, const std::string& raw, float x, float yCentre, Color c, float adv, float lineH, float alpha) {
    const std::string& s = tr(raw);
    int n = textLines(s); float yTop = yCentre + n * lineH / 2 - (lineH - f.cell()) / 2;
    size_t st = 0;
    for (int i = 0; i < n; i++) {
        size_t e = s.find('\n', st); std::string l = s.substr(st, e == std::string::npos ? e : e - st);
        text(f, l, x - textWidth(l, adv) / 2, yTop - i * lineH, c, adv, lineH, alpha);
        if (e == std::string::npos) break;
        st = e + 1;
    }
}
void commonBg() {
    static TexP t; if (!t) t = g_gfx.load("common_bg.txg");
    g_gfx.clear({255, 255, 255, 255});
    if (!t) return;
    for (int i = 0; i < 7; i++) ui(*t, {0, 0, 72, 12}, -204 + 72.f * i, 130);
    for (int j = 0; j < 9; j++) for (int i = 0; i < 16; i++) ui(*t, {72, 0, 104, 32}, -224 + 31.f * i, 108 - 32.f * j);
}
void menuBg() {
    static TexP t; if (!t) t = g_gfx.load("menu_bg.txg");
    if (!t) return;
    for (int i = 0; i < 8; i++) for (int j = 0; j < 5; j++) ui(*t, {0, 0, 64, 64}, -240 + 64.f * i, 136 - 64.f * j);
}
void ring(const Tex& t, const Ring& r, float x, float y, float hw, float hh, float a, Color c) {
    float tw = r.t.x1 - r.t.x0, lh = r.l.y1 - r.l.y0;
    float sx = (hw * 2) / tw, sy = (hh * 2) / lh;
    float ch = (r.tl.y1 - r.tl.y0) / 2.f, cw = (r.tl.x1 - r.tl.x0) / 2.f;
    ui(t, r.tl, x - hw - cw, y + hh + ch, a, 1, 1, 0, c); ui(t, r.t, x, y + hh + ch, a, sx, 1, 0, c); ui(t, r.tr, x + hw + cw, y + hh + ch, a, 1, 1, 0, c);
    ui(t, r.l, x - hw - cw, y, a, 1, sy, 0, c); ui(t, r.r, x + hw + cw, y, a, 1, sy, 0, c);
    ui(t, r.bl, x - hw - cw, y - hh - ch, a, 1, 1, 0, c); ui(t, r.b, x, y - hh - ch, a, sx, 1, 0, c); ui(t, r.br, x + hw + cw, y - hh - ch, a, 1, 1, 0, c);
}
void blinkArrow(const Tex& t, Spr left, Spr right, float xl, float xr, float y, int tick) {
    float a = triAlpha(tick, 30) / 255.f;
    ui(t, left, xl, y, a); ui(t, right, xr, y, a);
}
void valuesToRadar(const GD& g, float out[6]) {
    static const int order[6] = {3, 4, 1, 0, 5, 2};   // FUN_00086198
    for (int i = 0; i < 6; i++) out[i] = (float)std::min(g.total[order[i]], 300);
}
void radar(const Tex& t, float cx, float cy, const float* v, float scale, const SDL_FPoint uv[3]) {
    SDL_SetTextureAlphaMod(t.t, 255); SDL_SetTextureColorMod(t.t, 255, 255, 255);
    SDL_FPoint p[6]; const float d = (float)M_PI / 180;
    for (int i = 0; i < 6; i++) {
        float a = (-90 + 60.f * i) * d, r = v[i] / 300 * 31 * scale;   // value/300*31 (FUN_00086198)
        p[i] = {240 + cx + r * std::cos(a), 136 - (cy + r * std::sin(a))};
    }
    for (int i = 0; i < 6; i++) {
        SDL_Vertex tri[3] = {{{240 + cx, 136 - cy}, {255, 255, 255, 255}, uv[0]}, {p[i], {255, 255, 255, 255}, uv[1]}, {p[(i + 1) % 6], {255, 255, 255, 255}, uv[2]}};
        g_gfx.geometry(&t, tri, 3, nullptr, 0);
    }
}
void picture(const Tex& t, float x, float y, float a, float sx, float sy) { ui(t, {0, 0, t.w, t.h}, x, y, a, sx, sy); }
}
