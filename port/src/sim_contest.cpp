// Job 0xe: contest result (spec_sim section 7.3, 7.4).
// Comment selection is exact; the comment window layout is ASSUMED (spec: unresolved). The frame (FUN_0007c2b8 /
// FUN_0007c744) and the ranking list (FUN_000810dc, update FUN_000815f8, draw FUN_0007fc2c) follow the original.
#include "tr.h"
#include "sim_scenes.h"
#include "sim_ui.h"
#include "sim_contest_sprites.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace {
struct Contest : Job {
    TexP bg, index, window, parts, count, shot;
    int st = 0, f = 0, shown = 0, likes = 0, sub = 0, camera = 0, stage = 0, rows = 51;
    std::vector<int> slots; bool leaving = false;
    int bgDrift = 0;

    void pickComments() {
        const auto& C = simTables().comments; GD& g = gd();
        int s = stage, bucket = s <= 3 ? 1 : s < 9 ? 2 : s < 11 ? 3 : 4;
        int v[6], idx[6]; for (int i = 0; i < 6; i++) { v[i] = g.total[i]; idx[i] = i; }
        for (int i = 0; i < 6; i++) for (int j = i + 1; j < 6; j++) if (v[i] < v[j]) { std::swap(v[i], v[j]); std::swap(idx[i], idx[j]); }
        int best = idx[0] + 1, second = idx[1] + 1, worst = idx[5] + 1, secondWorst = idx[4] + 1;
        int score = gameScore();
        bool bad = score < simTables().teams[0].scoreMax[s];
        std::vector<int> PR, P1, P2, P3, P4, P5;
        for (int i = 0; i < (int)C.size(); i++) {
            const CommentDef& c = C[i];
            if (c.route != 0 && c.route == g.routeB && c.sentiment == (bad ? 1 : 0)) PR.push_back(i);
            else if (c.route == 0 && (c.stageBucket == 0 || c.stageBucket == bucket)) {
                int tag = c.tag;
                if (tag == best && c.sentiment == 0) P1.push_back(i);
                else if (tag == worst && c.sentiment == 1) P2.push_back(i);
                else if ((!bad && tag == second && c.sentiment == 0) || (bad && tag == secondWorst && c.sentiment == 1)) P3.push_back(i);
                else if (tag == 0) (c.sentiment == 0 ? P4 : P5).push_back(i);
            }
        }
        std::vector<int> pick, who;
        auto take = [&](std::vector<int>& p, int n) {
            for (int k = 0; k < n && !p.empty(); k++) {
                for (int tries = 0; tries < 200; tries++) {
                    int i = p[simRand() % p.size()], w = C[i].commenter;
                    if (std::find(who.begin(), who.end(), w) == who.end()) { pick.push_back(i); who.push_back(w); break; }
                }
            }
        };
        take(PR, 1);
        take(P1, bad ? 1 : 2); take(P2, bad ? 2 : 1); take(P3, 1);
        take(P4, bad ? (PR.empty() ? 2 : 1) : 4); take(P5, bad ? 4 : (PR.empty() ? 2 : 1));
        std::shuffle(pick.begin(), pick.end(), std::minstd_rand((unsigned)simRand()));
        slots = pick;
    }
    void onEnter() override {
        GD& g = gd(); stage = std::clamp(g.rankStage, 0, 11);
        bg = g_gfx.load("contest_bg.txg"); index = g_gfx.load("contest_index.txg"); window = g_gfx.load("contest_window.txg");
        parts = g_gfx.load("contest_parts.txg"); count = g_gfx.load("contest_count.txg");
        static const char* beta[7] = {"img_beta_msr", "img_beta_msr", "img_beta_rns", "img_beta_nnm", "img_beta_oth", "img_beta_msk", "img_beta_zin"};
        std::string im = stage < 4 ? "img_plan_01" : stage < 9 ? (g.routeB == 1 ? "img_alpha_01" : g.routeB == 3 ? "img_alpha_03" : "img_alpha_02") : beta[std::clamp(g.routeB, 0, 6)];
        shot = g_gfx.load(im + ".txg");
        pickComments();
        likes = g.team[0].score / 100;   // ASSUMED: spec names a "prior field" that is not stored in GameData
        const char* me = stage == 11 && (g.routeB == 5 || g.routeB == 6) ? "SK_ME_03.adx" : (g.routeB >= 1 && g.routeB <= 4) ? "SK_ME_02.adx" : "SK_ME_01.adx";
        g_audio.play(CH_BGM, me, false, 10);
        rank.init(stage);
    }
    void onResume() override { g_trans.style(0); g_trans.reveal(10); }
    void onUpdate() override {
        if (leaving) { if (g_trans.covered()) { g_audio.stop(CH_BGM, 10); st = 99; leaving = false; selfPop(); } return; }
        if (st == 99) return;
        bool skip = g_input.isPressed(BTN_CIRCLE);
        f++;
        switch (st) {
        case 0: if (g_trans.clear()) { st = 1; f = 0; } break;
        case 1:   // one comment at a time; ○ shows the next at once
            if ((skip || f >= 90) && shown < (int)slots.size()) {
                if (simTables().comments[slots[shown]].sentiment == 0) likes++;
                shown++; f = 0;
            } else if (skip && shown >= (int)slots.size()) { st = 2; f = 0; }
            break;
        case 2:
            if (rank.update()) { leaving = true; g_trans.style(0); g_trans.cover(10); }
            break;
        }
    }
    void drawComments() {
        const auto& C = simTables().comments;
        if (shot) { float s = 100.f / shot->w; ui(*shot, {0, 0, shot->w, shot->h}, 170, 50, 1, s, s); }
        int n = std::min(shown, 5), st0 = std::max(shown - 5, 0);
        for (int i = 0; i < n; i++) {
            const CommentDef& c = C[slots[st0 + i]];
            float y = 96 - i * 44.f;
            g_gfx.fillRect(240 - 236, 136 - y - 20, 340, 40, {255, 255, 255, 220});
            int id = c.commenter;
            ui(*window, {(id % 4) * 32, 32 + ((id / 4) % 6) * 32, (id % 4) * 32 + 32, 64 + ((id / 4) % 6) * 32}, -216, y);
            simText(c.who, -196, y + 19, 10, {40, 40, 120, 255}, 11);
            simText(c.body, -196 + 96, y + 17, 10, {0, 0, 0, 255}, 12);
        }
        ui(*count, {0, 0, 40, 32}, 80, -116);
        simText(std::to_string(likes), 110, -108, 14, {0, 0, 0, 255});
    }
    std::string genre() {
        GD& g = gd(); int p = g.routeA, r = g.routeB, k = 0;
        if (p == 3 || p == 4) k = r == 1 ? 2 : r == 5 ? 3 : r == 3 ? 4 : 1;
        else if (p == 5 || p == 6) k = r == 1 ? 5 : r == 2 ? 6 : r == 3 ? 7 : r == 5 ? 8 : r == 6 ? 9 : r == 4 ? 10 : 0;
        else if (p >= 7) k = r == 1 ? 11 : r == 2 ? 12 : r == 3 ? 13 : r == 5 ? 14 : r == 6 ? 15 : r == 4 ? 16 : 0;
        return kGenre[k];
    }
    void onDraw() override {   // FUN_0007e068: frame under, comments or ranking, frame over
        g_gfx.clear({210, 225, 235, 255});
        frameUnder();
        if (st == 2 || st == 99) rank.draw(*this); else drawComments();
        frameOver();
    }
    void frameUnder() {   // FUN_0007c2b8: stripes, side pieces, the strip under the bar
        const SQ* F = T_cframe;
        for (int i = 0; i < 34; i++) for (int j = 0; j < 6; j++) q(*bg, F[3], i * 14.f - 220, 80 - j * 40.f);
        for (int i = 0; i < 2; i++) { q(*bg, F[1], i * 464.f - 232, 36); q(*bg, F[2], i * 464.f - 232, -84); }
        q(*bg, F[0], 0, 84, 1, {}, 30, 1);
    }
    void frameOver() {    // FUN_0007c744: the bar with the stage tab and the date
        const SQ* F = T_cframe;
        q(*index, F[4], -228, 118); q(*index, F[5], -130, 118, 1, {}, 22, 1); q(*index, F[6], -28, 118);
        q(*index, F[7], -12, 118); q(*index, F[8], 84, 118); q(*index, F[9], 208, 118);
        int tab = stage < 4 ? 10 : stage < 9 ? 11 : stage < 11 ? 12 : 13;
        q(*index, F[tab], -130, 110);
        q(*index, F[std::clamp(stage + 14, 14, 25)], -220, 128);
    }

