// GameData init, base natives 0-58, data tables and the task/contest rules (docs/spec_sim.md sections 1, 2, 5, 7).
#include "sim_api.h"
extern int g_eventViewId;
#include "sim_sprites.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

GameData g_game;

// ---- tables ----
namespace {
template <class T> void loadRecords(const char* file, const char* magic, std::vector<T>& out, size_t hdr = 8) {
    Bytes d = g_fs.read(file);
    if (d.size() < hdr || memcmp(d.data(), magic, strlen(magic))) { fprintf(stderr, "sim: bad table %s\n", file); return; }
    int n; memcpy(&n, &d[4], 4);
    if (hdr + n * sizeof(T) > d.size()) { fprintf(stderr, "sim: short table %s\n", file); return; }
    out.resize(n);
    memcpy(out.data(), &d[hdr], n * sizeof(T));
}
SimTables s_tb; bool s_loaded = false;
uint32_t s_seed = 0; bool s_seeded = false;
int s_timer = 0;   // wait counter of natives 3, 4
}

const SimTables& simTables() {
    if (s_loaded) return s_tb;
    s_loaded = true;
    loadRecords("task_list.tskd", "TSKD", s_tb.tasks);
    loadRecords("team_list.tmd", "TMD", s_tb.teams);
    loadRecords("slump.slpd", "SLPD", s_tb.slumps);
    loadRecords("comment_list.cmed", "CMED", s_tb.comments);
    loadRecords("tips_list.tipd", "TIPD", s_tb.tips);
    Bytes t = g_fs.read("task_table.tskt");
    if (t.size() >= 0x10 && !memcmp(t.data(), "TSKT", 4)) {
        memcpy(&s_tb.tcols, &t[4], 4); memcpy(&s_tb.trows, &t[8], 4);
        size_t n = (size_t)s_tb.tcols * s_tb.trows;
        if (0x10 + n * 4 <= t.size()) { s_tb.cells.resize(n); memcpy(s_tb.cells.data(), &t[0x10], n * 4); }
    } else fprintf(stderr, "sim: bad table task_table.tskt\n");
    Bytes r = g_fs.read("team_rank.rkd");
    if (r.size() >= 0xd4 && !memcmp(r.data(), "RKD", 3)) {
        int count; memcpy(&count, &r[4], 4);
        s_tb.thresholds.resize(51); memcpy(s_tb.thresholds.data(), &r[8], 51 * 4);
        s_tb.rankTable.resize((size_t)count * 51);
        for (int k = 0; k < count && 0xd4u * (k + 2) <= r.size(); k++)
            memcpy(&s_tb.rankTable[(size_t)k * 51], &r[0xd4 * (k + 1) + 8], 51 * 4);
    } else fprintf(stderr, "sim: bad table team_rank.rkd\n");
    return s_tb;
}

// ---- rng ----
int simRand() {
    if (!s_seeded) { s_seeded = true; s_seed = (uint32_t)time(nullptr) ^ (uint32_t)SDL_GetPerformanceCounter(); }
    s_seed = s_seed * 0x41c64e6du + 0x3039;
    return (int)((s_seed >> 16) & 0x7fff);
}

std::string stripNewlines(const char* s) {
    std::string o; for (; *s; s++) if (*s != '\n' && *s != '\r') o += *s;
    return o;
}

