// In-game menus: scenes 0x10, 0x11, 0x12 and the factory (spec_flow_menus 5.2-5.4, FUN_0008b0dc, FUN_00089ee0).
#include "menus.h"
#include "menus_ui.h"

int g_eventViewId = 0;
std::string g_dlcMovieName;

namespace {
Spr btn(int i) {   // sprites 1..8 of menu_button.txg
    int k = i - 1; return {88 * (k & 1), 56 * (k / 2), 88 * (k & 1) + 88, 56 * (k / 2) + 56};
}
const Spr kPsrts[] = {   // sprites 34.. of menu_psrts.txg (table 0x1ffd80)
    {0,0,8,24},{0,24,8,32},{0,32,8,40},{8,0,16,24},{8,24,16,32},{8,32,16,40},{16,0,24,24},{16,24,24,32},{16,32,24,40},
    {32,0,72,40},{24,26,32,32},{24,32,32,40},{0,40,8,64},{8,40,16,64},{16,40,24,72},{24,40,32,72},{32,40,40,72}};

// status panel: FUN_000895ac (setup), FUN_00089ee0 (draw)
struct StatusPanel {
    TexP psrts, imgs; bool shown = false; float s = 1.1f; int alpha = 0; bool done = false;
    int phase = 0, sc = 0, rank = 0, prev = 0, imgIdx = 0, textIdx = 0; std::string title; float radar[6]{};
    void init(TexP p, TexP im) {
        psrts = p; imgs = im; const GD& g = gd(); phase = g.routeA; s = 1.1f; alpha = 0;
        if (phase < 2) { shown = false; return; }
        shown = true; title = g.gameTitle; sc = g.team[0].score; rank = g.team[0].rank; prev = g.team[0].prevRank;
        int ge = g.routeB, a = 0, b = 0;
        if (phase == 3 || phase == 4) { if (ge == 1) a = 2, b = 1; else if (ge == 5) a = 3, b = 2; else if (ge == 3) a = 4, b = 3; else a = 1, b = 1; }
        else if (phase == 5 || phase == 6) { static const int ma[7] = {0, 5, 6, 7, 10, 8, 9}, mb[7] = {0, 4, 5, 6, 9, 7, 8}; if (ge >= 1 && ge <= 6) a = ma[ge], b = mb[ge]; }
        else if (phase > 6) { static const int ma[7] = {0, 11, 12, 13, 16, 14, 15}, mb[7] = {0, 4, 5, 6, 9, 7, 8}; if (ge >= 1 && ge <= 6) a = ma[ge], b = mb[ge]; }
        textIdx = a; imgIdx = b;
        mu::valuesToRadar(g, radar);
    }
    void update() {
        if (!shown) { done = true; return; }
        if (done) return;
        s -= 0.01f; alpha = std::min(255, alpha + 24);
        if (s <= 1) { s = 1; alpha = 255; done = true; }
    }
    void p(int sprite, float x, float y, float sx = 1, float sy = 1) const {
        ui(*psrts, kPsrts[sprite - 34], x * s, y * s, alpha / 255.f, sx * s, sy * s);
    }
    void q(Spr r, float x, float y, float sx = 1, float sy = 1) const { ui(*psrts, r, x * s, y * s, alpha / 255.f, sx * s, sy * s); }
    void draw() const {
        if (!shown) return;
        static const char* kPeriod[6] = {"", "11/22（木）企画書提出予定", "12/21（金）α版提出予定", "1/25（金）β版提出予定", "2/8（金）マスター版提出予定", "配信中"};
        static const char* kKind[17] = {"ＲＰＧ","超スーパーＲＰＧ","コマンド入力式\n王道ＲＰＧ","ハリウッド映画級\n超大作ＲＰＧ","ボイスアクション\nＲＰＧ","コマンド入力式\n王道ＲＰＧ","オープンワールド\nＦＰＳ","ボイスアクション\nＲＰＧ","ハリウッド映画級\n超大作ＲＰＧ","ドラマティック\n大河ＲＰＧ","だれでもかんたん\nリズムＲＰＧ","コマンド入力式\n王道ＲＰＧ","オープンワールド\nＦＰＳ","完全フルボイス\nアクションＲＰＧ","ハリウッド映画級\n超大作ＲＰＧ","ドラマティック\n大河ドラマＲＰＧ","だれでもかんたん\nリズムＲＰＧ"};
        static const char* kDesc[17] = {
            "美しいイラスト・ＣＧで表現される\n重厚なシナリオを、フルボイスで！\n簡単な操作で誰でも楽しめる、\n王道ＲＰＧです！",
            "前衛的なイラスト・ＣＧで表現される\n奇抜なシナリオを、奇跡のボイスで！\n不思議な操作でキリンでも楽しめる、\n超スーパーＲＰＧです！",
            "シンプルに、ファンタジーな世界観を\n楽しむことができる王道ＲＰＧ！\n美麗なイラストが、貴方を\n作品世界の奥へと引き込む――。",
            "迫力あるイベントムービーが、\nドラマを、戦闘を、全てを盛り上げる！\nハリウッド映画も公開を見送るほどの\n超大作ＲＰＧ！",
            "テンポ良く攻撃を繰り出して戦おう！\n戦闘中にちょこまかと動き回る\nキャラ達がかわいい、ラブリーな\nボイスアクションＲＰＧ！",
            "言葉では表現できないほどに美しく、\n貴方の心を引き込むイラスト。\n鮮やかに広がる白く眩しい世界――。\nこれが本物の「王道」ＲＰＧだ！",
            "世界基準はやっぱりＦＰＳ！\nＲＰＧ要素は残しつつ、\n自由に世界を旅することができる、\nオープンワールドＦＰＳだ！",
            "「声」によって彩られた、\n不思議で楽しいゲーム体験！\n七色の声に包まれた世界で、\n動き回れる新感覚アクションＲＰＧ！",
            "イベントムービーは更に進化を遂げ、\nまるで映画の中に居るような感覚に！\n端末の限界に迫る３Ｄ表現が、\n貴方を異世界へと誘う超大作ＲＰＧ！",
            "複雑に絡み合うシナリオ、登場人物。\nこの壮大な物語の行く末は――。\nゲーム脚本の限界に挑む、\nドラマティック大河ＲＰＧ！",
            "だれでもかんたん、リズムでバトル！\nむずかしいそうさは、いっさいなし！\nリズムにあわせてたたかうだけで、\nだれでもたのしめるＲＰＧです！",
            "言葉では表現できないほどに美しく、\n貴方の心を引き込むイラスト。\n鮮やかに広がる白く眩しい世界――。\nこれが本物の「王道」ＲＰＧだ！",
            "世界基準はやっぱりＦＰＳ！\nＲＰＧ要素は残しつつ、\n自由に世界を旅することができる、\nオープンワールドＦＰＳだ！",
            "何もかもが「声」によって彩られた、\n不思議で楽しいゲーム体験！\n七色の声に包まれた世界で、\n動き回れる新感覚アクションＲＰＧ！",
            "イベントムービーは更に進化を遂げ、\nまるで映画の中に居るような感覚に！\n端末の限界に迫る３Ｄ表現が、\n貴方を異世界へと誘う超大作ＲＰＧ！",
            "複雑に絡み合うシナリオ、登場人物。\nこの壮大な物語の行く末は――。\nゲーム脚本の限界に挑む、\nドラマティック大河ＲＰＧ！",
            "だれでもかんたん、リズムでバトル！\nむずかしいそうさは、いっさいなし！\nリズムにあわせてたたかうだけで、\nだれでもたのしめるＲＰＧです！"};
        for (int i = 0; i < 3; i++) {
            p(34 + i, -220, i == 1 ? -59.f : -56.f * i, 1, i == 1 ? 12.5f : 1);
            p(40 + i, 0, i == 1 ? -59.f : -56.f * i, 54, i == 1 ? 12.5f : 1);
        }
        p(43, 212, 0); p(44, 220, -64, 1, 15); p(45, 220, -112);
        p(37, 80, 0); p(39, 80, -112);
        for (int i = 0; i < 12; i++) p(38, 80, -16 - 8.f * i);
        q({72, 0, 200, 96}, 152, -56);
        if (done) {
            static const SDL_FPoint uv[3] = {{0.828125f, 0.2421875f}, {0.828125f, 0.015625f}, {0.8769531f, 0.1328125f}};
            mu::radar(*psrts, 152, -56, radar, 1, uv);
        }
        q({200, 0, 256, 64}, 152, -56);
        { int k = imgIdx; float x = 25 * s, y = -78 * s; ui(*imgs, {96 * (k % 5), 56 * (k / 5), 96 * (k % 5) + 96, 56 * (k / 5) + 56}, x, y, alpha / 255.f, s, s); }
        p(46, -210, -25); p(47, -115, -25, 23); p(48, -20, -29); p(49, 25, -29, 10.2f); p(50, 70, -29);
        q({256, 56, 376, 80}, 170, -115);
        q({256, 40, 368, 56}, 130, 2);
        if (rank != 0) { int f = prev == 0 ? 2 : rank < prev ? 0 : rank > prev ? 1 : 2; q({368 + 24 * f, 40, 392 + 24 * f, 56}, 171, 2); }
        // sales, right aligned with thousands marks
        { int v = sc, n = 1; for (int t = v; t >= 10; t /= 10) n++; float x = 208;
          for (int i = 0; i < n; i++) {
              if (i == 0 || i % 3 != 0) x -= 9;
              else { q({416, 0, 424, 8}, x - 7, -119); x -= 14; }
              int d = v % 10; v /= 10; q({256 + 16 * d, 0, 272 + 16 * d, 16}, x, -115);
          } }
        if (rank != 0) { int v = rank, n = 1; for (int t = v; t >= 10; t /= 10) n++; float x = n == 1 ? 212 : n == 2 ? 218 : 224;
          for (int i = 0; i < n; i++) { int d = v % 10; v /= 10; q({256 + 16 * d, 16, 272 + 16 * d, 40}, x, 2); x -= 13; } }
        float a = alpha / 255.f;
        mu::textCentered(g_fontM, title, -92, 2, {255, 255, 255, 255}, 15, 17, a);
        int per = phase <= 2 ? 1 : phase <= 4 ? 2 : phase <= 6 ? 3 : phase == 7 ? 4 : 5;
        mu::textCentered(g_fontS, kPeriod[per], -110, -25, {0, 0, 0, 255}, 10, 14, a);
        mu::textCentered(g_fontS, kKind[textIdx], 25, -30, {0, 0, 0, 255}, 10, 12, a);
        mu::text(g_fontS, kDesc[textIdx], -206, -48, {0, 0, 0, 255}, 10, 14, a);   // ASSUMED left edge of the 512 px text buffer
    }
};

struct SysRow { float x, y, tx; int spr; };

// 0x10 and 0x12 share one layout: 0x12 is the reduced list without square and tasks
struct GameMenu : Job {
    bool reduced; TexP bg, button, psrts, imgs; PageHeader hdr; StatusPanel panel;
    int state = 0, cur = 0, sub = 0, pending = -1; float bx[4]; int nTop = 4; bool unlocked = true, expanded = false, askTitle = false, entered = false;
    float t = 0; SysRow sys[4]{};
    explicit GameMenu(bool r) : reduced(r) {}
    void onEnter() override {
        bg = g_gfx.load("menu_bg.txg"); button = g_gfx.load("menu_button.txg"); psrts = g_gfx.load("menu_psrts.txg"); imgs = g_gfx.load("menu_img.txg");
        hdr.set(8); hdr.start();
        unlocked = !reduced && gd().routeA != 0;
        nTop = unlocked ? 4 : 2;
        for (auto& x : bx) x = -280;
        for (int i = 0; i < 4; i++) sys[i] = {164, 100, -170 + 90.f * i, 5 + i};
        panel.init(psrts, imgs); if (reduced) panel.shown = false;
    }
    void onResume() override {
        if (!entered) { g_trans.reveal(0); entered = true; }   // ASSUMED: entry is always behind the style 2 cover
        else g_trans.reveal(10);
        pending = -1; state = std::max(state, 0);
    }
    float topX(int i) const { return -180 + 90.f * i; }
    int overTop() const {
        for (int i = 0; i < nTop; i++) if (mu::hit(topX(i), 60, 88, 56)) return i;
        if (mu::hit(180, 60, 100, 100)) return 4;
        return -1;
    }
    int overSub() const { for (int i = 0; i < 4; i++) if (mu::hit(sys[i].x, sys[i].y, 88, 56)) return i; return -1; }
    int mapResult(int r) const {
        if (reduced) { static const int m[7] = {0, 0x1f, 0x13, 0x1b, 0x1c, 0xf, 0x14}; return m[r]; }
        static const int m[9] = {0, 0xa, 0xd, 0x1f, 0x13, 0x1b, 0x1c, 0xf, 0x14}; return m[r];
    }
    void result(int r) {
        if (r == 0) { g_trans.style(2); g_trans.cover(10); pending = 0; return; }
        int id = mapResult(r);
        if (id == 0xa) simChartMode(0);
        if (id == 0x14) rerollVoiceIndex();
        g_trans.style(0); g_trans.cover(10); pending = id;
    }
    void onUpdate() override {
        hdr.update(); panel.update();
        if (pending >= 0) {
            if (g_trans.covered()) {
                int id = pending; pending = -2;
                if (id == 0) selfPop();
                else if (id == 0x14) { g_root->popAll(); g_root->push(3); }
                else parent->push(id);
            }
            return;
        }
        if (pending == -2) return;
        switch (state) {
        case 0: {
            bool all = true;
            for (int i = 0; i < nTop; i++) { bool was = bx[i] >= topX(i); bx[i] = std::min(topX(i), bx[i] + 20); if (!was && bx[i] >= topX(i)) mu::se(0x21); all &= bx[i] >= topX(i); }
            if (all) state = 1;
            break; }
        case 1: if (panel.done) state = 5; break;
        case 3:
            t += 0.8f;
            for (auto& s : sys) { s.x += (s.tx - s.x) * 0.3f; s.y += (30 - s.y) * 0.3f; }
            if (t >= 8) { t = 8; for (auto& s : sys) { s.x = s.tx; s.y = 30; } state = 5; }
            break;
        case 4:
            t -= 0.8f;
            for (auto& s : sys) { s.x += (164 - s.x) * 0.3f; s.y += (100 - s.y) * 0.3f; }
            if (t <= 0) { t = 0; expanded = false; state = 5; }
            break;
        case 5: input(); break;
        }
    }
    void input() {
        if (askTitle) {
            if (g_popup.isOpen()) return;
            askTitle = false;
            if (g_popup.yes()) result(reduced ? 6 : 8);
            return;
        }
        if (!expanded) {
            int o = cur, over = overTop();
            if (g_input.mouseMoved && over >= 0) cur = over;
            if (g_input.isPressed(BTN_RIGHT)) cur = unlocked ? (cur + 1) % 5 : cur == 1 ? 4 : cur == 4 ? 0 : cur + 1;
            if (g_input.isPressed(BTN_LEFT)) cur = unlocked ? (cur + 4) % 5 : cur == 4 ? 1 : cur == 0 ? 4 : cur - 1;
            if (cur != o) mu::se(0x2a);
            if (mu::confirm(over >= 0)) {
                mu::se(0x29);
                if (cur == 4) { expanded = true; state = 3; }
                else result(reduced ? cur + 1 : unlocked ? cur + 1 : cur + 3);
            } else if (mu::cancel()) { mu::se(0x1f); result(0); }
        } else {
            int o = sub, over = overSub();
            if (g_input.mouseMoved && over >= 0) sub = over;
            if (g_input.isPressed(BTN_RIGHT)) sub = (sub + 1) % 4;
            if (g_input.isPressed(BTN_LEFT)) sub = (sub + 3) % 4;
            if (sub != o) mu::se(0x2a);
            if (mu::confirm(over >= 0)) {
                mu::se(0x29);
                if (sub == 3) { g_popup.open("タイトルへ戻りますか？", true, true); askTitle = true; }
                else result(reduced ? sub + 3 : sub + 5);
            } else if (mu::cancel()) { mu::se(0x1f); sub = 0; state = 4; }
        }
    }
    void ring(float x, float y, float z) const {
        ui(*button, {32, 224, 40, 232}, x - 42, y + 30); ui(*button, {40, 224, 48, 232}, x, y + 30, 1, 9.5f); ui(*button, {48, 224, 56, 232}, x + 42, y + 30);
        ui(*button, {32, 240, 40, 248}, x - 42, y - 27); ui(*button, {40, 240, 48, 248}, x, y - 27, 1, 9.5f); ui(*button, {48, 240, 56, 248}, x + 42, y - 27);
        ui(*button, {32, 232, 40, 240}, x - 42, y + 1.5f, 1, 1, 6); ui(*button, {48, 232, 56, 240}, x + 42, y + 1.5f, 1, 1, 6);
    }
    void onDraw() override {
        mu::menuBg(); hdr.draw();
        if (!button) return;
        for (int i = 0; i < nTop; i++) ui(*button, btn(unlocked ? 1 + i : 3 + i), bx[i], 60);
        // folder, FUN_00086bc8 (position ASSUMED: fifth cell of the top row)
        const float fx = 180, fy = 60;
        bool fsel = cur == 4;
        if (expanded) ui(*button, {400, 0, 512, 120}, fx, fy);
        else if (fsel) ui(*button, {288, 0, 400, 120}, fx, fy);
        ui(*button, {176, 120, 288, 240}, fx, fy);
        if (!expanded) ui(*button, {176, 0, 288, 120}, fx - 16, fy + 27);
        panel.draw();
        if (expanded) {
            const float mid = 30, x0 = -230, x1 = -37, x2 = 160, top = mid + t * 4 + 4, bot = mid - (t * 4 + 8);
            ui(*button, {0, 224, 8, 232}, x0, top); ui(*button, {8, 224, 16, 232}, x1, top, 1, 47.3f); ui(*button, {16, 224, 32, 232}, x2, top);
            ui(*button, {0, 232, 8, 240}, x0, mid, 1, 1, t); ui(*button, {8, 232, 16, 240}, x1, mid, 1, 47.3f, t); ui(*button, {16, 232, 32, 240}, x2, mid, 1, 1, t);
            ui(*button, {0, 240, 8, 256}, x0, bot); ui(*button, {8, 240, 16, 256}, x1, bot, 1, 47.3f); ui(*button, {16, 240, 32, 256}, x2, bot);
            if (t > 1) for (auto& s : sys) ui(*button, btn(s.spr), s.x, s.y);
        }
        if (state == 5 && pending == -1) {
            if (expanded) ring(sys[sub].x, sys[sub].y, 1);
            else if (cur == 4) ring(fx, fy, 0);
            else ring(topX(cur), 60, 0);
        }
    }
};

// 0x11: drama menu, FUN_0008c... save, load, option, back to DLC menu
struct DramaMenu : Job {
    TexP button; PageHeader hdr; int cur = 0, pending = -1; bool ask = false, entered = false, leaving = false;
    void onEnter() override { button = g_gfx.load("menu_button.txg"); hdr.set(8); hdr.start(); }
    void onResume() override { if (!entered) { g_trans.reveal(0); entered = true; } else g_trans.reveal(10); pending = -1; }
    static float itemX(int i) { return -150 + 100.f * i; }
    void go(int r) {
        if (r == 0) { g_trans.style(2); g_trans.cover(10); pending = 0; return; }
        g_trans.style(0); g_trans.cover(10); pending = r;
    }
    void onUpdate() override {
        hdr.update();
        if (pending >= 0) {
            if (!g_trans.covered()) return;
            int r = pending; pending = -2;
            static const int m[4] = {0, 0x25, 0x26, 0xf};
            if (r == 0) selfPop();
            else if (r == 4) { rerollVoiceIndex(); Job* h = progressHost(); if (h && h->parent) h->parent->pop(); else selfPop(); }
            else parent->push(m[r]);
            return;
        }
        if (pending == -2) return;
        if (ask) { if (g_popup.isOpen()) return; ask = false; if (g_popup.yes()) go(4); return; }
        int o = cur, over = -1;
        for (int i = 0; i < 4; i++) if (mu::hit(itemX(i), 30, 88, 56)) over = i;
        if (g_input.mouseMoved && over >= 0) cur = over;
        if (g_input.isPressed(BTN_RIGHT)) cur = (cur + 1) % 4;
        if (g_input.isPressed(BTN_LEFT)) cur = (cur + 3) % 4;
        if (cur != o) mu::se(0x2a);
        if (mu::confirm(over >= 0)) {
            mu::se(0x29);
            if (cur == 3) { g_popup.open("ダウンロードコンテンツの\nメニューへ戻りますか？", true, true); ask = true; }
            else go(cur + 1);
        } else if (mu::cancel()) { mu::se(0x1f); go(0); }
    }
    void onDraw() override {
        mu::menuBg(); hdr.draw();
        if (!button) return;
        for (int i = 0; i < 4; i++) ui(*button, i == 3 ? Spr{288, 176, 352, 224} : btn(5 + i), itemX(i), 30);
        float x = itemX(cur), y = 30;
        ui(*button, {32, 224, 40, 232}, x - 42, y + 30); ui(*button, {40, 224, 48, 232}, x, y + 30, 1, 9.5f); ui(*button, {48, 224, 56, 232}, x + 42, y + 30);
        ui(*button, {32, 240, 40, 248}, x - 42, y - 27); ui(*button, {40, 240, 48, 248}, x, y - 27, 1, 9.5f); ui(*button, {48, 240, 56, 248}, x + 42, y - 27);
        ui(*button, {32, 232, 40, 240}, x - 42, y + 1.5f, 1, 1, 6); ui(*button, {48, 232, 56, 240}, x + 42, y + 1.5f, 1, 1, 6);
    }
};
}

std::unique_ptr<Job> createMenuScene(int id) {
    switch (id) {
    case 0x10: return std::make_unique<GameMenu>(false);
    case 0x12: return std::make_unique<GameMenu>(true);
    case 0x11: return std::make_unique<DramaMenu>();
    }
    if (auto j = createSaveLoadScene(id)) return j;
    if (auto j = createOmakeScene(id)) return j;
    return createMiscScene(id);
}
