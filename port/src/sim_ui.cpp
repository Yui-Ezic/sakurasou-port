#include "sim_ui.h"
#include "tr.h"
#include <algorithm>
#include <sstream>

void q(const Tex& t, const SQ& s, float x, float y, float a, Color c, float sx, float sy) {
    float x0 = std::min(s.x0, s.x1) * sx, x1 = std::max(s.x0, s.x1) * sx, yt = s.y0 * sy, yb = s.y1 * sy;
    SDL_FRect d{240 + x + x0, 136 - y - yt, x1 - x0, yt - yb};
    SDL_Rect src{(int)std::lround(s.u0 * t.w), (int)std::lround(s.v0 * t.h), (int)std::lround((s.u1 - s.u0) * t.w), (int)std::lround((s.v1 - s.v0) * t.h)};
    SDL_SetTextureBlendMode(t.t, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(t.t, (Uint8)std::clamp(a * c.a, 0.f, 255.f));
    SDL_SetTextureColorMod(t.t, c.r, c.g, c.b);
    SDL_RenderCopyF(g_gfx.renderer(), t.t, &src, &d);
}
void qStrip(const Tex& t, const SQ& l, const SQ& r, float x, float y, float a, Color c, float sx, float sy) {
    q(t, l, x, y, a, c, sx, sy);
    SQ m{l.u1, l.v0, r.u0, r.v1, l.x1, l.y0, r.x0, r.y1};
    if (m.u1 > m.u0 && m.x1 > m.x0) q(t, m, x, y, a, c, sx, sy);
    q(t, r, x, y, a, c, sx, sy);
}

static bool half(uint32_t cp) { return cp < 0x80; }
static Font& simFont(int cell) { return cell > 11 ? g_fontM : g_fontS; }
float textWidth(const std::string& text, int cell) {
    const std::string& s = tr(text); bool prop = isCyrillic(s);
    Font& f = simFont(cell); float sc = (float)cell / f.cell();
    float w = 0; const char* p = s.c_str();
    while (*p) { uint32_t cp = utf8Next(p); if (cp == '\n') break; w += prop ? f.propAdv(cp) * sc : half(cp) ? cell * 0.5f : cell; }
    return w;
}
void simText(const std::string& text, float x, float y, int cell, Color c, int lineH) {
    const std::string& s = tr(text); bool prop = isCyrillic(s);
    if (!lineH) lineH = cell + 4;
    Font& f = simFont(cell);
    float sc = (float)cell / f.cell(), px = 240 + x, py = 136 - y;
    const char* p = s.c_str();
    while (*p) {
        uint32_t cp = utf8Next(p);
        if (cp == '\n') { px = 240 + x; py += lineH; continue; }
        if (prop) { f.drawGlyph(cp, px - f.propLead(cp) * sc, py, c, sc); px += f.propAdv(cp) * sc; continue; }
        f.drawGlyph(cp, px, py, c, sc);
        px += half(cp) ? cell * 0.5f : cell;
    }
}
void simTextCentered(const std::string& text, float x, float y, int cell, Color c, int lineH) {
    const std::string& s = tr(text);
    if (!lineH) lineH = cell + 4;
    std::vector<std::string> lines; std::stringstream ss(s); std::string l;
    while (std::getline(ss, l)) lines.push_back(l);
    float h = cell + (lines.size() - 1) * lineH, ty = y + h / 2;
    for (auto& ln : lines) { simText(ln, x - textWidth(ln, cell) / 2, ty, cell, c); ty -= lineH; }
}

void drawCommonBg() {
    static std::weak_ptr<Tex> cache;
    TexP t = cache.lock();
    if (!t) { t = g_gfx.load("common_bg.txg"); cache = t; }
    g_gfx.clear({250, 248, 240, 255});
    if (!t) return;
    for (int j = 0; j < 9; j++) for (int i = 0; i < 16; i++) q(*t, T_1e6f48[0], -224 + i * 31.f, 108 - j * 32.f);
    for (int i = 0; i < 7; i++) q(*t, T_1e6f48[1], -204 + i * 72.f, 130);
}
int phaseGroup() {
    int p = gd().routeA;
    return p <= 2 ? 0 : p <= 4 ? 1 : p <= 6 ? 2 : p == 7 ? 3 : 4;
}

void DeadlineWidget::init(float nx, float ny) {
    x = nx; y = ny; group = phaseGroup(); turn = gd().turn;
    static const int dl[4] = {13, 42, 77, 91};
    deadline = group < 4 ? dl[group] : 0;
    font = g_gfx.load("common_font.txg");
}
void DeadlineWidget::draw() {
    if (group == 4 || !font) return;
    const Tex& f = *font;
    const SQ* T = T_1e8a38;
    if (group != 3) q(f, T[20], x + 5, y + 10);
    q(f, T[21], x + 30, y + 10);
    q(f, T[22], x - 20, y - 22);
    q(f, T[23], x + 30, y - 22);
    float lx = x - 23 + (group == 0 ? -7 : group == 3 ? 3 : 0);
    q(f, T[24 + group], lx, y + 10);
    int v = std::max(deadline - turn, 0);
    Color red = argb(0xff0035ff), black = argb(0xff000000);
    if (v < 10) {
        if (v < 4) q(f, T[v], x + 8, y - 16, 1, red);
        else q(f, T[10 + v], x + 8, y - 18, 1, black);
    } else for (int i = 0; i < 2; i++, v /= 10) q(f, T[10 + v % 10], x + 16 - i * 15.f, y - 18, 1, black);
}

// ---- toast ----
void Toast::init() {
    parts = g_gfx.load("action_parts.txg"); faces = g_gfx.load("action_parts_icon.txg");
    game = g_gfx.load("action_parts_game.txg"); quick = g_gfx.load("action_parts_quick.txg");
}
void Toast::show(const std::string& s, int ic, uint32_t b, int, bool k) {
    text = s; icon = ic; bits = b; key = k; st = 1; t = 0;
    g_audio.playCue(0x25);
}
void Toast::update() {
    if (!st) return;
    t++;
    if (st == 1 && t >= 30) { st = 2; t = 0; }
    else if (st == 2) {
        if (g_input.isPressed(BTN_CIRCLE)) t += 0x78;
        if (t >= 0x78) { st = 3; t = 0; }
    } else if (st == 3 && t >= 30) { st = 0; t = 0; }
}
void Toast::drawAt(float dx) {
    if (bits && game) {      // FUN_0002cbe0: hexagon with the six stat labels
        float hx = dx - 180;
        q(*game, T_1ebd9c[0], hx, 0);
        static const float off[6][2] = {{0, 29}, {40, 16}, {40, -16}, {0, -29}, {-40, -16}, {-40, 16}};
        for (int i = 0; i < 6; i++)
            q(*game, T_1ebdf4[i], hx + off[i][0], off[i][1], 1, (bits >> i & 1) ? Color{} : Color{128, 128, 128, 255});
    }
    if (icon == 0) simTextCentered(text, dx, 0, 14, {0, 0, 0, 255}, 18);
    else {
        if (faces) q(*faces, T_1ebb64[icon - 1], dx, 18);
        simTextCentered(text, dx, -14, 14, {0, 0, 0, 255}, 18);
    }
    if (key && quick) q(*quick, T_1ebfdc[0], dx, 34);
}
void Toast::draw() {
    if (!st) return;
    float dx = 0, sy = 1;
    if (st == 1) { dx = 512 - std::sin((float)M_PI / 2 * t / 30) * 512; sy = std::min(t / 10.f, 1.f); }
    if (st == 3) { dx = -std::sin((float)M_PI / 2 * t / 30) * 512; if (t >= 20) sy = 1 - std::min((t - 20) / 10.f, 1.f); }
    if (parts) q(*parts, T_1eba30[0], 0, 0, 1, {}, 1, sy);   // ASSUMED: bar is the action_parts band
    drawAt(dx);
}