// ---- tasks ----
void taskUpdate(int t) {
    const TaskDef& d = simTables().tasks[t];
    TaskRT& r = gd().task[t];
    if (r.state == 0 && r.visible > 0) r.state = 1;
    if (r.state == 1) {
        if (d.planCost < 1) r.state = 3;
        else if (r.planDone > 0) r.state = 2;
    }
    if (r.state == 2 && r.planDone >= d.planCost) { r.state = 3; r.planFinished = 1; }
    if (r.state == 3) {
        if (d.workCost < 1) r.state = 5;
        else if (r.workDone > 0) r.state = 4;
    }
    if (r.state == 4 && r.workDone >= d.workCost) r.state = 5;
}
void taskCheckPrereq(int t) {
    const auto& tb = simTables();
    TaskRT& r = gd().task[t];
    if (r.prereqOK >= 1) return;
    for (auto& q : tb.tasks[t].req) {
        if (q.task < 0) break;
        const TaskDef& o = tb.tasks[q.task]; const TaskRT& or_ = gd().task[q.task];
        if (q.kind == 0 ? or_.planDone < o.planCost : or_.workDone < o.workCost) return;
    }
    r.prereqOK = 1;
}
void taskRevealChildren(int t) {
    for (auto& c : simTables().tasks[t].child) { if (c.task < 0) break; gd().task[c.task].visible = 1; }
}
void taskUpdateAll() {
    int n = (int)simTables().tasks.size();
    for (int i = 0; i < n; i++) taskUpdate(i);
    for (int i = 0; i < n; i++) taskCheckPrereq(i);
}
bool taskSelectable(int t) {
    const TaskDef& d = simTables().tasks[t]; const TaskRT& r = gd().task[t];
    return r.prereqOK > 0 && d.minPhase <= gd().routeA && ((d.routeMask >> gd().routeB) & 1) && r.state == 1;
}
bool anySelectable() {
    for (int i = 0; i < (int)simTables().tasks.size(); i++) if (taskSelectable(i)) return true;
    return false;
}

void GameData::reset() {
    if (this != &g_game) {   // the rules below work on g_game; carryOverSnapshot resets a private copy
        auto live = g_game.raw; g_game.reset(); raw = g_game.raw; g_game.raw = live;
        return;
    }
    raw.fill(0);
    GD& g = gd();
    g.turn = 3;
    for (auto& p : g.plan) p = {-1, -1};
    g.selectedTask = -1;
    for (auto& s : g.stat) s.eventCount = 1;
    for (auto& a : g.available) a = 1;
    const auto& tb = simTables();
    if (!tb.teams.empty()) memcpy(g.gameTitle, tb.teams[0].gameTitle, 64);
    if (tb.tasks.empty()) return;
    g.task[0].visible = 1;
    taskUpdate(0);
    taskUpdateAll();
}

// ---- contest ranking, FUN_0001c314 ----
int gameScore() {
    int s[6]; memcpy(s, gd().total, sizeof s);
    std::sort(s, s + 6, std::greater<int>());
    int v = s[0] + (int)(s[1] * 0.3);
    for (int i = 2; i < 6; i++) v += (int)(s[i] * 0.1);
    return v;
}

