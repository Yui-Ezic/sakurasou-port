#include "adv.h"
#include "characters_api.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <set>
#include <sstream>

std::string g_advScript;
int g_advLastNative = 0; bool g_advNoMenu = false;
__attribute__((weak)) void simGetDate(int& m, int& d, int& w, bool& h);
__attribute__((weak)) bool simTipText(int i, std::string& t, std::string& b);

static int ms2f(int ms) { return ms * 60 / 1000; }
static int ms2fn(int ms) { return ms < 0 ? ms : ms * 60 / 1000; }
static int tri(int t, int h) { return t < h ? t * 255 / h : 255 - (t - h) * 255 / h; }

static std::deque<BacklogEntry> s_log; static std::vector<BacklogEntry> s_logV; static bool s_logDirty = true;
void backlogAdd(int t, const std::string& n, const std::string& v, const std::string& x) {
    s_log.push_back({t, n, v, x}); if (s_log.size() > 1000) s_log.pop_front(); s_logDirty = true;
}
void backlogClear() { s_log.clear(); s_logDirty = true; }
const std::vector<BacklogEntry>& backlog() { if (s_logDirty) { s_logV.assign(s_log.begin(), s_log.end()); s_logDirty = false; } return s_logV; }

AdvScene::AdvScene() { memset(clTimer, 0, sizeof clTimer); memset(clAt, -1, sizeof clAt); }

void AdvScene::start(const std::string& s) {
    vm.handler = this;
    Bytes c = g_fs.read(s);
    if (c.empty()) fprintf(stderr, "missing script %s\n", s.c_str());
    vm.load(std::move(c), s);
}
void AdvScene::onEnter() {
    texWin = g_gfx.load("conversation_window.txg"); texMail = g_gfx.load("conversation_mail.txg");
    texChoice = g_gfx.load("conversation_choice.txg"); texCloud = g_gfx.load("conversation_cloud.txg");
    texCloudP = g_gfx.load("conversation_cloud_parts.txg"); texPlace = g_gfx.load("conversation_place.txg");
    texDate = g_gfx.load("conversation_date.txg"); texFriend = g_gfx.load("conversation_friend.txg");
    for (int c : {CH_BGM, CH_BGM2, CH_AMB, CH_SE2, CH_VOICE}) { g_audio.stop(c); g_audio.setVolume(c, 1); }
    if (!g_advScript.empty()) start(g_advScript);
    g_trans.style(0); g_trans.reveal(5);    // FUN_0004f614: VM waits for the reveal (FUN_0004f660)
}
void AdvScene::onResume() { if (resumeFade) { resumeFade = false; g_trans.style(0); g_trans.reveal(10); } }
void AdvScene::onPause() {}

// ---- tokens ----
namespace { struct Tok { int type; uint32_t cp; int id; }; }
static Tok nextTok(const std::string& s, size_t& p) {
    if (p >= s.size()) return {0, 0, 0};
    const char* c = s.c_str() + p;
    if (*c == '\n') { p++; return {1, 0, 0}; }
    if (!strncmp(c, "▼", 3)) { p += 3; return {2, 0, 0}; }
    if (!strncmp(c, "†", 3)) {
        size_t q = p + 3; int id = 0;
        while (q < s.size() && strncmp(s.c_str() + q, "‡", 3)) { if (s[q] >= '0' && s[q] <= '9') id = id * 10 + s[q] - '0'; else if ((uint8_t)s[q] >= 0xef) {} q++; }
        p = q + 3; return {3, 0, id};
    }
    if (!strncmp(c, "‡", 3)) { p += 3; return {4, 0, 0}; }
    if (*c == '\\') { c++; p++; }
    const char* e = c; uint32_t cp = utf8Next(e); p = e - s.c_str();
    bool half = (cp >= 0x20 && cp <= 0x7e) || (cp >= 0xff61 && cp <= 0xff9f);
    return {half ? 5 : 6, cp, 0};
}

void AdvScene::voicePrepare(const std::string& n) { voiceName = n; }

void AdvScene::typing(TextWin& w) {
    float inc = g_sys.textSpeed() == 0 ? 0.5f : g_sys.textSpeed() == 1 ? 1.f : g_sys.textSpeed() == 2 ? 2.f : 0;
    w.acc += inc;
    if (w.acc <= 0) return;
    for (;;) {
        Tok t = nextTok(w.raw, w.pos);
        switch (t.type) {
        case 0: g_audio.stop(CH_VOICE, 4); g_sys.setTextRead(w.msgId); w.text = TextWin::T_IDLE; return;
        case 1: w.penX = 0; w.penY += 18; break;
        case 2: w.text = TextWin::T_WAIT; w.a = w.b = 0; return;
        case 3:
            if (w.links.size() < 4) { w.links.push_back({t.id, (int)w.penX, w.penY, 0, 0}); w.inLink = true; g_sys.setBit(0x3154, t.id - 1); }
            break;
        case 4: if (!w.links.empty() && w.inLink) { w.links.back().x1 = (int)w.penX; w.links.back().y1 = w.penY; } w.inLink = false; break;
        default: {
            Color c = w.inLink ? Color{0x22, 0x92, 0xe9, 255} : Color{0, 0, 0, 255};
            w.glyphs.push_back({t.cp, (int)w.penX, w.penY, c});
            w.penX += t.type == 5 ? 7 : 14; w.acc -= 1;
            if (w.acc <= 0) return;
        }
        }
    }
}

void AdvScene::winUpdate1(TextWin& w, bool isB) {
    switch (w.text) {
    case TextWin::T_TYPING:
        if (g_input.isPressed(BTN_CIRCLE) || g_input.isHeld(BTN_CROSS) || (skip && readOk(w.msgId))) w.acc = isB ? w.acc + 999 : 999;
        break;
    case TextWin::T_WAIT: {
        bool go = g_input.isPressed(BTN_CIRCLE) || g_input.isPressed(BTN_CROSS) || (skip && readOk(w.msgId));
        if (g_input.isPressed(BTN_CIRCLE | BTN_CROSS)) autoMode = false;
        if (go) { w.text = TextWin::T_TYPING; w.acc = 0; }
        else if (!g_input.isHeld(BTN_L) && g_input.isPressed(BTN_SQUARE) && !w.links.empty()) { w.sel = 0; w.text = TextWin::T_TIPSEL; g_audio.playCue(0x2a); }
        break; }
    case TextWin::T_TIPSEL:
        if (g_input.isPressed(BTN_CROSS)) { w.sel = -1; w.text = TextWin::T_WAIT; g_audio.playCue(0x1f); }
        else if (g_input.isPressed(BTN_CIRCLE)) { w.text = TextWin::T_TIPVIEW; g_audio.playCue(0x29); }
        else if (g_input.isPressed(BTN_SQUARE) && !g_input.isHeld(BTN_L)) { w.sel = (w.sel + 1) % (int)w.links.size(); g_audio.playCue(0x2a); }
        break;
    case TextWin::T_TIPVIEW:
        if (g_input.isPressed(BTN_CROSS)) { w.text = TextWin::T_TIPSEL; g_audio.playCue(0x1f); }
        break;
    }
}

