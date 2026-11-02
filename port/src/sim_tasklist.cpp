// Job 0xd: task list. spec_sim section 11 leaves the layout unresolved: read-only queue view.
// ASSUMED: layout approximated from task_*.txg contents, one page per character, LEFT/RIGHT to switch.
#include "tr.h"
#include "sim_scenes.h"
#include "sim_ui.h"

namespace {
struct TaskList : Job {
    TexP bg, win, name, parts, count; PageHeader hdr;
    int page = 0, st = 0, row = 0;
    void onEnter() override {
        bg = g_gfx.load("task_bg.txg"); win = g_gfx.load("task_window.txg"); name = g_gfx.load("task_name.txg");
        parts = g_gfx.load("task_psrts.txg"); count = g_gfx.load("task_count.txg");
        hdr.set(2); hdr.start();
        g_help.show("×戻る　キャラクターの作業予定を確認できます");
    }
    void onResume() override { g_trans.style(0); g_trans.reveal(10); }
    void onUpdate() override {
        hdr.update();
        if (st == 1) { if (g_trans.covered()) { st = 2; selfPop(); } return; }
        if (st) return;
        if (g_input.isPressed(BTN_CROSS)) { g_audio.playCue(0x1f); g_help.hide(); g_trans.style(0); g_trans.cover(10); st = 1; return; }
        if (g_input.isRepeat(BTN_RIGHT)) { page = (page + 1) % 6; row = 0; g_audio.playCue(0x2a); }
        if (g_input.isRepeat(BTN_LEFT)) { page = (page + 5) % 6; row = 0; g_audio.playCue(0x2a); }
        const CharQueue& cq = gd().queue[page];
        int n = cq.count - cq.doneIndex;
        if (g_input.isRepeat(BTN_DOWN) && row + 6 < n) { row++; g_audio.playCue(0x2a); }
        if (g_input.isRepeat(BTN_UP) && row > 0) { row--; g_audio.playCue(0x2a); }
    }
    void onDraw() override {
        for (int j = 0; j < 5; j++) for (int i = 0; i < 9; i++) ui(*bg, {0, 0, 64, 64}, -240 + 32 + i * 64.f, 136 - 32 - j * 64.f);
        hdr.draw();
        const auto& tb = simTables(); const GD& g = gd();
        for (int c = 0; c < 6; c++) {
            float x = -200 + c * 40.f;
            ui(*win, {c % 6 * 40, 0, c % 6 * 40 + 40, 40}, x, 72, c == page ? 1.f : 0.5f);
        }
        const CharQueue& cq = g.queue[page];
        simText(kCharName[page], -220, 50, 14, {0, 0, 0, 255});
        int n = cq.count - cq.doneIndex;
        for (int i = 0; i < 6 && row + i < n; i++) {
            int t = cq.task[cq.doneIndex + row + i];
            const TaskDef& d = tb.tasks[t]; const TaskRT& r = g.task[t];
            float y = 22 - i * 26.f;
            g_gfx.fillRect(240 - 220, 136 - y - 12, 440, 24, {255, 255, 255, 200});
            simText(stripNewlines(d.name), -212, y + 7, 14, {0, 0, 0, 255});
            int rest = d.workCost - r.workDone; char b[32];
            snprintf(b, sizeof b, tr("残 %s.%s日").c_str(), kDigitStr[rest / 3], kDigitStr[(rest % 3) * 3]);
            simText(b, 150, y + 5, 10, {0, 0, 0, 255});
        }
        if (n == 0) simText("タスクなし", -212, 20, 14, {80, 80, 80, 255});
    }
};
}
std::unique_ptr<Job> makeTaskList() { return std::make_unique<TaskList>(); }
