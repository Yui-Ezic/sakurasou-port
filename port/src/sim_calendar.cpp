// Job 0xc: calendar (spec_sim section 9).
#include "sim_scenes.h"
#include "tr.h"
#include "sim_ui.h"

namespace {
const SQ* const P = T_1f68e8;                 // parts 0..13, font 14..34, font02 35..50
const char* kHelp = "○進む　スクロール　　コンテストの提出期限などが確認できます";

struct Calendar : Job {
    TexP parts, font, font2; PageHeader hdr; DeadlineWidget dead;
    int st = 0, wait = 0, today = 0, first = 0, last = 0, limit = 0, group = 0, flip = 0;
    float scroll = 0, frameX = 0, scrollFrom = 0;
    bool countdown = false, exiting = false, hideDead = false;
    int headerT = 0, arrowA = 255, arrowD = -5, days = 0;
    // countdown card
    int cst = 1, ct = 0; float s0 = 1.2f, s1 = 1.2f, s2 = 1.2f; int a0 = 0, a1 = 0, a2 = 0; float ox = 0, oy = 0; int dim = 0xb4;

    float base(int d) const { return -256 - (d - 2) * 72.f; }
    void setupRange() {
        GD& g = gd(); int p = g.routeA; const SimDay& d = kCalendar[std::clamp(g.turn, 0, 102)];
        if (p <= 2) group = 0; else if (p <= 4) group = 1; else if (p <= 6) group = 2; else if (p == 7) group = 3;
        else group = (d.month == 2 && d.day <= 7) ? 4 : (d.month == 2 && d.day <= 12) ? 5 : 6;
        static const int R[7][3] = {{3, 13, 10}, {14, 42, 0x27}, {43, 77, 0x4a}, {78, 91, 0x58}, {92, 96, 0x5d}, {92, 97, 0x5e}, {3, 97, 0x5e}};
        first = R[group][0]; last = R[group][1]; limit = R[group][2];
    }
    void onEnter() override {
        parts = g_gfx.load("calendar_parts.txg"); font = g_gfx.load("calendar_font.txg"); font2 = g_gfx.load("calendar_font02.txg");
        hdr.set(1); hdr.start(); dead.init(185, 90);
        GD& g = gd();
        today = std::max(g.turn - 1, 2);
        setupRange();
        scroll = base(std::min(today, limit));
        frameX = scroll + today * 72 + 0.5f;
        days = last - g.turn;
        countdown = group <= 3 && days <= 7;
    }
    void onResume() override { g_trans.style(3); g_trans.reveal(30); }
    void clampScroll() {
        scroll = std::min(scroll, base(first)); scroll = std::max(scroll, -256 - (last - 5) * 72.f);
        frameX = scroll + today * 72 + 0.5f;
    }
    void updateCard() {  // FUN_000702e0
        auto step = [&](float& s, int& a) { s -= 0.02f; a = std::min(a + 0x14, 255); if (s <= 1) { s = 1; return true; } return false; };
        switch (cst) {
        case 1: if (step(s0, a0)) cst = 2; break;
        case 2: if (step(s1, a1)) cst = 3; break;
        case 3: if (step(s2, a2)) cst = 4; break;
        case 4: if (++ct > 0xc) { ct = 0; cst = 5; } break;
        case 5: ox += 9; oy += 4; s0 -= 0.025f; dim = std::max(dim - 10, 0);
            if (++ct >= 30) cst = 6;      // ASSUMED exit length
            break;
        }
    }
    void onUpdate() override {
        hdr.update();
        arrowA += arrowD; if (arrowA < 0x33) arrowD = 5; if (arrowA > 0xf9) arrowD = -5;
        if (headerT < 25) headerT++;
        bool allow = headerT >= 25;
        switch (st) {
        case 0:
            if (g_trans.clear() && ++wait > 10) { wait = 0; if (!countdown) { st = 2; g_help.show(kHelp); } else st = 1; }
            break;
        case 1: updateCard(); if (cst == 6) { countdown = false; st = 2; g_help.show(kHelp); } break;
        case 2: if (allow && ++wait > 10) { wait = 0; st = 3; flip = 0; if (today == last) { st = 4; } } break;
        case 3:
            if (flip > 9) { today = std::max(gd().turn, 3); frameX = scroll + today * 72 + 0.5f; flip = 0; st = 4; break; }
            if (today < last - 3) { scroll = std::max(scroll - 7.2f, -256 - (last - 5) * 72.f); frameX = scroll + today * 72 + 0.5f; }
            else frameX += 7.2f;
            flip++;
            break;
        case 4: if (++wait > 10) { wait = 0; st = 5; } break;
        case 5:
            if (g_input.isHeld(BTN_RIGHT)) { scroll -= 7.2f; clampScroll(); }
            else if (g_input.isHeld(BTN_LEFT)) { scroll += 7.2f; clampScroll(); }
            else if (g_input.isPressed(BTN_CIRCLE)) { g_audio.playCue(0x29); st = 6; flip = 0; scrollFrom = scroll; }
            break;
        case 6: {  // FUN_0006fd5c: return to today, 10 frames
            float tgt = base(std::min(today, limit));
            flip++;
            scroll = scrollFrom + (tgt - scrollFrom) * flip / 10;
            frameX = scroll + today * 72 + 0.5f;
            if (flip >= 10) { wait = 0; st = 7; }
            break; }
        case 7:
            if (!exiting && ++wait > 10) { exiting = true; g_trans.style(0); g_trans.cover(10); g_help.hide(); }
            if (exiting && g_trans.covered()) { st = 99; selfPop(); }
            break;
        }
    }
    void drawDay(int i) {
        float x = scroll + i * 72, y = -10;
        if (x < -300 || x > 300) return;
        const SimDay& d = kCalendar[i];
        q(*parts, P[1], x, y, 1, argb(d.holiday ? 0xffeed8ff : 0xfffceab8));
        Color c = d.holiday ? argb(0xff0000ff) : d.weekday == 6 ? argb(0xffff4d00) : argb(0xff000000);
        if (trOn()) { static const char* wr[7] = {"вс", "пн", "вт", "ср", "чт", "пт", "сб"}; simTextCentered(wr[d.weekday % 7], x, y + 20, 14, c); }   // baked kanji replaced by text
        else q(*font, P[14 + d.weekday], x, y + 20, 1, c);
        if (d.day < 10) q(*font, P[21 + d.day], x, y - 17, 1, c);
        else for (int k = 0, v = d.day; k < 2; k++, v /= 10) q(*font, P[21 + v % 10], x + 12 - k * 24.f, y - 17, 1, c);
    }
    void drawCard() {
        g_gfx.fillRect(0, 0, 480, 272, {0, 0, 0, (uint8_t)dim});
        if (group > 3) return;
        float dx = ox, dy = oy;   // ASSUMED layout of the countdown card (constants of FUN_00070684 not traced)
        if (trOn()) {             // the card is baked kanji ("企画書 提出まで あと 7 日"): two lines of text instead
            static const char* kTitle[4] = {"企画書提出まで", "α版提出まで", "β版提出まで", "マスターUP提出まで"};
            auto line = [&](const std::string& t, float y, float a, float s, int cell, Color c) {
                int sz = std::max(10, (int)(cell * s)); c.a = (uint8_t)std::clamp(a * 255, 0.f, 255.f);
                Color sh{0, 0, 0, c.a};
                for (int ox2 : {-1, 1}) for (int oy2 : {-1, 1}) simTextCentered(t, dx + ox2, y + oy2, sz, sh);
                simTextCentered(t, dx, y, sz, c);
            };
            line(tr(kTitle[group]), 30 + dy, a0 / 255.f, s0, 22, {255, 255, 255, 255});
            line(trFmt("あと%d日", std::clamp(days, 0, 7)), -30 + dy, a2 / 255.f, s2, 28, days <= 3 ? Color{255, 60, 60, 255} : Color{255, 255, 255, 255});
            return;
        }
        q(*font2, P[35 + group], -60 + dx, 30 + dy, a0 / 255.f, {}, s0, s0);
        q(*font2, P[39], 60 + dx, 30 + dy, a0 / 255.f, {}, s0, s0);
        q(*font2, P[40], -40 + dx, -30 + dy, a1 / 255.f, {}, s1, s1);
        q(*font2, P[43 + std::clamp(days, 0, 7)], 16 + dx, -30 + dy, a2 / 255.f, {}, s2, s2);
        q(*font2, P[41], 70 + dx, -30 + dy, a2 / 255.f, {}, s2, s2);
    }
    void onDraw() override {
        drawCommonBg();
        hdr.draw();
        q(*parts, P[0], 0, -10, 1, {}, 60, 1);
        for (int i = 0; i < 103; i++) drawDay(i);
        q(*parts, P[2], frameX, -10);
        static const int ms[5] = {13, 42, 77, 91, 96};
        for (int i = 0; i < 5; i++) {
            float x = scroll + ms[i] * 72;
            q(*parts, P[4 + i], x, -10 + 40);
            q(*parts, P[3], x, -10 - 20);
        }
        static const int ev[3] = {46, 53, 97};
        for (int i = 0; i < 3; i++) q(*parts, P[9 + i], scroll + ev[i] * 72 + 25, -10 - 40);
        const SimDay& d = kCalendar[std::clamp(gd().turn, 0, 102)];
        int mi = d.month < 0xb ? d.month + 1 : d.month - 0xb;
        if (trOn()) { static const char* mr[4] = {"Ноябрь", "Декабрь", "Январь", "Февраль"}; simTextCentered(mr[std::clamp(mi, 0, 3)], 0, 80, 28, {0, 0, 0, 255}); }
        else q(*font, P[31 + std::clamp(mi, 0, 3)], 0, 80);
        if (st == 5 || st == 4) {
            float a = arrowA / 255.f;
            if (scroll < base(first) - 1) q(*parts, P[12], -230, -10, a);
            if (scroll > -256 - (last - 5) * 72.f + 1) q(*parts, P[13], 230, -10, a);
        }
        if (st <= 1 && countdown && cst < 6) drawCard();
        if (!hideDead) dead.draw();
    }
};
}
std::unique_ptr<Job> makeCalendar() { return std::make_unique<Calendar>(); }
