// Module interfaces. Owners: game.cpp/sim* = simulation; characters* = sprites and Live2D;
// menus* = save/load/menu/omake screens; adv*, progress*, save* = scenario, progress, save buffer.
#pragma once
#include "common.h"
#include "vm.h"
#include <array>

// ---- GameData: byte image of G+0x348544 (0x3c78 bytes), docs/spec_sim.md ----
struct GameData {
    std::array<uint8_t, 0x3c78> raw{};
    int32_t& dw(size_t i) { return *(int32_t*)&raw[i * 4]; }
    void reset();                          // FUN_00010c78
};
extern GameData g_game;
extern int g_mode;                         // G+0x59f978: 0 main, 1 eventview, 2 trial, 3-6 drama
extern int g_actionChoiceArg;               // argument of native 65, read by scene 8
int baseNative(Vm& vm, int id, int32_t& ret);   // natives 0..58, shared by every script VM (sim module)

// ---- scene factories: return nullptr for ids they do not own ----
std::unique_ptr<Job> createBootScene(int id);      // 1-4,0xf,0x14,0x20-0x23
std::unique_ptr<Job> createProgressScene(int id);  // 5,6,0xb
std::unique_ptr<Job> createSimScene(int id);       // 7,8,9,0xa,0xc,0xd,0xe
std::unique_ptr<Job> createMenuScene(int id);      // 0x10-0x13,0x15-0x19,0x1a-0x1f,0x24-0x27

// ---- save buffer: image of G+0x526240, GAMExx.DAT, 0x79640 bytes (spec_flow_menus 8.2) ----
constexpr size_t kSaveSize = 0x79640, kSnapSize = 0xb090;
uint8_t* saveBuffer();
void clearSaveBuffer();                    // FUN_00001cf8
void takeSnapshot();                       // FUN_00001f6c: fills snapshot 0 from live state and the job stack
void shiftHistory();                       // FUN_00001e80
void selectHistory(int i);                 // FUN_00001f18: snapshot i+1 becomes the current one
void carryOverSnapshot();                  // FUN_00001d2c
bool writeSaveFile(const std::string& file);          // from the buffer
bool readSaveFile(const std::string& file, Bytes& out); // false = no file
void useSaveFile(const Bytes& file);       // copy into buffer
void restoreFromSnapshot(Job& progressHost);  // FUN_00001e18
Job* progressHost();                       // scene 5 while it exists, else null
extern int g_lastSlot;                     // G+0x5f87bc

// ---- ADV extras reachable from menus ----
struct BacklogEntry { int type; std::string name, voice, text; };
const std::vector<BacklogEntry>& backlog();     // ring of 1000, oldest first
void backlogAdd(int type, const std::string& name, const std::string& voice, const std::string& text);
void backlogClear();

// ---- character stage: natives 161-196 (characters module) ----
class CharStage {
public:
    CharStage(); ~CharStage();
    void update(bool skip);                // every frame
    void drawSlot(int i);                  // slot 0 or 1, ADV draw order step 3
    void drawLive2D();                     // ADV draw order step 4
    int native(Vm& vm, int id, int32_t& ret);  // 161..196
    void saveState(Bytes& out);
    void loadState(const uint8_t*& p);
private:
    struct Impl; std::unique_ptr<Impl> p;
};
