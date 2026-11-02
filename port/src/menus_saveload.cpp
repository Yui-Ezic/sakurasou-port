// Save, load and quick load screens: scenes 0x1a, 0x1b, 0x1c, 0x24, 0x25, 0x26 (spec_flow_menus section 7).
// The PSP savedata dialogs are gone: files are read and written directly through the game.h file API.
#include "tr.h"
#include "menus.h"
#include "menus_data.h"
#include "menus_ui.h"
#include <cstdio>

namespace {
enum Kind { KSave, KLoad, KQuick };

static const char* kHelpSave = "○決定　×キャンセル　￺￻ページ切替　　セーブするスロットを決定してください";
static const char* kHelpLoad = "○決定　×キャンセル　￺￻ページ切替　　ロードするスロットを決定してください";
static const char* kHelpDlcSave = "○決定　×キャンセル　　セーブするスロットを決定してください";
static const char* kHelpDlcLoad = "○決定　×キャンセル　　ロードするスロットを決定してください";
static const char* kHelpQuick = "○決定　×キャンセル　　クイックロードするスロットを決定してください";
static const char* kAskLoad = "選択したセーブデータをロードしますか？";
static const char* kAskSave = "選択した場所にセーブしますか？";
static const char* kAskOverwrite = "選択した場所にはすでにセーブデータが存在します。\n上書きセーブしますか？";

struct Slot {
    bool valid = false; Bytes snap; TexP thumb;
    const GD& gd() const { return *reinterpret_cast<const GD*>(snap.data() + 0x4010); }
    void date(int v[6]) const { uint16_t d[6]; memcpy(d, snap.data() + 4, sizeof d); for (int i = 0; i < 6; i++) v[i] = d[i]; }
};

// heroine with the top affection, FUN_000be338: field[4] (eventCount) first, then field[3] (affinity)
int topHeroine(const GD& g) {
    int best = 0;
    for (int i = 1; i < 6; i++) {
        const CharStat& a = g.stat[i], & b = g.stat[best];
        if (a.eventCount > b.eventCount || (a.eventCount == b.eventCount && a.affinity > b.affinity)) best = i;
    }
    return best;
}

TexP thumbFromSnap(const uint8_t* snap) {
    Image im; im.w = 64; im.h = 64; im.px.resize(64 * 64);
    const uint8_t* s = snap + 0x10; uint8_t* dst = (uint8_t*)im.px.data();
    for (int by = 0; by < 8; by++) for (int bx = 0; bx < 16; bx++) for (int l = 0; l < 8; l++) {
        memcpy(dst + (by * 8 + l) * 256 + bx * 16, s, 16); s += 16;
    }
    return g_gfx.fromImage(im);
}

const char* kFullDigit[10] = {"０", "１", "２", "３", "４", "５", "６", "７", "８", "９"};
std::string fullDigit(int v, bool blankZero) { return blankZero && v == 0 ? "\xE3\x80\x80" : kFullDigit[v % 10]; }

struct SaveScene : Job {
    Kind kind; bool dlc;
    int pages = 6, page = 0, cur = 0, state = 0, t = 0, dir = 0, tick = 0;
    int panelT = 0;               // detail panel slide, 0..10 (FUN_000be9dc)
    Slot slots[10];
    Bytes fileBuf;
    TexP parts, win, pageT, arrow;
    uint32_t base, accent;
    PageHeader hdr;
    bool leaving = false, loading = false;
    SaveScene(Kind k, bool d) : kind(k), dlc(d) {
        base = k == KSave ? 0xffeab6ff : k == KLoad ? 0xffffd5a9 : 0xffaeeaae;
        accent = k == KSave ? 0xffa877f0 : k == KLoad ? 0xffff8b60 : 0xff3fb343;
        if (d || k == KQuick) pages = 1;
    }
    int dramaIndex() const { return std::clamp(g_mode - 3, 0, 3); }
    std::string fileName(int s) const {
        char b[32];
        if (dlc) snprintf(b, sizeof b, "DR0%d0%d.DAT", dramaIndex() + 1, s % 10);
        else snprintf(b, sizeof b, "GAME%02d.DAT", s);
        return b;
    }
    void onEnter() override {
        parts = g_gfx.load("saveload_parts.txg"); win = g_gfx.load("saveload_window.txg");
        pageT = g_gfx.load("saveload_page.txg"); arrow = g_gfx.load("saveload_arrow.txg");
        hdr.set(kind == KQuick ? 9 : dlc ? (kind == KSave ? 20 : 24) + dramaIndex() : kind == KSave ? 10 : 11);
        hdr.start();
        g_help.show(kind == KQuick ? kHelpQuick : dlc ? (kind == KSave ? kHelpDlcSave : kHelpDlcLoad) : kind == KSave ? kHelpSave : kHelpLoad);
        if (pages > 1) { page = std::clamp(g_lastSlot / 10, 0, pages - 1); cur = g_lastSlot % 10; }
    }
    void onResume() override { g_trans.style(0); g_trans.reveal(10); state = 0; t = 0; }
    void readPage() {
        for (int i = 0; i < 10; i++) {
            Slot& s = slots[i]; s.valid = false; s.thumb.reset();
            if (kind == KQuick) {
                const uint8_t* p = saveBuffer() + 0xb0a0 + i * kSnapSize;
                s.snap.assign(p, p + kSnapSize);
                s.valid = *(const uint32_t*)p == 1;
            } else {
                Bytes f;
                if (!readSaveFile(fileName(page * 10 + i), f)) continue;
                s.snap.assign(f.begin() + 0x10, f.begin() + 0x10 + kSnapSize);
                s.valid = *(const uint32_t*)s.snap.data() == 1;
            }
            if (s.valid) s.thumb = thumbFromSnap(s.snap.data());
        }
    }
    int slotAt() const {
        for (int i = 0; i < 10; i++) if (mu::hit(-180 + 90.f * (i % 5), 55 - 78.f * (i / 5), 88, 76)) return i;
        return -1;
    }
    void pageTurn(int d) { dir = d; mu::se(0x2a); state = 4; t = 0; }
    void input() {
        if (mu::cancel()) { mu::se(0x1f); exitScene(); return; }
        int old = cur, over = slotAt();
        if (g_input.mouseMoved && over >= 0) cur = over;
        int col = cur % 5, row = cur / 5, pd = 0;
        if (g_input.isRepeat(BTN_RIGHT)) { if (col == 4) { col = 0; pd = 1; } else col++; }
        if (g_input.isRepeat(BTN_LEFT)) { if (col == 0) { col = 4; pd = -1; } else col--; }
        if (g_input.isRepeat(BTN_UP) || g_input.isRepeat(BTN_DOWN)) row ^= 1;
        cur = row * 5 + col;
        if (pages > 1) {
            if (g_input.isPressed(BTN_R) || g_input.wheel < 0) pd = 1;
            if (g_input.isPressed(BTN_L) || g_input.wheel > 0) pd = -1;
            if (g_input.mouseClick && over < 0 && mu::hit(230, 16, 32, 48)) pd = 1;
            if (g_input.mouseClick && over < 0 && mu::hit(-230, 16, 32, 48)) pd = -1;
            if (pd) { pageTurn(pd); return; }
        }
        if (cur != old) mu::se(0x2a);
        if (mu::confirm(over >= 0)) {
            Slot& s = slots[cur];
            if (kind == KSave) { mu::se(0x29); g_popup.open(s.valid ? kAskOverwrite : kAskSave, true, !s.valid); state = s.valid ? 6 : 5; }
            else if (s.valid) { mu::se(0x29); g_popup.open(kAskLoad, true, true); state = 5; }
        }
    }
    void afterPopup() {
        if (g_popup.isOpen()) return;
        if (!g_popup.yes()) { state = 3; return; }
        if (kind == KSave) {
            g_sys.save();
            int sl = pages > 1 ? page * 10 + cur : cur;
            if (writeSaveFile(fileName(sl))) {
                g_lastSlot = sl;
                readPage();
            } else fprintf(stderr, "save failed: %s\n", fileName(sl).c_str());
            state = 3;
        } else {
            if (kind == KLoad && !readSaveFile(fileName(page * 10 + cur), fileBuf)) { readPage(); state = 3; return; }
            g_help.hide(); g_trans.style(0); g_trans.cover(10); loading = true; state = 8;
        }
    }
    void finishLoad() {
        if (kind == KQuick) selectHistory(cur);
        else { useSaveFile(fileBuf); g_lastSlot = page * 10 + cur; }
        if (!dlc) g_mode = 0;
        g_audio.stop(CH_VOICE); g_audio.stop(CH_BGM);   // ASSUMED: the restored scene starts its own music
        Job* h = progressHost();
        if (h && h->parent) { h->parent->pop(); h->parent->push(5); }
        else { g_root->popAll(); g_root->push(5); }
    }
    void exitScene() { g_trans.style(0); g_trans.cover(10); g_help.hide(); state = 9; }
    void onUpdate() override {
        hdr.update(); tick = (tick + 1) % 60;
        bool wantPanel = state >= 2 && state != 4 && slots[cur].valid;
        if (wantPanel) panelT = std::min(panelT + 1, 10); else panelT = std::max(panelT - 1, 0);
        switch (state) {
        case 0: if (g_trans.clear()) state = 1; break;
        case 1: readPage(); t = 0; state = 2; break;
        case 2: if (++t >= 20) state = 3; break;
        case 3: input(); break;
        case 4: if (++t >= 20) { page = (page + dir + pages) % pages; state = 1; } break;
        case 5: case 6: afterPopup(); break;
        case 8: if (g_trans.covered()) { state = 10; finishLoad(); } break;
        case 9: if (g_trans.covered()) { selfPop(); state = 10; } break;
        }
    }
    bool columnShown(int c) const {
        if (state == 2) return t > 2 * c;
        if (state == 4) return t <= 2 * (4 - c) + 12;
        return state != 0 && state != 1;
    }
    void drawSlot(int i, bool selected) {
        const Slot& s = slots[i];
        float x = -180 + 90.f * (i % 5), y = 55 - 78.f * (i / 5);
        bool empty = !s.valid;
        Color b = mu::abgr(empty ? 0xffd0d0d0 : selected ? 0xff80e0ff : base);
        ui(*parts, {0, 0, 88, 72}, x, y, 1, 1, 1, 0, b);
        if (selected) ui(*parts, {88, 0, 176, 72}, x, y);
        float py = y + 34;
        ui(*parts, {0, 88, 88, 104}, x, py, 1, 1, 1, 0, mu::abgr(empty ? 0xffd0d0d0 : 0xffffffff));
        ui(*parts, {0, 72, 88, 88}, x, py, 1, 1, 1, 0, mu::abgr(selected ? 0xff2090ff : empty ? 0xff808080 : accent));
        int n = page * 10 + i;
        auto dg = [&](int v, float dx) { ui(*parts, {8 * v, 240, 8 * v + 8, 248}, x - 35 + dx, py); };
        dg(n / 10 % 10, -3); dg(n % 10, 3);
        if (s.valid) {
            int d[6]; s.date(d);
            auto sm = [&](int f, float dx) { ui(*parts, {8 * f, 248, 8 * f + 8, 256}, x + 8 + dx, py); };
            auto two = [&](int v, float a, float b2) { sm(v / 10 % 10, a); sm(v % 10, b2); };
            two(d[0] % 100, -29, -24); sm(10, -20); two(d[1], -16, -11); sm(10, -7); two(d[2], -3, 2);
            two(d[3], 10, 15); sm(11, 19); two(d[4], 23, 28);
            float ty = y - 24;
            ui(*parts, {0, 104, 88, 120}, x, ty);
            static const int gv[7] = {216, 120, 184, 152, 200, 136, 168};
            const GD& g = s.gd();
            int genre = std::clamp(g.routeB, 0, 6), h = topHeroine(g);
            ui(*parts, {0, gv[genre], 48, gv[genre] + 16}, x - 17, ty);
            ui(*parts, {48, 120 + 16 * h, 96, 136 + 16 * h}, x + 17, ty);
            if (s.thumb) ui(*s.thumb, {0, 0, 60, 34}, x, y + 5, 1, 1.2f, 1.2f);
        } else ui(*parts, {88, 72, 168, 120}, x, y + 5);
    }
    void drawPanel() {
        if (panelT <= 0) return;
        const Slot& s = slots[cur];
        float oy = -136 + (-80 + 8.f * panelT);
        static const int fx[7][2] = {{208, 216}, {216, 224}, {224, 232}, {216, 224}, {224, 232}, {216, 224}, {232, 240}};
        static const float px[8] = {-224, -216, -4, 4, 82, 90, 216, 224};
        for (int i = 0; i < 7; i++) {
            float w = px[i + 1] - px[i];
            ui(*win, {fx[i][0], 16, fx[i][1], 96}, (px[i] + px[i + 1]) / 2, oy + 40, 1, w / 8, 1);
        }
        if (!s.valid) return;
        const GD& g = s.gd();
        // date (FUN_000bef90) and phase (FUN_000bd6b8)
        const DateEntry& de = kDates[std::clamp(g.turn, 0, 102)];
        Color wc = de.holiday || de.weekday == 0 ? mu::abgr(0xff0000ff) : de.weekday == 6 ? mu::abgr(0xffff5000) : Color{0, 0, 0, 255};
        if (trOn()) {      // Russian: DD.MM (day of week), one centred run
            static const char* wr[7] = {"вс", "пн", "вт", "ср", "чт", "пт", "сб"};
            char d[16]; snprintf(d, sizeof d, "%02d.%02d (", de.day, de.month);
            std::string wk = wr[de.weekday % 7];
            float w = mu::textWidth(d, 14) + mu::textWidth(wk, 14) + mu::textWidth(")", 14), tx = -166 - w / 2, ty = oy + 56 + 7;
            mu::text(g_fontM, d, tx, ty, {0, 0, 0, 255}, 14, 18); tx += mu::textWidth(d, 14);
            mu::text(g_fontM, wk, tx, ty, wc, 14, 18); tx += mu::textWidth(wk, 14);
            mu::text(g_fontM, ")", tx, ty, {0, 0, 0, 255}, 14, 18);
        } else {
        static const char* wd[7] = {"日", "月", "火", "水", "木", "金", "土"};
        std::string head = fullDigit(de.month / 10 % 10, true) + kFullDigit[de.month % 10] + "月" + fullDigit(de.day / 10 % 10, true) + kFullDigit[de.day % 10] + "日(";
        float w = mu::textWidth(head, 14) + 14 + 14, tx = -166 - w / 2, ty = oy + 56 + 7;
        mu::text(g_fontM, head, tx, ty, {0, 0, 0, 255}, 14, 18); tx += mu::textWidth(head, 14);
        mu::text(g_fontM, wd[de.weekday % 7], tx, ty, wc, 14, 18); tx += 14;
        mu::text(g_fontM, ")", tx, ty, {0, 0, 0, 255}, 14, 18);
        }
        static const char* phase[9] = {"", "企画段階", "企画段階", "α版", "α版", "β版", "β版", "マスター版", "マスター版"};
        mu::textCentered(g_fontS, phase[std::clamp(g.routeA, 0, 8)], -166, oy + 34, {0, 0, 0, 255}, 10, 14);
        // SCE ranking and sales (FUN_000bda7c, FUN_000bdd70)
        ui(*win, {0, 232, 80, 248}, -54 - 14, oy + 56);
        int rank = g.team[0].rank;
        std::string rs = rank < 1 ? "ーー" : fullDigit(rank / 10 % 10, true) + kFullDigit[rank % 10];
        mu::textCentered(g_fontS, rs, -54 + 40, oy + 56, {0, 0, 0, 255}, 14, 18);
        int sc = g.team[0].score; std::string ss; int p10 = 1000000;
        for (int k = 0; k < 5; k++, p10 /= 10) ss += sc < p10 ? fullDigit(sc / (p10 / 10) % 10, true) : std::string(kFullDigit[sc / (p10 / 10) % 10]);
        ss += kFullDigit[sc % 10];
        mu::textCentered(g_fontS, ss, -54 - 12, oy + 34, {0, 0, 0, 255}, 14, 18);
        ui(*win, {208, 0, 232, 16}, -54 + 30, oy + 34);
        // genre + heroine box, clear route box
        ui(*win, {0, 0, 80, 56}, 43, oy + 46);
        static const int gu[7][2] = {{0, 192}, {0, 72}, {48, 112}, {0, 152}, {48, 152}, {0, 112}, {48, 72}};
        int genre = std::clamp(g.routeB, 0, 6), h = topHeroine(g);
        ui(*win, {gu[genre][0], gu[genre][1], gu[genre][0] + 48, gu[genre][1] + 37}, 43 - 14, oy + 46 - 6 + 0.5f);
        static const int hu[6][2] = {{96, 72}, {96, 112}, {96, 152}, {144, 72}, {144, 112}, {144, 152}};
        ui(*win, {hu[h][0], hu[h][1], hu[h][0] + 48, hu[h][1] + 37}, 43 + 14, oy + 46 - 6 + 0.5f);
        ui(*win, {80, 0, 208, 56}, 155, oy + 46);
        struct Ic { int bit, frame; float x, y; };
        static const Ic ic[6] = {{1, 0, -16, 9}, {5, 4, 1, 9}, {3, 2, 18, 9}, {6, 5, -18, -8}, {2, 1, -1, -8}, {4, 3, 16, -8}};
        static const int iu[6] = {0, 64, 32, 80, 16, 48};
        for (auto& c : ic) if (g.clearBits >> c.bit & 1) ui(*win, {iu[c.frame], 56, iu[c.frame] + 16, 72}, 155 - 32 + c.x, oy + 46 - 6 + c.y);
        if (g.badBits & 1) ui(*win, {96, 56, 144, 72}, 155 + 32 + 2, oy + 46 - 6 + 9);
        if (g.badBits >> 2 & 1) ui(*win, {144, 56, 192, 72}, 155 + 32 - 2, oy + 46 - 6 - 8);
    }
    void onDraw() override {
        mu::commonBg(); hdr.draw();
        if (!parts) return;
        for (int i = 0; i < 10; i++) if (columnShown(i % 5)) drawSlot(i, i == cur && state >= 3 && state != 4);
        drawPanel();
        if (pages > 1) {
            for (int i = 0; i < pages; i++) {
                int f = i == page ? 0 : 1;
                ui(*pageT, {16 * f, 0, 16 * f + 16, 16}, (pages - 1) * -16.f + 16 * i + 216, 110);
            }
            float a = mu::triAlpha(tick, 30) / 255.f;
            if (page > 0) ui(*arrow, {0, 0, 16, 32}, -230, 16, a);
            if (page < pages - 1) ui(*arrow, {16, 0, 32, 32}, 230, 16, a);
        }
    }
};
}

std::unique_ptr<Job> createSaveLoadScene(int id) {
    switch (id) {
    case 0x1a: return std::make_unique<SaveScene>(KQuick, false);
    case 0x1b: return std::make_unique<SaveScene>(KSave, false);
    case 0x1c: return std::make_unique<SaveScene>(KLoad, false);
    case 0x24: return std::make_unique<SaveScene>(KQuick, true);
    case 0x25: return std::make_unique<SaveScene>(KSave, true);
    case 0x26: return std::make_unique<SaveScene>(KLoad, true);
    }
    return nullptr;
}
