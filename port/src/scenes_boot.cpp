// Boot chain, logos, movies, title, option (spec_flow_menus sections 2-5.1).
#include "common.h"
#include "game.h"
#include "movie.h"
#include <cmath>
#include <algorithm>

void RootJob::onEnter() {
    g_sys.defaults();
    g_audio.loadCues("sound_se.awb");
    push(1);
}
void RootJob::onUpdate() {
    g_trans.update(); g_popup.update(); g_help.update();
    if (!hasChild()) push(3);
}
void RootJob::overlayDraw() { g_help.draw(); g_popup.draw(); g_trans.draw(); }

namespace {
static const char* kNotice = "本作はオートセーブに対応しています。\nメモリースティック™アクセスランプ点灯中は\n記録メディアを抜き差ししたり、\n電源を切ったりしないでください。";

struct Boot1 : Job {
    bool opened = false;
    void onEnter() override { g_popup.open(kNotice, false); opened = true; }
    void onResume() override { g_trans.style(0); g_trans.reveal(0); }
    void onUpdate() override { if (!g_popup.isOpen()) { selfPop(); siblingPush(2); } }
};
struct Boot2 : Job {
    void onEnter() override { g_sys.load(); g_sys.applyVolumes(); }
    void onUpdate() override { selfPop(); siblingPush(0x20); }
};
struct Boot3 : Job { // save SYSTEM.DAT then title
    void onEnter() override { g_sys.save(); }
    void onUpdate() override { selfPop(); siblingPush(0x14); }
};
struct Boot4 : Job {
    void onEnter() override { g_sys.save(); }
    void onUpdate() override { selfPop(); }
};

struct LogoDontCopy : Job {
    TexP t; int hold = 0; bool closing = false;
    void onEnter() override { t = g_gfx.load("logo_dontcopy.txgtl"); }
    void onResume() override { g_trans.reveal(30); hold = 0; closing = false; }
    void onUpdate() override {
        if (!closing) { if (g_trans.clear() && ++hold >= 180) { g_trans.cover(30); closing = true; } }
        else if (g_trans.covered()) { parent->popAll(); parent->push(0x21); }
    }
    void onDraw() override { g_gfx.clear({255, 255, 255, 255}); if (t) g_gfx.draw(*t, 0, 0); }
};

struct Logos : Job {
    static constexpr const char* names[4] = {"logo_amw.txgtl", "logo_kadokawa.txgtl", "logo_nb.txgtl", "logo_cri.txgtl"};
    TexP t; int n = 0, state = 0, hold = 0;
    void onEnter() override { rerollVoiceIndex(); t = g_gfx.load(names[0]); }
    void onResume() override { g_trans.reveal(30); }
    void voice() {
        static const char* tb[3][31] = {
            {"MSR_0010a","SRT_0010b","SRT_0010c","SRT_0010e","SRT_0010f","NNM_0010a","NNM_0010c","MSK_0020a","MSK_0020c","MSK_0020d","JIN_0010b","JIN_0010c","JIN_0010e","RNS_0010","OTH_0010a","OTH_0010b","OTH_0010c","SNG_0010a","SNG_0010b","MID_0010","RIT_0010","YUK_0010","CHR_0010b","KHR_0010a","KHR_0010b","SIR_0010","SOR_0010a","SOR_0010b","KZK_0010","DIC_0010","CAT_0060"},
            {"MSR_0020","SRT_0020b","SRT_0020d","SRT_0020e","SRT_0020f","NNM_0020a","NNM_0020c","MSK_0030a","MSK_0030c","MSK_0030d","JIN_0020a","JIN_0020d","JIN_0020e","RNS_0020","OTH_0020a","OTH_0020b","OTH_0020c","SNG_0020a","SNG_0020b","MID_0020","RIT_0020","YUK_0020","CHR_0020b","KHR_0020a","KHR_0020b","SIR_0020","SOR_0020a","SOR_0020b","KZK_0020","DIC_0020","CAT_0070"},
            {"MSR_0030","SRT_0030a","SRT_0030d","SRT_0030e","SRT_0030f","NNM_0030a","NNM_0030b","MSK_0040a","MSK_0040c","MSK_0040d","JIN_0030a","JIN_0030d","JIN_0030e","RNS_0030","OTH_0030a","OTH_0030b","OTH_0030c","SNG_0030a","SNG_0030b","MID_0030","RIT_0030","YUK_0030","CHR_0030b","KHR_0030a","KHR_0030b","SIR_0030","SOR_0030a","SOR_0030b","KZK_0030","DIC_0030","CAT_0080"}};
        int vi = (int)g_sys.g(0x3164); if (vi < 0 || vi > 30) vi = 0;
        playVoiceIfExists(std::string("V_G04_") + tb[n][vi]);
    }
    void onUpdate() override {
        switch (state) {
        case 0: if (g_trans.clear()) { if (n < 3) voice(); state = 1; hold = 0; } break;
        case 1:
            if (g_input.isPressed(BTN_START | BTN_CIRCLE) || ++hold >= 300) {
                g_trans.style(n < 3 ? 4 : 0); g_trans.cover(30); state = 2; n++;
            } break;
        case 2:
            if (g_trans.covered()) {
                if (n == 4) { parent->popAll(); parent->push(0x22); }
                else { t = g_gfx.load(names[n]); state = 3; }
            } break;
        case 3: g_trans.reveal(30); state = 0; hold = 0; break;
        }
    }
    void onDraw() override { g_gfx.clear({255, 255, 255, 255}); if (t) g_gfx.draw(*t, 0, 0); }
};

struct MovieJob : Job {
    const char* file; int after; Movie m; int st = 0;
    MovieJob(const char* f, int a) : file(f), after(a) {}
    void onUpdate() override {
        switch (st) {
        case 0:
            g_audio.setGroupVolume(0, g_sys.movie() / 5.f);
            if (!m.open(file)) { st = 4; break; }
            g_trans.style(0); g_trans.reveal(10); st = 2; break;
        case 2:
            if (!m.update() || g_input.isPressed(BTN_START | BTN_CIRCLE)) { g_trans.cover(10); st = 3; } break;
        case 3: if (g_trans.covered()) { m.stop(); g_sys.applyVolumes(); st = 4; } break;
        case 4:
            if (after) { parent->popAll(); parent->push(after); } else selfPop();
            st = 5; break;
        }
    }
    void onDraw() override { g_gfx.clear({0, 0, 0, 255}); m.draw(); }
};

struct Petal { float x, y, base, speed; int frame, dir, tick, rot; Color c; float a, maxA, scale; int drift; float dv; };

struct Title : Job {
    TexP parts, logo, copy, startT, obj;
    int state = 0, idle = 0, cur = 0, wait = 0, pending = -1;
    float bx[6]; int rotA = 0, spin = 0, hinge = 1; bool spinning = true; bool voiced = false;
    int blink = 255, blinkD = -6; bool logoDone = false;
    std::vector<Petal> petals;
    static constexpr Spr blue[6] = {{80,0,150,16},{80,16,150,32},{80,48,127,64},{80,64,160,96},{80,32,116,48},{80,96,158,112}};
    static constexpr Spr org[6] = {{0,0,70,16},{0,16,70,32},{0,48,47,64},{0,64,80,96},{0,32,36,48},{0,96,78,112}};
    void initPetal(Petal& p, int i) {
        static const uint32_t cols[5] = {0x80ffcdff, 0x80707bf2, 0x8040b7f7, 0x80b0ff96, 0x80ffebbb};
        p.x = (float)(-120 + 16 * i); p.base = 140; p.y = p.base + (rng() % 10) * 20;
        p.frame = rng() % 7; p.dir = rng() & 1; p.tick = 0; p.rot = rng() % 360;
        uint32_t c = cols[rng() % 5]; p.c = {(uint8_t)c, (uint8_t)(c >> 8), (uint8_t)(c >> 16), 255};
        int v = rng() % 10; p.speed = v < 5 ? 0.5f + v * 0.025f : 0.6f + (v - 5) * 0.025f;
        p.a = 0; p.maxA = (float)(0x80 - (rng() & 0x3f)); p.scale = 1 - (rng() & 3) * 0.1f;
        int r = rng() % 3; p.dv = r == 0 ? p.speed / ((rng() & 3) + 2) : r == 1 ? p.speed : p.speed * 0.5f;
    }
    void onEnter() override {
        parts = g_gfx.load("title_parts.txg"); logo = g_gfx.load("title_logo.txg");
        copy = g_gfx.load("title_copyright.txg"); startT = g_gfx.load("title_start.txg"); obj = g_gfx.load("title_obj.txgtl");
        g_audio.setVolume(CH_BGM, 1); g_audio.play(CH_BGM, "SK_BGM_01.adx", true); g_audio.playCue(0x23);
        for (auto& b : bx) b = -280;
        petals.resize(30); for (int i = 0; i < 30; i++) initPetal(petals[i], i);
    }
    void onResume() override { g_trans.reveal(10); }
    int logoAnim() {
        if (spinning) { spin += 18; if (spin >= 360) { spin = 0; spinning = false; } }
        if (hinge == 1) { rotA += 18; if (rotA >= 180) { rotA = 180; hinge = 2; } }
        else if (hinge == 2) { rotA -= 12; if (rotA <= -24) { rotA = -24; hinge = 3; } }
        else if (hinge == 3) { rotA += 3; if (rotA >= 0) { rotA = 0; hinge = 0; } }
        return !spinning && hinge == 0;
    }
    int menu() {
        switch (state) {
        case 0:
            if (logoAnim()) {
                if (!voiced) { voiced = true;
                    static const char* tv[31] = {"MSR_0040","SRT_0040c","SRT_0040d","SRT_0040e","SRT_0040f","NNM_0040a","NNM_0040b","MSK_0010a","MSK_0010c","MSK_0010d","JIN_0040a","JIN_0040c","JIN_0040e","RNS_0040","OTH_0040a","OTH_0040b","OTH_0040c","SNG_0040a","SNG_0040b","MID_0040","RIT_0040","YUK_0040","CHR_0040b","KHR_0040a","KHR_0040b","SIR_0040","SOR_0040a","SOR_0040b","KZK_0040","DIC_0040","CAT_0090"};
                    int vi = (int)g_sys.g(0x3164); if (vi < 0 || vi > 30) vi = 0;
                    playVoiceIfExists(std::string("V_G04_") + tv[vi]); }
                state = 1;
            } break;
        case 1:
            if (++idle >= 3600) return 7;
            blink += blinkD; if (blink < 0x4c) { blink = 0x4b; blinkD = 6; } else if (blink >= 255) { blink = 255; blinkD = -6; }
            if (g_input.isPressed(BTN_CIRCLE | BTN_CROSS | BTN_SQUARE | BTN_TRIANGLE | BTN_START | BTN_SELECT)) { g_audio.playCue(0x29); state = 2; }
            break;
        case 2: {
            bool all = true;
            for (int i = 0; i < 6; i++) { float tg = -175.f + 70 * i; bx[i] = std::min(tg, bx[i] + 20); all &= bx[i] >= tg; }
            if (all) state = 4; break; }
        case 4:
            if (g_input.isPressed(BTN_RIGHT)) { cur = (cur + 1) % 6; g_audio.playCue(0x2a); }
            if (g_input.isPressed(BTN_LEFT)) { cur = (cur + 5) % 6; g_audio.playCue(0x2a); }
            if (g_input.mouseMoved) for (int i = 0; i < 6; i++) if (std::abs(g_input.mouseX - (240 - 175 + 70 * i)) < 32 && std::abs(g_input.mouseY - 216) < 34 && i != cur) { cur = i; g_audio.playCue(0x2a); }
            if (g_input.isPressed(BTN_CIRCLE)) {
                g_audio.playCue(0x29);
                if (cur == 0) { g_audio.play(CH_VOICE, "V_G04_MID_0130.hca", false); wait = 180; state = 3; }
                else if (cur == 5) { /* port: nothing to install */ }
                else return cur + 1;
            } break;
        case 3:
            if (--wait < 1 && !g_audio.playing(CH_VOICE)) { wait = 0; state = 4; return 1; }
            break;
        }
        return -1;
    }
    void onUpdate() override {
        for (int i = 0; i < 30; i++) {
            Petal& p = petals[i];
            p.y -= p.speed;
            if (p.y > -130 && p.y < 140) p.a = std::min(p.maxA, p.a + 2);
            if (p.y <= -130) p.a = std::max(0.f, p.a - 2);
            if (p.y < 170) p.x -= p.dv;
            if (++p.tick >= 5) { p.tick = 0; p.frame += p.dir ? 1 : -1; if (p.frame >= 6) { p.frame = 6; p.dir = 0; } if (p.frame <= 0) { p.frame = 0; p.dir = 1; } }
            if (p.y < -150) initPetal(p, i);
        }
        if (pending >= 0) {
            if (g_trans.covered()) {
                int id = pending; pending = -1;
                if (id == 5) { g_audio.stop(CH_BGM); clearSaveBuffer(); parent->popAll(); parent->push(5); }
                else if (id == 0x20) { g_audio.stop(CH_BGM); parent->popAll(); parent->push(0x20); }
                else if (id == 0x15 || id == 0x1e) { parent->popAll(); parent->push(id); }
                else parent->push(id);
            }
            return;
        }
        int r = menu();
        if (r >= 0) {
            static const int map[8] = {0, 5, 0x1c, 0x15, 0x1e, 0xf, 0, 0x20};
            if (g_audio.playing(CH_VOICE)) g_audio.stop(CH_VOICE);
            g_trans.cover(10); pending = map[r];
        }
    }
    void onDraw() override {
        g_gfx.clear({255, 255, 255, 255});
        if (obj) g_gfx.drawSub(*obj, {0, 0, 344, 272}, {136, 0, 344, 272});
        if (state == 3 || state == 4)
            for (auto& p : petals) if (p.a > 0)
                ui(*parts, {216, p.frame * 16, 232, p.frame * 16 + 16}, p.x, p.y, p.a / 255.f, p.scale, p.scale, (float)p.rot, p.c);
        if (state >= 2) for (int i = 0; i < 6; i++) {
            bool sel = state >= 3 && i == cur;
            ui(*parts, sel ? Spr{160, 0, 216, 56} : Spr{160, 56, 216, 112}, bx[i], -80);
            ui(*parts, sel ? org[i] : blue[i], bx[i], -80);
        }
        if (state < 2) ui(*startT, {0, 0, 184, 32}, 0, -80, blink / 255.f);
        ui(*copy, {0, 0, 256, 16}, -90, -120); ui(*copy, {0, 16, 256, 32}, 166, -120);
        float rs = (float)spin;
        ui(*logo, {0, 0, 200, 152}, -145, 45, 1, 1, 1, rs);
        const float hx = -151 + 145, hy = -13 - 45; float ra = rotA * (float)M_PI / 180;
        ui(*logo, {424, 232, 512, 256}, -145 + hx * cos(ra) - hy * sin(ra), 45 + hx * sin(ra) + hy * cos(ra), 1, 1, 1, (float)rotA);
        ui(*logo, {200, 0, 400, 152}, -145, 45, 1, 1, 1, rs);
    }
};

struct Option : Job {
    TexP bg, board, cursor, text; PageHeader hdr; int cur = 0; uint32_t v[8], saved[8]; int exitSt = 0; int tick = 0, hl = 5, hld = 3;
    void pull(uint32_t* o) { o[0]=g_sys.textSpeed(); o[1]=g_sys.skipAll(); o[2]=g_sys.bgm(); o[3]=g_sys.se(); o[4]=g_sys.voice(); o[5]=g_sys.movie(); o[6]=g_sys.sorataVoice(); o[7]=g_sys.install(); }
    void push_(uint32_t* o) { g_sys.textSpeed()=o[0]; g_sys.skipAll()=o[1]; g_sys.bgm()=o[2]; g_sys.se()=o[3]; g_sys.voice()=o[4]; g_sys.movie()=o[5]; g_sys.sorataVoice()=o[6]; g_sys.install()=o[7]; g_sys.applyVolumes(); }
    void onEnter() override {
        bg = g_gfx.load("option_bg.txg"); board = g_gfx.load("option_bg_board.txg"); cursor = g_gfx.load("option_cursor.txg"); text = g_gfx.load("option_bg_text.txg");
        pull(v); pull(saved); hdr.set(7); hdr.start();
        g_help.show("○設定変更を決定　×変更せずに戻る");
    }
    void onResume() override { g_trans.style(0); g_trans.reveal(10); }
    void onUpdate() override {
        hdr.update(); tick++;
        hl += hld; if (hl >= 0x80) { hl = 0x80; hld = -3; } if (hl <= 5) { hl = 5; hld = 3; }
        if (exitSt == 1) { if (g_trans.covered()) { selfPop(); siblingPush(4); exitSt = 2; } return; }
        if (exitSt) return;
        auto chg = [&](int nv) { if ((uint32_t)nv != v[cur]) { v[cur] = nv; g_audio.playCue(0x2a); push_(v); } };
        if (g_input.isPressed(BTN_DOWN)) { cur = (cur + 1) % 8; g_audio.playCue(0x2a); }
        if (g_input.isPressed(BTN_UP)) { cur = (cur + 7) % 8; g_audio.playCue(0x2a); }
        bool r = g_input.isPressed(BTN_RIGHT), l = g_input.isPressed(BTN_LEFT);
        if (r || l) {
            if (cur == 0) chg(std::clamp((int)v[0] + (r ? 1 : -1), 0, 2));
            else if (cur == 1) chg(r ? 1 : 0);
            else if (cur <= 5) chg(std::clamp((int)v[cur] + (r ? 1 : -1), 0, 5));
            else chg(l ? 1 : 0);
        }
        if (g_input.isPressed(BTN_CIRCLE)) { push_(v); g_audio.playCue(0x29); exit(); }
        else if (g_input.isPressed(BTN_CROSS)) { push_(saved); g_audio.playCue(0x1f); exit(); }
    }
    void exit() { g_trans.cover(10); g_help.hide(); exitSt = 1; }
    void onDraw() override {
        for (int j = 0; j < 5; j++) for (int i = 0; i < 8; i++) ui(*bg, {0, 0, 64, 64}, -240 + 64 * i + 32, 136 - 64 * j - 32);
        ui(*bg, {64, 0, 168, 64}, -120, 130); ui(*bg, {64, 0, 168, 64}, 120, 130, 1, -1, 1);
        hdr.draw();
        ui(*board, {0, 0, 456, 216}, 5, -2); ui(*board, {0, 216, 80, 256}, 175, -75);
        for (int r = 0; r < 8; r++) ui(*text, {0, 16 * r, 104, 16 * r + 16}, -110, 74 - 21.f * r);
        ui(*cursor, {0, 0, 24, 24}, -170, 75 - 21.f * cur);
        ui(*cursor, {56, 0, 64, 20}, 5, 74 - 21.f * cur, hl / 255.f, 45, 1);
        ui(*text, {104, 0, 120, 16}, -25, 74); ui(*text, {120, 0, 136, 16}, 165, 74);
        for (uint32_t i = 0; i <= v[0]; i++) ui(*cursor, {24, 0, 56, 24}, 13 + 56.f * i, 74);
        ui(*text, {128, 16, 176, 32}, 30, 53); ui(*text, {104, 16, 128, 32}, 100, 53); ui(*text, {216, 0, 232, 16}, 65, 52);
        ui(*cursor, {0, 24, 56, 48}, v[1] == 0 ? 30 : 100, 53);
        const float ys[4] = {32, 11, -10, -31};
        for (int k = 0; k < 4; k++) {
            ui(*text, {136, 0, 152, 16}, -25, ys[k]); ui(*text, {152, 0, 168, 16}, 165, ys[k]);
            for (uint32_t i = 0; i < v[2 + k]; i++) ui(*cursor, {24, 0, 56, 24}, -1 + 28.f * (i + 1), ys[k]);
        }
        for (int k = 0; k < 2; k++) {
            float y = k ? -73.f : -52.f;
            ui(*text, {168, 0, 192, 16}, 30, y); ui(*text, {192, 0, 216, 16}, 100, y); ui(*text, {216, 0, 232, 16}, 65, y - 1);
            ui(*cursor, {0, 24, 56, 48}, v[6 + k] ? 30 : 100, y);
        }
    }
};
}

std::unique_ptr<Job> createBootScene(int id) {
    switch (id) {
    case 1: return std::make_unique<Boot1>();
    case 2: return std::make_unique<Boot2>();
    case 3: return std::make_unique<Boot3>();
    case 4: return std::make_unique<Boot4>();
    case 0x20: return std::make_unique<LogoDontCopy>();
    case 0x21: return std::make_unique<Logos>();
    case 0x22: return std::make_unique<MovieJob>("opening.pmf", 3);
    case 0x23: return std::make_unique<MovieJob>("ending.pmf", 0);
    case 0x14: return std::make_unique<Title>();
    case 0xf: return std::make_unique<Option>();
    }
    return nullptr;
}
void playVoiceIfExists(const std::string& name) {
    std::string f = name + ".hca";
    if (g_fs.exists(f)) g_audio.play(CH_VOICE, f, false);
}
