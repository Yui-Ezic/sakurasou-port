// Job 8: action choice (spec_sim section 4).
#include "sim_scenes.h"
#include "sim_ui.h"
#include <cstdio>
#include <cstring>

namespace {
const char* kHelp1 = "[仕様作成]仕様の作成　［ミーティング］モチベーション上昇、悩み解決　［お世話］全員健康回復　［お休み］相手の健康大回復";
const char* kHelp0 = "[仕様作成]仕様の作成　［ミーティング］モチベーション上昇、悩み解決　［お世話］全員健康回復";

struct Card {
    enum { IDLE, MENU, LIST_M, LIST_R, BACK, ADVANCE };
    int st = IDLE;
    int kind = -1, target = -1;     // shown in the slot
    int cursor = -1, listCur = -1;
    int grow = 0;                   // header y-scale, 0..5
    bool shown = false, active = false;
    int menuGrow = 0;
};

struct Notice { int type, sub, arg; };

struct Choice : Job, NativeHandler {
    TexP on, off; PageHeader hdr; DeadlineWidget dead; Toast toast;
    Card card[3]; int mode = 0, cur = 0, st = 0, f = 0;
    std::vector<Notice> notices; size_t nextNotice = 0;
    Vm vm; bool restored = false; int helpX = 0;

    int rows() const { return mode == 1 ? 4 : 3; }
    bool rowEnabled(int r) const { return mode != 2 || r == 0; }

    void onEnter() override {
        on = g_gfx.load("action_choice_parts_on.txg"); off = g_gfx.load("action_choice_parts.txg");
        mode = g_actionChoiceArg;
        hdr.set(0); hdr.start(); dead.init(198, 107); toast.init();
        vm.handler = this; vm.load(g_fs.read("script_system_status.ssb"), "script_system_status.ssb");
        // S0 runs synchronously on the first update, after a possible loadState
    }
    void loadState(const uint8_t* p, size_t n) override {   // FUN_00037a50
        if (n >= 4) memcpy(&mode, p, 4);
        restored = true;
        card[0].active = true; cur = 0;
        for (auto& c : card) { c.shown = true; c.grow = 5; }
        st = 4; g_trans.style(0); g_trans.reveal(10); g_audio.play(CH_BGM, "SK_BGM_02.adx", true, 10);
    }
    void saveState(Bytes& o) override { o.insert(o.end(), (uint8_t*)&mode, (uint8_t*)&mode + 4); }

