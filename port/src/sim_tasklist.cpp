// Job 0xd: task list (spec_sim section 11). Follows the original: ctor FUN_00076fa8, init FUN_00077770,
// update FUN_000773a0 / FUN_000768d8, draw FUN_000774a0. One row per character (30 px apart), its queued tasks as
// chips along the row (50 px apart), "remaining days" badges, the deadline marker before the first task that cannot
// be finished in time, and a panel with the cursor task.
#include "tr.h"
#include "sim_scenes.h"
#include "sim_ui.h"
#include "sim_tasklist_sprites.h"
#include <sstream>

namespace {
const char* kHelp = "×戻る　�スクロール　　スタッフのタスクが作業順に並んでいます。提出期限に間に合わないタスクを確認できます";

// a chip or badge appearing: wait, then fade in while shrinking by 0.3 (FUN_00073ed4 / FUN_000744d4)
struct Pop {
    int st = 0, t = 0; float scale = 1; int alpha = 0;
    void start(int delay, float s) { st = 1; t = delay; scale = s; alpha = 0; }
    void tick() {
        if (st == 1) { if (--t < 0) { t = 0; st = 2; } }
        else if (st == 2) { alpha += 20; scale -= 0.025f; if (++t > 11) { t = 0; alpha = 255; st = 3; } }
    }
};
struct Chip { Pop pop; bool on = true; };
struct Row {
    float y = 75; int count = 0, days = 0, overDays = 0; bool over = false;
    Chip chip[32]; Pop badge;
};

struct TaskList : Job {
    TexP bg, win, count, name, psrts, cursor; PageHeader hdr; DeadlineWidget dead;
    Row row[6]; int rows = 6, sel = 0, cur = 1, st = 0, scrollMode = 0, out = 0, cursorAlpha = 0;
    float x0 = -50, panelY = -160, cursorX = 0;

    const CharQueue& queue(int c) const { return gd().queue[c]; }
    float chipX(int i) const { return x0 + i * 50.f; }   // FUN_00074ce0 with i = cursor - 1

    void onEnter() override {
        bg = g_gfx.load("task_bg.txg"); win = g_gfx.load("task_window.txg"); count = g_gfx.load("task_count.txg");
        name = g_gfx.load("task_name.txg"); psrts = g_gfx.load("task_psrts.txg"); cursor = g_gfx.load("task_cursor.txg");
        hdr.set(2); hdr.start();
        dead.init(190, 116);
        if (gd().routeA < 2) { rows = 5; scrollMode = -1; }     // Otoha is not in the team yet
        for (int c = 0; c < 6; c++) setupRow(c);
    }
    // FUN_00075a48: remaining days, whether they fit before the deadline, and which chips still do
    void setupRow(int c) {
        Row& r = row[c]; const GD& g = gd(); const auto& tb = simTables(); const CharQueue& q = queue(c);
        r.count = std::clamp(q.count, 0, 32);
        double units = 0;
        for (int i = 0; i < r.count; i++) {
            int t = q.task[i];
            if (g.task[t].state != 5) units += tb.tasks[t].workCost - g.task[t].workDone;
        }
        if (units != 0) units = units / 3.0 + 0.9;
        r.days = (int)units;
        int phase = g.routeA, turn = g.turn, left = 0;
        bool known = true;
        if (phase <= 2) left = 14 - turn;
        else if (phase <= 4) left = 43 - turn;
        else if (phase <= 6) left = 78 - turn;
        else if (phase == 7) left = 92 - turn;
        else known = false;
        r.over = known ? left < r.days : r.days > 0;
        r.overDays = r.days - left;
        double budget = left * 3.0;
        for (int i = 0; i < r.count; i++) {
            int t = q.task[i]; bool on = true;
            if (r.over && g.task[t].state != 5) { budget -= tb.tasks[t].workCost - g.task[t].workDone; on = budget >= 0; }
            r.chip[i].on = on;
            r.chip[i].pop.start((c + i) * 5, (c == 0 && i == 0 ? 1.f : 0.6f) + 0.3f);
        }
        r.badge.start(c * 5, (c == 0 ? 1.f : 0.75f) + 0.3f);
    }
    void onResume() override { g_trans.style(0); g_trans.reveal(10); }
    void moved() { g_audio.playCue(0x2a); }
    void clampCursor() { int n = row[sel].count; if (n < cur) cur = n ? n : 1; }
    void onUpdate() override {
        hdr.update();
        if (panelY < -107) { panelY += 3; if (panelY >= -107) panelY = -107; }   // FUN_00073128
        if (out) { if (g_trans.covered()) { g_help.hide(); out = 2; selfPop(); } return; }
        if (st == 0) { st = 1; g_help.show(kHelp); return; }
        if (st == 1) {
            bool done = true;
            for (int c = 0; c < 6; c++) {
                row[c].badge.tick(); done &= row[c].badge.st == 3;
                for (int i = 0; i < row[c].count; i++) { row[c].chip[i].pop.tick(); done &= row[c].chip[i].pop.st == 3; }
            }
            if (done) st = 2;
            return;
        }
        // FUN_000768d8 state 2
        if (g_input.isRepeat(BTN_UP)) {
            if (sel > 0) {
                moved(); sel--; clampCursor();
                if (scrollMode == 1 && sel < 2) scrollMode = 0;
                while (chipX(cur - 1) < -50) x0 += 50;
            }
        } else if (g_input.isRepeat(BTN_DOWN)) {
            if (sel < rows - 1) {
                moved(); sel++; clampCursor();
                if (scrollMode == 0 && sel > 3) scrollMode = 1;
                while (chipX(cur - 1) < -50) x0 += 50;
            }
        } else if (g_input.isRepeat(BTN_RIGHT)) {
            if (cur < row[sel].count) { moved(); cur++; if (cursorX > 185) x0 -= 50; }
        } else if (g_input.isRepeat(BTN_LEFT)) {
            if (cur > 1) { moved(); cur--; if (cursorX < -45) x0 += 50; }
        } else if (g_input.isPressed(BTN_CROSS)) {
            g_audio.playCue(0x1f); g_trans.style(0); g_trans.cover(10); out = 1;
        }
    }