void contestRun(int stage) {
    const auto& tb = simTables(); GD& g = gd();
    static uint32_t lcg = 0;
    lcg = (uint32_t)SDL_GetTicks() ^ (uint32_t)time(nullptr);
    auto rnd = [&]() -> uint32_t { lcg = lcg * 0x41c64e6du + 0x3039; return (lcg >> 16) & 0x7fff; };
    int n = (int)tb.teams.size();
    for (int t = 0; t < n; t++) g.team[t].prevRank = g.team[t].rank;
    int score = gameScore();
    int myRank = 0, rivalRank = 0;
    for (int i = 0; i < 51; i++)
        if (tb.thresholds[i] <= score) { myRank = tb.rankAt(stage, 0, i); rivalRank = tb.rankAt(stage, 1, i); break; }
    if (myRank == 0) { myRank = tb.rankAt(stage, 0, 50); rivalRank = tb.rankAt(stage, 1, 50); }
    int B[60] = {}, I[60] = {}, C[60] = {}, D[60] = {};
    for (int t = 0; t < n; t++) {
        I[t] = t;
        int mx = tb.teams[t].scoreMax[stage], mn = tb.teams[t].scoreMin[stage];
        if (mx != 0 && t != 0) {
            uint32_t r = (uint32_t)(mx - mn);
            B[t] = (int)(mn + (r ? rnd() % r : 0));
        }
    }
    for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++)
        if (B[i] < B[j]) { std::swap(B[i], B[j]); std::swap(I[i], I[j]); }
    for (int i = 0; i < n; i++) {
        if (rivalRank == 1 || rivalRank == 2) {
            if (i == 0) { D[0] = 1; C[0] = B[0] + 10; }
            else { D[i] = I[i - 1]; C[i] = B[i - 1]; if (I[i - 1] == 1) D[i] = 0; }
        } else {
            int gi = rivalRank - 1;
            if (i == gi) {
                D[i] = 1;
                if (i == 0) C[i] = B[i] + 10;
                else { if (B[i - 1] != B[i]) rnd(); C[i] = B[i]; }   // the original draws and discards
            } else if (i < gi) { D[i] = I[i]; C[i] = B[i]; }
            else { D[i] = I[i - 1]; C[i] = B[i - 1]; if (I[i - 1] == 1) D[i] = 0; }
        }
    }
    int cap = stage < 4 ? 50 : stage < 9 ? 40 : 30;
    int gi = myRank - 1;
    for (int i = 0; i < n; i++) {
        if (i == gi) {
            I[i] = 0;
            if (i == 0) B[i] = score < C[i] ? C[i] + 10 : score;
            else if (C[i - 1] < score || score < C[i]) {
                uint32_t r = (uint32_t)(C[i - 1] - C[i]);
                B[i] = (int)(C[i] + (r ? rnd() % r : 0));
            } else B[i] = score;
        } else if (i < gi) { I[i] = D[i]; B[i] = C[i]; }
        else if (i > cap) { I[i] = i; B[i] = 0; }
        else { I[i] = D[i - 1]; B[i] = C[i - 1]; }
    }
    for (int i = 0; i < n; i++) { g.team[I[i]].rank = i + 1; g.team[I[i]].score = B[i] * B[i] / 2; }
}

