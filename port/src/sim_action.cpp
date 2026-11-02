// Job 9: action scene (spec_sim sections 5, 6).
#include "tr.h"
#include "adv.h"
#include "sim_scenes.h"
#include "sim_ui.h"
#include <cstdio>
#include <cstring>

namespace {
const float kPos[6][2] = {{60, 58}, {-34, 58}, {154, 58}, {154, -27}, {60, -27}, {-112, -27}};
const float kHud[6][2] = {{60, 90}, {-34, 90}, {154, 90}, {154, 5}, {60, 5}, {-112, 5}};
const float kAway[4][2] = {{-168, -27}, {-168, -27}, {-168, -27}, {-128, 72}};   // ASSUMED order of the away table

// sheet order differs from character order
const int kSheetOfChar[6] = {0, 2, 1, 3, 4, 5};    // msr msk nnm zin rns oth -> anim index
const char* kSheet[7] = {"action_ch_srt.txg", "action_ch_msr.txg", "action_ch_msk.txg", "action_ch_nnm.txg", "action_ch_zin.txg", "action_ch_rns.txg", "action_ch_oth.txg"};

struct Actor {
    TexP tex; const SQ* quads = nullptr; const SimAnim* anims = nullptr;
    int anim = 0, tick = 0, frame = 0; float x = 0, y = 0;
    void set(int a, float nx, float ny) { anim = a; x = nx; y = ny; tick = 0; frame = 0; }
    void update() { if (++tick >= anims[anim].period) { tick = 0; frame ^= 1; } }
    void draw() {
        if (!tex) return;
        const SimAnim& a = anims[anim];
        q(*tex, quads[frame ? a.f1 : a.f0], x + a.dx, y + a.dy);
    }
};

struct Row { int face[2]; std::string text; int id, kind, sub; };

// status panel +0x11ebec
struct Panel {
    TexP win, faces;
    std::vector<Row> rows; int shown = 0, st = 0, t = 0;   // st 0 show row, 1 scroll, 2 done
    void init() { win = g_gfx.load("action_window.txg"); faces = win; }
    void reset() { rows.clear(); shown = 0; st = 2; t = 0; }
    void voice(const Row& r) {
        const char* v = nullptr; int c = r.id;
        if (r.kind == 6) v = kVoiceKind6[(c * 6 + r.sub) % 42];
        else if (r.kind == 7) v = kVoiceKind7[(c * 3 + r.sub) % 21];
        else if (r.kind == 8) v = kVoiceKind8[(c * 4 + r.sub) % 28];
        else v = kVoiceBase[(c * 9 + r.kind) % 63];
        if (v) g_audio.play(CH_VOICE, v, false);
    }
    void start() {
        if (rows.empty()) { st = 2; return; }
        shown = 1; st = 0; t = 0; voice(rows[0]);
    }
    void update() {
        if (st == 0) {
            t++;
            if (g_input.isPressed(BTN_CIRCLE)) t += 9999;
            if (g_input.isHeld(BTN_R)) t++;
            if (t >= 90) {
                g_audio.stop(CH_VOICE, 5); t = 0;
                st = shown < (int)rows.size() ? 1 : 2;
            }
        } else if (st == 1) {
            t++;
            if (g_input.isHeld(BTN_CIRCLE) || g_input.isHeld(BTN_R)) t++;
            if (t >= 30) { shown++; t = 0; st = 0; voice(rows[shown - 1]); }
        }
    }
    bool done() const { return st == 2; }
    void draw() {
        if (win) qStrip(*win, T_1eb2c4[0], T_1eb2c4[1], 0, -140);
        float sc = st == 1 ? -32.f * t / 30 : 0;
        for (int i = std::max(0, shown - 3); i < shown; i++) {
            float y = -140 + 56 + (i - (shown - 1)) * 32.f + sc, x = -208, fx = x;
            if (y < -170) continue;
            for (int f : rows[i].face) if (f >= 0) { q(*faces, T_1eb374[f], fx, y); fx += 32; }
            simText(rows[i].text, fx, y + 8, 14, {0, 0, 0, 255});
        }
    }
};

std::string fmt(const char* f, const std::string& name, int done, int cost) {
    char b[256];
    snprintf(b, sizeof b, tr(f).c_str(), name.c_str(), kDigitStr[done / 3], kDigitStr[(done % 3) * 3], kDigitStr[cost / 3], kDigitStr[(cost % 3) * 3]);
    return b;
}

struct Action : Job, NativeHandler {
    TexP bg[2]; TexP icon; int bgIdx = 0, bgPrev = 0, bgFade = 30;
    Actor sora, hero[6]; Panel panel; Toast toast; PageHeader hdr;
    Vm vm; Launcher launcher;
    int st = 0, count = 0;
    int soraAction = 0, meetingTarget = -1, breakTarget = -1, code[6] = {};
    struct Log { int type, task; }; std::vector<Log> log; size_t logPos = 0;

