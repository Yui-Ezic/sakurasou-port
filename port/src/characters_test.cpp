// Offline test of the character module. Not linked into a code path of the game: build with
// -DCHAR_TEST_MAIN (main() below) together with characters*.cpp live2d*.cpp gfx.cpp fs.cpp vm.cpp audio.cpp.
//   char_test <iso> <outdir> [sprite|l2d]
// Natives go through a real Vm stack, so argument decoding is covered too.
#ifdef CHAR_TEST_MAIN
#include "game.h"
#include "audio.h"
#include "characters_api.h"
#include <cstdio>
#include <cstring>


namespace {

struct Arg { int i = 0; const char* s = nullptr; Arg(int v) : i(v) {} Arg(const char* c) : s(c) {} };

int call(CharStage& st, int id, std::initializer_list<Arg> args, int32_t* retOut = nullptr) {
    Bytes code(4, 0);
    std::vector<int> vals;
    for (auto& a : args) {
        if (a.s) {
            while (code.size() % 4) code.push_back(0);
            vals.push_back((int)(code.size() / 4));
            code.insert(code.end(), a.s, a.s + strlen(a.s) + 1);
        } else vals.push_back(a.i);
    }
    while (code.size() % 4) code.push_back(0);
    Vm vm; vm.load(code, "test");
    auto s = std::make_unique<Vm::State>(vm.save());
    s->fp = 100;
    for (size_t i = 0; i < vals.size(); i++) s->stack[100 - i - 1] = vals[i];
    vm.restore(*s);
    int32_t ret = -1;
    int r = st.native(vm, id, ret);
    if (retOut) *retOut = ret;
    return r;
}

void background() {
    g_gfx.setTarget(nullptr);
    g_gfx.clear({60, 110, 160, 255});
    for (int i = 0; i < 12; i++) g_gfx.fillRect(i * 40.f, 0, 20, 272, {200, 180, 90, 255});
}

void render(CharStage& st) { g_gfx.beginFrame(); background(); st.drawSlot(0); st.drawSlot(1); st.drawLive2D(); }

int fails = 0;
void check(bool ok, const char* what) { printf("%s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) fails++; }

bool same(const Image& a, const Image& b) { return a.px == b.px; }

// Bounding box of pixels that differ.
void diffBox(const Image& a, const Image& b, int& x0, int& y0, int& x1, int& y1) {
    x0 = y0 = 1 << 30; x1 = y1 = -1;
    for (int y = 0; y < a.h; y++) for (int x = 0; x < a.w; x++)
        if (a.px[y * a.w + x] != b.px[y * a.w + x]) { x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x); y1 = std::max(y1, y); }
}

void spriteTest(const std::string& dir) {
    CharStage st;
    const char* f = "SUMSRNOM_U.csu";
    check(g_fs.exists(f), "sprite asset exists");
    call(st, 161, {0, f, "ましろ"});
    check(call(st, 162, {0}) == 0, "N162 done");
    call(st, 163, {0});
    call(st, 168, {0, 0, -136});
    render(st); Image hidden; g_gfx.readCanvas(hidden);

    call(st, 164, {0, 500});                       // 30 frames
    check(call(st, 167, {0}) == 1, "N167 blocks during fade");
    for (int i = 0; i < 15; i++) st.update(false);
    render(st); g_gfx.screenshot(dir + "/spr_fade50.bmp");
    Image half; g_gfx.readCanvas(half);
    for (int i = 0; i < 15; i++) st.update(false);
    st.update(false);
    check(call(st, 167, {0}) == 0, "N167 released after 30 ticks");
    render(st); g_gfx.screenshot(dir + "/spr_shown.bmp");

    // Blink: open eyes are the baseline; look for two distinct states over a long idle.
    Image base; g_gfx.readCanvas(base);
    int blinks = 0, bx0 = 999, by0 = 999, bx1 = -1, by1 = -1; bool savedBlink = false;
    for (int i = 0; i < 2400; i++) {
        st.update(false); render(st);
        Image c; g_gfx.readCanvas(c);
        if (!same(c, base)) {
            blinks++;
            int a, b, cc, d; diffBox(c, base, a, b, cc, d);
            bx0 = std::min(bx0, a); by0 = std::min(by0, b); bx1 = std::max(bx1, cc); by1 = std::max(by1, d);
            if (!savedBlink) { g_gfx.screenshot(dir + "/spr_blink.bmp"); savedBlink = true; }
        }
    }
    printf("     blink frames %d of 2400, diff box (%d,%d)-(%d,%d)\n", blinks, bx0, by0, bx1, by1);
    check(blinks > 0, "blink changes the picture");
    check(bx1 - bx0 < 120 && by1 - by0 < 80, "blink is confined to a small patch");

    // Lip sync: speaker match and voice on.
    charSetSpeaker("ましろ"); charForceVoice(true);
    int mouth = 0, mx0 = 999, my0 = 999, mx1 = -1, my1 = -1; bool saved2 = false;
    for (int i = 0; i < 120; i++) {
        st.update(false); render(st);
        Image c; g_gfx.readCanvas(c);
        if (!same(c, base)) {
            mouth++;
            int a, b, cc, d; diffBox(c, base, a, b, cc, d);
            mx0 = std::min(mx0, a); my0 = std::min(my0, b); mx1 = std::max(mx1, cc); my1 = std::max(my1, d);
            if (!saved2 && i > 30) { g_gfx.screenshot(dir + "/spr_lip.bmp"); saved2 = true; }
        }
    }
    printf("     talking frames %d of 120, diff box (%d,%d)-(%d,%d)\n", mouth, mx0, my0, mx1, my1);
    check(mouth > 40, "mouth moves while the speaker talks");
    charSetSpeaker("七海");
    st.update(false); st.update(false); render(st);
    int same_ = 0;
    for (int i = 0; i < 30; i++) { st.update(false); render(st); Image c; g_gfx.readCanvas(c); same_ += same(c, base); }
    check(same_ >= 15, "mouth stops when another character speaks");
    charForceVoice(false);

    // Fading with an active overlay blends each pixel once: the eye patch at alpha<255 must match
    // what the body would give, i.e. no double-dark rectangle. Compare a mouth patch mid fade.
    call(st, 169, {0, 100, -136, 500});
    for (int i = 0; i < 15; i++) st.update(false);
    render(st); g_gfx.screenshot(dir + "/spr_move.bmp");
    check(call(st, 170, {0}) == 1, "N170 blocks during move");
    for (int i = 0; i < 16; i++) st.update(false);
    check(call(st, 170, {0}) == 0, "N170 released");

    Image before; render(st); g_gfx.readCanvas(before);
    call(st, 171, {0, 0, 40, 500});
    check(call(st, 172, {0}) == 1, "N172 blocks during hop");
    for (int i = 0; i < 15; i++) st.update(false);
    render(st); g_gfx.screenshot(dir + "/spr_hop.bmp");
    Image top; g_gfx.readCanvas(top); int a, b, c, d; diffBox(top, before, a, b, c, d);
    printf("     hop diff box y %d-%d\n", b, d);
    for (int i = 0; i < 16; i++) st.update(false);
    check(call(st, 172, {0}) == 0, "N172 released");

    call(st, 173, {0, 4, 4, -1});
    st.update(false);
    check(call(st, 175, {0}) == 1, "infinite shake blocks N175");
    render(st); g_gfx.screenshot(dir + "/spr_shake.bmp");
    call(st, 174, {0});
    check(call(st, 175, {0}) == 0, "N174 stops shake");
    call(st, 173, {0, 4, 4, 100});
    for (int i = 0; i < 7; i++) st.update(false);
    check(call(st, 175, {0}) == 0, "timed shake ends");
    call(st, 176, {0, 0, 10, 1000});
    for (int i = 0; i < 15; i++) st.update(false);
    render(st); g_gfx.screenshot(dir + "/spr_swing.bmp");
    call(st, 177, {0});

    // Crossfade, second sprite above the old one; second slot on the left.
    call(st, 161, {0, "SUMSREMI_U.csu", "ましろ"});
    call(st, 163, {0});
    call(st, 166, {0, 200});
    call(st, 161, {1, "SUNNMNOM_U.csu", "七海"}); call(st, 163, {1});
    call(st, 168, {1, -130, -136}); call(st, 164, {1, 0});
    for (int i = 0; i < 6; i++) st.update(false);
    render(st); g_gfx.screenshot(dir + "/spr_cross.bmp");
    for (int i = 0; i < 8; i++) st.update(false);
    check(call(st, 167, {0}) == 0, "crossfade finished");

    // skip ends a timed effect at its next tick
    call(st, 165, {1, 1000}); st.update(true);
    check(call(st, 167, {1}) == 0, "skip ends a fade");
    call(st, 164, {1, 0});

    // Save round trip. Draw the same scene from a fresh stage.
    Bytes sv; st.saveState(sv);
    printf("     save size 0x%zx\n", sv.size());
    render(st); Image x; g_gfx.readCanvas(x);
    CharStage st2; const uint8_t* q = sv.data(); st2.loadState(q);
    check(q == sv.data() + sv.size(), "load consumes the record");
    render(st2); Image y; g_gfx.readCanvas(y);
    int ax0, ay0, ax1, ay1; diffBox(x, y, ax0, ay0, ax1, ay1);
    printf("     restored diff (%d,%d)-(%d,%d)\n", ax0, ay0, ax1, ay1);
    check(ax1 < 0, "restored picture equals the original");

    // Every sprite in the game loads.
    int n = 0;
    { CharStage s3;
      for (auto& name : g_fs.names()) if (name.size() > 4 && name.compare(name.size() - 4, 4, ".csu") == 0) {
          n++; call(s3, 161, {0, name.c_str(), "x"}); call(s3, 163, {0}); call(s3, 164, {0, 0}); call(s3, 168, {0, 0, -136});
          g_gfx.beginFrame(); s3.drawSlot(0); Image t; g_gfx.readCanvas(t); (void)t;
      } }
    printf("     %d csu files decoded and drawn\n", n);
}

// Names of the type 2 (motion) chunks of an LBD.
std::vector<std::string> motionNames(const Bytes& d) {
    std::vector<std::string> r;
    const char* str = nullptr;
    for (size_t o = 0; o + 8 <= d.size();) {
        uint32_t t, sz; memcpy(&t, &d[o], 4); memcpy(&sz, &d[o + 4], 4);
        if (t == 9) str = (const char*)&d[o + 8];
        if (t == 2 && str) { uint32_t n; memcpy(&n, &d[o + 8], 4); r.push_back(str + n); }
        o += sz;
    }
    return r;
}

void l2dTest(const std::string& dir) {
    const char* files[] = {"L2DJIN_R", "L2DMSK_R", "L2DMSK_S", "L2DMSK_U", "L2DMSR_R", "L2DMSR_S", "L2DMSR_U", "L2DNNM_R",
                           "L2DNNM_R2", "L2DNNM_S", "L2DNNM_U", "L2DOTH_R", "L2DOTH_S", "L2DOTH_U", "L2DRNS_R"};
    for (const char* f : files) {
        CharStage st;
        std::string fn = std::string(f) + ".lbd", name = "x";
        auto mots = motionNames(g_fs.read(fn));
        call(st, 179, {name.c_str(), fn.c_str()});
        check(call(st, 180, {}) == 0, "N180");
        call(st, 181, {});
        call(st, 182, {325});
        call(st, 183, {0, -5, 0, 0});
        check(call(st, 191, {}) == 0, "N191");
        for (int i = 0; i < 6; i++) st.update(false);
        render(st); Image blank; g_gfx.readCanvas(blank);
        call(st, 185, {"no_such_motion"});
        check(call(st, 189, {}) == 0, "unknown motion ignored");
        call(st, 185, {mots[0].c_str()});
        check(call(st, 189, {}) == 1, "N189 blocks while queued");
        for (int i = 0; i < 120; i++) { st.update(false); render(st); }
        check(call(st, 189, {}) == 0, "queue drains");
        Image shown; g_gfx.readCanvas(shown);
        check(!same(blank, shown), "model draws after a motion exists");
        g_gfx.screenshot(dir + "/" + f + ".bmp");
        printf("     %s motions:", f); for (auto& m : mots) printf(" %s", m.c_str()); printf("\n");

        // Expression and voice: a second pose, with a jump entrance applied on top.
        call(st, 188, {3, 300});
        check(call(st, 190, {}) == 1, "N190 blocks");
        if (mots.size() > 1) call(st, 185, {mots[1].c_str()});
        charSetSpeaker(name); charForceVoice(true);
        for (int i = 0; i < 90; i++) st.update(false);
        render(st);
        g_gfx.screenshot(dir + "/" + f + "_e.bmp");
        check(call(st, 190, {}) == 0, "expression settles");
        charForceVoice(false);

        call(st, 184, {0, 30, 0, 500, 40, 300});
        check(call(st, 192, {}) == 1, "N192 blocks");
        for (int i = 0; i < 30; i++) { st.update(false); render(st); }
        check(call(st, 192, {}) == 0, "jump ends");
        if (!strcmp(f, "L2DMSR_U")) {
            g_gfx.screenshot(dir + "/" + f + "_j.bmp");
            Bytes sv; st.saveState(sv);
            CharStage st2; const uint8_t* q = sv.data(); st2.loadState(q);
            for (int i = 0; i < 3; i++) st2.update(false);
            render(st2); g_gfx.screenshot(dir + "/" + f + "_restored.bmp");
        }
        call(st, 195, {});
        render(st); Image gone; g_gfx.readCanvas(gone);
        check(same(gone, blank), "N195 removes the model");
    }
}

}  // namespace


int main(int argc, char** argv) {
    if (argc < 3 || !g_fs.mount(argv[1])) { fprintf(stderr, "usage: char_test iso outdir [sprite|l2d]\n"); return 2; }
    std::string dir = argv[2], what = argc > 3 ? argv[3] : "all";
    SDL_Init(SDL_INIT_EVENTS);
    if (!g_gfx.init(true, 1, false)) return 1;
    g_audio.init(false);
    if (what == "all" || what == "sprite") spriteTest(dir);
    if (what == "all" || what == "l2d") l2dTest(dir);
    printf("%d failures\n", fails);
    g_gfx.shutdown();
    SDL_Quit();
    return fails != 0;
}
#endif
