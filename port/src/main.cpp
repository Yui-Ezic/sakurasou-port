#include "audio.h"
#include "tr.h"
#include "fs.h"
#include "gfx.h"
#include "vm.h"
#include "movie.h"
#include "adv.h"
#include "engine.h"
#include "common.h"
#include "game.h"
#include <SDL.h>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <filesystem>
#include <algorithm>

static constexpr uint32_t kFakeF5 = 0xf5000000u, kFakeF8 = 0xf8000000u;   // --keys values for real key events

static std::string findIso(const char* argv0) {
    for (auto p : {"sakuraso-no-pet-na-kanojo-psp.iso", "../sakuraso-no-pet-na-kanojo-psp.iso", "../../sakuraso-no-pet-na-kanojo-psp.iso", "game.iso"})
        if (std::filesystem::exists(p)) return p;
    return "";
}

// ---- developer test modes (no window needed) ----
static int testAudio(const char* name, const char* out) {
    auto p = loadSound(name);
    if (!p) { fprintf(stderr, "decode failed\n"); return 1; }
    FILE* f = fopen(out, "wb");
    uint32_t bytes = (uint32_t)(p->s.size() * 2), rate = p->rate, br = rate * p->ch * 2, v32; uint16_t v16;
    fwrite("RIFF", 1, 4, f); v32 = 36 + bytes; fwrite(&v32, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    v32 = 16; fwrite(&v32, 4, 1, f); v16 = 1; fwrite(&v16, 2, 1, f); v16 = p->ch; fwrite(&v16, 2, 1, f);
    fwrite(&rate, 4, 1, f); fwrite(&br, 4, 1, f); v16 = p->ch * 2; fwrite(&v16, 2, 1, f); v16 = 16; fwrite(&v16, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f); fwrite(p->s.data(), 2, p->s.size(), f); fclose(f);
    printf("%d Hz %d ch %zu samples loop %lld-%lld\n", p->rate, p->ch, p->s.size() / p->ch, (long long)p->loopStart, (long long)p->loopEnd);
    return 0;
}

struct TraceHandler : NativeHandler {
    std::map<int,int> seen; int limit = 40; int lines = 0;
    int native(Vm& vm, int id, int32_t& ret) override {
        ret = 0; seen[id]++;
        if (id == 126 && lines++ < limit) printf("[%d] %s\n", vm.arg(2), vm.argStr(1));
        if (id == 129 && lines < limit) printf("  <%s>\n", vm.argStr(0));
        return 0;
    }
};
static int testVm(const char* script) {
    Vm vm; TraceHandler h; vm.handler = &h;
    Bytes c = g_fs.read(script);
    if (c.empty()) { fprintf(stderr, "no script\n"); return 1; }
    vm.load(std::move(c), script);
    int frames = 0;
    while (!vm.halted() && frames < 2000000) { vm.run(); frames++; }
    printf("halted=%d after %d resumes; %zu distinct natives\n", vm.halted(), frames, h.seen.size());
    return 0;
}

static int testImage(const char* name, const char* out) {
    if (!g_gfx.init(true, 1, false)) return 1;
    g_gfx.beginFrame(); g_gfx.clear({255, 0, 255, 255});
    if (!g_fontM.load("font_m.fnt") || !g_fontS.load("font_s.fnt")) { fprintf(stderr, "font\n"); return 1; }
    if (auto t = g_gfx.load(name)) g_gfx.draw(*t, 0, 0);
    g_fontM.drawText("こいつは、椎名　ましろ。\n水明芸術大学付属高等学校、美術科二年。▼", 40, 200, {255, 255, 255, 255});
    g_fontS.drawText("神田空太 さくら荘のペットな彼女", 40, 180, {255, 255, 0, 255});
    g_gfx.screenshot(out);
    return 0;
}

extern uint32_t g_injectButtons;
std::unique_ptr<Job> createScene(int id) {
    if (auto j = createBootScene(id)) return j;
    if (auto j = createProgressScene(id)) return j;
    if (auto j = createSimScene(id)) return j;
    return createMenuScene(id);
}
// dev: run every scenario through the real ADV at skip speed; prints per-script result
extern int g_advLastNative; extern bool g_advNoMenu;
static int scanAll(const std::string& filter) {
    g_advNoMenu = true;
    g_headless = true;
    SDL_Init(SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER);
    if (!g_gfx.init(true, 1, false)) return 1;
    g_audio.init(false); g_audio.loadCues("sound_se.awb");
    g_fontM.load("font_m.fnt"); g_fontS.load("font_s.fnt");
    g_sys.defaults(); g_sys.skipAll() = 1; g_game.reset();
    auto names = g_fs.names(); std::sort(names.begin(), names.end());
    int ok = 0, bad = 0; long frames = 0;
    for (auto& n : names) {
        if (n.size() < 4 || n.substr(n.size() - 4) != ".ssb" || n.rfind("SC", 0) != 0) continue;
        if (!filter.empty() && n.find(filter) == std::string::npos) continue;
        Job root; g_advScript = n; root.push(0xb);
        bool quit = false; int f = 0; const int limit = 400000;
        g_trans.reveal(0);
        for (; f < limit; f++) {
            static const uint32_t cyc[4] = {BTN_CIRCLE, BTN_SQUARE, BTN_CROSS, BTN_TRIANGLE};
            g_injectButtons |= BTN_R | (f % 2 ? cyc[(f / 2) % 4] : 0);
            pumpEvents(quit); g_trans.update(); g_popup.update(); g_help.update();
            root.update();
            if (f > 2 && !root.hasChild()) break;
        }
        frames += f;
        if (f >= limit) { printf("TIMEOUT %s waiting in native %d\n", n.c_str(), g_advLastNative); bad++; } else ok++;
    }
    printf("scan: %d finished, %d timed out, %ld frames\n", ok, bad, frames);
    return 0;
}

static int g_devScene = -1;   // dev: --scene ID pushes one scene (ids as in createScene) instead of the boot chain
static int runMain(bool headless, int maxFrames, int shotEvery, const std::string& dir, const std::string& keys, const std::string& playScript, bool autoClick) {
    g_headless = headless;
    SDL_Init(SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER);
    if (!g_gfx.init(headless, 3, false)) return 1;
    g_audio.init(!headless);
    g_fontM.load("font_m.fnt"); g_fontS.load("font_s.fnt");
    RootJob root; g_root = &root; root.onEnter();
    if (!playScript.empty()) { // dev: jump straight into a scenario file
        root.popAll(); g_advScript = playScript; g_game.reset(); g_sys.load();
        root.push(0xb);
    }
    if (g_devScene >= 0) { root.popAll(); g_game.reset(); g_sys.load(); root.push(g_devScene); }
    bool quit = false; uint64_t next = SDL_GetTicks64() * 1000;
    // scripted keys: "frame:button,frame:button" using PSP masks, for automated testing
    std::vector<std::pair<int,uint32_t>> script;
    for (size_t p = 0; p < keys.size();) {
        size_t e = keys.find(',', p); if (e == std::string::npos) e = keys.size();
        auto t = keys.substr(p, e - p); auto c = t.find(':');
        if (c != std::string::npos) {
            std::string v = t.substr(c + 1);   // PSP mask, or f5 / f8 for a real key event
            uint32_t m = v == "f5" ? kFakeF5 : v == "f8" ? kFakeF8 : (uint32_t)strtoul(v.c_str(), nullptr, 0);
            script.push_back({atoi(t.c_str()), m});
        }
        p = e + 1;
    }
    while (!quit && (maxFrames == 0 || (int)g_frame < maxFrames)) {
        for (auto& k : script) if (k.first == (int)g_frame) {
            if (k.second == kFakeF5 || k.second == kFakeF8) {
                SDL_Event ev{}; ev.type = SDL_KEYDOWN; ev.key.keysym.sym = k.second == kFakeF5 ? SDLK_F5 : SDLK_F8;
                SDL_PushEvent(&ev);
            } else g_injectButtons |= k.second;
        }
        if (autoClick && g_frame % 30 == 7) g_injectButtons |= BTN_CIRCLE;
        if (!playScript.empty() && g_frame > 2 && !root.hasChild()) break;
        pumpEvents(quit);
        if (playScript.empty()) root.onUpdate();
        else { g_trans.update(); g_popup.update(); g_help.update(); }
        root.update();
        g_gfx.beginFrame();
        root.draw(); root.overlayDraw(); trDrawToast();
        if (shotEvery && g_frame % shotEvery == 0) g_gfx.screenshot(dir + "/f" + std::to_string(g_frame) + ".bmp");
        g_gfx.endFrame();
        g_frame++;
        if (!headless) {
            next += 16667;
            uint64_t now = SDL_GetTicks64() * 1000;
            if (next > now) SDL_Delay((Uint32)((next - now) / 1000)); else next = now;
        }
    }
    g_audio.shutdown(); g_gfx.shutdown(); SDL_Quit();
    return 0;
}

static int testMovie(const char* name, const char* out) {
    if (!g_gfx.init(true, 1, false)) return 1;
    g_audio.init(false);
    Movie m;
    if (!m.open(name)) { fprintf(stderr, "open failed\n"); return 1; }
    uint64_t t0 = SDL_GetTicks64();
    while (SDL_GetTicks64() - t0 < 3000) { if (!m.update()) { puts("ended"); break; } SDL_Delay(5); }
    g_gfx.beginFrame(); g_gfx.clear({0, 0, 0, 255}); m.draw(); g_gfx.screenshot(out);
    puts("ok");
    return 0;
}

int main(int argc, char** argv) {
    std::string iso;
    std::vector<std::string> args(argv + 1, argv + argc);
    for (size_t i = 0; i + 1 < args.size(); i++) if (args[i] == "--iso") iso = args[i + 1];
    if (iso.empty()) iso = findIso(argv[0]);
    if (iso.empty() || !g_fs.mount(iso)) {
        fprintf(stderr, "Cannot open the game image (%s). Use --iso <path>. %s\n", iso.c_str(), g_fs.error.c_str());
        return 2;
    }
    for (size_t i = 0; i + 1 < args.size(); i++) if (args[i] == "--tl" && !trInit(args[i + 1])) return 2;
    for (size_t i = 0; i + 1 < args.size(); i++) if (args[i] == "--tl-dump") { g_fontM.load("font_m.fnt"); trDump(args[i + 1]); return 0; }
    for (size_t i = 0; i < args.size(); i++) {
        if (args[i] == "--test-audio" && i + 2 < args.size()) return testAudio(args[i+1].c_str(), args[i+2].c_str());
        if (args[i] == "--scan") return scanAll(i + 1 < args.size() && args[i+1][0] != '-' ? args[i+1] : "");
        if (args[i] == "--test-movie" && i + 2 < args.size()) return testMovie(args[i+1].c_str(), args[i+2].c_str());
        if (args[i] == "--test-vm" && i + 1 < args.size()) return testVm(args[i+1].c_str());
        if (args[i] == "--test-image" && i + 2 < args.size()) return testImage(args[i+1].c_str(), args[i+2].c_str());
    }
    {
        int fr = 0, every = 0; std::string dir = ".", keys, play; bool hl = false, ac = false;
        for (size_t j = 0; j < args.size(); j++) {
            if (args[j] == "--frames" && j + 1 < args.size()) fr = atoi(args[j+1].c_str());
            if (args[j] == "--shot-every" && j + 2 < args.size()) { every = atoi(args[j+1].c_str()); dir = args[j+2]; }
            if (args[j] == "--keys" && j + 1 < args.size()) keys = args[j+1];
            if (args[j] == "--headless") hl = true;
            if (args[j] == "--auto") ac = true;
            if (args[j] == "--scene" && j + 1 < args.size()) g_devScene = (int)strtol(args[j+1].c_str(), nullptr, 0);
            if (args[j] == "--play" && j + 1 < args.size()) play = args[j+1];
        }
        return runMain(hl, fr, every, dir, keys, play, ac);
    }
}