void AdvScene::winUpdate2(TextWin& w, bool isB) {
    auto st = [&](int& s, int& t) {
        if (s == TextWin::W_OPENING) { if (skip || ++t >= 12) { s = TextWin::W_OPEN; t = 0; } }
        else if (s == TextWin::W_CLOSING) { if (skip || ++t >= 12) { s = TextWin::W_CLOSED; t = 0; } }
    };
    st(w.win, w.winT); st(w.plate, w.plateT);
    if (w.text == TextWin::T_VOICEWAIT) {
        if (skip && readOk(w.msgId)) voiceName.clear();
        if (!g_sys.sorataVoice() && voiceName.size() > 8 && voiceName.compare(6, 3, "SRT") == 0) voiceName.clear();
        if (!voiceName.empty() && g_fs.exists(voiceName)) g_audio.play(CH_VOICE, voiceName, false);
        voiceName.clear();
        w.text = TextWin::T_TYPING; w.acc = 0;
    }
    if (w.text == TextWin::T_TYPING) typing(w);
    else if (w.text == TextWin::T_WAIT) {
        w.a = std::min(w.a + 1, 600);
        if (!g_audio.playing(CH_VOICE)) { if (++w.b > 600) w.a = 600; }
        if (autoMode && w.a >= 120 && w.b >= 30) { w.text = TextWin::T_TYPING; w.acc = 0; }
    }
}

void AdvScene::menuInput() {
    bool l = g_input.isHeld(BTN_L);
    bool canMenu = g_mode == 0 || (g_mode >= 3 && g_mode <= 6);
    auto openScene = [&](int id, int style) {
        pendingScene = id; g_audio.stop(CH_VOICE, 10); g_trans.style(style); g_trans.cover(10);
    };
    if (g_input.isPressed(BTN_TRIANGLE)) {
        if (l && canMenu) { takeSnapshot(); shiftHistory(); g_audio.playCue(0x29); }
        else if (!l && canMenu) { takeSnapshot(); openScene(g_mode == 0 ? 0x10 : 0x11, 2); }
        else if (!l && g_mode == 1) { g_popup.open("終了しますか？", true); exitState = -1; }
    } else if (g_input.isPressed(BTN_SELECT)) { g_audio.playCue(0x29); openScene(0x1d, 0); }
    else if (l && g_input.isPressed(BTN_SQUARE) && canMenu) { g_audio.playCue(0x29); openScene(g_mode == 0 ? 0x1a : 0x24, 0); }
}

void AdvScene::bgStep() {
    auto cm = [&](BgImg& b) {
        if (b.state == 2) { b.t++; b.alpha = b.n > 0 ? b.t * 255 / b.n : 255; if (b.t >= b.n || skip) { b.alpha = 255; b.state = 1; } }
        else if (b.state == 3) { b.t++; b.alpha = b.n > 0 ? 255 - b.t * 255 / b.n : 0; if (b.t >= b.n || skip) { b.alpha = 0; b.state = 0; } }
    };
    cm(bg[0]); cm(bg[1]);
    BgImg& c = bg[cur];
    if (scrollN > 0) {
        scrollT++; float k = (float)scrollT / scrollN;
        if (scrollT >= scrollN || skip) { k = 1; scrollN = 0; }
        c.x = sx0 + (sx1 - sx0) * k; c.y = sy0 + (sy1 - sy0) * k;
    }
    if (shakeT != -2) {
        auto r = [&] { seed = seed * 0x41c64e6d + 0x3039; return (seed >> 16) & 0x7fff; };
        shOffX = 2.f * shakeX * r() / 32767 - shakeX; shOffY = 2.f * shakeY * r() / 32767 - shakeY; r();
        if (shakeT >= 0 && (--shakeT <= 0 || skip)) { shakeT = -2; shOffX = shOffY = 0; }
    }
}

void AdvScene::choiceUpdate1() {
    if (chState != 2) return;
    int n = (int)ch.size();
    if (g_input.isPressed(BTN_CIRCLE)) {
        if (chCur < 0) { chCur = 0; g_audio.playCue(0x2a); }
        else { backlogAdd(1, "", "", ch[chCur]); chState = 3; chT = 0; g_audio.playCue(0x29); }
    } else if (g_input.isRepeat(BTN_UP)) { chCur = chCur <= 0 ? n - 1 : chCur - 1; g_audio.playCue(0x2a); }
    else if (g_input.isRepeat(BTN_DOWN)) { chCur = (chCur < 0 || chCur >= n - 1) ? 0 : chCur + 1; g_audio.playCue(0x2a); }
    if (g_input.mouseMoved || g_input.mouseClick)
        for (int i = 0; i < n; i++) {
            int cy = 136 - (16 + n * 12 - 12 - i * 24);
            if (std::abs(g_input.mouseX - 240) < 134 && std::abs(g_input.mouseY - cy) < 12 && chCur != i) { chCur = i; g_audio.playCue(0x2a); }
        }
}
void AdvScene::choiceUpdate2() {
    if (chState == 0) return;
    chT++;
    if (chState == 1 && chT >= 12 + 6 * ((int)ch.size() - 1) + 12) chState = 2;
    else if (chState == 3 && chT >= 60) { chState = 4; chT = 0; }
    else if (chState == 4 && chT >= 24) chState = 0;
}