    // a sprite record with rotation (radians, counter-clockwise), for the two rotated pieces
    void rot(const Tex& t, const SQ& s, float x, float y, float rad) {
        Spr p{(int)std::lround(s.u0 * t.w), (int)std::lround(s.v0 * t.h), (int)std::lround(s.u1 * t.w), (int)std::lround(s.v1 * t.h)};
        ui(t, p, x, y, 1, (s.x1 - s.x0) / (p.x1 - p.x0), (s.y0 - s.y1) / (p.y1 - p.y0), rad * 180 / (float)M_PI);
    }
    static Color alphaOf(Color c, int a) { c.a = (uint8_t)std::clamp(a, 0, 255); return c; }

    void onDraw() override {
        const SQ* T = T_task;
        // FUN_00072ae4: cork, then the grid paper scrolled with the chips
        for (int i = 0; i < 8; i++) for (int j = 0; j < 5; j++) q(*bg, T[0], -240 + i * 64.f, 136 - j * 64.f);
        float gx = std::max(x0 - 24, -284.f);
        for (int i = 0; i < 17; i++) for (int j = 0; j < 5; j++) q(*bg, T[1], gx + i * 32.f, 78.5f - j * 31.f);
        hdr.draw();
        // FUN_000733ec: deadline note and its tape, then the deadline text (FUN_00015f6c)
        for (int i = 0; i < 3; i++) q(*count, T[11 + i], i * 55.8f + 119.2f, 110, 1, {}, i == 1 ? 13.f : 1.f);
        rot(*count, T[14], 125, 142, 0.7f);
        dead.draw();
        drawGrid();
        // FUN_00072fd0: bottom panel band and the selected character's face
        q(*win, T[8], -224, panelY); q(*win, T[9], -4, panelY, 1, {}, 108); q(*win, T[10], 220, panelY);
        q(*win, T[2 + sel], -175, panelY + 12);
        drawPanelText();
    }
    void drawGrid() {   // FUN_0007610c
        const SQ* T = T_task; const auto& tb = simTables(); const GD& g = gd();
        float base = 75 + (scrollMode == 1 ? 30.f : 0.f);
        for (int c = 0; c < rows; c++) { row[c].y = base - c * 30.f; drawChips(c); }
        // the orange frame behind the names (9-slice)
        for (int i = 0; i < 3; i++) {
            float x = -225 + i * 50.f, sx = i == 1 ? 12.f : 1.f;
            q(*name, T[15 + i], x, 93.5f, 1, {}, sx);
            q(*name, T[18 + i], x, 15, 1, {}, sx, 19);
            q(*name, T[21 + i], x, -62.5f, 1, {}, sx);
        }
        for (int c = 0; c < rows; c++) {   // FUN_00074718: name plates
            float y = row[c].y;
            if (!(y < 80 && y > -70)) continue;
            for (int i = 0; i < 3; i++) q(*name, T[24 + i], -218 + i * 43.f, y, 1, c == sel ? argb(0xffbaffff) : Color{}, i == 1 ? 10.f : 1.f);
            q(*name, T[30 + c], -175, y, 1, argb(c == sel ? 0xff000000 : 0xff7e7e7e));
        }
        for (int i = 0; i < 3; i++) q(*name, T[27 + i], -218 + i * 43.f, row[sel].y, 1, {}, i == 1 ? 10.f : 1.f);
        if (scrollMode != -1) {   // FUN_00073484 / FUN_000736e8: the frame's end with the scroll arrow
            float y = scrollMode == 0 ? -55.f : 85.f;
            for (int i = 0; i < 3; i++) q(*name, T[18 + i], -225 + i * 50.f, y, 1, {}, i == 1 ? 12.f : 1.f, 2);
            if (scrollMode == 0) q(*name, T[36], -175, -58);
            else rot(*name, T[36], -175, 88, 3.14f);
        }
        for (int c = 0; c < rows; c++) {   // badges; the bottom rows' badges would overlap the arrow
            if (scrollMode == 0 && c >= 4) continue;
            if (scrollMode == 1 && c <= 1) continue;
            drawBadge(c);
        }
        // cursor chip: its short name (FUN_000e654c into record 79), then the frame (FUN_00073940)
        const Row& r = row[sel];
        if (r.count <= 0) return;
        int i = std::clamp(cur - 1, 0, r.count - 1);
        const Chip& ch = r.chip[i];
        float x = chipX(i), y = r.y, s = ch.pop.st == 3 ? 1.f : ch.pop.scale;
        if (y < 80 && y > -70) {
            std::string nm = tr(tb.tasks[queue(sel).task[i]].shortName), l; std::stringstream ss(nm); float w = 0;
            while (std::getline(ss, l)) w = std::max(w, textWidth(l, 10));
            int cell = w > 52 ? std::max(6, (int)(10 * 52 / w)) : 10;
            simTextCentered(nm, x, y - 2, (int)std::lround(cell * s), alphaOf({0, 0, 0, 255}, ch.pop.alpha), (int)std::lround(11 * s * cell / 10));
        }
        if (ch.pop.st == 3) {
            cursorX = x; cursorAlpha = std::min(cursorAlpha + 40, 255);
            float a = cursorAlpha / 255.f;
            for (int k = 0; k < 3; k++) {
                q(*cursor, T[71 + k], x - 28 + k * 28.f, y + 19, a, {}, k == 1 ? 6.f : 1.f);
                q(*cursor, T[76 + k], x - 28 + k * 28.f, y - 18, a, {}, k == 1 ? 6.f : 1.f);
            }
            for (int k = 0; k < 2; k++) q(*cursor, T[74 + k], x - 28 + k * 56.f, y + 0.5f, a, {}, 1, 3.6f);
        }
        (void)g;
    }
    void drawChips(int c) {   // FUN_000748ec
        const SQ* T = T_task; const GD& g = gd(); Row& r = row[c];
        if (!(r.y < 80 && r.y > -70)) return;
        Color col = c == sel ? Color{} : argb(0xffc0c0c0);
        bool marked = false;
        for (int i = 0; i < r.count; i++) {
            const Chip& ch = r.chip[i]; float x = chipX(i);
            if (i < r.count - 1 && ch.pop.st == 3) q(*psrts, T[56], x + 22, r.y);
            bool isCur = c == sel && i == cur - 1;
            float s = ch.pop.st == 3 ? (isCur ? 1.f : 0.6f) : ch.pop.scale;
            Color cc = alphaOf(col, ch.pop.alpha * col.a / 255);
            q(*psrts, T[ch.on ? 49 + c : 55], x, r.y, 1, cc, s, s);
            int state = g.task[queue(c).task[i]].state;
            if (!isCur && state >= 3) q(*psrts, T[54 + state], x, r.y - 2, 1, cc);
            if (r.over && !ch.on && !marked) {   // "cannot be finished: N days over"
                marked = true;
                q(*psrts, T[60], x + 10, r.y + 10, 1, cc);
                if (ch.pop.st == 3) {
                    int v = std::max(r.overDays, 0), n = v < 10 ? 1 : 2; float dx = v < 10 ? -2.f : 0.f;
                    for (int k = 0; k < n; k++, v /= 10) q(*psrts, T[61 + v % 10], x + 18 + dx - k * 8.f, r.y + 16, 1, argb(0xff0000ff));
                }
            }
        }
    }
    void drawBadge(int c) {   // FUN_000742cc: "N days of tasks left", red when they do not fit
        const SQ* T = T_task; const Row& r = row[c];
        if (!(r.y < 80 && r.y > -70)) return;
        float s = r.badge.st == 3 ? (c == sel ? 1.f : 0.75f) : r.badge.scale;
        float a = r.badge.alpha / 255.f;
        q(*name, T[r.over ? 48 : 47], -115, r.y, a, {}, s, s);
        Color dc = argb(r.over ? 0xff0000ff : 0xffff0000);
        int v = r.days, n = v < 10 ? 1 : 2; float bx = v < 10 ? -120.f : -118.f;
        for (int k = 0; k < n; k++, v /= 10) q(*name, T[37 + v % 10], bx - k * 8 * s, r.y - 4, a, dc, s, s);
    }
    void drawPanelText() {   // FUN_0001bd18: description and days left of the cursor task, at (-15, panelY + 13)
        const CharQueue& q = queue(sel);
        if (q.count == 0 && q.doneIndex == 0) return;
        int i = std::clamp(cur - 1, 0, std::max(q.count - 1, 0));
        const TaskDef& d = simTables().tasks[q.task[i]];
        int left = d.workCost - gd().task[q.task[i]].workDone, tot = d.workCost;
        std::string s = trFmt("%s\nあと %d.%d/%d.%d日", tr(d.desc).c_str(), left / 3, (left % 3) * 3, tot / 3, (tot % 3) * 3);
        simText(s, -15 - 128, panelY + 13 + 16, 15, {0, 0, 0, 255}, 18);
    }
};
}
std::unique_ptr<Job> makeTaskList() { return std::make_unique<TaskList>(); }
