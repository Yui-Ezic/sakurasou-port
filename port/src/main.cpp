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

static std::string exeDir() {
    char* b = SDL_GetBasePath();
    std::string d = b ? b : "";
    SDL_free(b);
    return d;
}

static std::string u8(const std::filesystem::path& p) { auto u = p.u8string(); return std::string(u.begin(), u.end()); }
static std::string lower(std::string s) { std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)tolower(c); }); return s; }
static bool isIsoName(const std::string& s) { return s.size() > 4 && lower(s.substr(s.size() - 4)) == ".iso"; }

// Where the chosen ISO is remembered, so the player only ever has to point at it once.
static std::string savedIsoFile() {
    char* b = SDL_GetPrefPath("Sakurasou", "SakurasouPort");
    std::string d = b ? b : "";
    SDL_free(b);
    return d.empty() ? "" : d + "iso_path.txt";
}
static std::string loadSavedIso() {
    std::string f = savedIsoFile(), p;
    if (FILE* fp = f.empty() ? nullptr : fopen(f.c_str(), "rb")) {
        char buf[4096] = {}; size_t n = fread(buf, 1, sizeof buf - 1, fp); fclose(fp);
        p.assign(buf, n);
        while (!p.empty() && (p.back() == '\n' || p.back() == '\r')) p.pop_back();
    }
    std::error_code ec;
    return !p.empty() && std::filesystem::exists(std::filesystem::path(p), ec) ? p : "";
}
static void saveIso(const std::string& iso) {
    std::error_code ec;
    std::string f = savedIsoFile();
    if (f.empty()) return;
    std::string abs = u8(std::filesystem::absolute(std::filesystem::path(iso), ec));
    if (FILE* fp = fopen(f.c_str(), "wb")) { fputs(abs.c_str(), fp); fclose(fp); }
}

// Candidate ISO files, best first: the remembered one, well-known names, then any .iso next to the program or in the
// current folder (players keep their own file name).
static std::vector<std::string> isoCandidates() {
    std::vector<std::string> out;
    auto add = [&](const std::string& p) { if (!p.empty() && std::find(out.begin(), out.end(), p) == out.end()) out.push_back(p); };
    std::error_code ec;
    add(loadSavedIso());
    std::string base = exeDir();
    for (auto p : {"sakuraso-no-pet-na-kanojo-psp.iso", "../sakuraso-no-pet-na-kanojo-psp.iso", "../../sakuraso-no-pet-na-kanojo-psp.iso", "game.iso"}) {
        if (std::filesystem::exists(p, ec)) add(p);
        if (!base.empty() && std::filesystem::exists(base + p, ec)) add(base + p);
    }
    for (std::string dir : {base, std::string(".")}) {
        if (dir.empty()) continue;
        for (auto& e : std::filesystem::directory_iterator(dir, ec))
            if (e.is_regular_file(ec) && isIsoName(u8(e.path()))) add(u8(e.path()));
    }
    return out;
}