void AdvScene::cloudSpawn(int slot, int pos) {
    Cloud& c = cl[slot & 31];
    if (clAt[pos] >= 0) { cl[clAt[pos]].state = 4; cl[clAt[pos]].t = 0; }
    g_audio.playCue(0x1b);
    c.state = 1; c.t = 0; c.age = 0; c.pos = pos; clAt[pos] = slot & 31;
    static const float P[2][4][2] = {{{-152, 30}, {152, 30}, {0, -36}, {0, 92}}, {{-152, 4}, {152, 52}, {96, -36}, {-96, 92}}};
    bool alt = clAt[2] >= 0 || clAt[3] >= 0; c.x = P[alt][pos][0]; c.y = P[alt][pos][1];
}
void AdvScene::cloudsUpdate() {
    if (clMgr == 0) return;
    clFrame++;
    static const uint32_t btn[4] = {BTN_SQUARE, BTN_CIRCLE, BTN_CROSS, BTN_TRIANGLE};
    if (clMgr == 1) {
        for (int i = 0; i < 4; i++) if (g_input.isPressed(btn[i]) && clAt[i] >= 0) {
            Cloud& c = cl[clAt[i]]; bool any = false;
            for (int k = 0; k < 3; k++) if (c.ok[k] >= 0) { any = true; }
            if (any) { int p = c.pos; c.state = 3; c.t = 0; clAt[p] = -1; for (int k = 0; k < 3; k++) if (c.ok[k] >= 0) cloudSpawn(c.ok[k], cl[c.ok[k]].pos); }
            else {
                g_audio.playCue(0x1c); c.state = 2; c.t = 0; clResult = (int)(&c - cl);
                for (int j = 0; j < 32; j++) if (cl[j].state == 1 && &cl[j] != &c) { cl[j].state = 4; cl[j].t = 0; }
                backlogAdd(2, "", "", c.text); clMgr = 2; memset(clTimer, 0, sizeof clTimer);  // DRAIN: no further spawns
            }
            break;
        }
        for (int i = 0; i < 32; i++) if (clTimer[i] > 0 && --clTimer[i] == 0) cloudSpawn(i, cl[i].pos);
    }
    bool any = false;
    for (int i = 0; i < 32; i++) {
        Cloud& c = cl[i];
        if (c.state == 0) { any |= clTimer[i] > 0; continue; }
        any = true; c.t++; c.age++;
        if (c.state == 1 && c.timeout >= 0 && c.age >= c.timeout) {
            int p = c.pos; clAt[p] = -1; g_audio.playCue(0x1d); c.state = 3; c.t = 0;
            for (int k = 0; k < 3; k++) if (c.to[k] >= 0) cloudSpawn(c.to[k], p);
        } else if (c.state == 2 && c.t >= 120) { clAt[c.pos] = -1; c.state = 0; }
        else if ((c.state == 3 || c.state == 4) && c.t >= 50) { if (clAt[c.pos] == i) clAt[c.pos] = -1; c.state = 0; }
    }
    if (!any) clMgr = 0;
}

void AdvScene::bannerUpdate() {
    if (!banState) return;
    banT++;
    if (banState == 1 && banT > 24) { banState = 2; banT = 0; }
    else if (banState == 2 && banT >= 180) { banState = 3; banT = 0; }
    else if (banState == 3 && banT >= 20) banState = 0;
}
void AdvScene::dateUpdate() {
    if (!dateState) return;
    dateT++;
    if (dateState == 1 && dateT >= 30) { dateState = 2; dateT = 0; }
    else if (dateState == 2 && dateDur >= 0 && dateT >= dateDur) { dateState = 3; dateT = 0; }
    else if (dateState == 3 && dateT >= 10) dateState = 0;
}
void AdvScene::friendUpdate() {
    if (!frState) return;
    frT++;
    if (frState == 1 && frT >= 10) { frState = 2; frT = 0; }
    else if (frState == 2 && frT >= 120) { frState = 3; frT = 0; }
    else if (frState == 3 && frT >= 10) frState = 0;
}

// Native 145: game-title entry. Native text input replaces the PSP OSK; 24 characters as in the original.
void AdvScene::titleUpdate() {
    if (!ttl) return;
    ttlT++;
    const char* p = g_textTyped.c_str(); int n = 0;
    { const char* q = ttlText.c_str(); while (*q) { utf8Next(q); n++; } }
    while (*p && n < 0x18) { const char* b = p; uint32_t c = utf8Next(p); if (c >= 0x20) { ttlText.append(b, p - b); n++; } }
    g_textTyped.clear();
    while (g_textBackspace > 0 && !ttlText.empty()) {
        size_t e = ttlText.size() - 1;
        while (e > 0 && ((uint8_t)ttlText[e] & 0xc0) == 0x80) e--;
        ttlText.resize(e); g_textBackspace--;
    }
    g_textBackspace = 0;
    if (ttlT > 10 && g_input.isPressed(BTN_CIRCLE) && !ttlText.empty()) {
        g_audio.playCue(0x29);
        memset(&g_game.raw[0x3558], 0, 64); memcpy(&g_game.raw[0x3558], ttlText.data(), std::min<size_t>(ttlText.size(), 63));
        textEntry(false); ttl = 0;
    }
}
void AdvScene::drawTitle() {
    if (!ttl) return;
    float k = std::min(1.f, ttlT / 10.f), w = 360 * k, h = 96 * k;
    g_gfx.fillRect(240 - w / 2, 120 - h / 2, w, h, {255, 244, 214, 245});
    if (k < 1) return;
    g_gfx.fillRect(66, 112, 348, 22, {255, 255, 255, 255});
    uiFontCentered(g_fontM, "ゲームタイトル名", 0, 136 - 88, {224, 112, 32, 255});
    bool blink = (ttlT / 30) & 1;
    g_fontM.drawText(ttlText + (blink ? "" : "_"), 72, 116, {0, 0, 0, 255});
    uiFontCentered(g_fontM, ttlText.empty() ? "入力してください" : "決定 (Enter / ○)", 0, 136 - 150, {224, 112, 32, 255});
}
void AdvScene::leave() {   // FUN_0004f6e0
    exitState = 1; g_trans.style(0); g_trans.cover(5);
    for (int c = 0; c < CH_COUNT; c++) g_audio.stop(c, 5);
}
void AdvScene::onUpdate() {
    if (exitState == 1) { // FUN_0004f83c: subsystems keep ticking until covered
        bgStep(); charSetSpeaker(speaker); stage.update(skip);
        if (g_trans.covered()) { exitState = 2; selfPop(); }
        return;
    }
    if (exitState == 2) return;
    if (exitState == -1) {
        if (!g_popup.isOpen()) { if (g_popup.yes()) { vm = Vm(); leave(); } else exitState = 0; }
        return;
    }
    if (pendingScene >= 0) {
        if (g_trans.covered()) { int id = pendingScene; pendingScene = -1; resumeFade = true; parent->push(id); }
    } else if (intro) {
        if (g_trans.clear()) intro = false;
    } else {
        skip = g_input.isHeld(BTN_R);
        if (g_input.isPressed(BTN_START)) { autoMode = !autoMode; if (autoMode) g_audio.playCue(0x0f); }
        if (!vm.halted()) vm.run();
        if (vm.halted()) { leave(); return; }
        bool inWait = winA.text == TextWin::T_WAIT || winB.text == TextWin::T_WAIT;
        if (inWait && !g_advNoMenu) menuInput();
        if (pendingScene < 0) {
            winUpdate1(winA, false); winUpdate1(winB, true); choiceUpdate1();
            cloudsUpdate();
        }
    }
    hideUI = g_input.isHeld(BTN_L);
    t30 = (t30 + 1) % 30; t60 = (t60 + 1) % 60; s30 = (s30 + 1) % 30;
    winUpdate2(winA, false); winUpdate2(winB, true); choiceUpdate2();
    titleUpdate();
    bgStep(); charSetSpeaker(speaker); stage.update(skip); bannerUpdate(); dateUpdate(); friendUpdate();
    if (fadeState >= 2) {
        fadeT++; bool end = fadeT >= fadeN || skip;
        fadeAlpha = fadeState == 2 ? (fadeN > 0 ? fadeT * 255 / fadeN : 255) : (fadeN > 0 ? 255 - fadeT * 255 / fadeN : 0);
        if (end) { fadeAlpha = fadeState == 2 ? 255 : 0; fadeState = fadeState == 2 ? 1 : 0; }
    }
}

