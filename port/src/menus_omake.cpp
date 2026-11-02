// Omake (gallery) screens: scenes 0x15 menu, 0x16 poses, 0x17 voices, 0x18 character events, 0x19 event CG
// (spec_flow_menus section 10, plus decompile of FUN_0009d078, FUN_000a5710, FUN_000aa284, FUN_000adb5c, FUN_000b3e44).
#include "menus.h"
#include "menus_data.h"
#include "menus_ui.h"

namespace {
const uint32_t kBase = 0xffffd8ad, kSel = 0xff85e4ff, kLock = 0xffd2d2d2;

// every omake page exits the same way: fade 10, POP self
struct OmakeBase : Job {
    PageHeader hdr; int closing = 0, tick = 0;
    void onResume() override { g_trans.style(0); g_trans.reveal(10); }
    void beginExit() { g_trans.cover(10); closing = 1; }
    bool exiting() {
        tick++; hdr.update();
        if (!closing) return false;
        if (closing == 1 && g_trans.covered()) { closing = 2; onCovered(); }
        return true;
    }
    virtual void onCovered() { selfPop(); }
};

bool anyNew(size_t unlocked, size_t seen, int n) {
    for (int i = 0; i < n; i++) if (g_sys.bit(unlocked, i) && !g_sys.bit(seen, i)) return true;
    return false;
}

// ---- 0x15 ----
struct OmakeMenu : OmakeBase {
    TexP parts, names, imgs; int cur = 0, state = 0, panel = 0, pending = 0; float bx[5]; bool isNew[5]{};
    float sx = 4, sy = 0, ox = 30, oy = -48; int fa = 0, ca = 0;   // panel opening (FUN_0009cb34)
    void onEnter() override {
        parts = g_gfx.load("omake_menu_parts.txg"); names = g_gfx.load("omake_menu_name.txg"); imgs = g_gfx.load("omake_menu_img.txg");
        hdr.set(15); hdr.start();
        for (int k = 0; k < 5; k++) bx[k] = -320 - 10.f * k;
        refresh();
    }
    void refresh() {
        isNew[0] = anyNew(0x30f4, 0x3104, 0x61); isNew[1] = anyNew(0x3134, 0x3138, 11); isNew[2] = anyNew(0x3114, 0x3124, 0x41);
        isNew[3] = anyNew(0x313c, 0x3140, 12); isNew[4] = anyNew(0x3144, 0x3154, 99);
    }
    void onResume() override { OmakeBase::onResume(); closing = 0; refresh(); }
    void onCovered() override {
        if (pending) { static const int m[6] = {0, 0x19, 0x16, 0x18, 0x17, 0x13}; siblingPush(m[pending]); pending = 0; }
        else { rerollVoiceIndex(); selfPop(); siblingPush(3); }
    }
    void onUpdate() override {
        if (exiting()) return;
        if (state == 0) {
            bool all = true;
            for (auto& x : bx) { x = std::min(-120.f, x + 20); all &= x >= -120; }
            if (all) state = 1;
            return;
        }
        if (state == 1) {
            sx += 2.4f; sy += 1.2f; ox -= 3; oy += 4.8f; ca += 25;
            if (sx >= 28) { sx = 28; sy = 12; ox = 0; oy = 0; ca = 255; state = 2; }
            return;
        }
        if (state == 2) { fa += 20; if (fa >= 240) { fa = 255; state = 3; } return; }
        int over = -1;
        for (int k = 0; k < 5; k++) if (mu::hit(-120, 60 - 32.f * k, 150, 28)) over = k;
        int old = cur;
        if (g_input.mouseMoved && over >= 0) cur = over;
        if (g_input.isPressed(BTN_UP)) cur = (cur + 4) % 5;
        if (g_input.isPressed(BTN_DOWN)) cur = (cur + 1) % 5;
        if (cur != old) mu::se(0x2a);
        if (mu::confirm(over >= 0)) { mu::se(0x29); pending = cur + 1; beginExit(); }
        else if (mu::cancel()) { mu::se(0x1f); pending = 0; beginExit(); }
    }
    void onDraw() override {
        mu::commonBg(); hdr.draw();
        if (!parts) return;
        for (int k = 0; k < 5; k++) {
            float x = bx[k], y = 60 - 32.f * k; Color c = mu::abgr(k == cur && state == 3 ? kSel : kBase);
            ui(*parts, {0, 16, 8, 40}, x - 70, y, 1, 1, 1, 0, c); ui(*parts, {8, 16, 16, 40}, x, y, 1, 17, 1, 0, c); ui(*parts, {16, 16, 24, 40}, x + 70, y, 1, 1, 1, 0, c);
            ui(*names, {0, 16 * k, 96, 16 * k + 16}, x + 7, y);
            if (isNew[k]) ui(*parts, {0, 0, 32, 16}, x - 55, y);
            if (k == cur && state == 3) {
                ui(*parts, {0, 40, 16, 80}, x - 70, y); ui(*parts, {16, 40, 24, 80}, x, y, 1, 17); ui(*parts, {24, 40, 40, 80}, x + 70, y);
            }
        }
        if (state == 0) return;
        const float px = 96 + ox, py = 36 + oy, a = ca / 255.f;
        mu::Ring r{{0, 80, 8, 88}, {8, 80, 16, 88}, {16, 80, 24, 88}, {0, 88, 8, 96}, {16, 88, 24, 96}, {0, 96, 8, 104}, {8, 96, 16, 104}, {16, 96, 24, 104}};
        mu::ring(*parts, r, px, py, sx * 4, sy * 4, a);
        ui(*parts, {24, 80, 56, 104}, px + 30, py - 60, a);
        if (state < 2) return;
        int i = cur * 2;
        auto img = [&](int n, float x) { ui(*imgs, {88 * (n & 1), 48 * (n / 2), 88 * (n & 1) + 88, 48 * (n / 2) + 48}, x, py - 20, fa / 255.f); };
        img(i, px - 65 + 22); img(i + 1, px + 30 + 22 - 22 + 22);
        static const char* desc[5] = {"龍之介様やその他の皆様の\nイラストをご覧頂けます♪", "龍之介様やその他の皆様の\n素敵なお姿をご覧頂けます♪", "龍之介様やその他の皆様との\nできごとをご覧頂けます♪", "龍之介様やその他の皆様の\n特別な声をお聞き頂けます♪", "ゲーム制作を志す皆様なら\n知っておくべき用語集です♪"};
        mu::textCentered(g_fontM, desc[cur], px, py + 30, {0, 0, 0, 255}, 14.5f, 18, fa / 255.f);
    }
};

// scrolling list column shared by the pose and voice pages
struct Scroller {
    int cur = 0, top = 0, n = 1, vis = 7;
    void clampTop() { if (cur < top) top = cur; if (cur >= top + vis) top = cur - vis + 1; top = std::clamp(top, 0, std::max(0, n - vis)); }
    bool step() {
        int old = cur;
        if (mu::repUp()) cur = cur > 0 ? cur - 1 : cur;
        if (mu::repDown()) cur = cur < n - 1 ? cur + 1 : cur;
        clampTop(); return cur != old;
    }
};

void rowFrame(const Tex& t, Spr l, Spr m, Spr r, float x, float y, float half, float sx, Color c) {
    ui(t, l, x - half, y, 1, 1, 1, 0, c); ui(t, m, x, y, 1, sx, 1, 0, c); ui(t, r, x + half, y, 1, 1, 1, 0, c);
}

// ---- 0x16: FUN_000a43e4 composes `<base><suffix>.csu`; character 10 expands to lists 10..17 ----
struct PoseScene : OmakeBase {
    TexP parts, names, pic; Scroller list; int st = 0, pose = 0, expr = 0; std::string loaded;
    bool unlocked(int c) { return g_sys.bit(0x3134, c); }
    struct Ref { int list, base; };
    int poseCount(int c) const { if (c < 10) return kPoseBaseN[c]; int s = 0; for (int l = 10; l < kPoseLists; l++) s += kPoseBaseN[l]; return s; }
    Ref resolve(int c, int p) const {
        if (c < 10) return {c, p};
        for (int l = 10; l < kPoseLists; l++) { if (p < kPoseBaseN[l]) return {l, p}; p -= kPoseBaseN[l]; }
        return {10, 0};
    }
    void loadPic() {
        std::string f = "SUSECRET.csu";
        if (st == 1 || unlocked(list.cur)) {
            Ref r = resolve(list.cur, st == 1 ? pose : 0);
            std::string name = std::string(kPoseBase[r.list][r.base]) + kPoseSuf[r.list][std::min(st == 1 ? expr : 0, kPoseSufN[r.list] - 1)] + ".csu";
            if (g_fs.exists(name)) f = name;
        }
        if (f != loaded) { pic = g_gfx.load(f); loaded = f; }
    }
    void onEnter() override {
        parts = g_gfx.load("omake_pose_parts.txg"); names = g_gfx.load("omake_pose_name.txg");
        hdr.set(19); hdr.start(); list.n = 11; loadPic();
    }
    void onUpdate() override {
        if (exiting()) return;
        if (st == 0) {
            int over = -1;
            for (int i = 0; i < list.vis && i + list.top < list.n; i++) if (mu::hit(-120, 75 - 32.f * i, 130, 28)) over = i + list.top;
            int old = list.cur;
            if (g_input.mouseMoved && over >= 0) list.cur = over;
            list.step();
            if (list.cur != old) { mu::se(0x2a); loadPic(); }
            if (mu::confirm(over >= 0) && unlocked(list.cur)) {
                mu::se(0x29); g_sys.setBit(0x3138, list.cur); st = 1; pose = 0; expr = 0; loadPic();
            } else if (mu::cancel()) { mu::se(0x1f); beginExit(); }
        } else {
            int np = poseCount(list.cur); Ref r = resolve(list.cur, pose); int ne = kPoseSufN[r.list];
            bool ch = false;
            if (mu::repUp()) { pose = (pose + np - 1) % np; ch = true; }
            if (mu::repDown()) { pose = (pose + 1) % np; ch = true; }
            if (g_input.isRepeat(BTN_LEFT)) { expr = (expr + ne - 1) % ne; ch = true; }
            if (g_input.isRepeat(BTN_RIGHT)) { expr = (expr + 1) % ne; ch = true; }
            if (ch) { mu::se(0x2a); r = resolve(list.cur, pose); expr = std::min(expr, kPoseSufN[r.list] - 1); loadPic(); }
            if (mu::cancel()) { mu::se(0x1f); st = 0; loadPic(); }
        }
    }
    static Spr nameRect(int c) {
        static const Spr n[11] = {{0, 0, 72, 16}, {0, 32, 61, 48}, {0, 16, 75, 32}, {0, 48, 49, 64}, {0, 64, 74, 80}, {0, 80, 62, 96}, {0, 96, 63, 112}, {0, 112, 62, 128}, {80, 48, 141, 64}, {80, 0, 204, 16}, {160, 16, 208, 32}};
        return n[c];
    }
    void onDraw() override {
        mu::commonBg();
        if (pic) { float s = std::min(1.f, 512.f / pic->h); mu::picture(*pic, 110, 0, 1, s, s); }  // ASSUMED anchor: right half of the page, centred
        hdr.draw();
        if (!parts) return;
        for (int i = 0; i < list.vis && i + list.top < list.n; i++) {
            int c = i + list.top; float x = -120, y = 75 - 32.f * i; bool sel = c == list.cur, un = unlocked(c);
            Color col = mu::abgr(!un ? kLock : sel ? kSel : kBase);
            rowFrame(*parts, {0, 16, 8, 40}, {8, 16, 16, 40}, {16, 16, 24, 40}, x, y, 50, 11.5f, col);
            if (un) ui(*names, nameRect(c), x, y);
            else mu::textCentered(g_fontM, "？", x, y, {96, 96, 96, 255}, 14.5f, 16);
            if (un && !g_sys.bit(0x3138, c)) ui(*parts, {0, 0, 48, 16}, x - 42, y + 8);
            if (sel) rowFrame(*parts, {0, 40, 16, 80}, {16, 40, 24, 80}, {24, 40, 40, 80}, x, y, 50, 11.5f, {});
        }
        float a = mu::triAlpha(tick, 30) / 255.f;
        if (list.top > 0) ui(*parts, {40, 40, 64, 56}, -120, 85, a);
        if (list.top + list.vis < list.n) ui(*parts, {40, 56, 64, 72}, -120, -125, a);
        if (st == 1) {
            // "No." label and pose number, digits 14..23
            ui(*parts, {24, 112, 48, 128}, -40, -112);
            int n = pose + 1; auto dig = [&](int v, float x) { Spr s = v < 5 ? Spr{48 + 16 * v, 96, 64 + 16 * v, 112} : Spr{48 + 16 * (v - 5), 112, 64 + 16 * (v - 5), 128}; ui(*parts, s, x, -112); };
            dig(n / 10 % 10, -18); dig(n % 10, -6);
            ui(*parts, {40, 72, 56, 96}, 42, -5, a); ui(*parts, {56, 72, 72, 96}, 178, -5, a);
        }
    }
};

// ---- 0x17: FUN_000b3e44 ----
struct VoiceScene : OmakeBase {
    TexP parts, names, pic; Scroller chars, items; int st = 0; std::string loaded;
    bool unlocked(int c) { return g_sys.bit(0x313c, c); }
    void loadPic() {
        std::string f = unlocked(chars.cur) ? kVoicePortrait[chars.cur] : "SUSECRET.csu";
        if (!g_fs.exists(f)) f = "SUSECRET.csu";
        if (f != loaded) { pic = g_gfx.load(f); loaded = f; }
    }
    void onEnter() override {
        parts = g_gfx.load("omake_voice_parts.txg"); names = g_gfx.load("omake_voice_ch_name.txg");
        hdr.set(17); hdr.start(); chars.n = 12; chars.vis = 6; loadPic();
    }
    void play() {
        std::string f = std::string(kVoiceFile[chars.cur][items.cur]) + ".hca";
        if (g_audio.playing(CH_VOICE)) g_audio.stop(CH_VOICE);
        if (g_fs.exists(f)) g_audio.play(CH_VOICE, f, false);
    }
    void onCovered() override { g_audio.stop(CH_VOICE); selfPop(); }
    void onUpdate() override {
        if (exiting()) return;
        if (st == 0) {
            int over = -1;
            for (int i = 0; i < chars.vis && i + chars.top < chars.n; i++) if (mu::hit(-120, 60 - 32.f * i, 130, 28)) over = i + chars.top;
            int old = chars.cur;
            if (g_input.mouseMoved && over >= 0) chars.cur = over;
            chars.step();
            if (chars.cur != old) { mu::se(0x2a); loadPic(); }
            if (mu::confirm(over >= 0) && unlocked(chars.cur)) {
                mu::se(0x29); g_sys.setBit(0x3140, chars.cur); st = 1;
                items.cur = items.top = 0; items.n = kVoiceCount[chars.cur]; items.vis = 6;
            } else if (mu::cancel()) { mu::se(0x1f); beginExit(); }
        } else {
            int over = -1;
            for (int i = 0; i < items.vis && i + items.top < items.n; i++) if (mu::hit(70, 75 - 24.f * i, 230, 22)) over = i + items.top;
            int old = items.cur;
            if (g_input.mouseMoved && over >= 0) items.cur = over;
            items.step();
            if (items.cur != old) mu::se(0x2a);
            if (mu::confirm(over >= 0)) play();
            else if (mu::cancel()) { mu::se(0x1f); g_audio.stop(CH_VOICE); st = 0; }
        }
    }
    static Spr nameRect(int c) {
        static const Spr n[12] = {{0, 0, 68, 16}, {0, 32, 57, 48}, {0, 16, 71, 32}, {0, 48, 45, 64}, {0, 64, 70, 80}, {0, 80, 57, 96}, {0, 96, 57, 112}, {0, 112, 57, 128}, {0, 128, 57, 144}, {0, 144, 120, 160}, {0, 160, 58, 176}, {0, 176, 81, 194}};
        return n[c];
    }
    void onDraw() override {
        mu::commonBg();
        if (pic) { float s = std::min(1.f, 512.f / pic->h); mu::picture(*pic, 170, 0, st ? 0.45f : 1, s, s); }  // ASSUMED anchor
        hdr.draw();
        if (!parts) return;
        for (int i = 0; i < chars.vis && i + chars.top < chars.n; i++) {
            int c = i + chars.top; float x = -120, y = 60 - 32.f * i; bool sel = c == chars.cur, un = unlocked(c);
            rowFrame(*parts, {0, 16, 8, 40}, {8, 16, 16, 40}, {16, 16, 24, 40}, x, y, 50, 11.5f, mu::abgr(!un ? kLock : sel ? kSel : kBase));
            if (un) ui(*names, nameRect(c), x, y);
            else mu::textCentered(g_fontM, "？", x, y, {96, 96, 96, 255}, 14.5f, 16);
            if (un && !g_sys.bit(0x3140, c)) ui(*parts, {16, 0, 48, 16}, x - 42, y + 8);
            if (sel && st == 0) rowFrame(*parts, {0, 40, 16, 80}, {16, 40, 24, 80}, {24, 40, 40, 80}, x, y, 50, 11.5f, {});
        }
        float a = mu::triAlpha(tick, 30) / 255.f;
        if (chars.top > 0) ui(*parts, {40, 40, 64, 56}, -120, 85, a);
        if (chars.top + chars.vis < chars.n) ui(*parts, {40, 56, 64, 72}, -120, -125, a);
        if (st == 1) {
            for (int i = 0; i < items.vis && i + items.top < items.n; i++) {
                int k = i + items.top; float y = 75 - 24.f * i; bool sel = k == items.cur;
                rowFrame(*parts, {0, 16, 8, 40}, {8, 16, 16, 40}, {16, 16, 24, 40}, 70, y, 110, 26.5f, mu::abgr(sel ? kSel : kBase));
                mu::textCentered(g_fontM, kVoiceName[chars.cur][k], 70, y, {0, 0, 0, 255}, 14.5f, 16);
            }
            mu::Ring r{{0, 80, 8, 88}, {8, 80, 16, 88}, {16, 80, 24, 88}, {0, 88, 8, 96}, {16, 88, 24, 96}, {0, 96, 8, 104}, {8, 96, 16, 104}, {16, 96, 24, 104}};
            mu::ring(*parts, r, 70, -100, 120, 16);
            mu::textCentered(g_fontM, kVoiceCap[chars.cur][items.cur], 70, -100, {0, 0, 0, 255}, 14.5f, 16);
        }
    }
};

// ---- 0x18: FUN_000aa284. 6 tabs, 8 events per page, id = tab base + index ----
const int kEvCount[6] = {14, 15, 10, 8, 8, 10};
int evBase(int t) { int s = 0; for (int i = 0; i < t; i++) s += kEvCount[i]; return s; }
const char* kEvImg[6] = {"omake_event_img_msr.txg", "omake_event_img_nnm.txg", "omake_event_img_msk.txg", "omake_event_img_zin.txg", "omake_event_img_rns.txg", "omake_event_img_oth.txg"};
const char* kEvPortrait[6] = {"SUMSRNOM_U.csu", "SUNNMNOM_U.csu", "SUMSKNOM_U.csu", "SUJINNOM_U.csu", "SURNSNOM_U.csu", "SUOTHNMA_U.csu"};

struct EventScene : OmakeBase {
    TexP parts, names, img, pic; int tab = 0, st = 0, page = 1, col = 0, row = 0, pending = 0;
    bool tabOn(int t) { for (int i = 0; i < kEvCount[t]; i++) if (g_sys.bit(0x3114, evBase(t) + i)) return true; return false; }
    int pages() const { return (kEvCount[tab] + 7) / 8; }
    int sel() const { return (page - 1) * 8 + row * 4 + col; }
    void setTab() { img = g_gfx.load(kEvImg[tab]); pic = g_gfx.load(tabOn(tab) ? kEvPortrait[tab] : "SUSECRET.csu"); }
    void onEnter() override {
        parts = g_gfx.load("omake_event_parts.txg"); names = g_gfx.load("omake_event_name.txg");
        hdr.set(18); hdr.start(); setTab();
    }
    void onResume() override { OmakeBase::onResume(); closing = 0; g_mode = 0; }
    void onCovered() override {
        if (pending) {
            int id = evBase(tab) + sel(); pending = 0;
            g_eventViewId = id; clearSaveBuffer(); g_mode = 1;
            g_sys.setBit(0x3124, id);
            siblingPush(5);
        } else selfPop();
    }
    void onUpdate() override {
        if (exiting()) return;
        if (st == 0) {
            int over = -1;
            for (int t = 0; t < 6; t++) if (mu::hit(-120, 75 - 32.f * t, 130, 28)) over = t;
            int old = tab;
            if (g_input.mouseMoved && over >= 0) tab = over;
            if (mu::repUp()) tab = (tab + 5) % 6;
            if (mu::repDown()) tab = (tab + 1) % 6;
            if (tab != old) { mu::se(0x2a); setTab(); }
            if (mu::confirm(over >= 0) && tabOn(tab)) { mu::se(0x29); st = 1; page = 1; col = row = 0; }
            else if (mu::cancel()) { mu::se(0x1f); beginExit(); }
            return;
        }
        int oc = col, or_ = row, op = page, n = kEvCount[tab], over = -1;
        for (int i = 0; i < 8; i++) if (mu::hit(-160 + 92.f * (i % 4) - 20, 30 - 85.f * (i / 4), 80, 76)) over = i;
        if (g_input.mouseMoved && over >= 0 && (page - 1) * 8 + over < n) { col = over % 4; row = over / 4; }
        if (g_input.isPressed(BTN_RIGHT)) col = (col + 1) % 4;
        if (g_input.isPressed(BTN_LEFT)) col = (col + 3) % 4;
        if (g_input.isPressed(BTN_UP) || g_input.isPressed(BTN_DOWN)) row ^= 1;
        if (g_input.isPressed(BTN_R) || g_input.wheel < 0) page = page % pages() + 1;
        if (g_input.isPressed(BTN_L) || g_input.wheel > 0) page = (page + pages() - 2) % pages() + 1;
        while (sel() >= n && (row || col)) { if (row) row = 0; else col--; }
        if (oc != col || or_ != row || op != page) mu::se(0x2a);
        int id = evBase(tab) + sel();
        if (mu::confirm(over >= 0) && g_sys.bit(0x3114, id)) { mu::se(0x29); pending = 1; beginExit(); }
        else if (mu::cancel()) { mu::se(0x1f); st = 0; }
    }
    void onDraw() override {
        mu::commonBg();
        if (pic) { float s = std::min(1.f, 400.f / pic->h); mu::picture(*pic, st ? 196.f : 110.f, 0, 1, s, s); }  // ASSUMED anchor
        hdr.draw();
        if (!parts) return;
        static const Spr nm[7] = {{0, 0, 68, 16}, {0, 32, 57, 48}, {0, 16, 71, 32}, {0, 48, 45, 64}, {0, 64, 70, 80}, {0, 80, 57, 96}, {0, 96, 16, 112}};
        if (st == 0) {
            for (int t = 0; t < 6; t++) {
                float x = -120, y = 75 - 32.f * t; bool on = tabOn(t);
                Color c = mu::abgr(!on ? kLock : t == tab ? kSel : kBase);
                rowFrame(*parts, {128, 24, 136, 48}, {136, 24, 144, 48}, {144, 24, 152, 48}, x, y, 50, 11.5f, c);
                ui(*names, on ? nm[t] : nm[6], x, y);
            }
            return;
        }
        ui(*names, nm[tab], 40, 105);
        for (int i = 0; i < 8; i++) {
            int k = (page - 1) * 8 + i; if (k >= kEvCount[tab]) break;
            int id = evBase(tab) + k; bool on = g_sys.bit(0x3114, id), s = i == row * 4 + col;
            float x = -180 + 92.f * (i % 4), y = 30 - 85.f * (i / 4);
            ui(*parts, {0, 56, 88, 128}, x, y, 1, 1, 1, 0, mu::abgr(!on ? 0xffd2d2d2 : s ? 0xff8ae7ff : 0xffffd8a9));
            if (on) ui(*img, {72 * (k % 3), 40 * (k / 3), 72 * (k % 3) + 72, 40 * (k / 3) + 40}, x, y);
            else ui(*parts, {56, 0, 128, 40}, x, y);
            ui(*parts, {0, 24, 24, 40}, x - 32, y + 32, 1, 1, 1, 0, mu::abgr(!on ? 0xff7d7d7d : s ? 0xff318eff : 0xffe3915a));
            int n = k + 1; auto dg = [&](int v, float dx) { ui(*parts, {8 * v, 40, 8 * v + 8, 48}, x - 32 + dx, y + 32); };
            dg(n / 10 % 10, -4); dg(n % 10, 4);
            if (on && !g_sys.bit(0x3124, id)) ui(*parts, {24, 24, 56, 40}, x + 24, y + 32);
            if (s) { mu::Ring r{{0, 0, 8, 8}, {8, 0, 16, 8}, {16, 0, 24, 8}, {0, 8, 8, 16}, {16, 8, 24, 16}, {0, 16, 8, 24}, {8, 16, 16, 24}, {16, 16, 24, 24}}; mu::ring(*parts, r, x, y, 34, 20); }
        }
        for (int i = 0; i < pages(); i++) { int f = i == pages() - page ? 0 : 1; ui(*parts, {104, 40 + 16 * f, 128, 56 + 16 * f}, 180 - 16.f * i, 105); }
    }
};

// ---- 0x19: FUN_000adb5c. 10 thumbs per page, 97 stills without DLC ----
const int kCgN = 0x61;
struct CgScene : OmakeBase {
    TexP parts, img1, img2, full; int page = 1, col = 0, row = 0, viewMode = 0; float pan = 0; bool viewing = false; int loadSt = 0; int fileId = 0;
    float slide = 0;
    int pages() const { return (kCgN + 9) / 10; }
    int sel() const { return (page - 1) * 10 + row * 5 + col; }
    void onEnter() override {
        parts = g_gfx.load("omake_cg_parts.txg"); img1 = g_gfx.load("omake_cg_img.txg"); img2 = g_gfx.load("omake_cg_img02.txg");
        hdr.set(16); hdr.start();
    }
    void onCovered() override {
        if (loadSt == 1) {
            full = g_gfx.load(std::string(kCgFile[fileId]) + ".cbg"); viewing = full != nullptr; loadSt = 0; closing = 0;
            std::string f = kCgFile[fileId]; viewMode = f == "EV_035" || f == "EV_039" ? 1 : f == "EV_045" ? 2 : 0; pan = 0;
            g_trans.reveal(10);
        } else if (loadSt == 2) { full.reset(); viewing = false; loadSt = 0; closing = 0; g_trans.reveal(10); }
        else selfPop();
    }
    void onUpdate() override {
        if (exiting()) return;
        if (viewing) {
            float lim = viewMode == 1 ? std::max(0, full->w - 480) / 2.f : viewMode == 2 ? std::max(0, full->h - 272) / 2.f : 0;
            if (viewMode == 1) { if (g_input.isHeld(BTN_LEFT)) pan -= 10; if (g_input.isHeld(BTN_RIGHT)) pan += 10; }
            if (viewMode == 2) { if (g_input.isHeld(BTN_UP)) pan -= 10; if (g_input.isHeld(BTN_DOWN)) pan += 10; }
            pan = std::clamp(pan, -lim, lim);
            if (g_input.isPressed(BTN_CROSS)) { mu::se(0x1f); loadSt = 2; beginExit(); }
            return;
        }
        if (slide != 0) { slide = slide > 0 ? std::max(0.f, slide - 24) : std::min(0.f, slide + 24); return; }
        int oc = col, or_ = row, op = page, over = -1;
        for (int i = 0; i < 10; i++) if (mu::hit(-180 + 90.f * (i % 5), 40 - 90.f * (i / 5), 86, 80)) over = i;
        if (g_input.mouseMoved && over >= 0 && (page - 1) * 10 + over < kCgN) { col = over % 5; row = over / 5; }
        if (g_input.isPressed(BTN_RIGHT)) col = (col + 1) % 5;
        if (g_input.isPressed(BTN_LEFT)) col = (col + 4) % 5;
        if (g_input.isPressed(BTN_UP) || g_input.isPressed(BTN_DOWN)) row ^= 1;
        if (g_input.isPressed(BTN_R) || g_input.wheel < 0) { page = page % pages() + 1; slide = 480; }
        if (g_input.isPressed(BTN_L) || g_input.wheel > 0) { page = (page + pages() - 2) % pages() + 1; slide = -480; }
        while (sel() >= kCgN && (row || col)) { if (row) row = 0; else col--; }
        if (oc != col || or_ != row || op != page) mu::se(0x2a);
        int k = sel();
        if (mu::confirm(over >= 0) && g_sys.bit(0x30f4, k)) {
            mu::se(0x29); g_sys.setBit(0x3104, k); fileId = k; loadSt = 1; beginExit();
        } else if (mu::cancel()) { mu::se(0x1f); loadSt = 0; beginExit(); }
    }
    void drawPage(int pg, float ox) {
        for (int i = 0; i < 10; i++) {
            int k = (pg - 1) * 10 + i; if (k >= kCgN) break;
            bool on = g_sys.bit(0x30f4, k), s = pg == page && i == row * 5 + col;
            float x = ox - 180 + 90.f * (i % 5), y = 40 - 90.f * (i / 5);
            ui(*parts, {0, 56, 88, 128}, x, y, 1, 1, 1, 0, mu::abgr(!on ? 0xffd2d2d2 : s ? 0xff8ae7ff : 0xffffd8a9));
            if (on) {
                if (k < 84) ui(*img1, {72 * (k % 7), 40 * (k / 7), 72 * (k % 7) + 72, 40 * (k / 7) + 40}, x, y);
                else { int j = k - 84; ui(*img2, {72 * (j % 3), 40 * (j / 3), 72 * (j % 3) + 72, 40 * (j / 3) + 40}, x, y); }
            } else ui(*parts, {56, 0, 128, 40}, x, y);
            float nx = x - 32, ny = y + 32;
            ui(*parts, {0, 24, 24, 40}, nx, ny, 1, 1, 1, 0, mu::abgr(!on ? 0xff7d7d7d : s ? 0xff318eff : 0xffe3915a));
            int n = k + 1; auto dg = [&](int v, float dx) { ui(*parts, {8 * v, 40, 8 * v + 8, 48}, nx + dx, ny); };
            if (n >= 100) { dg(n / 100, -8); dg(n / 10 % 10, 0); dg(n % 10, 8); } else { dg(n / 10, -4); dg(n % 10, 4); }
            if (on && !g_sys.bit(0x3104, k)) ui(*parts, {24, 24, 56, 40}, x + 24, ny);
            if (s) { mu::Ring r{{0, 0, 8, 8}, {8, 0, 16, 8}, {16, 0, 24, 8}, {0, 8, 8, 16}, {16, 8, 24, 16}, {0, 16, 8, 24}, {8, 16, 16, 24}, {16, 16, 24, 24}}; mu::ring(*parts, r, x, y, 34, 20); }
        }
    }
    void onDraw() override {
        if (viewing && full) {
            g_gfx.clear({0, 0, 0, 255});
            float cx = full->cx ? (float)full->cx : full->w / 2.f, cy = full->cy ? (float)full->cy : full->h / 2.f;
            g_gfx.draw(*full, 240 - cx - (viewMode == 1 ? pan : 0), 136 - cy - (viewMode == 2 ? pan : 0));
            return;
        }
        mu::commonBg(); hdr.draw();
        if (!parts) return;
        drawPage(page, slide);
        if (slide) drawPage(slide > 0 ? (page + pages() - 2) % pages() + 1 : page % pages() + 1, slide - (slide > 0 ? 480 : -480));
        for (int i = 0; i < pages(); i++) { int f = i == pages() - page ? 0 : 1; ui(*parts, {104, 40 + 16 * f, 128, 56 + 16 * f}, 220 - 16.f * i, 105); }
        float a = mu::triAlpha(tick, 30) / 255.f;
        ui(*parts, {24, 0, 40, 24}, -230, -5, a); ui(*parts, {40, 0, 56, 24}, 230, -5, a);
    }
};
}

std::unique_ptr<Job> createOmakeScene(int id) {
    switch (id) {
    case 0x15: return std::make_unique<OmakeMenu>();
    case 0x16: return std::make_unique<PoseScene>();
    case 0x17: return std::make_unique<VoiceScene>();
    case 0x18: return std::make_unique<EventScene>();
    case 0x19: return std::make_unique<CgScene>();
    }
    return nullptr;
}
