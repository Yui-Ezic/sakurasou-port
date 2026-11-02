#include "common.h"
#include <SDL.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <random>
#include <sstream>

SysData g_sys; Transition g_trans; Popup g_popup; HelpBar g_help; RootJob* g_root = nullptr;

static std::mt19937 s_rng((unsigned)SDL_GetPerformanceCounter());
int rng() { return (int)(s_rng() & 0x7fffffff); }

std::string savePath(const std::string& f) {
    static std::string base;
    if (base.empty()) {
        if (const char* e = getenv("SAKURA_SAVE_DIR")) { base = e; base += "/"; std::filesystem::create_directories(base); }
        else { char* p = SDL_GetPrefPath("Sakurasou", "SakurasouPort"); base = p ? p : "./"; SDL_free(p); }
    }
    return base + f;
}

void SysData::defaults() {
    raw.fill(0);
    se() = bgm() = voice() = movie() = 5; sorataVoice() = 1; textSpeed() = 1;
    g(0x3164) = 0xffffffffu;
}
void SysData::applyVolumes() {
    g_audio.setGroupVolume(0, bgm() / 5.f); g_audio.setGroupVolume(1, se() / 5.f); g_audio.setGroupVolume(2, voice() / 5.f);
}
bool SysData::load() {
    FILE* f = fopen(savePath("SYSTEM.DAT").c_str(), "rb");
    if (!f) return false;
    size_t n = fread(raw.data(), 1, raw.size(), f); fclose(f);
    if (n != raw.size()) { defaults(); return false; }
    return true;
}
bool SysData::save() {
    std::string p = savePath("SYSTEM.DAT"), t = p + ".tmp";
    FILE* f = fopen(t.c_str(), "wb");
    if (!f) return false;
    bool ok = fwrite(raw.data(), 1, raw.size(), f) == raw.size();
    ok = fclose(f) == 0 && ok;
    if (ok) std::filesystem::rename(t, p);
    return ok;
}
void rerollVoiceIndex() { auto& v = g_sys.g(0x3164); v = (int32_t)v < 0 ? 0 : rng() % 31; }
void clearBit(int n) {
    g_sys.setBit(0, n);
    static const int map[7] = {-1, 0, 4, 1, 5, 2, 3};
    if (n >= 1 && n <= 6) { g_sys.setBit(0x3134, map[n]); g_sys.setBit(0x313c, map[n]); }
    for (int i = 7; i <= 10; i++) g_sys.setBit(0x3134, i);
    for (int i = 7; i <= 11; i++) g_sys.setBit(0x313c, i);
    if ((g_sys.g(0) & 0x7e) == 0x7e) { g_sys.setBit(0x3134, 6); g_sys.setBit(0x313c, 6); }
}