    void setBg(int i) { if (i != bgIdx) { bgPrev = bgIdx; bgIdx = i; bgFade = 0; } }
    void onEnter() override {
        bg[0] = g_gfx.load("action_bg.txgtl"); bg[1] = g_gfx.load("action_bg02.txgtl");
        icon = g_gfx.load("action_icon.txg");
        static const SQ* Q[7] = {T_1ec228, T_1ec730, T_1ed280, T_1ecc38, T_1ed788, T_1edbf0, T_1ee058};
        sora.tex = g_gfx.load(kSheet[0]); sora.quads = Q[0]; sora.anims = kAnimSrt;
        for (int c = 0; c < 6; c++) {
            Actor& a = hero[c]; a.tex = g_gfx.load(kSheet[c + 1]); a.quads = Q[c + 1]; a.anims = kAnimHero[kSheetOfChar[c]];
            a.set(0x10, kPos[c][0], kPos[c][1]);
        }
        sora.set(8, -34, -27);
        panel.init(); panel.reset(); toast.init();
        vm.handler = this; vm.load(g_fs.read("script_system_action.ssb"), "script_system_action.ssb");
        setBg(gd().period == 2 ? 1 : 0); bgFade = 30;
        hdr.set(4 + std::min(gd().period, 2));
        g_trans.style(0); g_trans.reveal(10);
        if (!g_audio.playing(CH_BGM)) g_audio.play(CH_BGM, "SK_BGM_02.adx", true, 10);
        st = 1;
    }
    void onResume() override { launcher.resumed(); }
    // ---- natives 79..86 ----
    int native(Vm& v, int id, int32_t& ret) override {
        ret = 0;
        if (id < 59) return baseNative(v, id, ret);
        switch (id) {
        case 79: g_advScript = v.argStr(0); launcher.start(this, 0xb); break;
        case 80: case 82: if (launcher.busy()) return 1; break;
        case 81: startSlump(v.arg(0)); break;
        case 83: ret = soraAction; break;
        case 84: ret = meetingTarget; break;
        case 85: ret = breakTarget; break;
        case 86: ret = code[v.arg(0)]; break;
        }
        return 0;
    }
    void startSlump(int c) {   // FUN_00026bb4
        const auto& S = simTables().slumps; GD& g = gd();
        bool dayOff = kCalendar[std::clamp(g.turn, 0, 102)].dayOff != 0; int p = g.period;
        std::vector<int> A, B;
        for (int i = 0; i < (int)S.size(); i++) {
            const SlumpDef& r = S[i];
            if (r.character != c || r.minTurn > g.turn || g.turn > r.maxTurn) continue;
            if (r.workdayOnly && dayOff) continue;
            if ((r.periodMask >> p) & 1) {
                if (g.slumpSeen[i] < 1) A.push_back(i);
                if (r.repeatable) B.push_back(i);
            }
        }
        const auto& pool = !A.empty() ? A : B;
        if (pool.empty()) return;   // guard: the original divides by zero here [ASSUMED intent]
        int total = 0; for (int i : pool) total += S[i].weight;
        int k = simRand() % total, pick = pool.back();
        for (int i : pool) { k -= S[i].weight; if (k < 0) { pick = i; break; } }
        g.slumpSeen[pick]++;
        g_advScript = S[pick].file; launcher.start(this, 0xb);
    }
    // ---- 5.1 ----
    void decide() {
        GD& g = gd();
        for (int c = 0; c < 6; c++) {
            int rest = g.queue[c].count - g.queue[c].doneIndex;
            if (!g.available[c]) code[c] = 10;
            else switch (g.cs[c].state) {
                case 0: code[c] = rest > 0 ? 2 : 0; break;
                case 1: code[c] = rest > 0 ? 4 : 0; break;
                case 2: code[c] = 6; break;
                case 3: code[c] = 8; break;
                default: code[c] = 0;
            }
        }
        meetingTarget = breakTarget = -1;
        PlanSlot p = g.plan[std::clamp(g.period, 0, 2)];
        switch (p.kind) {
        case 0: soraAction = g.selectedTask >= 0 ? 1 : 0; break;
        case 1: {
            meetingTarget = p.target; int& t = code[p.target];
            if (t == 0) { soraAction = 2; t = 1; } else if (t == 2) { soraAction = 3; t = 3; }
            else if (t == 4) { soraAction = 4; t = 5; } else if (t == 6) { soraAction = 5; t = 7; }
            else soraAction = 6;
            break; }
        case 2: soraAction = 7; break;
        case 3: breakTarget = p.target;
            if (code[p.target] == 10) soraAction = 9; else { soraAction = 8; code[p.target] = 9; }
            break;
        default: soraAction = 0;
        }
    }
    // ---- 5.2 ----
    void setupSprites() {
        GD& g = gd(); const auto& tb = simTables();
        panel.reset();
        const float hx = -34, hy = -27;
        auto row = [&](int f0, int f1, const std::string& s, int id, int kind, int sub = 0) { panel.rows.push_back({{f0, f1}, s, id, kind, sub}); };
        int t = meetingTarget;
        switch (soraAction) {
        case 0: sora.set(8, hx, hy); row(0, -1, "作業なし", 0, 0); break;
        case 1: {
            sora.set(0, hx, hy);
            const TaskDef& d = tb.tasks[g.selectedTask];
            row(0, -1, fmt("%s 仕様作成中 %s.%s/%s.%s日", stripNewlines(d.shortName), g.task[g.selectedTask].planDone + 1, d.planCost), 0, 7, simRand() % 3);
            break; }
        case 2: case 3: case 5: sora.set(2, kPos[t][0], kPos[t][1]); break;
        case 4: sora.set(3, kPos[t][0], kPos[t][1]); break;
        case 6: sora.set(8, hx, hy); row(0, -1, "ミーティングなし", 0, 0); break;
        case 7: { int r = simRand() & 3; sora.set(4 + r, kAway[r][0], kAway[r][1]); row(0, -1, "さくら荘の仕事中", 0, 8, r); break; }
        case 8: sora.set(8, kPos[breakTarget][0], kPos[breakTarget][1]); break;
        case 9: sora.set(8, hx, hy); row(0, -1, "お休みなし", 0, 0); break;
        }
        for (int c = 0; c < 6; c++) {
            int cd = code[c], anim = 0x10, kind = 0; std::string s; int f1 = -1;
            const CharQueue& cq = g.queue[c];
            int tk = cq.doneIndex < cq.count ? cq.task[cq.doneIndex] : -1;
            const TaskDef* d = tk >= 0 ? &tb.tasks[tk] : nullptr;
            std::string nm = d ? stripNewlines(d->shortName) : ""; int done = tk >= 0 ? g.task[tk].workDone : 0, cost = d ? d->workCost : 0;
            int wa = d ? d->workAnim : 1;
            switch (cd) {
            case 0: anim = 0x10; s = "作業なし"; kind = 0; break;
            case 1: anim = 0xc; s = "ミーティング中"; kind = 2; f1 = 0; break;
            case 2: anim = wa; s = fmt("%s 作業中 %s.%s/%s.%s日", nm, done + 1, cost); kind = 6; break;
            case 3: anim = 0xc; s = fmt("%s ミーティング中 %s.%s/%s.%s日", nm, done + 1, cost); kind = 2; f1 = 0; break;
            case 4: anim = wa + 6; s = fmt("%s 倍速作業中 %s.%s/%s.%s日", nm, done + 2, cost); kind = 6; break;
            case 5: anim = 0xd; s = fmt("%s 倍速ミーティング中 %s.%s/%s.%s日", nm, done + 2, cost); kind = 2; f1 = 0; break;
            case 6: anim = 0x11; s = "悩み中"; kind = 3; break;
            case 7: anim = 0xc; s = "ミーティング中"; kind = 2; f1 = 0; break;
            case 8: anim = 0xf; s = "作業不可"; kind = 4; break;
            case 9: anim = 0xe; s = "お休み"; kind = 1; f1 = 0; break;
            case 10: anim = 0x12; s = "不在"; kind = 5; break;
            }
            hero[c].set(anim, kPos[c][0], kPos[c][1]);
            int sub = cd == 2 || cd == 4 ? wa : 0;
            row(c + 1, f1, s, c + 1, kind, sub);
        }
        panel.start();
    }
    // ---- 5.4 ----
    void workUnit(int c) {
        GD& g = gd(); CharQueue& cq = g.queue[c];
        if (cq.doneIndex >= cq.count) return;
        int t = cq.task[cq.doneIndex];
        g.task[t].workDone++; taskUpdate(t); taskUpdateAll();
        if (g.task[t].state > 4) {
            cq.doneIndex++;
            for (int i = 0; i < 6; i++) g.total[i] += simTables().tasks[t].gains[i];
            log.push_back({1, t});
        }
    }
    void resolve() {
        GD& g = gd(); const auto& tb = simTables(); int n = (int)tb.tasks.size();
        log.clear(); logPos = 0;
        std::vector<int> before(n);
        auto snap = [&] { for (int i = 0; i < n; i++) before[i] = g.task[i].prereqOK; };
        auto diff = [&] { for (int i = 0; i < n; i++) if (!before[i] && g.task[i].prereqOK) log.push_back({2, i}); };
        snap();
        int t = g.selectedTask;
        if (soraAction == 1) {
            g.task[t].planDone++; taskUpdate(t);
            if (g.task[t].state > 2) taskRevealChildren(t);
            taskUpdateAll();
            if (g.task[t].state > 2) log.push_back({0, t});
        }
        diff();
        for (int c = 0; c < 6; c++) {
            snap();
            if (code[c] == 2 || code[c] == 3) workUnit(c);
            else if (code[c] == 4 || code[c] == 5) { workUnit(c); workUnit(c); }
            diff();
        }
        if (soraAction == 1 && g.task[t].state > 2) {
            CharQueue& cq = g.queue[tb.tasks[t].owner];
            if (cq.count < 32) cq.task[cq.count++] = t;
        }
    }
    void showLog(const Log& l) {
        const TaskDef& d = simTables().tasks[l.task];
        char b[256]; std::string nm = stripNewlines(d.name);
        if (l.type == 0) { snprintf(b, sizeof b, tr("%s\n仕様が完成しました").c_str(), nm.c_str()); toast.show(b, 1, 0, 3, d.key != 0); }
        else if (l.type == 1) {
            uint32_t bits = 0; for (int i = 0; i < 6; i++) if (d.gains[i] > 3) bits |= 1u << i;
            snprintf(b, sizeof b, tr("%s\n作業が完了しました").c_str(), nm.c_str()); toast.show(b, d.owner + 2, bits, 3, d.key != 0);
        } else { snprintf(b, sizeof b, tr("%s\n他のタスクによる[LOCK]が解除されました").c_str(), nm.c_str()); toast.show(b, 0, 0, 3, d.key != 0); }
    }
    void enterA3() {
        st = 3; count = 0;
        hdr.set(4 + gd().period); hdr.start();
        if (gd().period == 2) setBg(1);
    }
    void onUpdate() override {
        launcher.update();
        if (bgFade < 30) bgFade++;
        sora.update(); for (auto& h : hero) h.update();
        hdr.update(); toast.update();
        GD& g = gd();
        switch (st) {
        case 1: if (g_trans.clear()) { if (g.period < 3) enterA3(); else toA2(); } break;
        case 2: if (g_trans.covered()) { st = 99; selfPop(); } break;
        case 3: if (++count >= 30) {
            st = 4;
            PlanSlot p = g.plan[g.period];
            if (p.kind == 0 && (g.selectedTask < 0 || g.task[g.selectedTask].state > 2)) { simChartMode(1); launcher.start(this, 0xa); }
        } break;
        case 4: if (!launcher.busy()) { decide(); setupSprites(); st = 5; } break;
        case 5: panel.update(); if (panel.done()) { st = 6; vm.reset(); } break;
        case 6: vm.run(); if (vm.halted()) { resolve(); st = 7; } break;
        case 7:
            if (!toast.active()) {
                if (logPos < log.size()) showLog(log[logPos++]);
                else { st = 8; count = 0; hdr.leave(); }
            }
            break;
        case 8: if (++count >= 30) {
            g.period++;
            if (g.period < 3) enterA3(); else toA2();
        } break;
        }
    }
    void toA2() { st = 2; g_trans.style(0); g_trans.cover(10); g_audio.stop(CH_BGM, 10); }
    void onDraw() override {
        if (bg[bgIdx]) {
            if (bgFade < 30 && bg[bgPrev]) { g_gfx.draw(*bg[bgPrev], 0, 0); g_gfx.draw(*bg[bgIdx], 0, 0, bgFade / 30.f); }
            else g_gfx.draw(*bg[bgIdx], 0, 0);
        }
        if (st >= 5) {
            if (soraAction != 8) sora.draw();
            for (auto& h : hero) h.draw();
            GD& g = gd();
            for (int c = 0; c < 6; c++) {      // FUN_00024bd8
                float x = kHud[c][0], y = kHud[c][1];
                if (!icon) break;
                ui(*icon, {0, 0, 32, 16}, x, y);
                const CharState& s = g.cs[c];
                q(*icon, T_1eacc8[s.state == 2 ? 3 : std::clamp(s.moodIcon, 0, 2)], x - 8, y);
                q(*icon, T_1eacc8[s.state == 3 ? 7 : 4 + std::clamp(s.healthIcon, 0, 2)], x + 8, y);
            }
            panel.draw();
        }
        hdr.draw(); toast.draw();
    }
    // ---- save ----
    struct Blob { int32_t st, count, soraAction, meetingTarget, breakTarget, code[6], launcher, bgIdx; };
    void saveState(Bytes& o) override {
        Blob b{st, count, soraAction, meetingTarget, breakTarget, {}, launcher.state, bgIdx};
        memcpy(b.code, code, sizeof code);
        Vm::State vs = vm.save();
        o.insert(o.end(), (uint8_t*)&b, (uint8_t*)(&b + 1));
        o.insert(o.end(), (uint8_t*)&vs, (uint8_t*)(&vs + 1));
    }
    void loadState(const uint8_t* p, size_t n) override {
        if (n < sizeof(Blob) + sizeof(Vm::State)) return;
        Blob b; memcpy(&b, p, sizeof b);
        Vm::State vs; memcpy(&vs, p + sizeof b, sizeof vs);
        st = b.st; count = b.count; soraAction = b.soraAction; meetingTarget = b.meetingTarget; breakTarget = b.breakTarget;
        memcpy(code, b.code, sizeof code); bgIdx = bgPrev = b.bgIdx; bgFade = 30;
        vm.restore(vs);
        // a saved scene always sits under the ADV job it launched
        if (b.launcher) { launcher.restore(this); g_audio.stop(CH_BGM); }
        if (st >= 5) setupSprites();
        if (st >= 5) panel.st = 2;
        hdr.set(4 + std::min(gd().period, 2)); hdr.start();
    }
};
}
std::unique_ptr<Job> makeAction() { return std::make_unique<Action>(); }