// ---- natives 0..58 ----
int baseNative(Vm& vm, int id, int32_t& ret) {
    GD& g = gd();
    ret = 0;
    static const bool dbg = getenv("SAKURA_SIM_DEBUG") != nullptr;
    int a0 = id >= 3 && id != 4 ? vm.arg(0) : 0;   // meaningless for zero-arg natives, read only by those with args
    switch (id) {
    case 0: if (dbg) fputs(vm.argStr(0), stderr); return 0;
    case 1: if (dbg) fprintf(stderr, "%d", vm.arg(0)); return 0;
    case 2: return 0;
    case 3: s_timer = a0 * 60 / 1000; return 0;
    case 4: if (s_timer == 0) return 0; s_timer--; return 1;
    case 5: case 6: return 0;
    case 7: ret = simRand(); return 0;
    case 8: { uint32_t span = (uint32_t)(vm.arg(1) - a0) + 1u; ret = (int)((uint32_t)simRand() % span + (uint32_t)a0); return 0; }
    case 9: ret = g_eventViewId; return 0;   // G+0x59f97c, set by the event gallery
    case 10: ret = g.turn; return 0;
    case 11: ret = g.period; return 0;
    case 12: ret = g.routeA; return 0;
    case 13: g.routeA = a0; return 0;
    case 14: ret = g.routeB; return 0;
    case 15: g.routeB = a0; return 0;
    case 16: g.rankStage = a0; contestRun(a0); return 0;
    case 17: ret = g.team[0].rank; return 0;
    case 18: ret = g.team[0].score; return 0;
    case 19: ret = g.task[a0].state > 4; return 0;
    case 20: ret = g.queue[a0].count - g.queue[a0].doneIndex; return 0;
    case 21: ret = g.cs[a0].state; return 0;
    case 22: ret = g.cs[a0].sickTurns; return 0;
    case 23: ret = g.stat[a0].bio; return 0;
    case 24: ret = g.stat[a0].mood; return 0;
    case 25: ret = g.stat[a0].health; return 0;
    case 26: ret = g.stat[a0].affinity; return 0;
    case 27: ret = g.stat[a0].eventCount; return 0;
    case 28: ret = g.stat[a0].love; return 0;
    case 29: g.stat[a0].mood += vm.arg(1); return 0;
    case 30: g.stat[a0].health += vm.arg(1); return 0;
    case 31: g.stat[a0].affinity += vm.arg(1); return 0;
    case 32: g.stat[a0].eventCount++; return 0;
    case 33: g.stat[a0].love += vm.arg(1); return 0;
    case 34: g.cs[a0].state = 0; return 0;
    case 35: g.available[a0] = vm.arg(1); return 0;
    case 36: ret = g.available[a0]; return 0;
    case 37: case 38: case 39: case 40: case 41: case 42: ret = g.total[id - 37]; return 0;
    case 43: {
        uint32_t& w = g.flags[(a0 >> 5) & 127]; uint32_t b = 1u << (a0 & 31);
        if (vm.arg(1)) w |= b; else w &= ~b;
        return 0; }
    case 44: ret = (g.flags[(a0 >> 5) & 127] >> (a0 & 31)) & 1; return 0;
    case 45: g.vars[a0 & 127] = vm.arg(1); return 0;
    case 46: ret = g.vars[a0 & 127]; return 0;
    case 47: g.clearBits |= 1u << (a0 & 31); clearBit(a0); return 0;
    case 48: ret = (g.clearBits >> (a0 & 31)) & 1; return 0;
    case 49: g.badBits |= 1u << (a0 & 31); g_sys.g(0xc) |= 1u << (a0 & 31); return 0;
    case 50: ret = (g.badBits >> (a0 & 31)) & 1; return 0;
    case 51: g_sys.g(0x4) |= 1u << (a0 & 31); return 0;
    case 52: g_sys.g(0x8) |= 1u << (a0 & 31); return 0;
    case 53: g_sys.g(0x10) |= 1u << (a0 & 31); return 0;
    case 54: g_sys.g(0x14) |= 1u << (a0 & 31); return 0;
    case 55: g_sys.g(0x18) |= 1u << (a0 & 31); return 0;
    case 56: g_sys.g(0x1c) |= 1u << (a0 & 31); return 0;
    case 57: g_sys.setBit(0x30f4, a0); return 0;
    case 58: g_sys.setBit(0x3114, a0); return 0;
    }
    return 0;
}

// ---- natives 73, 74 ----
int simProgressNative(Vm& vm, int id, int32_t& ret) {
    GD& g = gd();
    ret = 0;
    if (id == 73) {
        g.turn++; g.period = 0;
        for (auto& c : g.cs) if (c.state == 3) c.sickTurns = std::max(c.sickTurns - 1, 0);
    } else if (id == 74) {
        int c = vm.arg(0), a1 = vm.arg(1), p1 = vm.arg(2), a2 = vm.arg(3), p2 = vm.arg(4);
        const float tau = 6.2831855f;
        float s1 = std::sin(tau * (float)g.turn / (float)p1), s2 = std::sin(tau * (float)g.turn / (float)p2);
        g.stat[c].bio = (int)((float)a1 * s1 + 0.f + (float)a2 * s2);
    }
    return 0;
}

// ---- ADV helpers ----
void simGetDate(int& month, int& day, int& weekday, bool& holiday) {
    const SimDay& d = kCalendar[std::clamp(gd().turn, 0, 102)];
    month = d.month; day = d.day; weekday = d.weekday; holiday = d.holiday != 0;
}
bool simTipText(int index, std::string& title, std::string& body) {
    const auto& t = simTables().tips;
    if (index < 0 || index >= (int)t.size()) return false;
    title = t[index].title; body = t[index].body;
    return true;
}