// Nothing usable found: explain, then wait with a small window where the ISO can be dropped.
// Returns the dropped path, or "" if the player quit.
static std::string askForIso(const std::string& problem) {
    std::string msg = "To play, Sakurasou needs your own copy of the game: the PSP image of\n"
                      "\"Sakurasou no Pet na Kanojo\" (an .iso file). It is not included.\n\n";
    if (!problem.empty()) msg += problem + "\n\n";
    msg += "Next, drag the .iso file onto the window that opens.\n"
           "You can also copy it into the same folder as this program and start again.\n"
           "It is remembered afterwards.";
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) { fprintf(stderr, "%s\n", msg.c_str()); return ""; }
    SDL_MessageBoxButtonData btn[2] = {{SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Choose the file"},
                                       {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Quit"}};
    SDL_MessageBoxData box = {SDL_MESSAGEBOX_INFORMATION, nullptr, "Sakurasou - game image needed", msg.c_str(), 2, btn, nullptr};
    int hit = 0;
    if (SDL_ShowMessageBox(&box, &hit) < 0) { fprintf(stderr, "%s\n", msg.c_str()); return ""; }
    if (hit != 1) return "";
    SDL_Window* w = SDL_CreateWindow("Drag your game .iso file into this window (Esc to quit)", SDL_WINDOWPOS_CENTERED,
                                     SDL_WINDOWPOS_CENTERED, 640, 360, 0);
    if (!w) return "";
    SDL_Renderer* r = SDL_CreateRenderer(w, -1, 0);
    std::string got;
    bool done = false;
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
    while (!done) {
        SDL_Event e;
        while (SDL_WaitEventTimeout(&e, 100)) {
            if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) done = true;
            if (e.type == SDL_DROPFILE) { got = e.drop.file; SDL_free(e.drop.file); done = true; break; }
        }
        if (r) {   // a plain dashed "drop zone"
            SDL_SetRenderDrawColor(r, 34, 28, 44, 255); SDL_RenderClear(r);
            SDL_SetRenderDrawColor(r, 255, 160, 190, 255);
            for (int x = 30; x < 610; x += 24) { SDL_Rect a{x, 30, 14, 4}, b{x, 326, 14, 4}; SDL_RenderFillRect(r, &a); SDL_RenderFillRect(r, &b); }
            for (int y = 30; y < 330; y += 24) { SDL_Rect a{30, y, 4, 14}, b{606, y, 4, 14}; SDL_RenderFillRect(r, &a); SDL_RenderFillRect(r, &b); }
            SDL_RenderPresent(r);
        }
    }
    if (r) SDL_DestroyRenderer(r);
    SDL_DestroyWindow(w);
    return got;
}

// A release folder ships the translation as "translation/ru.pack" next to the executable.
static std::string findTranslation() {
    std::string d = exeDir() + "translation";
    return std::filesystem::exists(d + "/ru.pack") ? d : "";
}

static void fatal(const std::string& msg) {
    fprintf(stderr, "%s\n", msg.c_str());
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Sakurasou", msg.c_str(), nullptr);
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

// dev: --ctl FILE drives a run line by line (a FIFO for interactive use, a plain file to replay):
//   w N | p MASK [N] (press one frame, then wait N, default 10) | h MASK N (hold) | s PATH (screenshot)
//   skip 0|1 (read-skip everything) | rewind (help bar ticker back to its start) | nohelp |
//   chartfirst (chart cursor to its first selectable task) | push ID (start scene ID on top) | poke OFF VAL (int32 into GameData) | peek OFF [N] (print N int32 from GameData) | skipto JOB [MAX] (hold R and tap CIRCLE until job id JOB is the innermost
//   scene, at most MAX frames, default 100000; no CIRCLE on the sim's decision screens) | q.  Prints "ready FRAME" to stdout whenever it waits for the next line.
struct Ctl {
    FILE* f = nullptr; int wait = 0; uint32_t hold = 0; std::string shot; bool quit = false; int until = -1;
    static int innermost() { Job* j = g_root; while (j && j->top()) j = j->top(); return j && j != g_root ? j->id : -1; }
    void step() {
        if (!f) return;
        if (until >= 0) {
            int in = innermost();
            bool decides = in == 8 || in == 0xd || in == 0xe;   // sim screens where CIRCLE would choose (the square just takes its first task)
            if (in == until || !--wait) { until = -1; wait = 0; }
            else { g_injectButtons |= BTN_R | (g_frame % 8 == 0 && !decides ? BTN_CIRCLE : 0); return; }
        }
        if (wait > 0) { wait--; if (hold) g_injectButtons |= hold; if (!wait) hold = 0; return; }
        printf("ready %llu\n", (unsigned long long)g_frame); fflush(stdout);
        char line[1024];
        while (!wait && !quit) {
            if (!fgets(line, sizeof line, f)) { quit = true; break; }
            char cmd[16] = {}, arg[1000] = {}; int n = 0;
            int k = sscanf(line, "%15s %999s %d", cmd, arg, &n);
            if (k < 1 || cmd[0] == '#') continue;
            std::string c = cmd;
            uint32_t m = (uint32_t)strtoul(arg, nullptr, 0);
            if (c == "w") wait = atoi(arg);
            else if (c == "p") { g_injectButtons |= m; wait = k >= 3 ? n : 10; }
            else if (c == "h") { hold = m; wait = n; g_injectButtons |= m; }
            else if (c == "s") { shot = arg; wait = 1; }
            else if (c == "skip") g_sys.skipAll() = atoi(arg);
            else if (c == "rewind") g_help.rewind();
            else if (c == "nohelp") g_help.hide();
            else if (c == "push") g_root->push((int)strtol(arg, nullptr, 0));   // a scene over everything (e.g. 14, the contest)
            else if (c == "chartfirst") { extern bool g_devChartFirst; g_devChartFirst = true; }
            else if (c == "peek") { uint32_t o = (uint32_t)strtoul(arg, nullptr, 0); int32_t v = 0; for (int i = 0; i < std::max(n, 1); i++, o += 4) { if (o + 4 <= g_game.raw.size()) memcpy(&v, &g_game.raw[o], 4); printf("peek %#x %d\n", o, v); } fflush(stdout); }
            else if (c == "poke") { uint32_t o = (uint32_t)strtoul(arg, nullptr, 0); if (o + 4 <= g_game.raw.size()) memcpy(&g_game.raw[o], &n, 4); }
            else if (c == "skipto") { until = atoi(arg); wait = k >= 3 ? n : 100000; break; }
            else if (c == "q") quit = true;
            else fprintf(stderr, "ctl: unknown %s", line);
        }
    }
};
static std::string g_ctlPath;
static int runMain(bool headless, int maxFrames, int shotEvery, const std::string& dir, const std::string& keys, const std::string& playScript, bool autoClick) {
    g_headless = headless;
    SDL_Init(SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER);
    if (!g_gfx.init(headless, headless ? 1 : 3, false)) return 1;   // headless: no 3x window copy each frame
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
    Ctl ctl;
    if (!g_ctlPath.empty() && !(ctl.f = fopen(g_ctlPath.c_str(), "r+"))) { fprintf(stderr, "ctl: cannot open %s\n", g_ctlPath.c_str()); return 1; }
    while (!quit && (maxFrames == 0 || (int)g_frame < maxFrames)) {
        ctl.step();
        if (ctl.quit) break;
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
        if (!ctl.shot.empty()) { g_gfx.screenshot(ctl.shot); ctl.shot.clear(); }
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
    if (iso.empty())   // an .iso dragged onto the executable arrives as an argument
        for (auto& a : args) if (a[0] != '-' && isIsoName(a)) { iso = a; break; }
    bool mounted = false;
    std::string problem;
    if (!iso.empty()) {
        mounted = g_fs.mount(iso);
        if (!mounted) problem = "Cannot use \"" + iso + "\": " + g_fs.error + ".";
    } else {
        for (auto& c : isoCandidates()) if ((mounted = g_fs.mount(c))) { iso = c; break; }
    }
    while (!mounted) {
        iso = askForIso(problem);
        if (iso.empty()) return 2;
        mounted = g_fs.mount(iso);
        problem = mounted ? "" : "That file is not the PSP game image (" + g_fs.error + "). Please pick the .iso of \"Sakurasou no Pet na Kanojo\".";
    }
    saveIso(iso);
    bool tlGiven = false;
    for (size_t i = 0; i + 1 < args.size(); i++) if (args[i] == "--tl") {
        tlGiven = true;
        if (!trInit(args[i + 1])) { fatal("Cannot load the translation from " + args[i + 1]); return 2; }
    }
    if (!tlGiven && std::find(args.begin(), args.end(), "--no-tl") == args.end())
        if (std::string d = findTranslation(); !d.empty()) trInit(d);
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
            if (args[j] == "--ctl" && j + 1 < args.size()) g_ctlPath = args[j+1];
        }
        return runMain(hl, fr, every, dir, keys, play, ac);
    }
}
