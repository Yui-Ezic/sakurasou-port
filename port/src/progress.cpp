// Scenes 5 (progress host) and 6 (progress script VM), natives 64-74 (spec_flow_menus section 9).
#include "adv.h"
#include <cstdio>
#include <cstring>
#include "game.h"
__attribute__((weak)) int simProgressNative(Vm&, int, int32_t&);

extern std::string g_dlcMovieName;
static Job* s_host = nullptr;
Job* progressHost() { return s_host; }

namespace {
struct Script : Job, NativeHandler {
    Vm vm;
    void onEnter() override {
        static const char* f[7] = {"script_system_progress.ssb", "script_eventview_progress.ssb", "script_trial_progress.ssb",
            "script_drama1_progress.ssb", "script_drama2_progress.ssb", "script_drama3_progress.ssb", "script_drama4_progress.ssb"};
        vm.handler = this; vm.load(g_fs.read(f[g_mode]), f[g_mode]);
    }
    void onUpdate() override { if (!vm.halted()) vm.run(); if (vm.halted()) selfPop(); }
    void onDraw() override {}
    int native(Vm& v, int id, int32_t& ret) override {
        ret = 0;
        if (id < 59) return baseNative(v, id, ret);
        switch (id) {
        case 64: s_host->push(0xc); return 0;
        case 65: g_actionChoiceArg = v.arg(0); s_host->push(8); return 0;
        case 66: s_host->push(9); return 0;
        case 67: g_advScript = v.argStr(0); s_host->push(0xb); return 0;
        case 68: s_host->push(0xe); return 0;
        case 69: s_host->push(0x23); return 0;
        case 70: s_host->push(7); return 0;
        case 71: g_dlcMovieName = v.argStr(0); s_host->push(0x27); return 0;
        case 59: case 60: case 61: case 62: case 63: return 0;   // no-ops in every handler (0x5654..)
        case 72: selfPop(); return 0;
        case 73: case 74: if (simProgressNative) return simProgressNative(v, id, ret); return 0;
        }
        fprintf(stderr, "[progress: unhandled native %d]\n", id);
        return 0;
    }
    void saveState(Bytes& o) override { Vm::State s = vm.save(); o.insert(o.end(), (uint8_t*)&s, (uint8_t*)&s + sizeof s); }
    void loadState(const uint8_t* p, size_t) override { Vm::State s; memcpy(&s, p, sizeof s); vm.restore(s); }
};
struct Host : Job {
    void onEnter() override {
        s_host = this; g_game.reset(); backlogClear();
        g_advScript.clear();     // FUN_0004e3a8: no scenario until native 67 names one
        restoreFromSnapshot(*this);
        if (!hasChild()) push(6);
    }
    void onUpdate() override { if (!hasChild()) selfPop(); }
    void onExit() override { s_host = nullptr; }
};
}
int g_actionChoiceArg = 0;
std::unique_ptr<Job> createProgressScene(int id) {
    if (id == 5) return std::make_unique<Host>();
    if (id == 6) return std::make_unique<Script>();
    if (id == 0xb) return std::make_unique<AdvScene>();
    return nullptr;
}