    int native(Vm& v, int id, int32_t& ret) override {
        ret = 0;
        if (id < 59) return baseNative(v, id, ret);
        GD& g = gd(); int c = v.arg(0), a = id >= 75 && id <= 78 ? v.arg(1) : 0;
        switch (id) {
        case 75: {
            int old = g.cs[c].state; g.cs[c].state = a;
            if (a == 1) notices.push_back({0, 0, c});
            else if (a == 3) notices.push_back({0, 2, c});
            else if (a == 2) notices.push_back({0, 1, c});
            else if (old == 1 && a == 0) notices.push_back({0, 3, c});
            else if (old == 3 && a == 0) notices.push_back({0, 4, c});
            break; }
        case 76: g.cs[c].sickTurns = a; break;
        case 77: g.cs[c].moodIcon = a; break;
        case 78: g.cs[c].healthIcon = a; break;
        }
        return 0;
    }
    void runStatus() {
        notices.clear();
        int t = gd().turn, sub = t == 0x0e ? 0 : t == 0x1d ? 1 : t == 0x2b ? 2 : t == 0x3b ? 3 : t == 0x4e ? 4 : -1;
        if (sub >= 0) notices.push_back({1, sub, 0});
        for (int i = 0; i < 20000000 && !vm.halted(); i++) vm.run();
        st = 1; g_trans.style(0); g_trans.reveal(10);
        g_audio.play(CH_BGM, "SK_BGM_02.adx", true, 10);
    }
    void focus(int i) { for (int k = 0; k < 3; k++) card[k].active = k == i; card[i].st = Card::IDLE; }
    void cardTick() {
        for (auto& c : card) {
            if (c.shown && c.grow < 5) c.grow++;
            bool open = c.st == Card::MENU || c.st == Card::LIST_M || c.st == Card::LIST_R;
            if (open && c.menuGrow < 5) c.menuGrow++;
            if (!open && c.menuGrow > 0) c.menuGrow--;
        }
        hdr.update(); toast.update(); helpX++;
    }
    // FUN_00030b34 dispatch; cancelling/advancing is reported through Card.st
    void cardInput(Card& c) {
        auto wrap = [](int v, int n) { return ((v % n) + n) % n; };
        switch (c.st) {
        case Card::IDLE:
            if (g_input.isPressed(BTN_CIRCLE)) {
                c.st = Card::MENU; if (c.cursor < 0) c.cursor = 0; c.kind = c.cursor; c.target = -1;
                g_audio.playCue(0x29);
            } else if (g_input.isPressed(BTN_CROSS)) { c.st = Card::BACK; g_audio.playCue(0x1f); }
            break;
        case Card::MENU:
            if (g_input.isPressed(BTN_CROSS)) { c.st = Card::IDLE; c.kind = -1; c.target = -1; c.cursor = -1; g_audio.playCue(0x1f); break; }
            if (g_input.isPressed(BTN_CIRCLE)) {
                if (!rowEnabled(c.cursor)) { g_audio.playCue(0x28); break; }
                g_audio.playCue(0x29);
                if (c.cursor == 1) { c.st = Card::LIST_M; c.listCur = -1; }
                else if (c.cursor == 3) { c.st = Card::LIST_R; c.listCur = -1; }
                else c.st = Card::ADVANCE;
                break;
            }
            if (g_input.isPressed(BTN_UP)) { c.cursor = c.cursor < 0 ? rows() - 1 : wrap(c.cursor - 1, rows()); c.kind = c.cursor; g_audio.playCue(0x2a); }
            else if (g_input.isPressed(BTN_DOWN)) { c.cursor = c.cursor < 0 ? 0 : wrap(c.cursor + 1, rows()); c.kind = c.cursor; g_audio.playCue(0x2a); }
            break;
        case Card::LIST_M: case Card::LIST_R:
            if (g_input.isPressed(BTN_CROSS)) { c.st = Card::MENU; c.target = -1; g_audio.playCue(0x1f); break; }
            if (g_input.isPressed(BTN_CIRCLE)) {
                if (c.listCur < 0) { c.listCur = 0; c.target = 0; g_audio.playCue(0x2a); }
                else { c.st = Card::ADVANCE; g_audio.playCue(0x29); }
                break;
            }
            if (g_input.isPressed(BTN_UP)) { c.listCur = c.listCur < 0 ? 5 : wrap(c.listCur - 1, 6); c.target = c.listCur; g_audio.playCue(0x2a); }
            else if (g_input.isPressed(BTN_DOWN)) { c.listCur = c.listCur < 0 ? 0 : wrap(c.listCur + 1, 6); c.target = c.listCur; g_audio.playCue(0x2a); }
            break;
        }
    }
    void pushSub(int next, int style) { st = next; g_trans.style(style); g_trans.cover(10); }
    void onResume() override {
        if (st >= 100) { st -= 100; g_trans.style(st == 7 ? 2 : 0); g_trans.reveal(10); st = 50; }   // back from a sub-job
    }
    void onUpdate() override {
        if (!restored && st == 0) { runStatus(); return; }
        switch (st) {
        case 1: cardTick(); if (g_trans.clear()) { st = 2; f = 0; } break;
        case 2:
            if (f == 0 || f == 20 || f == 40) card[f / 20].shown = true;
            cardTick();
            if (++f >= 60) st = 3;
            break;
        case 3:
            cardTick();
            if (!toast.active()) {
                if (nextNotice >= notices.size()) { cur = 0; focus(0); st = 4; }
                else showNotice(notices[nextNotice++]);
            }
            break;
        case 4: main4(); break;
        case 5:
            cardTick();
            if (!g_popup.isOpen()) {
                if (g_popup.yes()) {
                    for (int i = 0; i < 3; i++) gd().plan[i] = {card[i].kind, card[i].target};
                    st = 6; g_trans.style(0); g_trans.cover(10);
                } else { cur = 2; focus(2); st = 4; }
            }
            break;
        case 6: cardTick(); if (g_trans.covered()) { st = 99; selfPop(); } break;
        case 7: case 10: case 13: case 16:
            cardTick();
            if (g_trans.covered()) {
                int job = st == 7 ? 0x10 : st == 10 ? 0x1a : st == 13 ? 0x0d : 0x1f;
                siblingPush(job); st += 100;
            }
            break;
        case 50: cardTick(); if (g_trans.clear()) st = 4; break;
        }
    }
    void showNotice(const Notice& n) {
        static const char* t0[5] = {"%sがヤル気最高潮になりました", "%sが悩み始めました", "%sが作業不可になりました", "%sがヤル気最高潮から戻りました", "%sが作業不可から回復しました"};
        static const char* t1[5] = {"α版制作期間タスク\nタスクの[LOCK]が解除されました", "α版後半制作期間タスク\nタスクの[LOCK]が解除されました", "β版制作期間タスク\nタスクの[LOCK]が解除されました", "β版後半制作期間タスク\nタスクの[LOCK]が解除されました", "マスター版制作期間タスク\nタスクの[LOCK]が解除されました"};
        char b[256];
        if (n.type == 0) { snprintf(b, sizeof b, t0[n.sub], kCharName[n.arg]); toast.show(b, n.arg + 2, 0, 0, false); }
        else toast.show(t1[n.sub], 0, 0, 0, false);
    }
    void main4() {
        bool l = g_input.isHeld(BTN_L);
        if (!l && g_input.isPressed(BTN_TRIANGLE)) { takeSnapshot(); pushSub(7, 2); cardTick(); return; }
        if (g_input.isPressed(BTN_SELECT)) { g_audio.playCue(0x29); pushSub(13, 0); cardTick(); return; }
        if (l && g_input.isPressed(BTN_TRIANGLE)) { takeSnapshot(); shiftHistory(); g_audio.playCue(0x29); cardTick(); return; }
        if (l && g_input.isPressed(BTN_SQUARE)) { g_audio.playCue(0x29); pushSub(10, 0); cardTick(); return; }
        Card& c = card[cur];
        cardInput(c);
        if (c.st == Card::ADVANCE) {
            c.active = false; c.st = Card::IDLE;
            cur++;
            if (cur > 2) { st = 5; g_popup.open("これでよろしいですか？", true); }
            else focus(cur);
        } else if (c.st == Card::BACK) {
            c.st = Card::IDLE;
            if (cur >= 1) { c.active = false; cur--; focus(cur); }
        }
        cardTick();
    }

