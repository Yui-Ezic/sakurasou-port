// Job (scene) system and input, mirroring the game's own design (docs/spec_flow_menus.md).
#pragma once
#include "audio.h"
#include "fs.h"
#include "gfx.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

enum : uint32_t {
    BTN_SELECT = 0x1, BTN_START = 0x8, BTN_UP = 0x10, BTN_RIGHT = 0x20, BTN_DOWN = 0x40, BTN_LEFT = 0x80,
    BTN_L = 0x100, BTN_R = 0x200, BTN_TRIANGLE = 0x1000, BTN_CIRCLE = 0x2000, BTN_CROSS = 0x4000, BTN_SQUARE = 0x8000,
};

struct Input {
    uint32_t held = 0, pressed = 0, released = 0, repeat = 0;
    int mouseX = 0, mouseY = 0;
    bool mouseMoved = false, mouseClick = false;
    bool escMenu = false;   // set by a scene each frame it wants Esc to act as Triangle (open menu) instead of Cross
    int wheel = 0;
    uint8_t rep[32] = {};
    void beginFrame(uint32_t now);
    bool isHeld(uint32_t m) const { return held & m; }
    bool isPressed(uint32_t m) const { return pressed & m; }
    bool isRepeat(uint32_t m) const { return repeat & m; }
};
extern Input g_input;

class Job {
public:
    virtual ~Job() = default;
    virtual void onEnter() {}
    virtual void onExit() {}
    virtual void onResume() {}
    virtual void onPause() {}
    virtual void onUpdate() {}
    virtual void onDraw() {}
    virtual std::unique_ptr<Job> create(int id);
    // save-game state of a scene that sits on the progress job stack (spec_flow_menus 8.2)
    virtual void saveState(Bytes&) {}
    virtual void loadState(const uint8_t*, size_t) {}

    void push(int id) { cmds.push_back({1, id}); }
    void pop() { cmds.push_back({2, 0}); }
    void popAll() { cmds.push_back({3, 0}); }
    // queue on the parent, as FUN_000dccdc etc. do
    void selfPop() { if (parent) parent->pop(); }
    void siblingPush(int id) { if (parent) parent->push(id); }

    void process();            // runs queued commands
    void update();             // process + update top child (FUN_000dd1c0)
    void draw();               // draws top child only
    Job* pushNow(int id);      // create and enter immediately (restoring a saved job stack)
    const std::vector<std::unique_ptr<Job>>& children() const { return kids; }
    Job* top() { return kids.empty() ? nullptr : kids.back().get(); }
    bool hasChild() const { return !kids.empty() || !cmds.empty(); }
    Job* parent = nullptr;
    int id = 0;
private:
    struct Cmd { int op, arg; };
    std::vector<Cmd> cmds;
    std::vector<std::unique_ptr<Job>> kids;
};

std::unique_ptr<Job> createScene(int id);

void pumpEvents(bool& quit);
void runGame(int firstScene);
extern bool g_headless;
// text entry (replaces the PSP on-screen keyboard): letters stop acting as buttons while on
void textEntry(bool on);
extern std::string g_textTyped;   // UTF-8 typed since last read
extern int g_textBackspace;       // backspaces since last read
extern uint64_t g_frame;
