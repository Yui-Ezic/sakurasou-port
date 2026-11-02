// Job 0xe: contest result (spec_sim section 7.3, 7.4).
// Comment selection is exact; row tweening and the comment window layout are ASSUMED (spec: unresolved).
#include "sim_scenes.h"
#include "sim_ui.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace {
struct Contest : Job {
    TexP bg, index, window, parts, count, rankT, rankFont, shot;
    int st = 0, f = 0, shown = 0, likes = 0, sub = 0, camera = 0, stage = 0, rows = 51;
    std::vector<int> slots; float listY = 0, listTarget = 0; bool leaving = false;
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
        rankT = g_gfx.load("contest_ranking.txg"); rankFont = g_gfx.load("contest_ranking_font.txg");
        static const char* beta[7] = {"img_beta_msr", "img_beta_msr", "img_beta_rns", "img_beta_nnm", "img_beta_oth", "img_beta_msk", "img_beta_zin"};
        std::string im = stage < 4 ? "img_plan_01" : stage < 9 ? (g.routeB == 1 ? "img_alpha_01" : g.routeB == 3 ? "img_alpha_03" : "img_alpha_02") : beta[std::clamp(g.routeB, 0, 6)];
        shot = g_gfx.load(im + ".txg");
        pickComments();
        likes = g.team[0].score / 100;   // ASSUMED: spec names a "prior field" that is not stored in GameData
        const char* me = stage == 11 && (g.routeB == 5 || g.routeB == 6) ? "SK_ME_03.adx" : (g.routeB >= 1 && g.routeB <= 4) ? "SK_ME_02.adx" : "SK_ME_01.adx";
        g_audio.play(CH_BGM, me, false, 10);
        listTarget = (g.team[0].rank - 1) * 50.f; listY = (float)std::max(g.team[0].rank + 4, 8) * 50.f;
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
            listY += (listTarget - listY) * 0.08f;
            if (std::abs(listY - listTarget) < 1) listY = listTarget;
            if (skip) {
                if (listY != listTarget) listY = listTarget;
                else { leaving = true; g_trans.style(0); g_trans.cover(10); }
            }
            break;
        }
    }
    void number(int v, float x, float y) {   // right-aligned; ASSUMED: font instead of the white contest_ranking digits
        std::string t = std::to_string(v);
        simText(t, x - textWidth(t, 14) + 8, y + 8, 14, {30, 80, 200, 255});
    }
    void drawComments() {
        const auto& C = simTables().comments;
        if (shot) { float s = 100.f / shot->w; ui(*shot, {0, 0, shot->w, shot->h}, 170, 50, 1, s, s); }
        ui(*index, {0, 16, 256, 48}, -112, 118);
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
    void drawRanking() {
        const auto& tb = simTables(); GD& g = gd();
        ui(*rankFont, {0, 0, 240, 64}, 0, 108, 1, 0.6f, 0.6f);
        std::vector<int> order(tb.teams.size(), -1);
        for (int t = 0; t < (int)tb.teams.size(); t++) { int r = g.team[t].rank; if (r >= 1 && r <= (int)order.size() && order[r - 1] < 0) order[r - 1] = t; }
        for (int r = 1; r <= (int)order.size(); r++) {
            int t = order[r - 1]; if (t < 0) continue;
            float y = 60 - (r - 1) * 50.f + listY;
            if (y < -160 || y > 100) continue;
            bool me = t == 0;
            g_gfx.fillRect(240 - 220, 136 - y - 22, 440, 44, me ? Color{255, 240, 180, 235} : Color{255, 255, 255, 220});
            number(r, -176, y);
            simText(tb.teams[t].teamName, -150, y + 18, 10, {60, 60, 60, 255});
            simText(me ? g.gameTitle : tb.teams[t].gameTitle, -150, y + 6, 14, {0, 0, 0, 255});
            simText(me ? genre() : tb.teams[t].genre, -150, y - 10, 10, {80, 80, 80, 255});
            char b[32]; snprintf(b, sizeof b, "%dDL", g.team[t].score); simText(b, 110, y + 8, 14, {200, 30, 30, 255});
            int p = g.team[t].prevRank;
            Spr s = p == 0 ? Spr{0, 240, 32, 256} : p > r ? Spr{32, 240, 64, 256} : p < r ? Spr{64, 240, 96, 256} : Spr{96, 240, 128, 256};
            ui(*rankT, s, 200, y);
        }
    }
    std::string genre() {
        GD& g = gd(); int p = g.routeA, r = g.routeB, k = 0;
        if (p == 3 || p == 4) k = r == 1 ? 2 : r == 5 ? 3 : r == 3 ? 4 : 1;
        else if (p == 5 || p == 6) k = r == 1 ? 5 : r == 2 ? 6 : r == 3 ? 7 : r == 5 ? 8 : r == 6 ? 9 : r == 4 ? 10 : 0;
        else if (p >= 7) k = r == 1 ? 11 : r == 2 ? 12 : r == 3 ? 13 : r == 5 ? 14 : r == 6 ? 15 : r == 4 ? 16 : 0;
        return kGenre[k];
    }
    void onDraw() override {
        g_gfx.clear({210, 225, 235, 255});
        if (bg) ui(*bg, {0, 0, 32, 128}, 0, 0, 1, 15, 2.2f);   // ASSUMED: gradient strip stretched
        if (st == 2 || st == 99) drawRanking(); else drawComments();
    }
};
}
std::unique_ptr<Job> makeContest() { return std::make_unique<Contest>(); }
