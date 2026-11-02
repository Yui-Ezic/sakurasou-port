#include "engine.h"
#include <SDL.h>
#include <cstdio>
#include <cstring>

Input g_input; bool g_headless = false; uint64_t g_frame = 0;

// Repeat: fires on press frame, then after 15 frames, then every 4 frames (FUN_000dd2e8) [CONFIRMED].
void Input::beginFrame(uint32_t now) {
    pressed = now & ~held; released = held & ~now; held = now; repeat = 0;
    for (int i = 0; i < 16; i++) {
        uint32_t m = 1u << i;
        if (!(now & m)) { rep[i] = 0; continue; }
        if (pressed & m) rep[i] = 0;
        int c = rep[i];
        if (c == 0 || (c >= 15 && (c - 15) % 4 == 0)) repeat |= m;
        c++; if (c > 18) c = 15;
        rep[i] = (uint8_t)c;
    }
}

std::unique_ptr<Job> Job::create(int i) { return createScene(i); }
void Job::process() {
    if (cmds.empty()) return;
    if (Job* t = top()) t->onPause();
    auto q = std::move(cmds); cmds.clear();
    for (auto& c : q) {
        if (c.op == 1) {
            auto j = create(c.arg);
            if (!j) { fprintf(stderr, "scene %#x not implemented\n", c.arg); continue; }
            j->parent = this; j->id = c.arg;
            kids.push_back(std::move(j));
            kids.back()->onEnter();
        } else if (c.op == 2) {
            if (!kids.empty()) { kids.back()->onExit(); kids.pop_back(); }
        } else {
            while (!kids.empty()) { kids.back()->onExit(); kids.pop_back(); }
        }
    }
    if (Job* t = top()) t->onResume();
}
Job* Job::pushNow(int i) {
    auto j = create(i);
    if (!j) return nullptr;
    j->parent = this; j->id = i;
    kids.push_back(std::move(j));
    kids.back()->onEnter();
    return kids.back().get();
}
void Job::update() { process(); if (Job* t = top()) { t->onUpdate(); t->update(); } }
void Job::draw() { if (Job* t = top()) { t->onDraw(); t->draw(); } }

static SDL_GameController* pad = nullptr;
static bool s_text = false; std::string g_textTyped; int g_textBackspace = 0;
void textEntry(bool on) { s_text = on; g_textTyped.clear(); g_textBackspace = 0; if (on) SDL_StartTextInput(); else SDL_StopTextInput(); }
static uint32_t keyMask() {
    const Uint8* k = SDL_GetKeyboardState(nullptr);
    uint32_t b = 0;
    auto on = [&](std::initializer_list<int> sc, uint32_t m) { for (int s : sc) if (k[s]) b |= m; };
    if (s_text) { on({SDL_SCANCODE_RETURN}, BTN_CIRCLE); goto padOnly; }
    on({SDL_SCANCODE_UP}, BTN_UP); on({SDL_SCANCODE_DOWN}, BTN_DOWN);
    on({SDL_SCANCODE_LEFT}, BTN_LEFT); on({SDL_SCANCODE_RIGHT}, BTN_RIGHT);
    on({SDL_SCANCODE_Z, SDL_SCANCODE_RETURN, SDL_SCANCODE_SPACE}, BTN_CIRCLE);
    on({SDL_SCANCODE_X, SDL_SCANCODE_ESCAPE, SDL_SCANCODE_BACKSPACE}, BTN_CROSS);
    on({SDL_SCANCODE_A}, BTN_SQUARE); on({SDL_SCANCODE_S}, BTN_TRIANGLE);
    on({SDL_SCANCODE_Q, SDL_SCANCODE_PAGEUP}, BTN_L); on({SDL_SCANCODE_W, SDL_SCANCODE_PAGEDOWN, SDL_SCANCODE_LCTRL}, BTN_R);
    on({SDL_SCANCODE_TAB}, BTN_SELECT); on({SDL_SCANCODE_F}, BTN_START);
padOnly:
    if (pad) {
        auto B = [&](SDL_GameControllerButton x, uint32_t m) { if (SDL_GameControllerGetButton(pad, x)) b |= m; };
        B(SDL_CONTROLLER_BUTTON_DPAD_UP, BTN_UP); B(SDL_CONTROLLER_BUTTON_DPAD_DOWN, BTN_DOWN);
        B(SDL_CONTROLLER_BUTTON_DPAD_LEFT, BTN_LEFT); B(SDL_CONTROLLER_BUTTON_DPAD_RIGHT, BTN_RIGHT);
        B(SDL_CONTROLLER_BUTTON_A, BTN_CIRCLE); B(SDL_CONTROLLER_BUTTON_B, BTN_CROSS);
        B(SDL_CONTROLLER_BUTTON_X, BTN_SQUARE); B(SDL_CONTROLLER_BUTTON_Y, BTN_TRIANGLE);
        B(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, BTN_L); B(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, BTN_R);
        B(SDL_CONTROLLER_BUTTON_BACK, BTN_SELECT); B(SDL_CONTROLLER_BUTTON_START, BTN_START);
        int lx = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX), ly = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
        if (lx < -16000) b |= BTN_LEFT; if (lx > 16000) b |= BTN_RIGHT;
        if (ly < -16000) b |= BTN_UP; if (ly > 16000) b |= BTN_DOWN;
    }
    return b;
}

// pulses injected for one frame (mouse clicks), or by scripted test input
uint32_t g_injectButtons = 0;

void pumpEvents(bool& quit) {
    SDL_Event e;
    uint32_t pulse = g_injectButtons; g_injectButtons = 0;
    static uint32_t mouseHeld = 0;
    g_input.mouseMoved = false; g_input.mouseClick = false; g_input.wheel = 0;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT: quit = true; break;
        case SDL_TEXTINPUT: if (s_text) g_textTyped += e.text.text; break;
        case SDL_KEYDOWN:
            if (s_text && e.key.keysym.sym == SDLK_BACKSPACE) g_textBackspace++;
            if (e.key.keysym.sym == SDLK_F11 || (e.key.keysym.sym == SDLK_RETURN && (e.key.keysym.mod & KMOD_ALT))) g_gfx.toggleFullscreen();
            break;
        case SDL_MOUSEMOTION: g_gfx.toCanvas(e.motion.x, e.motion.y, g_input.mouseX, g_input.mouseY); g_input.mouseMoved = true; break;
        case SDL_MOUSEBUTTONDOWN:
            g_gfx.toCanvas(e.button.x, e.button.y, g_input.mouseX, g_input.mouseY);
            if (e.button.button == SDL_BUTTON_LEFT) { mouseHeld |= BTN_CIRCLE; g_input.mouseClick = true; }
            if (e.button.button == SDL_BUTTON_RIGHT) mouseHeld |= BTN_CROSS;
            break;
        case SDL_MOUSEBUTTONUP:
            if (e.button.button == SDL_BUTTON_LEFT) mouseHeld &= ~BTN_CIRCLE;
            if (e.button.button == SDL_BUTTON_RIGHT) mouseHeld &= ~BTN_CROSS;
            break;
        case SDL_MOUSEWHEEL: g_input.wheel += e.wheel.y; break;
        case SDL_CONTROLLERDEVICEADDED: if (!pad) pad = SDL_GameControllerOpen(e.cdevice.which); break;
        case SDL_CONTROLLERDEVICEREMOVED: if (pad) { SDL_GameControllerClose(pad); pad = nullptr; } break;
        }
    }
    g_input.beginFrame(keyMask() | mouseHeld | pulse);
}
