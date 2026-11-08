// Drawing helpers shared by the simulation scenes.
#pragma once
#include "common.h"
#include "sim_api.h"
#include "sim_sprites.h"
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <string>

// draw one table quad; coordinates are PSP (centre origin, y up)
void q(const Tex& t, const SQ& s, float x, float y, float a = 1, Color c = {}, float sx = 1, float sy = 1);
// triangle strip of two quads (left cap, implied middle, right cap)
void qStrip(const Tex& t, const SQ& l, const SQ& r, float x, float y, float a = 1, Color c = {}, float sx = 1, float sy = 1);
inline Color argb(uint32_t abgr) { return {(uint8_t)abgr, (uint8_t)(abgr >> 8), (uint8_t)(abgr >> 16), (uint8_t)(abgr >> 24)}; }

// text with half-width advance for ASCII. x,y = top left of the first line in PSP coordinates, y up
float textWidth(const std::string& line, int cell);
void simText(const std::string& s, float x, float y, int cell, Color c, int lineH = 0);
void simTextCentered(const std::string& s, float x, float y, int cell, Color c, int lineH = 0); // centred block about (x,y)

void drawCommonBg();                       // FUN_0000c9b4: common_bg.txg paper
int phaseGroup();                          // 0..4 from routeA

// FUN_00015c50: "submission in N days" widget
class DeadlineWidget {
public:
    void init(float x, float y);
    void draw();
private:
    float x = 0, y = 0; int group = 4, deadline = 0, turn = 0; TexP font;
};

// FUN_0000d840: today's date from common_calendar.txg ("11月12日(月)"), drawn about (x,y)
class DateWidget {
public:
    void draw(float x, float y);
private:
    TexP tex;
};

// action-log toast, FUN_0002bcac
class Toast {
public:
    void init();
    void show(const std::string& text, int icon, uint32_t statBits, int voice, bool key);
    bool active() const { return st != 0; }
    void update();
    void draw();
private:
    int st = 0, t = 0; std::string text; int icon = 0; uint32_t bits = 0; bool key = false;
    TexP parts, faces, game, quick;
    void drawAt(float dx);
};

// job launcher FUN_00028238: fade out 10 + BGM stop, push job, on return fade BGM back in
class Launcher {
public:
    int state = 0;           // 0 idle, 2 fading out, 3 job running
    void start(Job* self, int job) { this->self = self; pending = job; state = 2; g_trans.style(0); g_trans.cover(10); g_audio.stop(CH_BGM, 10); }
    void restore(Job* s) { self = s; state = 3; }   // saved while the job was on top
    void update() {
        if (state == 2 && g_trans.covered()) { self->siblingPush(pending); state = 3; }
    }
    void resumed() {     // job returned
        if (state != 3) return;
        state = 0; g_trans.style(0); g_trans.reveal(10); g_audio.play(CH_BGM, "SK_BGM_02.adx", true, 10);
    }
    bool busy() const { return state != 0; }
private:
    Job* self = nullptr; int pending = 0;
};
