// Shared drawing and input helpers for the menu module (PSP centre, Y-up coordinates).
#pragma once
#include "common.h"
#include "game.h"
#include "sim_api.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

extern int g_eventViewId;          // G+0x59f97c, read by native 9 in the eventview script
extern std::string g_dlcMovieName; // 0x247f00, set by native 71

namespace mu {
inline float mx() { return g_input.mouseX - 240.f; }
inline float my() { return 136.f - g_input.mouseY; }
inline bool hit(float x, float y, float w, float h) { return std::abs(mx() - x) < w / 2 && std::abs(my() - y) < h / 2; }
// a left click also pulses CIRCLE; one that landed outside every item must not confirm
inline bool confirm(bool overItem) { return g_input.isPressed(BTN_CIRCLE) && !(g_input.mouseClick && !overItem); }
inline bool cancel() { return g_input.isPressed(BTN_CROSS); }
inline bool repUp() { return g_input.isRepeat(BTN_UP) || g_input.wheel > 0; }
inline bool repDown() { return g_input.isRepeat(BTN_DOWN) || g_input.wheel < 0; }
inline void se(int id) { g_audio.playCue(id); }
inline uint8_t triAlpha(int t, int half) { t %= half * 2; return (uint8_t)(t < half ? t * 255 / half : 255 - (t - half) * 255 / half); }
inline Color abgr(uint32_t c) { return {(uint8_t)c, (uint8_t)(c >> 8), (uint8_t)(c >> 16), (uint8_t)(c >> 24)}; }

// text in the game's proportional-by-class layout: adv for full width, adv/2 for ASCII
float textWidth(const std::string& s, float adv);                // widest line
int textLines(const std::string& s);
void text(Font& f, const std::string& s, float x, float yTop, Color c, float adv, float lineH, float alpha = 1);
void textCentered(Font& f, const std::string& s, float x, float yCentre, Color c, float adv, float lineH, float alpha = 1);

void commonBg();                   // G+0x341000, FUN_0000c9b4 (on a white clear)
void menuBg();                     // menu_bg tiles, FUN_00086030

// 3x3 frame around (x,y) with half sizes; corners cs px, fills stretched. rect = top-left atlas cell.
struct Ring { Spr tl, t, tr, l, r, bl, b, br; };
void ring(const Tex& t, const Ring& r, float x, float y, float halfW, float halfH, float alpha = 1, Color c = {});

void blinkArrow(const Tex& t, Spr left, Spr right, float xl, float xr, float y, int tick);

// triangle fan used by the radar charts: centre plus points, textured from one flat spot of tex
void radar(const Tex& t, float cx, float cy, const float* values, float scale, const SDL_FPoint uv[3]);
void valuesToRadar(const GD& g, float out[6]);

// a CSU/TXG picture, drawn with its centre at (x,y), bottom-clipped by the canvas
void picture(const Tex& t, float x, float y, float alpha = 1, float sx = 1, float sy = 1);
}