    void onDraw() override {
        drawCommonBg();
        hdr.draw(); dead.draw();
        drawStatus();
        // plan window, translated (0,50)
        q(*on, T_1ee5b0[0], 0, 50);
        for (int i = 0; i < 2; i++) q(*on, T_1ee608[0], -80 + i * 160.f, 50);
        for (int i = 0; i < 3; i++) drawCard(i, -160 + i * 160.f, 50);
        for (int i = 0; i < 3; i++) if (card[i].st != Card::IDLE || card[i].menuGrow > 0) drawMenu(i, -160 + i * 160.f, 50);
        // help line at (0,104), ASSUMED ticker rendering of FUN_0000d840
        std::string h = mode == 1 ? kHelp1 : kHelp0;
        float span = 220 + textWidth(h, 10);
        SDL_Rect clip{140, 26, 220, 12};   // ASSUMED: between the page header and the deadline widget
        SDL_RenderSetClipRect(g_gfx.renderer(), &clip);
        simText(h, -100 + std::fmod((float)helpX, span) * -1 + 220, 104 + 5, 10, {60, 50, 40, 255});
        SDL_RenderSetClipRect(g_gfx.renderer(), nullptr);
        toast.draw();
    }
    void drawCard(int i, float x, float y) {
        Card& c = card[i];
        if (!c.shown) return;
        float sy = c.grow / 5.f;
        // slot picture, 120x28 pairs of quads at v=32*row (0 = 未選択)
        int row = c.kind + 1;
        ui(*on, {0, row * 32, 120, row * 32 + 28}, x, y, 1, 1, sy);
        if ((c.kind == 1 || c.kind == 3) && c.target >= 0) q(*on, T_1eeca0[c.target], x + 46, y, 1, {}, 1, sy);
        q(*on, T_1ee660[i * 2 + (c.active ? 1 : 0)], x - 68, y + 22, 1, {}, 1, sy);
        if (c.active) for (int k = 0; k < 6; k += 2) qStrip(*on, T_1ee848[k], T_1ee848[k + 1], x, y, 1, {}, 1, sy);
    }
    void drawMenu(int i, float x, float y) {
        Card& c = card[i];
        float sy = c.menuGrow / 5.f; y -= 22;
        const SQ* W = T_1eee88 + (rows() == 4 ? 4 : 0);
        qStrip(*on, W[0], W[1], x, y, 1, {}, 1, sy); qStrip(*on, W[2], W[3], x, y, 1, {}, 1, sy);
        for (int r = 0; r < rows(); r++) {
            float ry = y + (-20 - r * 32.f) * sy;
            bool en = rowEnabled(r), sel = r == c.cursor;
            Color bg = argb(!en ? 0xff808080 : sel ? 0xffc0ffff : 0xffc0c0c0);
            qStrip(*on, T_1ef118[0], T_1ef118[1], x, ry, 1, bg, 1, sy);
            q(*on, T_1ef1c8[r], x, ry, 1, argb(!en ? 0xff404040 : sel ? 0xff000000 : 0xff808080), 1, sy);
            if (sel) qStrip(*on, T_1ef310[0], T_1ef310[1], x, ry, 1, {}, 1, sy);
        }
        if (c.st == Card::LIST_M || c.st == Card::LIST_R) {
            float lx = x + 16, ly = y - 60 + 22;    // +22: the list is offset from the card, not the menu
            for (int k = 0; k < 6; k += 2) qStrip(*on, T_1ef3c0[k], T_1ef3c0[k + 1], lx, ly);
            for (int r = 0; r < 6; r++) {
                float ry = ly + 80 - r * 32.f; bool sel = r == c.listCur;
                qStrip(*on, T_1ef5b0[0], T_1ef5b0[1], lx, ry, 1, argb(sel ? 0xffc0ffff : 0xffc0c0c0));
                q(*on, T_1ef660[r], lx, ry, 1, argb(sel ? 0xff000000 : 0xff808080));
                if (sel) qStrip(*on, T_1ef848[0], T_1ef848[1], lx, ry);
            }
        }
    }
    void drawStatus() {
        GD& g = gd(); const auto& tb = simTables();
        for (int i = 0; i < 6; i++) {
            float x = (i & 1) * 232.f - 116, y = 2 - (i / 2) * 46.f;
            for (int k = 0; k < 6; k += 2) qStrip(*off, T_1ef918[k], T_1ef918[k + 1], x, y);
            const CharState& s = g.cs[i];
            q(*off, T_1efb08[g.available[i] ? i : 6], x - 84, y);
            q(*off, T_1efd40[s.state == 2 ? 3 : s.moodIcon & 3], x - 48, y);
            if (s.state == 3) {
                q(*off, T_1efe88[3], x - 12, y);
                int v = s.sickTurns;     // FUN_00036960: turns left on the icon
                if (v < 10) { q(*off, T_1efe88[4 + v % 10], x - 16, y + 12 - 12); q(*off, T_1efe88[14], x - 7, y); }
                else { q(*off, T_1efe88[4 + (v / 10) % 10], x - 21, y); q(*off, T_1efe88[4 + v % 10], x - 10, y); q(*off, T_1efe88[14], x - 1, y); }
            } else q(*off, T_1efe88[s.healthIcon & 3], x - 12, y);
            const CharQueue& cq = g.queue[i];
            int rest = cq.count - cq.doneIndex, n = std::min(rest, 4);
            for (int k = n - 1; k >= 0; k--) q(*off, T_1f0340[i], x + 36 + k * 12.f, y);
            if (rest > 4) q(*off, T_1f0528[0], x + 36 + 68, y);
            if (rest > 0) simTextCentered(stripNewlines(tb.tasks[cq.task[cq.doneIndex]].shortName), x + 36, y - 3, 10, {0, 0, 0, 255}, 10);
            if (s.state == 2) ui(*off, {80, 136, 152, 168}, x + 44, y);
            else if (s.state == 3) ui(*off, {0, 176, 72, 208}, x + 44, y);
        }
    }
};
}
std::unique_ptr<Job> makeChoice() { return std::make_unique<Choice>(); }