// ---- drawing ----
void AdvScene::drawBg(BgImg& b) {
    if (!b.tex || b.alpha <= 0) return;
    float px = 240 + b.x + shOffX - b.cx, py = 136 - (b.y + shOffY) - b.cy;
    g_gfx.draw(*b.tex, px, py, b.alpha / 255.f);
}
static void sub(const Tex& t, SDL_Rect s, float x, float y, float w, float h, float a = 1, Color c = {}) { g_gfx.drawSub(t, s, {x, y, w, h}, a, c); }

void AdvScene::drawText(TextWin& w, float sx, float sy) {
    for (auto& l : w.links) if (&l - w.links.data() == w.sel || w.sel < 0) {
        if (w.sel >= 0) g_gfx.fillRect(sx + l.x0, sy + l.y0 - 2, (float)(l.x1 - l.x0), 18, {0xff, 0xc7, 0x30, 255});
    }
    for (auto& g : w.glyphs) g_fontM.drawGlyph(g.cp, sx + g.x, sy + g.y, g.c);
}
void AdvScene::drawMarker(float x, float y) {
    auto q = [&](SDL_Rect s, float ox, float oy, float a) { sub(*texWin, s, 240 + x + ox - s.w / 2.f, 136 - y - oy - s.h / 2.f, (float)s.w, (float)s.h, a); };
    if (skip) { q({48, 8, 16, 16}, 0, 4, tri(s30, 15) / 255.f); q({24, 0, 24, 8}, 0, -8, 1); }
    else if (autoMode) { q({32, 8, 16, 16}, 0, 4, tri(t60, 30) / 255.f); q({0, 0, 24, 8}, 0, -8, 1); }
    else if (winA.text == TextWin::T_WAIT) q({32, 24, 24, 16}, 0, t30 < 20 ? 0 : (t30 - 20) * 8 / 10.f - 8, 1);
}
void AdvScene::drawWinA() {
    TextWin& w = winA;
    if (w.win != TextWin::W_CLOSED) {
        float dy = w.win == TextWin::W_OPEN ? 0 : w.win == TextWin::W_OPENING ? w.winT * 72 / 12.f - 72 : -w.winT * 72 / 12.f;
        float y = 220 - dy;
        sub(*texWin, {0, 12, 8, 52}, 8, y, 8, 52); sub(*texWin, {8, 12, 8, 52}, 16, y, 440, 52); sub(*texWin, {16, 12, 16, 52}, 456, y, 16, 52);
        if (w.plate != TextWin::W_CLOSED) {
            float s = w.plate == TextWin::W_OPEN ? 1 : w.plate == TextWin::W_OPENING ? w.plateT / 12.f : 1 - w.plateT / 12.f;
            float h = 20 * s, py = y - h;
            sub(*texWin, {32, 44, 8, 20}, 8, py, 8, h); sub(*texWin, {40, 44, 8, 20}, 16, py, 152, h); sub(*texWin, {48, 44, 8, 20}, 168, py, 8, h);
            if (w.plate == TextWin::W_OPEN) g_fontM.drawText(w.name, 22, y - 15, {255, 255, 255, 255});
        }
        drawText(w, 38, y + 12);
    }
    drawMarker(204, -112);
}
void AdvScene::drawWinB() {
    TextWin& w = winB;
    if (w.win == TextWin::W_CLOSED) return;
    float dy = w.win == TextWin::W_OPEN ? 0 : w.win == TextWin::W_OPENING ? 176 - w.winT * 176 / 12.f : w.winT * 176 / 12.f;
    auto nine = [&](float cy, float h) {
        float t = 136 - cy - h - dy; const float xs[4] = {8, 16, 464, 472};
        const int us[4] = {0, 8, 16, 24}; float rows[4] = {t, t + 8, t + 8 + (2 * h - 16), t + 2 * h};
        for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++)
            sub(*texMail, {us[c], us[r], 8, 8}, xs[c], rows[r], xs[c + 1] - xs[c], rows[r + 1] - rows[r]);
    };
    nine(64, 16); nine(0, 40);
    float hy = 136 - 64 - dy;
    g_fontM.drawText("差出人：", 36, hy - 7, {0, 0, 0, 255}); g_fontM.drawText(w.name, 92, hy - 7, {0, 0, 0, 255});
    drawText(w, 36, 136 - 16 - dy);
    if (w.text == TextWin::T_WAIT) sub(*texMail, {24, 0, 24, 16}, 228, 136 + 24 - 8 - dy, 24, 16);
}
void AdvScene::drawChoice() {
    if (chState == 0) return;
    int n = (int)ch.size(); float baseY = 136 - 16;
    float fk = chState == 1 ? std::min(1.f, chT / 12.f) : (chState == 4 && chT >= 12) ? 1 - (chT - 12) / 12.f : 1;
    float W = 268 * fk, H = n * 24 * fk, x0 = 240 - W / 2, y0 = baseY - H / 2;
    if (fk > 0) {
        const float xs[4] = {x0 - 8, x0, x0 + W, x0 + W + 8}, ys[4] = {y0 - 8, y0, y0 + H, y0 + H + 8};
        const int u[3] = {0, 8, 16}, v[3] = {0, 8, 16};
        for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) sub(*texChoice, {u[c], v[r], 8, 8}, xs[c], ys[r], xs[c + 1] - xs[c], ys[r + 1] - ys[r]);
    }
    for (int i = 0; i < n; i++) {
        float k = 1;
        if (chState == 1) k = std::clamp((chT - (12 + 6 * i)) / 12.f, 0.f, 1.f);
        else if (chState == 3 && i != chCur) k = std::max(0.f, 1 - chT / 12.f);
        else if (chState == 4) k = i == chCur ? std::max(0.f, 1 - chT / 12.f) : 0;
        if (k <= 0) continue;
        float cy = baseY - (n * 12 - 12 - i * 24), h = 24 * k, y = cy - h / 2;
        Color tint = i == chCur ? Color{0xff, 0xff, 0xcc, 255} : Color{};
        sub(*texChoice, {24, 0, 8, 24}, 114 - 8, y, 8, h, 1, tint); sub(*texChoice, {32, 0, 8, 24}, 114, y, 252, h, 1, tint); sub(*texChoice, {40, 0, 8, 24}, 366, y, 8, h, 1, tint);
        if (k >= 1) g_fontM.drawText(ch[i], 122, cy - 7, i == chCur ? Color{0, 0, 0, 255} : Color{0x80, 0x80, 0x80, 255}, 14);
        if (i == chCur && chState >= 2) { sub(*texChoice, {48, 0, 8, 24}, 106, y, 8, h); sub(*texChoice, {56, 0, 8, 24}, 114, y, 252, h); sub(*texChoice, {64, 0, 8, 24}, 366, y, 8, h); }
    }
}
void AdvScene::drawClouds() {
    for (auto& c : cl) {
        if (!c.state) continue;
        float a = 1, s = 1;
        if (c.state == 1 && c.t < 50) a = s = c.t / 50.f;
        if (c.state == 2) s = c.t < 10 ? 1 + 0.5f * c.t / 10 : c.t < 20 ? 1.5f - 0.5f * (c.t - 10) / 10 : 1;
        if (c.state >= 3) a = 1 - c.t / 50.f;
        int fi = clFrame % 60; fi = fi / 10; 
        float px = 240 + c.x, py = 136 - c.y, w = 160 * s, h = 80 * s;
        sub(*texCloud, {0, fi * 40, 80, 40}, px - w / 2, py - h / 2, w, h, a);
        if (a > 0.5f) {
            std::vector<std::string> ls; std::stringstream ss(c.text); std::string l; while (std::getline(ss, l)) ls.push_back(l);
            float H = 14 + 18.f * ((int)ls.size() - 1), y = py - H / 2;
            for (auto& t : ls) { uiFontCentered(g_fontM, t, c.x, 136 - y - 7, {0, 0, 0, (uint8_t)(255 * a)}); y += 18; }
        }
        static const int fr[4] = {3, 0, 1, 2};
        sub(*texCloudP, {fr[c.pos] * 24, 0, 24, 24}, px + 42 - 12, py + 30 - 12, 24, 24, a);
    }
}
void AdvScene::drawBanner() {
    if (!banState) return;
    float bx = -240, by = 112, dx = 0, alpha = 1;
    if (banState == 3) { dx = -160.f * banT / 20; alpha = 1 - banT / 20.f; }
    auto strip = [&](const Tex& t, int v, float k, float amp) {
        // 20 columns of 16px, bent; origin (-256,96) rotated 15 degrees
        std::vector<SDL_Vertex> vs; std::vector<int> ix;
        const float ca = cos(M_PI / 12), sa = sin(M_PI / 12);
        Uint8 al = (Uint8)(255 * alpha);
        for (int i = 0; i <= 20; i++) {
            float x = 16.f * i, lx = x * k;
            float nx = std::min(x / 160.f, 1.f), bend = amp * (1 - sqrt(1 - nx * nx));
            for (int e = 0; e < 2; e++) {
                float ly = (e ? -16.f : 16.f) + bend;
                float wx = bx - 16 + dx + lx * ca - ly * sa, wy = by - 16 + lx * sa + ly * ca;
                SDL_Vertex o; o.position = {240 + wx, 136 - wy}; o.color = {255, 255, 255, al};
                o.tex_coord = {x / (float)t.w, (v + (e ? 32.f : 0.f)) / (float)t.h};
                vs.push_back(o);
            }
            if (i < 20) for (int q : {0, 1, 2, 2, 1, 3}) ix.push_back(i * 2 + q);
        }
        g_gfx.geometry(&t, vs.data(), (int)vs.size(), ix.data(), (int)ix.size());
    };
    float t = (float)banT, k = 1, amp = 0, k2 = 1, amp2 = 0;
    if (banState == 1) {
        k = std::min(t / 15, 1.f); amp = t < 15 ? 32 * sin(t / 15 * M_PI / 2) : 0;
        k2 = std::clamp((t - 10) / 15, 0.f, 1.f); amp2 = t < 10 ? 32 : t < 25 ? 32 * sin((t - 10) / 15 * M_PI / 2) : 0;
    }
    strip(*texPlace, 0, k, amp); strip(*texPlace, 32, k, amp);
    if (locTex && (banState != 1 || t >= 10)) strip(*locTex, 0, k2, amp2);
}
void AdvScene::drawDate() {
    if (!dateState || g_mode != 0) return;
    int m = 4, d = 1, wd = 0; bool hol = false;
    if (simGetDate) simGetDate(m, d, wd, hol);
    float t = (float)dateT, oy = dateState == 1 ? 8 - 8 * std::min(t / 10, 1.f) : dateState == 3 ? 40 * t / 10 : 0;
    float a = dateState == 1 ? -((sin(M_PI * t / 30) + 1) / 2 * M_PI / 2 * sin(2 * M_PI * t / 40)) : 0;
    float x = 380, y = -oy;
    sub(*texDate, {0, 48, 96, 8}, x, y, 96, 8);
    float h = 40 * cos(a);
    sub(*texDate, {0, 8, 96, 40}, x, y + 8, 96, h);
    sub(*texDate, {0, 0, 96, 8}, x, y, 96, 8);
    if (fabs(a) > 0.05f) return;
    auto g = [&](int sx, int sy, int sw, float gx, Color c = {0, 0, 0, 255}) { sub(*texDate, {sx, sy, sw, 16}, x + gx - sw / 2.f, y + 24 - 8, (float)sw, 16, 1, c); };
    Color w{255, 255, 255, 255}; (void)w;
    if (m >= 10) g(96 + 16 * (m / 10), 16, 16, 12);
    g(96 + 16 * (m % 10), 16, 16, 20); g(96, 0, 16, 32);
    if (d >= 10) g(96 + 16 * (d / 10), 16, 16, 44);
    g(96 + 16 * (d % 10), 16, 16, 52); g(112, 0, 16, 64); g(128, 0, 8, 74);
    Color wc = (wd == 0 || hol) ? Color{255, 0, 0, 255} : wd == 6 ? Color{0, 0x50, 255, 255} : Color{0, 0, 0, 255};
    g(wd == 0 ? 240 : 144 + 16 * (wd - 1), 0, 16, 82, wc); g(136, 0, 8, 90);
}
void AdvScene::drawFriend() {
    if (!frState) return;
    float k = frState == 1 ? frT / 10.f : frState == 3 ? 1 - frT / 10.f : 1;
    float cy = 136 + 40; float h = 40 * k;
    sub(*texFriend, {0, 0, 8, 40}, 0, cy - h / 2, 8, h, k); 
    sub(*texFriend, {8, 0, 16, 40}, 8, cy - h / 2, 464, h, k);
    sub(*texFriend, {24, 0, 8, 40}, 472, cy - h / 2, 8, h, k);
    SDL_Rect r = frKind == 3 ? SDL_Rect{32, 32, 224, 32} : SDL_Rect{32, 0, 160, 32};
    float w = r.w * k, hh = r.h * k;
    sub(*texFriend, r, 240 - w / 2, cy - hh / 2, w, hh, k);
}
void AdvScene::drawTip() {
    TextWin& w = active();
    if (w.text != TextWin::T_TIPVIEW || w.sel < 0) return;
    std::string t, b; if (simTipText) simTipText(w.links[w.sel].id - 1, t, b);
    g_gfx.fillRect(40, 30, 400, 150, {255, 250, 240, 245});
    g_fontM.drawText(t, 52, 40, {0xe9, 0x60, 0x22, 255}); g_fontM.drawText(b, 52, 62, {0, 0, 0, 255});
}
void AdvScene::onDraw() {
    g_gfx.clear({0, 0, 0, 255});
    drawBg(bg[cur]); drawBg(bg[1 - cur]);
    if (!hideUI) drawWinB();
    stage.drawSlot(0); stage.drawSlot(1); stage.drawLive2D();
    if (!hideUI) { drawWinA(); drawChoice(); }
    drawClouds();
    if (!hideUI) { drawBanner(); drawDate(); drawTip(); }
    drawTitle();
    drawFriend();
    float a = fadeAlpha / 255.f;
    if (a > 0) g_gfx.fillRect(0, 0, 480, 272, {0, 0, 0, (uint8_t)(255 * a)});
}

