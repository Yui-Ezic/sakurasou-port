// Scene factory of the simulation module, plus job 7 (finish / carry-over save, spec_sim section 12).
#include "sim_scenes.h"
#include "sim_ui.h"

namespace {
struct Finish : Job {
    TexP bg; int st = 0, wait = 0;
    void onEnter() override { bg = g_gfx.load("finish_bg.txgtl"); }
    void onResume() override {
        if (st == 0) { g_trans.style(0); g_trans.reveal(10); st = 1; }
        else if (st == 7) { g_trans.style(0); g_trans.reveal(10); st = 8; }
    }
    void onUpdate() override {
        switch (st) {
        case 1: if (g_trans.clear()) { st = 2; g_popup.open("引き継ぎセーブは、シナリオのクリア情報、\n仕様スクエアのオープン状況を引き継ぎます。", false); } break;
        case 2: if (!g_popup.isOpen()) { st = 3; g_popup.open("引き継ぎデータをセーブしますか？", true); } break;
        case 3:
            if (!g_popup.isOpen()) {
                if (g_popup.yes()) { st = 4; wait = 0; }
                else { st = 9; g_trans.style(0); g_trans.cover(10); }
            }
            break;
        case 4: if (++wait >= 4) { takeSnapshot(); carryOverSnapshot(); st = 5; g_trans.style(0); g_trans.cover(10); } break;
        case 5: if (g_trans.covered()) { siblingPush(0x1b); st = 7; } break;
        case 8: if (g_trans.clear()) { st = 9; g_trans.cover(10); } break;
        case 9: if (g_trans.covered()) { st = 10; selfPop(); } break;
        }
    }
    void onDraw() override { g_gfx.clear({0, 0, 0, 255}); if (bg) g_gfx.draw(*bg, 0, 0); }
};
}

std::unique_ptr<Job> createSimScene(int id) {
    switch (id) {
    case 7: return std::make_unique<Finish>();
    case 8: return makeChoice();
    case 9: return makeAction();
    case 0xa: return makeChart();
    case 0xc: return makeCalendar();
    case 0xd: return makeTaskList();
    case 0xe: return makeContest();
    }
    return nullptr;
}
