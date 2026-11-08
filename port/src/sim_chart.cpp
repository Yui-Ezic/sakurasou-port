// Job 0xa: spec square / task chart (spec_sim section 10).
#include "tr.h"
#include "sim_scenes.h"
#include "sim_ui.h"
#include <sstream>

static int s_chartMode = 0;
void simChartMode(int m) { s_chartMode = m; }
bool g_devChartFirst = false;   // dev (--ctl chartfirst): move the cursor to the first selectable task

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
    bool known(int t) const {   // FUN_0003f530: revealed, or (grey) planned in an earlier playthrough (carry-over)
        const TaskRT& r = gd().task[t]; return r.state > 0 || r.planFinished > 0;
    }
    void onEnter() override {
        mode = s_chartMode; s_chartMode = 0;
        bg = g_gfx.load("square_bg.txg"); parts = g_gfx.load("square_parts.txg"); line = g_gfx.load("square_line.txg");
        cursor = g_gfx.load("square_cursor.txg"); win = g_gfx.load("square_window.txg");
        hdr.set(3); hdr.start();   // 仕様スクエア in both modes (tutorial art BG_TUT02..06, 17)
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
    float target(int x, int y, bool isX) { return isX ? x * -CW : y * CH; }   // FUN_0003ad20: the cursor cell is centred
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
        if (g_devChartFirst) {
            g_devChartFirst = false;
            const auto& tb = simTables();
            for (int i = 0; i < (int)tb.tasks.size(); i++) if (taskSelectable(i)) { cx = tb.tasks[i].gridX; cy = tb.tasks[i].gridY; break; }
        }
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
    void cellPos(int gx, int gy, float& x, float& y) const { x = camX + gx * CW; y = camY - gy * CH; }
    void drawLink(float x0, float y0, float x1, float y1, Color c) {   // FUN_00040024 frame 1 of T_1f19a8: the orange bar, stretched
        float dx = x1 - x0, dy = y1 - y0, len = std::sqrt(dx * dx + dy * dy);
        if (len < 1 || !line) return;
        ui(*line, {0, 4, 28, 12}, (x0 + x1) / 2, (y0 + y1) / 2, 1, len / 28, 1, std::atan2(dy, dx) * 180 / (float)M_PI, c);
    }
    // FUN_0003f140 + FUN_0003f32c. State 0 cells are only drawn (grey) once planned in an earlier playthrough.
    void drawCell(int t, float x, float y) {
        const auto& d = simTables().tasks[t]; const TaskRT& r = gd().task[t];
        Color c = r.state == 0 ? argb(0xff808080) : Color{};
        int own = std::min(d.owner + 1, 7);
        bool spec = r.state == 1 || r.state == 2;     // still being specified: Sorata's card and pin
        q(*parts, T_1f1a50[spec ? 0 : own], x, y, 1, c);
        // FUN_00040364 at (-20,16): "全 N.N 日" total, "あと N.N 日" left, or 完成
        int lab = 0, v = 0; float dx = 26;
        switch (r.state) {
        case 0: case 3: v = d.workCost; break;
        case 1: v = d.planCost; break;
        case 2: lab = 1; v = d.planCost - r.planDone; dx = 30; break;
        case 4: lab = 1; v = d.workCost - r.workDone; dx = 30; break;
        default: lab = 2; break;
        }
        q(*parts, T_1f1cd8[lab], x - 20, y + 16, 1, c);
        if (lab < 2) {
            v = std::max(v, 0);
            q(*parts, T_1f1dd0[std::min(v / 3, 9)], x - 20 + dx, y + 16, 1, c);
            q(*parts, T_1f1dd0[(v % 3) * 3], x - 20 + dx + 16, y + 16, 1, c);
        }
        {   // a translation wider than the card is drawn smaller
            std::string nm = tr(d.name), l; std::stringstream ss(nm); float w = 0;
            while (std::getline(ss, l)) w = std::max(w, textWidth(l, 10));
            int cell = w > 88 ? std::max(7, (int)(10 * 88 / w)) : 10;
            simTextCentered(nm, x, y - 10, cell, {0, 0, 0, 255}, cell + 4);
        }
        if (r.state == 5) q(*parts, T_1f2150[0], x + 28, y - 12);                 // 済 stamp
    }
    void drawCellTop(int t, float x, float y) {   // FUN_0003f32c, drawn over the cursor
        const auto& d = simTables().tasks[t]; const TaskRT& r = gd().task[t];
        Color c = r.state == 0 ? argb(0xff808080) : Color{};
        int own = std::min(d.owner + 1, 7);
        bool spec = r.state == 1 || r.state == 2;
        // FUN_0003fc64 at (-40,28): 未着手 / 作業中 / LOCK
        if (r.state == 1) q(*parts, T_1f21a8[taskSelectable(t) ? 0 : 2], x - 40, y + 28);
        else if (r.state == 2 || r.state == 4) q(*parts, T_1f21a8[1], x - 40, y + 28);
        else if (r.state == 3) q(*parts, T_1f21a8[0], x - 40, y + 28);
        if (spec) q(*parts, T_1f22a0[0], x + 40, y - 30);                          // FUN_0003fdf4: 仕様 tag
        q(*parts, T_1f22f8[spec ? 0 : own], x, y + 30, 1, c);                       // FUN_00040b18: pin, star for key tasks
        if (d.key) q(*parts, T_1f22f8[8], x, y + 30, 1, c);
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
                if (c.state == 0 && c.planFinished) drawLink(x, y, x + l.dx * CW, y - l.dy * CH, argb(0xff808080));
                else if (c.state >= 1) drawLink(x, y, x + l.dx * CW, y - l.dy * CH, {});
            }
        }
        auto cells = [&](bool top) {
            for (int t = 0; t < n; t++) {
                if (!known(t)) continue;
                float x, y; cellPos(tb.tasks[t].gridX, tb.tasks[t].gridY, x, y);
                if (x < -300 || x > 300 || y < -200 || y > 200) continue;
                if (top) drawCellTop(t, x, y); else drawCell(t, x, y);
            }
        };
        cells(false);
        float x, y; cellPos(cx, cy, x, y);
        // FUN_0003af74: square_cursor.txg 24x24 ring as a 9-slice over (-52,37)-(52,-37), 8 px corners (strips at 0x1f0b70)
        const Tex& c = *cursor;
        for (int j = 0; j < 3; j++) for (int i = 0; i < 3; i++) {
            float cx0 = i == 0 ? -48 : i == 1 ? 0 : 48, cy0 = j == 0 ? 33 : j == 1 ? 0 : -33;
            float sx = i == 1 ? 88 / 8.f : 1, sy = j == 1 ? 58 / 8.f : 1;
            ui(c, {i * 8, j * 8, i * 8 + 8, j * 8 + 8}, x + cx0, y + cy0, 1, sx, sy);
        }
        cells(true);
        hdr.draw();
        int t = tid();
        if (win) qStrip(*win, T_1f2628[0], T_1f2628[1], 0, -144);
        if (t >= 0 && known(t)) {
            const TaskDef& d = tb.tasks[t]; const TaskRT& r = g.task[t];
            // FUN_0004103c with the panel at (0,-136): face (-200,46), description (-168,62), days left (-168,44)
            q(*win, T_1f26d8[r.state == 1 || r.state == 2 ? 0 : std::min(d.owner + 1, 7)], -200, -90);   // FUN_000412ac
            simText(d.desc, -168, -74, 14, {0, 0, 0, 255});
            char b[64];
            if (r.state >= 5) simText("完成", -168, -92, 14, {0, 0, 0, 255});
            else {
                int rest = r.state >= 3 ? d.workCost - r.workDone : d.planCost - r.planDone, tot = r.state >= 3 ? d.workCost : d.planCost;
                snprintf(b, sizeof b, tr("あと %s.%s/%s.%s日").c_str(), kDigitStr[rest / 3], kDigitStr[(rest % 3) * 3], kDigitStr[tot / 3], kDigitStr[(tot % 3) * 3]);
                simText(b, -168, -92, 14, {0, 0, 0, 255});
            }
        }
    }
};
}
std::unique_ptr<Job> makeChart() { return std::make_unique<Chart>(); }
