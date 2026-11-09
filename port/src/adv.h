// ADV (conversation) scene, job 0xb. Implements docs/spec_adv.md.
#pragma once
#include "game.h"
#include <string>
#include <vector>

extern std::string g_advScript;   // set by native 67

struct AdvGlyph { uint32_t cp; int x, y; Color c; };

struct MsgLink { int id, x0, y0, x1, y1; };

struct TextWin {
    enum { W_CLOSED, W_OPEN, W_OPENING, W_CLOSING };
    enum { T_IDLE, T_VOICEWAIT, T_TYPING, T_WAIT, T_TIPSEL, T_TIPVIEW };
    int win = W_CLOSED, winT = 0;
    int plate = W_CLOSED, plateT = 0;
    int text = T_IDLE;
    std::string raw; size_t pos = 0;
    std::string name;
    int msgId = -1; float acc = 0; int a = 0, b = 0;
    float penX = 0; int penY = 0;
    bool prop = false; int step = 18;   // Russian: proportional glyphs; 3-line messages use a tighter pitch
    bool inLink = false; std::vector<MsgLink> links; int sel = -1;
    std::vector<AdvGlyph> glyphs;
    void clearText() { glyphs.clear(); links.clear(); penX = 0; penY = 0; inLink = false; sel = -1; }
    void beginText();
};

struct BgImg { TexP tex; std::string file; int alpha = 0, state = 0, t = 0, n = 0; float x = 0, y = 0, cx = 248, cy = 144; };

struct Cloud {
    int state = 0, pos = 0, timeout = -1, ok[3], to[3], age = 0, t = 0, frame = 0;
    std::string text; float x = 0, y = 0;
};

class AdvScene : public Job, public NativeHandler {
public:
    AdvScene();
    void onEnter() override;
    void onResume() override;
    void onPause() override;
    void onUpdate() override;
    void onDraw() override;
    int native(Vm& vm, int id, int32_t& ret) override;
    void saveState(Bytes&) override;
    void loadState(const uint8_t*, size_t) override;
private:
    void start(const std::string&);
    void leave();
    void winUpdate1(TextWin&, bool isB);
    void winUpdate2(TextWin&, bool isB);
    void typing(TextWin&);
    void drawWinA(); void drawWinB(); void drawText(TextWin&, float sx, float sy);
    void drawMarker(float x, float y);
    void menuInput();
    void bgStep(); void drawBg(BgImg&);
    void choiceUpdate1(); void choiceUpdate2(); void drawChoice();
    void cloudsUpdate(); void drawClouds(); void cloudSpawn(int slot, int pos);
    void bannerUpdate(); void drawBanner();
    void dateUpdate(); void drawDate();
    void friendUpdate(); void drawFriend();
    void drawTip();
    TextWin& active() { return activeWin ? winB : winA; }
    void updateSpeaker();
    bool readOk(int id) { return g_sys.skipAll() || g_sys.textRead(id); }
    void voicePrepare(const std::string&);
    void bgmPlay(const std::string&, int frames);

    Vm vm; CharStage stage;
    int waitCtr = 0; bool skip = false, autoMode = false;
    TextWin winA, winB; int activeWin = 0; std::string speaker;
    std::string voiceName;
    int t30 = 0, t60 = 0, s30 = 0;
    TexP texWin, texMail, texChoice, texCloud, texCloudP, texPlace, texDate, texFriend;
    // bg
    BgImg bg[2]; int cur = 0, group = 0, scrollT = 0, scrollN = 0; float sx0 = 0, sy0 = 0, sx1 = 0, sy1 = 0;
    int shakeT = -2, shakeX = 0, shakeY = 0; float shOffX = 0, shOffY = 0; uint32_t seed = 1;
    // screen fade
    int fadeState = 1, fadeT = 0, fadeN = 0, fadeAlpha = 255;
    // sound save info
    int bgmDeck = 0; std::string bgmName, ambName, bgmPrep, ambPrep, sePrep; int bgmState = 0, ambState = 0, bgmVol = 255, ambVol = 255;
    // choice
    std::vector<std::string> ch; int chState = 0, chT = 0, chCur = -1;
    // clouds
    Cloud cl[32]; int clTimer[32], clTimerSlot[32], clMgr = 0, clResult = -1, clAt[4]; int clFrame = 0;
    // location banner / date / friendship
    TexP locTex; int banState = 0, banT = 0;
    int dateState = 0, dateT = 0, dateDur = 0;
    int frState = 0, frT = 0, frKind = 0;
    struct Petal { float s; int delay; float phase, lift; };
    std::vector<Petal> petals;
    // exit / menu transition
    int exitState = 0, exitT = 0; bool intro = true;
    int pendingScene = -1; bool resumeFade = false;
    bool hideUI = false;
    int ttl = 0, ttlT = 0; std::string ttlText, ttlHint; void titleUpdate(); void drawTitle();
    bool restoring = false;
};
