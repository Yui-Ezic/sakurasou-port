// Tips 0x13, backlog 0x1d, DLC menu 0x1e, character information 0x1f, DLC movie 0x27.
#include "menus.h"
#include "menus_data.h"
#include "menus_ui.h"
#include "movie.h"
#include <cstdio>

namespace {
// mirrored copy of an atlas rect (tips arrows use reversed U)
void uiFlip(const Tex& t, Spr s, float x, float y, float a) {
    float w = (float)(s.x1 - s.x0), h = (float)(s.y1 - s.y0);
    SDL_Rect src{s.x0, s.y0, s.x1 - s.x0, s.y1 - s.y0}; SDL_FRect d{240 + x - w / 2, 136 - y - h / 2, w, h};
    SDL_SetTextureAlphaMod(t.t, (Uint8)(255 * a)); SDL_SetTextureColorMod(t.t, 255, 255, 255);
    SDL_RenderCopyExF(g_gfx.renderer(), t.t, &src, &d, 0, nullptr, SDL_FLIP_HORIZONTAL);
}

// ---- 0x13 tips: FUN_00095af8. 11 pages of 3x3, tip n = col + 3*row + 9*(page-1), 0-based here ----
struct TipRec { int index; std::string title, reading, body; };
const std::vector<TipRec>& tipData() {
    static std::vector<TipRec> v;
    if (!v.empty()) return v;
    Bytes d = g_fs.read("tips_list.tipd");
    if (d.size() < 8) return v;
    int n = *(const int*)(d.data() + 4); const uint8_t* p = d.data() + 8;
    for (int i = 0; i < n && p + 392 <= d.data() + d.size() + 8; i++, p += 392) {
        int avail = (int)(d.data() + d.size() - p);
        if (avail < 136) break;
        TipRec t; t.index = *(const int*)p;
        t.title = (const char*)p + 8; t.reading = (const char*)p + 72;
        t.body.assign((const char*)p + 136, strnlen((const char*)p + 136, std::min(256, avail - 136)));
        v.push_back(t);
    }
    return v;
}
struct TipsScene : Job {
    TexP parts, pageT, win; PageHeader hdr; int page = 1, col = 0, row = 0, exitSt = 0, tick = 0; float slide = 0; int prevPage = 1;
    int sel() const { return (page - 1) * 9 + row * 3 + col; }
    void onEnter() override {
        parts = g_gfx.load("tips_parts.txg"); pageT = g_gfx.load("tips_page.txg"); win = g_gfx.load("tips_window.txg");
        hdr.set(13); hdr.start(); markSeen();
    }
    void onResume() override { g_trans.style(0); g_trans.reveal(10); }
    bool on(int k) { return g_sys.bit(0x3144, k); }
    void markSeen() { if (on(sel())) g_sys.setBit(0x3154, sel()); }
    void turn(int d) { prevPage = page; page = (page - 1 + d + 11) % 11 + 1; slide = d > 0 ? 480 : -480; }
    void onUpdate() override {
        tick++; hdr.update();
        if (exitSt) { if (g_trans.covered() && exitSt == 1) { selfPop(); exitSt = 2; } return; }
        if (slide != 0) { slide = slide > 0 ? std::max(0.f, slide - 24) : std::min(0.f, slide + 24); return; }
        int oc = col, or_ = row, op = page, over = -1;
        for (int i = 0; i < 9; i++) if (mu::hit(-152 + 152.f * (i % 3), 80 - 36.f * (i / 3), 148, 32)) over = i;
        if (g_input.mouseMoved && over >= 0) { col = over % 3; row = over / 3; }
        if (mu::repUp() && !g_input.wheel) row = (row + 2) % 3;
        if (mu::repDown() && !g_input.wheel) row = (row + 1) % 3;
        if (g_input.isRepeat(BTN_LEFT)) { if (col == 0) { col = 2; turn(-1); } else col--; }
        if (g_input.isRepeat(BTN_RIGHT)) { if (col == 2) { col = 0; turn(1); } else col++; }
        if (g_input.isPressed(BTN_R) || g_input.wheel < 0) turn(1);
        if (g_input.isPressed(BTN_L) || g_input.wheel > 0) turn(-1);
        if (oc != col || or_ != row || op != page) { mu::se(0x2a); markSeen(); }
        if (mu::cancel()) { mu::se(0x1f); g_trans.cover(10); exitSt = 1; }
    }
    void drawPage(int pg, float ox) {
        const auto& d = tipData();
        mu::Ring grey{{0, 0, 8, 8}, {8, 0, 16, 8}, {16, 0, 24, 8}, {0, 8, 8, 16}, {16, 8, 24, 16}, {0, 16, 8, 24}, {8, 16, 16, 24}, {16, 16, 24, 24}};
        mu::Ring blue{{0, 24, 8, 32}, {8, 24, 16, 32}, {16, 24, 24, 32}, {0, 32, 8, 40}, {16, 32, 24, 40}, {0, 40, 8, 48}, {8, 40, 16, 48}, {16, 40, 24, 48}};
        for (int i = 0; i < 9; i++) {
            int k = (pg - 1) * 9 + i; if (k >= (int)d.size() && k >= 99) break;
            float x = ox - 152 + 152.f * (i % 3), y = 80 - 36.f * (i / 3); bool u = k < 99 && g_sys.bit(0x3144, k), s = pg == page && i == row * 3 + col;
            ui(*parts, u ? Spr{8, 32, 16, 40} : Spr{8, 8, 16, 16}, x, y, 1, 17, 3);
            mu::ring(*parts, u ? blue : grey, x, y, 68, 12);
            if (u && k < (int)d.size()) {
                float adv = mu::textWidth(d[k].title, 14.5f) > 130 ? 130.f / (mu::textWidth(d[k].title, 14.5f) / 14.5f) : 14.5f;
                mu::textCentered(g_fontM, d[k].title, x, y, {0, 0, 0, 255}, adv, 18);
                if (!g_sys.bit(0x3154, k)) ui(*parts, {0, 48, 32, 64}, x + 56, y + 12);
            }
            if (s) {
                mu::Ring r{{56, 32, 64, 40}, {64, 32, 72, 40}, {72, 32, 80, 40}, {56, 40, 64, 48}, {72, 40, 80, 48}, {56, 48, 64, 56}, {64, 48, 72, 56}, {72, 48, 80, 56}};
                mu::ring(*parts, r, x, y, 72, 12);
            }
        }
    }
    void onDraw() override {
        mu::commonBg(); hdr.draw();
        if (!parts) return;
        drawPage(page, slide);
        if (slide) drawPage(prevPage, slide - (slide > 0 ? 480 : -480));
        for (int i = 0; i < 11; i++) { int f = 10 - i == page - 1 ? 0 : 1; ui(*pageT, {16 * f, 0, 16 * f + 16, 16}, 220 - 16.f * i, 115); }
        float a = mu::triAlpha(tick, 51) / 255.f;
        ui(*parts, {40, 32, 56, 64}, -233, 44, a); uiFlip(*parts, {40, 32, 56, 64}, 233, 44, a);
        // text window at y -68: FUN_0001f2ec
        if (win) {
            const float wx = 0, wy = -68, h = 46;
            static const float rows[6] = {50, 42, 26, 10, -23, -60}, sy[6] = {1, 1, 3, 1, 7.25f, 1};
            for (int r = 0; r < 6; r++) {
                int v0 = r * 8, v1 = r == 5 ? 64 : v0 + 8; float y = wy + rows[r];
                ui(*win, {0, v0, 8, v1}, wx - (h * 4 + 4), y, 1, 1, sy[r]);
                ui(*win, {8, v0, 16, v1}, wx - 2, y, 1, h + 0.5f, sy[r]);   // +0.5 closes the 2 px gap to the right cap
                ui(*win, {16, v0, 32, v1}, wx + h * 4 + 8, y, 1, 1, sy[r]);
            }
        }
        const auto& d = tipData(); int k = sel();
                static const char* fd[10] = {"０", "１", "２", "３", "４", "５", "６", "７", "８", "９"};
        std::string hd = std::string("ＴＩＰＳ ＮＯ．") + fd[(k + 1) / 10] + fd[(k + 1) % 10];
        mu::textCentered(g_fontM, hd, -105, -68 + 25, {0, 0, 0, 255}, 14.5f, 18);
        if (k < (int)d.size() && g_sys.bit(0x3144, k)) {
            mu::textCentered(g_fontM, d[k].title, 62, -68 + 25, {0, 0, 0, 255}, 14.5f, 18);
            mu::text(g_fontM, d[k].body, -180, -68 + 2, {0, 0, 0, 255}, 14.5f, 18);   // ASSUMED top-left of the 1024x256 body text
        }
    }
};

// ---- 0x1d backlog: spec_adv section 5 ----
struct BacklogScene : Job {
    TexP bg, parts, arrow; PageHeader hdr; int sel = 0, exitSt = 0, tick = 0; float d = 0;
    static constexpr float kCy = 2;  // ASSUMED: the list centre sits just above the screen centre (help bar below)
    static int linesOf(const BacklogEntry& e) { int n = 1; for (char c : e.text) if (c == '\n') n++; return n; }
    static int heightOf(const BacklogEntry& e) { return linesOf(e) * 18 + 24; }
    void onEnter() override {
        bg = g_gfx.load("backlog_bg.txg"); parts = g_gfx.load("backlog_parts.txg"); arrow = g_gfx.load("backlog_arrow.txg");
        hdr.set(14); hdr.start(); g_help.show("○音声再生　×戻る　�スクロール");
        sel = std::max(0, (int)backlog().size() - 1);
    }
    void onResume() override { g_trans.style(0); g_trans.reveal(10); }
    void playVoice(const std::string& v) {
        if (v.empty()) return;
        std::string f = v; if (f.find('.') == std::string::npos) f += ".hca";
        if (g_fs.exists(f)) g_audio.play(CH_VOICE, f, false);
    }
    void onUpdate() override {
        tick++; hdr.update();
        if (exitSt) { if (g_trans.covered() && exitSt == 1) { selfPop(); exitSt = 2; } return; }
        d += (0 - d) * 0.5f; if (std::abs(d) < 1) d = 0;
        const auto& b = backlog(); int n = (int)b.size();
        if (mu::cancel()) { mu::se(0x1f); g_audio.stop(CH_VOICE, 10); g_help.hide(); g_trans.style(0); g_trans.cover(10); exitSt = 1; return; }
        if (!n) return;
        if (d == 0) {
            int o = sel;
            if (mu::repUp() && sel > 0) sel--;
            if (mu::repDown() && sel < n - 1) sel++;
            if (sel != o) { mu::se(0x2a); d = (sel < o ? -1 : 1) * (heightOf(b[o]) + heightOf(b[sel])) / 2.f; }
        }
        if (g_input.isPressed(BTN_CIRCLE)) playVoice(b[sel].voice);
    }
    void item(const BacklogEntry& e, float top, bool selected) {
        int h = heightOf(e); float ox = -200.f + (e.type ? 16 : 0);
        if (selected) ui(*bg, {8, 0, 16, 32}, 0, top - h / 2.f, 1, 60, h / 32.f);
        if (!e.voice.empty()) ui(*parts, {0, 8, 16, 24}, ox + 8, top - 12);
        if (e.type == 1) ui(*parts, {0, 40, 48, 56}, ox + 16 + 24, top - 12);
        if (e.type == 2) ui(*parts, {0, 24, 88, 40}, ox + 16 + 44, top - 12);
        if (e.type == 0 && !e.name.empty()) mu::text(g_fontM, e.name, ox + 16, top - 12 + 8 - 1, {0, 0, 0, 255}, 14, 16);
        mu::text(g_fontM, e.text, ox + 32, top - 24, {0, 0, 0, 255}, 14, 18);
    }
    void onDraw() override {
        if (bg) {
            float off = d - std::floor(d / 32) * 32;
            for (int r = -1; r < 11; r++) ui(*bg, {0, 0, 8, 32}, 0, 136 - 32.f * r + off - 16, 1, 60);
        }
        hdr.draw();
        const auto& b = backlog(); int n = (int)b.size();
        if (!n) return;
        float centre = kCy + d;
        float top = centre + heightOf(b[sel]) / 2.f;
        item(b[sel], top, true);
        float t = top;
        for (int i = sel - 1, c = 0; i >= 0 && c < 4; i--, c++) { t += heightOf(b[i]); item(b[i], t, false); }
        t = top - heightOf(b[sel]);
        for (int i = sel + 1, c = 0; i < n && c < 4; i++, c++) { item(b[i], t, false); t -= heightOf(b[i]); }
        float a = mu::triAlpha(tick, 15) / 255.f;
        if (sel > 0) ui(*arrow, {0, 0, 32, 32}, 160, 90, a);
        if (sel < n - 1) ui(*arrow, {32, 0, 64, 32}, 160, -96, a);
    }
};

// ---- 0x1e DLC menu: the EDATs are not on the UMD, every story is shown unavailable ----
struct DlcMenu : Job {
    TexP parts, title, story, img; PageHeader hdr; int cur = 0, exitSt = 0; float bx[4]; int state = 0; bool asked = false;
    void onEnter() override {
        parts = g_gfx.load("dlc_parts.txg"); title = g_gfx.load("dlc_parts_title.txg"); story = g_gfx.load("dlc_story.txg"); img = g_gfx.load("dlc_img00.txg");
        hdr.set(28); hdr.start();
        for (int k = 0; k < 4; k++) bx[k] = -265 - 10.f * k;
    }
    void onResume() override { g_trans.style(0); g_trans.reveal(10); }
    void onUpdate() override {
        hdr.update();
        if (exitSt) {
            if (exitSt == 1 && g_trans.covered()) { rerollVoiceIndex(); selfPop(); siblingPush(3); exitSt = 2; }
            return;
        }
        if (state == 0) { bool all = true; for (auto& x : bx) { x = std::min(-105.f, x + 20); all &= x >= -105; } if (all) state = 1; return; }
        if (asked) { if (!g_popup.isOpen()) asked = false; return; }
        int o = cur, over = -1;
        for (int k = 0; k < 4; k++) if (mu::hit(-105, 28 - 36.f * k, 200, 32)) over = k;
        if (g_input.mouseMoved && over >= 0) cur = over;
        if (g_input.isPressed(BTN_UP)) cur = (cur + 3) % 4;
        if (g_input.isPressed(BTN_DOWN)) cur = (cur + 1) % 4;
        if (cur != o) mu::se(0x2a);
        if (mu::confirm(over >= 0)) { mu::se(0x1f); g_popup.open("このダウンロードコンテンツは\nインストールされていません。", false); asked = true; }   // port message
        else if (mu::cancel()) { mu::se(0x1f); g_trans.cover(10); exitSt = 1; }
    }
    void onDraw() override {
        mu::commonBg(); hdr.draw();
        if (title) ui(*title, {0, 0, 256, 64}, 104, 90);
        if (img) ui(*img, {0, 0, 128, 152}, 170, -30);
        if (!parts) return;
        for (int k = 0; k < 4; k++) {
            float x = bx[k], y = 28 - 36.f * k; Color c = mu::abgr(k == cur && state ? 0xff85e4ff : 0xffd2d2d2);
            ui(*parts, {0, 0, 8, 24}, x - 80, y, 1, 1, 1, 0, c); ui(*parts, {8, 0, 16, 24}, x, y, 1, 19, 1, 0, c); ui(*parts, {16, 0, 24, 24}, x + 80, y, 1, 1, 1, 0, c);
            if (story) ui(*story, {0, 16 * k, 48, 16 * k + 16}, x - 50, y);
            mu::textCentered(g_fontS, "未配信", x + 30, y, {96, 96, 96, 255}, 10, 14);
            if (k == cur && state) { ui(*parts, {24, 0, 40, 40}, x - 80, y); ui(*parts, {40, 0, 48, 40}, x, y, 1, 19); ui(*parts, {24, 0, 40, 40}, x + 80, y); }
        }
        ui(*parts, {64, 0, 256, 48}, -84, 78);
    }
};

// ---- 0x1f character information: FUN_000d621c and friends ----
struct InfoScene : Job {
    TexP tab, parts, nameT, font, ch[6]; PageHeader hdr; int cur = 0, exitSt = 0, comment[6]{}; int tick = 0; float win = 3;
    void onEnter() override {
        tab = g_gfx.load("information_tab.txg"); parts = g_gfx.load("information_parts.txg"); nameT = g_gfx.load("information_font_name.txg"); font = g_gfx.load("information_font.txg");
        static const char* f[6] = {"information_ch_msr.txg", "information_ch_msk.txg", "information_ch_nnm.txg", "information_ch_zin.txg", "information_ch_rns.txg", "information_ch_oth.txg"};
        for (int i = 0; i < 6; i++) ch[i] = g_gfx.load(f[i]);
        hdr.set(12); hdr.start();
        const GD& g = gd(); int t = g.turn; bool crunch = (t >= 0x49 && t <= 0x4d) || (t >= 0x57 && t <= 0x5b);
        for (int i = 0; i < 6; i++) {
            int s = crunch ? 3 : g.cs[i].healthIcon;
            comment[i] = s == 0 ? rng() & 1 : s == 1 ? (rng() & 3) + 2 : s == 2 ? (rng() & 1) + 6 : (rng() & 1) + 8;
        }
    }
    void onResume() override { g_trans.style(0); g_trans.reveal(10); }
    void onUpdate() override {
        tick++; hdr.update();
        if (exitSt) { if (exitSt == 1 && g_trans.covered()) { selfPop(); exitSt = 2; } return; }
        win = std::min(44.f, win + 4.1f);
        int o = cur, over = -1;
        for (int i = 0; i < 6; i++) if (mu::hit(-190, 72 - 32.f * i, 72, 30)) over = i;
        if (over >= 0 && g_input.mouseMoved) cur = over;
        if (g_input.isPressed(BTN_DOWN)) cur = (cur + 1) % 6;
        if (g_input.isPressed(BTN_UP)) cur = (cur + 5) % 6;
        if (cur != o) { mu::se(0x2a); win = 3; }
        if (g_input.isPressed(BTN_CIRCLE)) {
            std::string f = std::string(kInfoVoice[cur][comment[cur] / 3 % 4]) + ".hca";   // ASSUMED: comment band selects the voice
            if (g_audio.playing(CH_VOICE)) g_audio.stop(CH_VOICE);
            if (g_fs.exists(f)) g_audio.play(CH_VOICE, f, false);
        }
        if (mu::cancel()) { mu::se(0x1f); g_audio.stop(CH_VOICE); g_trans.cover(10); exitSt = 1; }
    }
    void bar(float y, int v, int label) {
        v = std::clamp(v, -100, 100);
        ui(*parts, label == 0 ? Spr{0, 168, 88, 184} : Spr{0, 184, 88, 200}, 150 - 48, y + 12);
        ui(*parts, {0, 96, 184, 104}, 150 - 0, y, 1, 0.75f, 1);
        float f = 50 - (v + 100) * 0.25f;
        ui(*parts, {186, 96, 190, 104}, 150 + (92 - 1 - f * 1.822f) * 0.75f, y);
    }
    void onDraw() override {
        mu::commonBg(); hdr.draw();
        if (!parts) return;
        static const Spr face[6] = {{0, 40, 40, 72}, {80, 40, 120, 72}, {40, 40, 80, 72}, {0, 72, 40, 104}, {40, 72, 80, 104}, {80, 72, 120, 104}};
        for (int i = 5; i >= 0; i--) {
            float x = i == cur ? -184.f : -194.f, y = 72 - 32.f * i;
            ui(*tab, {0, 0, 72, 32}, x, y, 1, 1, 1, 0, mu::abgr(i == cur ? 0xff85e4ff : 0xffffffff));
            ui(*tab, face[i], x + 8, y);
        }
        if (ch[cur]) ui(*ch[cur], {0, 0, 168, 228}, -60, -8);
        static const Spr nm[6] = {{0, 0, 119, 32}, {0, 64, 124, 96}, {0, 32, 102, 64}, {128, 0, 208, 32}, {128, 32, 252, 64}, {128, 64, 229, 96}};
        static const Spr ro[6] = {{0, 96, 81, 112}, {0, 128, 93, 144}, {0, 112, 92, 128}, {112, 96, 170, 112}, {112, 112, 224, 128}, {112, 128, 192, 144}};
        ui(*nameT, nm[cur], 150, 106);
        ui(*nameT, ro[cur], 150, 86, 1, 1, 1, 0, {96, 96, 110, 255});   // ASSUMED: the white romaji is darkened, its backing is not identified
        // chart, centre (150,22): FUN_000d1758
        ui(*parts, {0, 0, 128, 96}, 150, 22);
        float v[6]; static const int order[6] = {3, 4, 1, 0, 5, 2};
        for (int i = 0; i < 6; i++) v[i] = kInfoRadar[cur][order[i]] * 3.f;
        static const SDL_FPoint uv[3] = {{0.828125f, 0.2421875f}, {0.828125f, 0.015625f}, {0.8769531f, 0.1328125f}};
        static TexP pst; if (!pst) pst = g_gfx.load("menu_psrts.txg");
        if (pst) mu::radar(*pst, 150, 22, v, 1, uv);
        ui(*parts, {128, 0, 184, 64}, 150, 22);
        const GD& g = gd();
        bar(-44, g.stat[cur].health, 0); bar(-72, g.stat[cur].mood, 1);
        int st = g.cs[cur].state == 3 ? 3 : std::clamp(g.cs[cur].healthIcon, 0, 3);
        ui(*parts, {24 * st, 200, 24 * st + 24, 224}, 214, -44);
        // comment window FUN_000d5518: frame sprites 6..13 around (32,-123), text centred on it
        mu::Ring r{{184, 16, 192, 24}, {192, 16, 200, 24}, {200, 16, 208, 24}, {184, 24, 192, 32}, {200, 24, 208, 32}, {184, 32, 192, 40}, {192, 32, 200, 40}, {200, 32, 208, 40}};
        float hw = win * 4;
        ui(*parts, {192, 24, 200, 32}, 32, -123, 1, hw / 4, 3);
        mu::ring(*parts, r, 32, -123, hw, 12);
        if (win >= 44) mu::textCentered(g_fontM, kInfoComment[cur][comment[cur]], 32, -120, {0, 0, 0, 255}, 14.5f, 18);
    }
};

// ---- 0x27 DLC movie (scene 0x22 machine, POP self at the end) ----
struct DlcMovie : Job {
    Movie m; int st = 0;
    void onUpdate() override {
        switch (st) {
        case 0:
            g_audio.setGroupVolume(0, g_sys.movie() / 5.f);
            if (g_dlcMovieName.empty() || !m.open(g_dlcMovieName + ".pmf")) { st = 3; break; }
            g_trans.style(0); g_trans.reveal(10); st = 1; break;
        case 1: if (!m.update() || g_input.isPressed(BTN_START | BTN_CIRCLE)) { g_trans.cover(10); st = 2; } break;
        case 2: if (g_trans.covered()) { m.stop(); g_sys.applyVolumes(); st = 3; } break;
        case 3: selfPop(); st = 4; break;
        }
    }
    void onDraw() override { g_gfx.clear({0, 0, 0, 255}); m.draw(); }
};
}

std::unique_ptr<Job> createMiscScene(int id) {
    switch (id) {
    case 0x13: return std::make_unique<TipsScene>();
    case 0x1d: return std::make_unique<BacklogScene>();
    case 0x1e: return std::make_unique<DlcMenu>();
    case 0x1f: return std::make_unique<InfoScene>();
    case 0x27: return std::make_unique<DlcMovie>();
    }
    return nullptr;
}