    // ---- ranking list (object +0x906c0) ----
    struct Row {   // FUN_0007fee0
        int team = -1, rank = 0; float y = 0, low = 0, high = 0; bool landed = false;
    };
    struct Particle { float x, y, tx, ty, s; int alpha = 128; };
    struct Ranking {
        TexP tex, ef, font, font02;
        std::vector<Row> rows; int n = 50, st = 0, t = 0, mine = 1;
        float gaugeX = -40; int markerA = 150; bool markerUp = true;
        float spark[4] = {0.5f, 0.8f, 1.1f, 1.4f}; bool sparkDown[4] = {false, true, false, true};
        int splashSt = 0, splashT = 0; float splashS = 1.4f, glowS = 2; int splashA = 0, glowA = 128; bool splashSe = false;
        Particle part[9];
        void init(int stage) {
            const GD& g = gd(); const auto& tb = simTables();
            n = stage <= 3 ? 50 : stage < 9 ? 40 : 30;
            tex = g_gfx.load("contest_ranking.txg"); ef = g_gfx.load("contest_ranking_ef.txg");
            font = g_gfx.load("contest_ranking_font.txg"); font02 = g_gfx.load("contest_ranking_font02.txg");
            mine = g.team[0].rank;
            rows.assign(n, Row{});
            for (int r = 1; r <= n; r++) {
                Row& w = rows[r - 1]; w.rank = r;
                for (int t = 0; t < (int)tb.teams.size() && t < 60; t++) if (g.team[t].rank == r) { w.team = t; break; }
                w.low = (n - r) * 50.f - 110; w.high = 60 - (r - 1) * 50.f;
                w.y = w.low + 300 + (n - r < 10 ? (n - r) * 20.f : 200.f);
            }
            static const float P[9][5] = {{-95, 35, -110, 80, .7f}, {-25, 42, -50, 80, 1}, {90, 45, 110, 80, .7f}, {110, 48, 150, 80, .4f},
                {-120, -12, -150, -50, .7f}, {-80, -25, -100, -50, .4f}, {70, -25, 100, -50, .7f}, {100, -25, 120, -50, .4f}, {120, -5, 160, -50, 1}};
            for (int k = 0; k < 9; k++) part[k] = {P[k][0], P[k][1], P[k][2], P[k][3], P[k][4]};
        }
        Row* row(int r) { return r >= 1 && r <= n ? &rows[r - 1] : nullptr; }
        void scroll(float d) { for (auto& w : rows) { if (d < 0 && w.y > w.high) w.y = std::max(w.y + d, w.high); if (d > 0 && w.y < w.low) w.y = std::min(w.y + d, w.low); } }
        // FUN_000815f8; true when CIRCLE closes the list
        bool update() {
            for (int i = 0; i < 4; i++) { spark[i] += sparkDown[i] ? -0.1f : 0.1f; if (spark[i] >= 1.5f) sparkDown[i] = true; if (spark[i] <= 0.5f) sparkDown[i] = false; }
            switch (st) {
            case 0: splash(); if (splashSt == 4) st = 3; break;
            case 3: if (++t > 60) { t = 0; st = 1; } break;
            case 1: gaugeX += 5; if (gaugeX >= 0) { gaugeX = 0; st = 2; } break;
            case 2:
                for (auto& w : rows) if (!w.landed) { w.y -= 20; if (w.y <= w.low) { w.y = w.low; w.landed = true; } }
                if (rows[0].landed) st = 4;
                break;
            case 4:
                if (mine > n - 3) { g_audio.playCue(0x25); st = 5; }
                else if (mine == 1 || mine == 2) { scroll(-10); if (rows[0].y <= rows[0].high) { g_audio.playCue(0x25); st = 5; } }
                else { scroll(-10); Row* m = row(mine); if (!m || m->y <= -10) { g_audio.playCue(0x25); st = 5; } }
                break;
            case 5:
                if (g_input.isHeld(BTN_DOWN)) scroll(10);
                else if (g_input.isHeld(BTN_UP)) scroll(-10);
                else if (g_input.isPressed(BTN_CIRCLE)) { g_audio.playCue(0x29); return true; }
                break;
            }
            return false;
        }
        void splash() {   // FUN_0007f200
            if (!splashSe) { g_audio.playCue(0x23); splashSe = true; }
            if (splashSt == 1 || splashSt == 2)
                for (auto& p : part) { if (p.alpha) { p.alpha -= 5; if (p.alpha < 21) p.alpha = 0; } p.x += (p.tx - p.x) * 0.04f; p.y += (p.ty - p.y) * 0.04f; }
            if (splashSt == 0) { splashS -= 0.025f; splashA = std::min(splashA + 15, 255); if (splashS <= 1) { splashA = 255; splashS = 1; splashSt = 1; } }
            else if (splashSt == 1) { if (glowA) { glowS += 0.02f; glowA = std::max(glowA - 12, 0); if (glowS >= 2.2f) { glowA = 0; glowS = 2.2f; splashSt = 2; } } }
            else if (splashSt == 2) { if (++splashT > 29) { splashT = 0; splashSt = 3; } }
            else if (splashSt == 3) { splashA -= 25; if (splashA < 11) { splashA = 0; splashSt = 4; } }
        }
        void draw(Contest& C) {   // FUN_0007fc2c
            const SQ* R = T_crank; const GD& g = gd(); const auto& tb = simTables();
            float y0 = rows[0].y;
            // gauge FUN_0007e524
            q(*tex, R[56], gaugeX - 220, -20);
            if (st == 4 || st == 5) {
                float range = rows[0].high - rows[0].low, d = rows[0].high - y0;
                float my = std::min(range != 0 && d != 0 ? 80 - d * 198 / range : 80.f, 80.f);
                q(*tex, R[57], gaugeX - 202, my, markerA / 255.f);
                if (markerUp) { markerA += 10; if (markerA > 249) markerUp = false; } else { markerA -= 10; if (markerA <= 150) markerUp = true; }
            }
            int ticks = n / 10;
            for (int i = 0; i < ticks; i++) q(*tex, R[58 + i], gaugeX - 220, 78 - (i + 1) * (float)(198 / ticks));
            for (auto& w : rows) drawRow(C, w, g, tb);
            static const float SP[4][2] = {{-180, 8}, {-172, -16}, {106, 20}, {138, -14}};
            for (int i = 0; i < 4; i++) q(*tex, R[63], SP[i][0], y0 + SP[i][1], 1, {}, spark[i], spark[i]);
            if (splashSt != 4) {   // FUN_0007f0ec
                if (splashSt == 1 || splashSt == 2) for (auto& p : part) q(*ef, R[79], p.x, p.y, p.alpha / 255.f, argb(0xfffefefe), p.s, p.s);
                q(*font, R[80], 0, -15, splashA / 255.f, {}, splashS, splashS);
                if (splashSt == 1) q(*font02, R[81], 0, -15, glowA / 255.f, {}, glowS, glowS);
            }
        }
        void drawRow(Contest& C, const Row& w, const GD& g, const SimTables& tb) {   // FUN_00080aa4
            const SQ* R = T_crank; float rx = -20, ry = w.y;
            if (w.team < 0 || ry < -170 || ry > 170) return;
            bool me = w.team == 0; int r = w.rank;
            int top = me ? (r < 4 ? 67 : 73) : r == 1 ? 0 : r == 2 ? 6 : r == 3 ? 12 : 18;
            for (int i = 0; i < 3; i++) {
                float sx = i == 1 ? 36.f : 1.f;
                q(*tex, R[top + i], rx - 150 + i * 150.f, ry + 12, 1, {}, sx);
                q(*tex, R[top + 3 + i], rx - 150 + i * 150.f, ry - 12, 1, {}, sx);
            }
            for (int i = 0; i < 3; i++) q(*tex, R[24 + i], rx + 50 + i * 45.f, ry + 8, 1, {}, i == 1 ? 10.5f : 1.f);
            q(*tex, R[40], rx + 140, ry - 8);
            if (r < 10) {
                int rec = r <= 3 ? (me ? 63 + r : 26 + r) : 30 + r;
                q(*tex, R[rec], rx - 128 - 6, r == 1 ? ry + 16 : ry + 8);
            } else for (int i = 0, v = r; i < 2; i++, v /= 10) q(*tex, R[30 + v % 10], rx - 128 - i * 14.f, ry + 8);
            int p = g.team[w.team].prevRank;
            q(*tex, R[p == 0 ? 52 : p > r ? 53 : p < r ? 54 : 55], rx - 134, ry - 10);
            int sc = std::max(g.team[w.team].score, 0), digits = sc ? (int)std::floor(std::log10((double)sc)) + 1 : 1;
            float x = rx + 135;
            for (int i = 0, v = sc; i < digits; i++, v /= 10) {
                if (i == 0 || i % 3) x -= 9;
                else { q(*tex, R[51], x - 5, ry - 13); x -= 11; }
                q(*tex, R[41 + v % 10], x, ry - 10);
            }
            const TeamDef& d = tb.teams[w.team];
            std::string genre = me ? C.genre() : std::string(d.genre), title = me ? std::string(g.gameTitle) : std::string(d.gameTitle);
            std::string gn = tr(genre); int gc = 9; float gw = textWidth(gn, gc);   // centred on the 92 px pill
            if (gw > 88) { gc = std::max(6, (int)(9 * 88 / gw)); gw = textWidth(gn, gc); }
            simText(gn, rx + 96 - gw / 2, ry + 8 + gc / 2.f, gc, {0, 0, 255, 255});
            fitText(tr(title), rx - 113, ry + 8 + 7, 14, 222, {255, 255, 255, 255});   // strip at (rx, ry+8), left-aligned
            fitText(tr(d.teamName), rx - 98, ry - 10 + 7, 14, 200, {0, 0, 0, 255});   // strip at (rx+15, ry-10), 15 px margin
            if (C.shot) q(*C.shot, T_cthumb[std::min(w.team, 50)], rx + 200, ry);
        }
        static void fitText(const std::string& s, float x, float y, int cell, float maxw, Color c) {
            float wd = textWidth(s, cell);
            if (wd > maxw) cell = std::max(8, (int)(cell * maxw / wd));
            simText(s, x, y, cell, c);
        }
    } rank;
};
}
std::unique_ptr<Job> makeContest() { return std::make_unique<Contest>(); }