void AdvScene::updateSpeaker() { speaker = (activeWin == 0 && winA.plate == TextWin::W_CLOSED) ? "" : active().name; }
void AdvScene::bgmPlay(const std::string& f, int fr) {
    int o = CH_BGM + bgmDeck; bgmDeck ^= 1; int n = CH_BGM + bgmDeck;
    g_audio.stop(o, fr); g_audio.setVolume(n, bgmVol / 255.f); g_audio.play(n, f, true, fr);
}

int AdvScene::native(Vm& v, int id, int32_t& ret) {
    ret = 0; g_advLastNative = id;
    auto W = [](bool busy) { return busy ? 1 : 0; };
    if (id < 59 && id != 3 && id != 4) return baseNative(v, id, ret);
    if (id >= 161 && id <= 196) return stage.native(v, id, ret);
    switch (id) {
    case 59: case 60: case 61: case 62: case 63: return 0;
    case 3: waitCtr = ms2f(v.arg(0)); return 0;
    case 4: if (skip) waitCtr = 0; if (waitCtr <= 0) return 0; waitCtr--; return 1;
    case 87: bgmState = 2; bgmName = v.argStr(0); bgmPlay(bgmName, ms2f(v.arg(1))); return 0;
    case 88: bgmState = 0; g_audio.stop(CH_BGM + bgmDeck, ms2f(v.arg(0))); return 0;
    case 89: bgmState = 1; bgmName = v.argStr(0); return 0;
    case 90: return 0;
    case 91: bgmState = 2; bgmPlay(bgmName, ms2f(v.arg(0))); return 0;
    case 92: bgmVol = v.arg(0); g_audio.setVolume(CH_BGM + bgmDeck, bgmVol / 255.f, ms2f(v.arg(1))); return 0;
    case 93: g_audio.play(CH_SE2, v.argStr(0), true, ms2f(v.arg(1))); return 0;
    case 94: g_audio.stop(CH_SE2, ms2f(v.arg(0))); return 0;
    case 95: sePrep = v.argStr(0); return 0;
    case 96: return 0;
    case 97: g_audio.play(CH_SE2, sePrep, true, ms2f(v.arg(0))); return 0;
    case 98: return W(!skip && g_audio.playing(CH_SE2));
    case 99: g_audio.setVolume(CH_SE2, v.arg(0) / 255.f, ms2f(v.arg(1))); return 0;
    case 100: ambState = 2; ambName = v.argStr(0); g_audio.setVolume(CH_AMB, ambVol / 255.f); g_audio.play(CH_AMB, ambName, true, ms2f(v.arg(1))); return 0;
    case 101: ambState = 0; g_audio.stop(CH_AMB, ms2f(v.arg(0))); return 0;
    case 102: ambState = 1; ambName = v.argStr(0); return 0;
    case 103: return 0;
    case 104: ambState = 2; g_audio.setVolume(CH_AMB, ambVol / 255.f); g_audio.play(CH_AMB, ambName, true, ms2f(v.arg(0))); return 0;
    case 105: ambVol = v.arg(0); g_audio.setVolume(CH_AMB, ambVol / 255.f, ms2f(v.arg(1))); return 0;
    case 106: g_audio.playCue(v.arg(0), CH_SE); return 0;
    case 107: g_audio.stop(CH_SE, ms2f(v.arg(0))); return 0;
    case 108: return W(!skip && g_audio.playing(CH_SE));
    case 109: g_audio.setVolume(CH_SE, v.arg(0) / 255.f, ms2f(v.arg(1))); return 0;
    case 110: fadeN = ms2f(v.arg(0)); fadeT = 0; fadeState = fadeN < 1 ? 1 : 2; if (fadeN < 1) fadeAlpha = 255; return 0;
    case 111: return W(fadeState != 1);
    case 112: fadeN = ms2f(v.arg(0)); fadeT = 0; fadeState = fadeN < 1 ? 0 : 3; if (fadeN < 1) fadeAlpha = 0; return 0;
    case 113: return W(fadeState != 0);
    case 114: return 0;
    case 115: if (winA.win == TextWin::W_CLOSED) { winA.win = TextWin::W_OPENING; winA.winT = 0; } return 0;
    case 116: return W(winA.win != TextWin::W_OPEN);
    case 117: if (winA.win == TextWin::W_OPEN) { winA.win = TextWin::W_CLOSING; winA.winT = 0; } return 0;
    case 118: return W(winA.win != TextWin::W_CLOSED);
    case 119: if (winA.plate == TextWin::W_CLOSED) { winA.plate = TextWin::W_OPENING; winA.plateT = 0; } speaker = winA.name; return 0;
    case 120: return W(winA.plate != TextWin::W_OPEN);
    case 121: if (winA.plate == TextWin::W_OPEN) { winA.plate = TextWin::W_CLOSING; winA.plateT = 0; } speaker.clear(); return 0;
    case 122: return W(winA.plate != TextWin::W_CLOSED);
    case 123: active().clearText(); return 0;
    case 124: voicePrepare(v.argStr(0)); return 0;
    case 125: return 0;
    case 126: {
        TextWin& w = active();
        w.raw = v.argStr(1); w.pos = 0; w.msgId = v.arg(2); w.clearText();
        voicePrepare(v.argStr(0));
        std::string plain; size_t p = 0;
        for (;;) { size_t b = p; Tok t = nextTok(w.raw, p); if (!t.type) break; if (t.type == 1) plain += '\n'; else if (t.type >= 5) plain.append(w.raw, b, p - b); }
        backlogAdd(0, speaker, v.argStr(0), plain);
        w.text = TextWin::T_VOICEWAIT; return 0;
    }
    case 127: return W(active().text != TextWin::T_IDLE);
    case 128: active().name.clear(); speaker.clear(); return 0;
    case 129: active().name = v.argStr(0); speaker = active().name; return 0;
    case 130: if (winB.win == TextWin::W_CLOSED) { winB.win = TextWin::W_OPENING; winB.winT = 0; } return 0;
    case 131: return W(winB.win != TextWin::W_OPEN);
    case 132: if (winB.win == TextWin::W_OPEN) { winB.win = TextWin::W_CLOSING; winB.winT = 0; } return 0;
    case 133: return W(winB.win != TextWin::W_CLOSED);
    case 134: activeWin = v.arg(0); updateSpeaker(); return 0;
    case 135: case 136: {
        std::stringstream ss(v.argStr(id == 135 ? 0 : 1)); std::string l; ch.clear();
        while (std::getline(ss, l) && ch.size() < 5) ch.push_back(l);
        chCur = -1; chState = 1; chT = 0; return 0;
    }
    case 137: return W(chState != 0);
    case 138: ret = chCur; return 0;
    case 139: for (auto& c : cl) c.state = 0; memset(clTimer, 0, sizeof clTimer); memset(clAt, -1, sizeof clAt); clMgr = 0; clResult = -1; return 0;
    case 140: {
        Cloud& c = cl[v.arg(0) & 31]; c.state = 0; c.text = v.argStr(1); c.pos = v.arg(2) & 3;
        c.timeout = v.arg(3) < 0 ? -1 : ms2f(v.arg(3));
        for (int i = 0; i < 3; i++) { c.ok[i] = v.arg(4 + i); c.to[i] = v.arg(7 + i); }
        return 0;
    }
    case 141: clTimer[v.arg(0) & 31] = std::max(1, ms2f(v.arg(1))); return 0;
    case 142: clMgr = 1; clResult = -1; return 0;
    case 143: return W(clMgr != 0);
    case 144: ret = clResult; return 0;
    case 145: if (getenv("SAKURA_TRACE")) fprintf(stderr, "[title prompt, default \"%s\"]\n", (const char*)&g_game.raw[0x3558]); ttl = 1; ttlT = 0; ttlText = (const char*)&g_game.raw[0x3558]; textEntry(true); return 0;
    case 146: return W(ttl != 0);
    case 147: { BgImg& b = bg[1 - cur]; b.file = v.argStr(0); b.tex = g_gfx.load(b.file); b.cx = b.tex ? (float)b.tex->cx : 248; b.cy = b.tex ? (float)b.tex->cy : 144; b.alpha = 0; b.state = 0; return 0; }
    case 148: return 0;
    case 149: cur = 1 - cur; return 0;
    case 150: { int n = ms2fn(v.arg(0)); bg[1 - cur].alpha = 0; bg[1 - cur].state = 0; BgImg& c = bg[cur]; c.state = n > 0 ? 2 : 1; c.t = 0; c.n = n; c.alpha = n > 0 ? 0 : 255; group = 1; return 0; }
    case 151: { int n = ms2fn(v.arg(0)); bg[1 - cur].alpha = 0; bg[1 - cur].state = 0; BgImg& c = bg[cur]; c.state = n > 0 ? 3 : 0; c.t = 0; c.n = n; c.alpha = n > 0 ? 255 : 0; group = 2; return 0; }
    case 152: { int n = ms2fn(v.arg(0)); BgImg& b = bg[1 - cur]; b.state = n > 0 ? 3 : 0; b.t = 0; b.n = n; if (n <= 0) b.alpha = 0; bg[cur].state = 1; bg[cur].alpha = 255; group = 3; return 0; }
    case 153: return W(bg[0].state >= 2 || bg[1].state >= 2);
    case 154: bg[cur].x = (float)v.arg(0); bg[cur].y = (float)v.arg(1); return 0;
    case 155: bg[1 - cur].x = (float)v.arg(0); bg[1 - cur].y = (float)v.arg(1); return 0;
    case 156: { BgImg& c = bg[cur]; sx0 = c.x; sy0 = c.y; sx1 = (float)v.arg(0); sy1 = (float)v.arg(1); scrollN = ms2fn(v.arg(2)); scrollT = 0; if (scrollN < 1) { c.x = sx1; c.y = sy1; scrollN = 0; } return 0; }
    case 157: return W(scrollN > 0);
    case 158: { shakeX = v.arg(0); shakeY = v.arg(1); int n = ms2fn(v.arg(2)); seed = 1; shakeT = n < 0 ? -1 : n == 0 ? -2 : n; if (n == 0) shOffX = shOffY = 0; return 0; }
    case 159: shakeT = -2; shOffX = shOffY = 0; return 0;
    case 160: return W(shakeT > 0);
    case 197: locTex = g_gfx.load(v.argStr(0)); return 0;
    case 198: return 0;
    case 199: banState = 1; banT = 0; return 0;
    case 200: return W(banState != 0);
    case 201: dateState = 1; dateT = 0; dateDur = 180; return 0;
    case 202: dateState = 1; dateT = 0; dateDur = ms2fn(v.arg(0)); return 0;
    case 203: return W(dateState != 2);
    case 204: if (dateState) { dateState = 3; dateT = 0; } return 0;
    case 205: return W(dateState != 0);
    case 206: frKind = v.arg(0); frState = 1; frT = 0; g_audio.playCue(0x24); return 0;
    case 207: return W(frState != 0);
    default:
        { static std::set<int> seen; if (seen.insert(id).second) fprintf(stderr, "[ADV: unhandled native %d]\n", id); }
        return 0;
    }
    return 0;
}

