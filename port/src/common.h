// Shared systems: SYSTEM.DAT state, screen transition, popup, help bar, UI drawing in PSP (centre, Y-up) space.
#pragma once
#include "engine.h"
#include <array>

// ---- persistent system data: same byte layout as SYSTEM.DAT (spec_flow_menus 8.1) ----
struct SysData {
    std::array<uint8_t, 0x356c> raw{};
    uint32_t& u32(size_t o) { return *(uint32_t*)&raw[o]; }
    // config block at +4
    uint32_t& se() { return u32(4); }
    uint32_t& bgm() { return u32(8); }
    uint32_t& voice() { return u32(12); }
    uint32_t& movie() { return u32(16); }
    uint32_t& sorataVoice() { return u32(20); }
    uint32_t& textSpeed() { return u32(24); }
    uint32_t& skipAll() { return u32(28); }
    uint32_t& install() { return u32(32); }
    // global block at +0x404
    uint32_t& g(size_t o) { return u32(0x404 + o); }
    bool bit(size_t base, int n) { return g(base + (n >> 5) * 4) >> (n & 31) & 1; }
    void setBit(size_t base, int n) { g(base + (n >> 5) * 4) |= 1u << (n & 31); }
    bool textRead(int id) { return id >= 0 && id < 100000 && bit(0x20, id); }
    void setTextRead(int id) { if (id >= 0 && id < 100000) setBit(0x20, id); }
    void defaults();
    void applyVolumes();
    bool load();
    bool save();
};
extern SysData g_sys;
std::string savePath(const std::string& file);
int rng();                 // the game's RNG stream
void clearBit(int n);      // FUN_00019a00: scenario clear unlock rules

// ---- UI helpers. Coordinates: x right, y up, origin at screen centre ----
struct Spr { int x0, y0, x1, y1; };
void ui(const Tex& t, Spr s, float x, float y, float alpha = 1, float sx = 1, float sy = 1, float rotDeg = 0, Color c = {});
void uiFontCentered(Font& f, const std::string& s, float x, float y, Color c);

// ---- screen transition (G+0x2ff080) ----
class Transition {
public:
    void cover(int n); void reveal(int n);
    void style(int s) { st = s; }
    bool covered() const { return mode == 1; }
    bool clear() const { return mode == 0; }
    void update(); void draw();
private:
    int st = 0, mode = 0, t = 0, n = 0;  // mode 0 clear 1 covered 2 covering 3 revealing
    TexP bg, band;
};
extern Transition g_trans;

// ---- popup (G+0x307040) ----
class Popup {
public:
    void open(const std::string& text, bool twoButtons, bool defaultYes = true);
    bool isOpen() const { return state != 0; }
    bool yes() const { return resultYes; }
    bool cancelled() const { return resultCancel; }
    void update(); void draw();
private:
    int state = 0; std::string text; bool two = false; int cur = 0; float h = 1, target = 20;
    bool resultYes = false, resultCancel = false; TexP win; std::vector<std::string> lines;
};
extern Popup g_popup;

// ---- help bar (G+0x3300c0) ----
class HelpBar {
public:
    void show(const std::string& t); void hide();
    void update(); void draw();
    void rewind() { scroll = 0; }   // dev (--ctl): show the start of a scrolling text
private:
    std::string text; int mode = 0; float y = -148; TexP tex; int scroll = 0;
};
extern HelpBar g_help;

// ---- page header (common_page_*.txg) ----
class PageHeader {
public:
    void set(int page); void start(); void leave();
    void update(); void draw();
private:
    TexP tex, deco; int f = 0, mode = 0;
};

struct RootJob : Job {
    void onEnter() override;
    void onUpdate() override;
    void overlayDraw();
};
extern RootJob* g_root;
void playVoiceIfExists(const std::string& name);
void rerollVoiceIndex();
