// Simulation module API: typed view of GameData, data tables, shared rules (docs/spec_sim.md).
#pragma once
#include "game.h"
#include <cstddef>
#include <string>
#include <vector>

// ---- GameData layout (spec section 1); the bytes live in g_game.raw ----
struct TaskRT { int32_t state, visible, prereqOK, planDone, workDone, planFinished; };
struct CharQueue { int32_t task[32]; int32_t count, doneIndex; };
struct CharState { int32_t state, sickTurns, moodIcon, healthIcon; };
struct CharStat { int32_t bio, mood, health, affinity, eventCount, love; };
struct TeamRT { int32_t prevRank, score, rank; };
struct PlanSlot { int32_t kind, target; };
struct GD {
    int32_t turn, rankStage, period;
    PlanSlot plan[3];
    int32_t selectedTask;
    int32_t slumpSeen[128];
    TaskRT task[500];
    CharQueue queue[6];
    CharState cs[6];
    CharStat stat[6];
    int32_t available[6];
    int32_t total[6];
    char gameTitle[64];
    TeamRT team[60];
    int32_t routeA, routeB;
    uint32_t clearBits, badBits;
    uint32_t flags[128];
    int32_t vars[128];
};
static_assert(sizeof(GD) == 0x3c78);
static_assert(offsetof(GD, cs) == 0x3438 && offsetof(GD, routeA) == 0x3868 && offsetof(GD, vars) == 0x3a78);
inline GD& gd() { return *reinterpret_cast<GD*>(g_game.raw.data()); }

// ---- data tables ----
struct TaskDef {          // task_list.tskd record (0x14c)
    int32_t owner; char name[64], shortName[64], desc[64];
    int32_t planCost, workCost, gridX, gridY;
    struct { int32_t task, dx, dy; } child[4];
    struct { int32_t task, kind; } req[4];
    int32_t gains[6], workAnim, minPhase;
    uint32_t routeMask; int32_t key;
};
static_assert(sizeof(TaskDef) == 0x14c);
struct TeamDef { char teamName[64], gameTitle[64], genre[64]; int32_t scoreMax[12], scoreMin[12]; };
static_assert(sizeof(TeamDef) == 0x120);
struct SlumpDef { char file[64]; int32_t character, minTurn, maxTurn, workdayOnly; uint32_t periodMask; int32_t repeatable, weight; };
static_assert(sizeof(SlumpDef) == 0x5c);
struct CommentDef { int32_t commenter; char who[64]; char body[0x108]; int32_t sentiment, stageBucket, tag, route; };
static_assert(sizeof(CommentDef) == 0x15c);
struct TipDef { int32_t index, id; char title[64], reading[64], body[256]; };
static_assert(sizeof(TipDef) == 0x188);

struct SimTables {
    std::vector<TaskDef> tasks; std::vector<TeamDef> teams; std::vector<SlumpDef> slumps;
    std::vector<CommentDef> comments; std::vector<TipDef> tips;
    int tcols = 0, trows = 0; std::vector<int32_t> cells;   // task_table.tskt
    std::vector<int32_t> thresholds;                        // team_rank.rkd
    std::vector<int32_t> rankTable;                         // (count) x 51
    int rankAt(int stage, int kind, int i) const { return rankTable[(size_t)(stage * 2 + kind) * 51 + i]; }
    int cell(int x, int y) const { return x < 0 || y < 0 || x >= tcols || y >= trows ? -1 : cells[(size_t)y * tcols + x]; }
};
const SimTables& simTables();     // loaded on first use

// ---- rules (spec section 2, 5, 7) ----
int simRand();                     // native 7, LCG 0x41c64e6d
bool taskSelectable(int t);
bool anySelectable();
void taskUpdate(int t);            // FUN_0001b690
void taskCheckPrereq(int t);       // FUN_0001b564
void taskRevealChildren(int t);    // FUN_0001b4e8
void taskUpdateAll();              // update all, then checkPrereq all
int gameScore();                   // weighted score of total[], spec 7.1
void contestRun(int stage);        // FUN_0001c314
std::string stripNewlines(const char* s);

// ---- called by the progress script host ----
int simProgressNative(Vm&, int id, int32_t& ret);   // natives 73, 74

// ---- ADV helpers ----
void simGetDate(int& month, int& day, int& weekday, bool& holiday);        // FUN_00010f08, current turn
bool simTipText(int index, std::string& title, std::string& body);         // 0-based tips_list entry

// ---- scene arguments ----
extern int g_actionChoiceArg;      // mode for job 8, set by native 65 (FUN_00036d8c)
void simChartMode(int mode);       // FUN_0003ce28: job 10 mode, 1 select, 0 view (default)
