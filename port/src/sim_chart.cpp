// Job 0xa: spec square / task chart (spec_sim section 10).
#include "sim_scenes.h"
#include "sim_ui.h"

static int s_chartMode = 0;
void simChartMode(int m) { s_chartMode = m; }

namespace {
const float CW = 104, CH = 72;
const char* kHelpSel = "○決定　SELECTタスク一覧　　仕様を作成するタスクを決定して下さい。[LOCK]は他のタスクの状態や、期間の進行で解除されます";
const char* kHelpView = "×戻る　スクロール　　スタッフのタスクの仕様のつながり順に並んでいます。次に仕様作成できるタスクを確認できます";

struct Chart : Job {
    TexP bg, parts, line, cursor, win; PageHeader hdr;
    int mode = 0, st = 0, cx = 0, cy = 0, px = 0;
    float camX = 0, camY = 0;
    enum { FADE_IN, BROWSE, NOTASK, CONFIRM, FADE_OUT, SUB_OUT, SUB, SUB_IN };
    int tid() const { return simTables().cell(cx, cy); }
    bool known(int t) const {   // revealed, or opened in an earlier playthrough (carry-over)  // ASSUMED
        const TaskRT& r = gd().task[t]; return r.state > 0 || r.prereqOK > 0 || r.planFinished > 0;
    }
    void onEnter() override {
        mode = s_chartMode; s_chartMode = 0;
        bg = g_gfx.load("square_bg.txg"); parts = g_gfx.load("square_parts.txg"); line = g_gfx.load("square_line.txg");
        cursor = g_gfx.load("square_cursor.txg"); win = g_gfx.load("square_window.txg");
        hdr.set(mode ? 2 : 3); hdr.start();
        const auto& tb = simTables();
        // start on the task in progress, or the first selectable one
        int sel = gd().selectedTask;
        if (sel < 0) { sel = -sel - 1; if (sel >= (int)tb.tasks.size()) sel = 0; }
        if (mode == 1) for (int i = 0; i < (int)tb.tasks.size(); i++) if (taskSelectable(i)) { sel = i; break; }
        cx = tb.tasks[sel].gridX; cy = tb.tasks[sel].gridY;
        camX = target(cx, cy, true); camY = target(cx, cy, false);
        g_trans.style(0); g_trans.reveal(10);
        if (mode == 1) { g_audio.play(CH_BGM, "SK_BGM_03.adx", true, 10); g_audio.play(CH_VOICE, "V_G04_MID_0060.hca", false); }
        g_help.show(mode ? kHelpSel : kHelpView);
    }
    float target(int x, int y, bool isX) {
        const auto& tb = simTables();
        return isX ? std::clamp(x * -CW, (tb.tcols - 1) * -CW, 0.f) : std::clamp(y * CH, 0.f, (tb.trows - 1) * CH);
    }
    void onResume() override { if (st == SUB) { st = SUB_IN; g_trans.style(0); g_trans.reveal(10); g_help.show(kHelpSel); } }
    void leave() { st = FADE_OUT; g_trans.style(0); g_trans.cover(10); g_help.hide(); if (mode == 1) g_audio.stop(CH_BGM, 10); }
    void move() {
        const auto& tb = simTables(); int ox = cx, oy = cy;
        if (g_input.isRepeat(BTN_LEFT)) cx = std::max(cx - 1, 0);
        if (g_input.isRepeat(BTN_RIGHT)) cx = std::min(cx + 1, tb.tcols - 1);
        if (g_input.isRepeat(BTN_UP)) cy = std::max(cy - 1, 0);
        if (g_input.isRepeat(BTN_DOWN)) cy = std::min(cy + 1, tb.trows - 1);
        if (cx != ox || cy != oy) g_audio.playCue(0x2a);
    }
    void onUpdate() override {
        hdr.update();
        float tx = target(cx, cy, true), ty = target(cx, cy, false);
        camX += (tx - camX) * 0.5f; camY += (ty - camY) * 0.5f;
        if (std::abs(tx - camX) < 1) camX = tx;
        if (std::abs(ty - camY) < 1) camY = ty;
        GD& g = gd();
        switch (st) {
        case FADE_IN:
            if (g_trans.clear()) {
                if (mode == 1 && !anySelectable()) { st = NOTASK; g_popup.open("現在作成できる仕様がありません", false); }
                else st = BROWSE;
            } break;
        case NOTASK:
            if (!g_popup.isOpen()) { int t = tid(); if (t >= 0) g.selectedTask = -(t + 1); leave(); }
            break;
        case BROWSE:
            if (mode == 0) {
                if (g_input.isPressed(BTN_CROSS)) { g_audio.playCue(0x1f); leave(); break; }
                move();
            } else {
                if (g_input.isPressed(BTN_SELECT)) { st = SUB_OUT; g_help.hide(); g_trans.style(0); g_trans.cover(10); break; }
                if (g_input.isPressed(BTN_CIRCLE)) {
                    int t = tid();
                    if (t >= 0 && taskSelectable(t)) {
                        g_audio.playCue(0x29); g_audio.play(CH_VOICE, "V_G04_SRT_0050.hca", false);
                        st = CONFIRM; g_popup.open("選択した仕様を作成しますか？", true);
                    } else g_audio.playCue(0x28);
                    break;
                }
                move();
            }
            break;
        case CONFIRM:
            if (!g_popup.isOpen()) { if (g_popup.yes()) { g.selectedTask = tid(); leave(); } else st = BROWSE; }
            break;
        case SUB_OUT: if (g_trans.covered()) { siblingPush(0xd); st = SUB; } break;
        case SUB_IN: if (g_trans.clear()) st = BROWSE; break;
        case FADE_OUT: if (g_trans.covered()) { st = 99; selfPop(); } break;
        }
    }
    void cellPos(int gx, int gy, float& x, float& y) const { x = camX + gx * CW; y = camY - gy * CH + 20; }
    void drawLink(float x0, float y0, float x1, float y1, Color c) {
        float dx = x1 - x0, dy = y1 - y0, len = std::sqrt(dx * dx + dy * dy);
        if (len < 1 || !line) return;
        ui(*line, {0, 20, 28, 28}, (x0 + x1) / 2, (y0 + y1) / 2, 1, len / 28, 1, std::atan2(dy, dx) * 180 / (float)M_PI, c);
    }
    void drawCell(int t, float x, float y) {
        const auto& d = simTables().tasks[t]; const TaskRT& r = gd().task[t];
        bool sel = taskSelectable(t), grey = r.state == 0;
        Color c = grey ? argb(0xff808080) : Color{};
        int spr = r.state == 1 || r.state == 2 ? 0 : d.owner + 1;
        q(*parts, T_1f1a50[std::min(spr, 7)], x, y, 1, c);
        simTextCentered(d.name, x, y - 10, 10, {0, 0, 0, 255}, 14);
        q(*parts, T_1f22f8[d.owner], x - 40, y + 28);
        if (d.key) q(*parts, T_1f22f8[8], x + 40, y - 28);
        if (r.state == 5) q(*parts, T_1f2150[0], x + 28, y - 12);
        else if (r.state == 2) q(*parts, T_1f22a0[0], x - 20, y + 16);
        else if (r.state >= 3) q(*parts, T_1f21a8[1], x - 20, y + 16);
        else if (r.state == 1 && sel) q(*parts, T_1f21a8[0], x - 20, y + 16);
        else q(*parts, T_1f21a8[2], x - 20, y + 16);
    }
    void onDraw() override {
        const auto& tb = simTables(); GD& g = gd();
        for (int j = 0; j < 5; j++) for (int i = 0; i < 9; i++) ui(*bg, {0, 0, 64, 64}, -240 + 32 + i * 64.f, 136 - 32 - j * 64.f);
        int n = (int)tb.tasks.size();
        for (int t = 0; t < n; t++) {       // FUN_0003ecd4
            if (!known(t)) continue;
            float x, y; cellPos(tb.tasks[t].gridX, tb.tasks[t].gridY, x, y);
            if (x < -360 || x > 360 || y < -240 || y > 240) continue;
            for (auto& l : tb.tasks[t].child) {
                if (l.task < 0) break;
                const TaskRT& c = g.task[l.task];
                if (c.state == 0 && c.prereqOK) drawLink(x, y, x + l.dx * CW, y - l.dy * CH, argb(0xff808080));
                else if (c.state >= 1) drawLink(x, y, x + l.dx * CW, y - l.dy * CH, {});
            }
        }
        for (int t = 0; t < n; t++) {
            if (!known(t)) continue;
            float x, y; cellPos(tb.tasks[t].gridX, tb.tasks[t].gridY, x, y);
            if (x < -300 || x > 300 || y < -200 || y > 200) continue;
            drawCell(t, x, y);
        }
        float x, y; cellPos(cx, cy, x, y);
        // ASSUMED: square_cursor.txg as a 9-slice over the cell
        const float hw = 50, hh = 34; const Tex& c = *cursor;
        ui(c, {0, 0, 8, 8}, x - hw, y + hh); ui(c, {24, 0, 32, 8}, x + hw, y + hh);
        ui(c, {0, 24, 8, 32}, x - hw, y - hh); ui(c, {24, 24, 32, 32}, x + hw, y - hh);
        ui(c, {8, 0, 24, 8}, x, y + hh, 1, (hw * 2 - 8) / 16, 1); ui(c, {8, 24, 24, 32}, x, y - hh, 1, (hw * 2 - 8) / 16, 1);
        ui(c, {0, 8, 8, 24}, x - hw, y, 1, 1, (hh * 2 - 8) / 16); ui(c, {24, 8, 32, 24}, x + hw, y, 1, 1, (hh * 2 - 8) / 16);
        hdr.draw();
        int t = tid();
        if (win) qStrip(*win, T_1f2628[0], T_1f2628[1], 0, -144);
        if (t >= 0 && known(t)) {
            const TaskDef& d = tb.tasks[t]; const TaskRT& r = g.task[t];
            q(*win, T_1f26d8[d.owner + 1 < 8 ? d.owner + 1 : 7], -208, -88);
            simText(stripNewlines(d.name), -184, -74, 14, {0, 0, 0, 255});
            simText(d.desc, -184, -94, 10, {40, 40, 40, 255}, 12);
            char b[64];
            if (r.state >= 5) simText("完成", 170, -74, 14, {220, 30, 30, 255});
            else {
                int rest = r.state >= 3 ? d.workCost - r.workDone : d.planCost - r.planDone, tot = r.state >= 3 ? d.workCost : d.planCost;
                snprintf(b, sizeof b, "あと %s.%s/%s.%s日", kDigitStr[rest / 3], kDigitStr[(rest % 3) * 3], kDigitStr[tot / 3], kDigitStr[(tot % 3) * 3]);
                simText(b, 100, -74, 10, {0, 0, 0, 255});
            }
        }
    }
};
}
std::unique_ptr<Job> makeChart() { return std::make_unique<Chart>(); }