void ui(const Tex& t, Spr s, float x, float y, float a, float sx, float sy, float rot, Color c) {
    float w = (float)(s.x1 - s.x0) * sx, h = (float)(s.y1 - s.y0) * sy;
    SDL_Rect src{s.x0, s.y0, s.x1 - s.x0, s.y1 - s.y0};
    SDL_FRect d{240 + x - w / 2, 136 - y - h / 2, w, h};
    SDL_SetTextureBlendMode(t.t, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(t.t, (Uint8)std::clamp(a * c.a, 0.f, 255.f));
    SDL_SetTextureColorMod(t.t, c.r, c.g, c.b);
    SDL_RenderCopyExF(g_gfx.renderer(), t.t, &src, &d, -rot, nullptr, SDL_FLIP_NONE);
}
void uiFontCentered(Font& f, const std::string& s, float x, float y, Color c) {
    int n = 0; const char* p = s.c_str(); while (*p) { utf8Next(p); n++; }
    f.drawText(s, 240 + x - n * f.cell() / 2.f, 136 - y - f.cell() / 2.f, c);
}

// ---- transition ----
void Transition::cover(int k) { if (k < 1) { mode = 1; return; } mode = 2; t = 0; n = k; }
void Transition::reveal(int k) { if (k < 1) { mode = 0; return; } mode = 3; t = 0; n = k; }
void Transition::update() {
    if (mode == 2 && ++t >= n) mode = 1;
    else if (mode == 3 && ++t >= n) mode = 0;
}
void Transition::draw() {
    if (mode == 0) return;
    float p = mode == 1 ? 1 : (float)t / n;
    if (st == 2) {
        if (!bg) { bg = g_gfx.load("menu_bg.txg"); band = g_gfx.load("menu_bg03.txg"); }
        float off = mode == 1 ? 0 : mode == 2 ? 272 - 272 * p : 272 * p;
        for (int j = 0; j < 5; j++) for (int i = 0; i < 8; i++)
            ui(*bg, {0, 0, 64, 64}, -240 + 64 * i + 32, 136 - 64 * j + off - 32);
        float by = mode == 1 ? -152 : mode == 2 ? 120 - 272 * p : -152 + 272 * p;
        ui(*band, {0, 0, 64, 32}, 16, by, 1, 8, 1);
        return;
    }
    Color c = st == 4 ? Color{255, 255, 255, 255} : Color{0, 0, 0, 255};
    if (st == 1) {
        float dy = mode == 1 ? 0 : mode == 2 ? -(1 - p) * 272 : -p * 272;
        g_gfx.fillRect(0, dy, 480, 272, c);
        return;
    }
    c.a = (uint8_t)(255 * (mode == 1 ? 1 : mode == 2 ? p : 1 - p));
    g_gfx.fillRect(0, 0, 480, 272, c);
}

// ---- popup (spec 6.1, FUN_000147f0, FUN_00013e78) ----
static float s_mx() { return g_input.mouseX - 240.f; }
static float s_my() { return 136.f - g_input.mouseY; }
static int glyphCount(const std::string& s) { int n = 0; const char* p = s.c_str(); while (*p) { if (utf8Next(p) != '\n') n++; } return n; }
// advance of the game's text layouters: full width w, half width w/2 (FUN_00013ad8)
static float layoutWidth(const std::string& s, float adv) {
    float w = 0; const char* p = s.c_str();
    while (*p) { uint32_t c = utf8Next(p); if (c == '\n') continue; w += (c < 0x80 ? adv / 2 : adv); }
    return w;
}
static void layoutLine(const std::string& s, float x, float y, Color c, float adv) {
    const char* p = s.c_str();
    while (*p) { uint32_t cp = utf8Next(p); if (cp == '\n') continue; g_fontM.drawGlyph(cp, x, y, c); x += cp < 0x80 ? adv / 2 : adv; }
}
void Popup::open(const std::string& t, bool twoB, bool defYes) {
    text = t; two = twoB; cur = two && !defYes ? 1 : 0; state = 1; h = 1; resultYes = resultCancel = false;
    lines.clear(); std::stringstream ss(t); std::string l; float tw = 0;
    while (std::getline(ss, l)) { lines.push_back(l); tw = std::max(tw, 5.5f + layoutWidth(l, 14.5f)); }
    target = std::max(tw / 14.5f * 2 - 2, two ? 38.f : 20.f);
    if (!win) win = g_gfx.load("common_pop_window.txg");
}
void Popup::update() {
    if (state == 1) { h += 4; if (h >= target) { h = target; state = 2; } }
    else if (state == 3) { h -= 4; if (h <= 1) { h = 1; state = 0; } }
    else if (state == 2) {
        float base = lines.size() < 3 ? 0.f : -16.f, by = base - 25;
        int over = -1;
        for (int i = 0; i < (two ? 2 : 1); i++) {
            float bx = two ? (i ? 75.f : -75.f) : 0.f;
            if (std::abs(s_mx() - bx) < 56 && std::abs(s_my() - by) < 18) over = i;
        }
        if (two && g_input.mouseMoved && over >= 0 && over != cur) { cur = over; g_audio.playCue(0x2a); }
        if (two && (g_input.isPressed(BTN_LEFT) || g_input.isPressed(BTN_RIGHT))) { bool l = g_input.isPressed(BTN_LEFT); if (cur != (l ? 0 : 1)) { cur = l ? 0 : 1; g_audio.playCue(0x2a); } }
        bool ok = g_input.isPressed(BTN_CIRCLE) && !(g_input.mouseClick && over < 0);
        if (ok) { resultYes = cur == 0; g_audio.playCue(resultYes ? 0x29 : 0x1f); state = 3; }
        else if (g_input.isPressed(BTN_CROSS)) { resultCancel = true; resultYes = false; g_audio.playCue(0x1f); state = 3; }
    }
}
void Popup::draw() {
    if (!state || !win) return;
    const bool few = lines.size() < 3;
    const float base = few ? 0.f : -16.f;
    static const float ya[6] = {50, 42, 24, 6, -22, -52}, yb[6] = {86, 78, 42, 6, -22, -52};
    for (int r = 0; r < 6; r++) {
        float ry = base + (few ? ya[r] : yb[r]);
        float sy = r == 2 ? (few ? 4.f : 8.f) : r == 4 ? 6.f : 1.f;
        int v0 = r * 8, v1 = r == 5 ? 52 : v0 + 8;
        ui(*win, {0, v0, 8, v1}, -(h * 4 + 4), ry, 1, 1, sy);
        ui(*win, {8, v0, 16, v1}, -2, ry, 1, h + 0.5f, sy);   // +0.5 closes the 2 px gap to the right cap
        ui(*win, {16, v0, 32, v1}, h * 4 + 8, ry, 1, 1, sy);
    }
    if (state != 2) return;
    float ty = base + (few ? 24 : 42) + lines.size() * 16 / 2.f - 1;
    for (auto& l : lines) { layoutLine(l, 240 - layoutWidth(l, 14.5f) / 2, 136 - ty, {0, 0, 0, 255}, 14.5f); ty -= 16; }
    float by = base - 25;
    for (int i = 0; i < (two ? 2 : 1); i++) {
        float bx = two ? (i ? 75.f : -75.f) : 0.f;
        Color c = cur == i ? Color{255, 255, 0xba, 255} : Color{};
        static const float rows[3] = {10, 0, -10}, rsy[3] = {1, 2, 1};
        for (int r = 0; r < 3; r++) {
            ui(*win, {40, r * 8, 48, r * 8 + 8}, bx - 50, by + rows[r], 1, 1, rsy[r], 0, c);
            ui(*win, {48, r * 8, 56, r * 8 + 8}, bx, by + rows[r], 1, 12, rsy[r], 0, c);
            ui(*win, {56, r * 8, 64, r * 8 + 8}, bx + 50, by + rows[r], 1, 1, rsy[r], 0, c);
        }
        std::string lab = i ? "いいえ" : "はい";
        layoutLine(lab, 240 + bx + 3 - layoutWidth(lab, 14.5f) / 2, 136 - by - 7, {0, 0, 0, 255}, 14.5f);
        if (cur == i) {
            ui(*win, {40, 24, 48, 32}, bx - 56, by + 16); ui(*win, {48, 24, 56, 32}, bx, by + 16, 1, 13); ui(*win, {56, 24, 64, 32}, bx + 56, by + 16);
            ui(*win, {40, 40, 48, 48}, bx - 56, by - 16); ui(*win, {48, 40, 56, 48}, bx, by - 16, 1, 13); ui(*win, {56, 40, 64, 48}, bx + 56, by - 16);
            ui(*win, {40, 32, 48, 40}, bx - 56, by, 1, 1, 3.2f); ui(*win, {56, 32, 64, 40}, bx + 56, by, 1, 1, 3.2f);
        }
    }
}

// ---- help bar (spec 6.2, FUN_000100d4). Two ticker items 480 apart, both move 1 px per frame, wrap at -480 ----
void HelpBar::show(const std::string& t) { text = t; mode = 1; scroll = 0; if (!tex) tex = g_gfx.load("common_help.txg"); }
void HelpBar::hide() { mode = 2; }
void HelpBar::update() {
    if (mode == 1) { if (y < -124) y = std::min(-124.f, y + 2); else scroll = (scroll + 1) % 960; }
    if (mode == 2) { y -= 4; if (y < -148) { y = -148; mode = 0; } }
}
void HelpBar::draw() {
    if (y <= -148 || !tex) return;
    ui(*tex, {1, 0, 7, 24}, 0, y, 1, 100, 1);
    // first 32 full-width columns go to item 0, the rest to item 1 (FUN_00010600); short text is centred in both
    int cols = 0, half = 0; size_t cut = text.size(); const char* p = text.c_str();
    while (*p) { const char* q = p; uint32_t c = utf8Next(p); int nc = cols; if (c < 0x80) { if (++half == 2) { half = 0; nc++; } } else nc++; if (nc > 32) { cut = q - text.c_str(); break; } cols = nc; }
    std::string a = text.substr(0, cut), b = text.substr(cut);
    bool shortText = b.empty();
    for (int i = 0; i < 2; i++) {
        int px = (480 * i - scroll + 480 + 960 * 2) % 960 - 480;
        const std::string& s = shortText ? a : (i ? b : a);
        float left = shortText ? 240 + px - layoutWidth(s, 15) / 2 : (float)px + 15;  // ASSUMED long-text anchor: left edge of the item's 480 px span
        layoutLine(s, left, 136 - (y - 2) - 7, {255, 255, 255, 255}, 15);
    }
}

// ---- header (spec 6.3, FUN_00018ffc, FUN_00018a4c, FUN_0001937c) ----
static const char* kPage[] = {"action_choice","calendar","task","square","action1","action2","action3","option","menu","quick_load","save","load","info","tips","backlog","omake_menu","omake_evcg","omake_voice","omake_friend","omake_standup",
    "dlc_save01","dlc_save02","dlc_save03","dlc_save04","dlc_load01","dlc_load02","dlc_load03","dlc_load04","dlc"};
void PageHeader::set(int p) { tex = g_gfx.load(std::string("common_page_") + kPage[p] + ".txg"); deco = g_gfx.load("common_page_decoration.txg"); }
void PageHeader::start() { mode = 1; f = 0; }
void PageHeader::leave() { mode = 2; f = std::min(f, 25); }
void PageHeader::update() {
    if (!g_trans.clear()) return;
    if (mode == 1 && f < 25) f++;
    if (mode == 2 && f > 0) f--;
}
// the curled strip mesh at 0x1e9a28: 21 columns over 256 px, y bent by bend*(1-sqrt(1-(x/256)^2))
static void headerStrip(const Tex& t, float ox, float oy, float rad, float sx, float bend) {
    SDL_SetTextureAlphaMod(t.t, 255); SDL_SetTextureColorMod(t.t, 255, 255, 255);
    SDL_Vertex v[42]; float c = std::cos(rad), s = std::sin(rad);
    for (int i = 0; i <= 20; i++) {
        float mx = 12.8f * i, k = std::min(1.f, mx / 256), off = bend * (1 - std::sqrt(1 - k * k));
        for (int j = 0; j < 2; j++) {
            float my = (j ? -16.f : 16.f) + off, wx = mx * sx, wy = my;
            SDL_FPoint p{240 + ox + wx * c - wy * s, 136 - (oy + wx * s + wy * c)};
            v[i * 2 + j] = SDL_Vertex{p, SDL_Color{255, 255, 255, 255}, SDL_FPoint{mx / 256, (float)j}};
        }
    }
    int ni = 0; int ix[120];
    for (int i = 0; i < 20; i++) { int b = i * 2; int t6[6] = {b, b + 1, b + 2, b + 2, b + 1, b + 3}; for (int k : t6) ix[ni++] = k; }
    g_gfx.geometry(&t, v, 42, ix, ni);
}
void PageHeader::draw() {
    if (!tex || !deco || !mode) return;
    const float pi = (float)M_PI;
    if (f < 15) {
        float t = f / 15.f, bend = std::cos(t * pi / 2) * 32;   // FUN_00018ffc: bands flatten out
        headerStrip(*deco, -310, 20, 0.8f, t, bend); headerStrip(*deco, 310, -20, 4.f, t, bend);
    } else { headerStrip(*deco, -310, 20, 0.8f, 1, 0); headerStrip(*deco, 310, -20, 4.f, 1, 0); }
    if (f < 10) return;
    float t = (f - 10) / 15.f;
    headerStrip(*tex, -240, 115, 0, t, std::cos(t * pi / 2) * 32);
}