// ---- save state: own compact encoding of everything the spec's 0x1a38 blob holds ----
namespace {
struct W { Bytes& b; void i(int32_t v) { b.insert(b.end(), (uint8_t*)&v, (uint8_t*)&v + 4); } void s(const std::string& x) { i((int)x.size()); b.insert(b.end(), x.begin(), x.end()); } };
struct R { const uint8_t* p; int32_t i() { int32_t v; memcpy(&v, p, 4); p += 4; return v; } std::string s() { int n = i(); std::string x((const char*)p, n); p += n; return x; } };
}
void AdvScene::saveState(Bytes& o) {
    W w{o};
    w.s(vm.name()); Vm::State st = vm.save();
    o.insert(o.end(), (uint8_t*)&st, (uint8_t*)&st + sizeof st);
    w.i(activeWin); w.s(speaker); w.i(autoMode);
    for (TextWin* t : {&winA, &winB}) { w.i(t->win == TextWin::W_OPEN); w.i(t->plate == TextWin::W_OPEN); w.s(t->name); w.s(t->raw); w.i(t->msgId); w.i((int)t->pos); }
    for (int i = 0; i < 2; i++) { w.s(bg[i].file); w.i(bg[i].alpha); w.i((int)bg[i].x); w.i((int)bg[i].y); }
    w.i(cur); w.i(fadeAlpha);
    w.i(bgmState); w.s(bgmName); w.i(bgmVol); w.i(ambState); w.s(ambName); w.i(ambVol);
    stage.saveState(o);
}
void AdvScene::loadState(const uint8_t* p, size_t) {
    R r{p};
    std::string nm = r.s(); start(nm);
    Vm::State st; memcpy(&st, r.p, sizeof st); r.p += sizeof st; vm.restore(st);
    activeWin = r.i(); speaker = r.s(); autoMode = r.i();
    for (TextWin* t : {&winA, &winB}) {
        t->win = r.i() ? TextWin::W_OPEN : TextWin::W_CLOSED; t->plate = r.i() ? TextWin::W_OPEN : TextWin::W_CLOSED;
        t->name = r.s(); t->raw = r.s(); t->msgId = r.i(); int pos = r.i();
        // retype up to the saved position, leaving the window waiting at the marker
        t->pos = 0; t->clearText();
        while ((int)t->pos < pos) { size_t q = t->pos; Tok k = nextTok(t->raw, q); if (!k.type || (int)q > pos) break; float save = 1e9f; t->acc = save; if (k.type == 2) { t->pos = q; break; } 
            if (k.type == 1) { t->penX = 0; t->penY += 18; t->pos = q; } else if (k.type >= 5) { t->glyphs.push_back({k.cp, (int)t->penX, t->penY, {0, 0, 0, 255}}); t->penX += k.type == 5 ? 7 : 14; t->pos = q; } else t->pos = q; }
        t->text = (pos > 0 && t->pos < t->raw.size() + 1 && t->pos > 0) ? TextWin::T_WAIT : TextWin::T_IDLE;
    }
    for (int i = 0; i < 2; i++) { bg[i].file = r.s(); bg[i].alpha = r.i(); bg[i].x = (float)r.i(); bg[i].y = (float)r.i(); bg[i].state = bg[i].alpha ? 1 : 0;
        if (!bg[i].file.empty()) { bg[i].tex = g_gfx.load(bg[i].file); if (bg[i].tex) { bg[i].cx = (float)bg[i].tex->cx; bg[i].cy = (float)bg[i].tex->cy; } } }
    cur = r.i(); fadeAlpha = r.i(); fadeState = fadeAlpha ? 1 : 0;
    bgmState = r.i(); bgmName = r.s(); bgmVol = r.i(); ambState = r.i(); ambName = r.s(); ambVol = r.i();
    if (bgmState == 2) bgmPlay(bgmName, 0);
    if (ambState == 2) { g_audio.setVolume(CH_AMB, ambVol / 255.f); g_audio.play(CH_AMB, ambName, true); }
    stage.loadState(r.p);
}
