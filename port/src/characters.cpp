// Conversation sprites (natives 161-178) and the Live2D layer (179-196). docs/spec_characters.md.
#include "game.h"
#include "audio.h"
#include "characters_api.h"
#include "live2d_game.h"
#include <cmath>
#include <cstring>

namespace {

std::string g_speaker;
bool g_forceVoice = false;
int64_t g_ticks = 0;

struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3 operator+(Vec3 o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator*(float f) const { return {x * f, y * f, z * f}; }
};
Vec3 lerp(Vec3 a, Vec3 b, float t) {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
}

constexpr size_t kSpriteRec = 0x124, kSlotRec = 0x260;

void wr32(uint8_t* p, uint32_t v) { memcpy(p, &v, 4); }
void wrf(uint8_t* p, float v) { memcpy(p, &v, 4); }
void wrv(uint8_t* p, Vec3 v) { memcpy(p, &v, 12); }
int rd32(const uint8_t* p) { int v; memcpy(&v, p, 4); return v; }
Vec3 rdv(const uint8_t* p) { Vec3 v; memcpy(&v, p, 12); return v; }
void wrs(uint8_t* p, const std::string& s, size_t n) {
    memset(p, 0, n);
    memcpy(p, s.data(), std::min(s.size(), n - 1));
}
std::string rds(const uint8_t* p, size_t n) { return std::string((const char*)p, strnlen((const char*)p, n)); }

// The renderer may be gone already when a scene is torn down at exit; its textures went with it.
void dropTex(TexP& t) {
    if (t && !g_gfx.renderer()) t->t = nullptr;
    t.reset();
}

TexP makeNearest(const Image& im) {
    TexP t = g_gfx.fromImage(im);
    if (t) SDL_SetTextureScaleMode(t->t, SDL_ScaleModeNearest);
    return t;
}

struct Overlay { int count = 0, x = 0, y = 0, w = 0, h = 0, texW = 0, texH = 0; TexP tex; };

enum { A_HIDDEN, A_SHOWN, A_FADEIN, A_FADEOUT };

struct Sprite {
    std::string file, speaker;
    int alpha = 0;
    Vec3 pos, bounce, shake, swing;
    int aState = A_HIDDEN, fadeInDur = 0, fadeInT = 0, fadeOutDur = 0, fadeOutT = 0;
    int moveState = 0, moveDur = 0, moveT = 0; Vec3 moveStart, moveTarget;
    int bounceState = 0, bounceDur = 0, bounceT = 0; Vec3 bounceAmp;
    int shakeState = 0, shakeDur = 0, shakeT = 0; Vec3 shakeAmp, shakeAmpInf; uint32_t rng2 = 1;
    int swingState = 0, swingPeriod = 0, swingT = 0; Vec3 swingAmp;
    int lipMode = 0;
    uint32_t rng = (uint32_t)SDL_GetPerformanceCounter();
    bool loaded = false;
    // art
    TexP body; int bodyW = 0, bodyH = 0, ax = 0, ay = 0;
    Overlay eye, mouth;
    // blink and lip machines
    int blinkState = 0, blinkC = 0, blinkLen = 0;
    int lipState = 0, lipC = 0, lipFrame = 0;

    ~Sprite() { dropArt(); }
    void dropArt() { dropTex(body); dropTex(eye.tex); dropTex(mouth.tex); }
    uint32_t next() { return charRngNext(rng); }

    void setAlpha(int s) {
        aState = s;
        if (s == A_HIDDEN) alpha = 0;
        else if (s == A_SHOWN) alpha = 255;
        else if (s == A_FADEIN) { alpha = 0; fadeInT = 0; }
        else { alpha = 255; fadeOutT = 0; }
    }
    void fadeIn(int d) { if (d < 1) setAlpha(A_SHOWN); else { fadeInDur = d; setAlpha(A_FADEIN); } }
    void fadeOut(int d) { if (d < 1) setAlpha(A_HIDDEN); else { fadeOutDur = d; setAlpha(A_FADEOUT); } }
    void setPos(Vec3 p) { pos = p; moveState = 0; }
    void moveTo(Vec3 p, int d) {
        if (d < 1) { setPos(p); return; }
        moveTarget = p; moveDur = d; moveState = 1; moveStart = pos; moveT = 0;
    }
    void hop(Vec3 a, int d) {
        if (d < 1) { bounceState = 0; bounce = {}; return; }
        bounceAmp = a; bounceDur = d; bounceState = 1; bounceT = 0;
    }
    void shakeStop() { shakeState = 0; shake = {}; }
    void shakeStart(Vec3 a, int d) {
        if (d < 0) { shakeAmpInf = a; shakeState = 2; rng2 = 1; }
        else if (d < 1) shakeStop();
        else { shakeAmp = a; shakeDur = d; shakeState = 1; shakeT = 0; rng2 = 1; }
    }
    void swingStop() { swingState = 0; swing = {}; }
    void swingStart(Vec3 a, int d) {
        if (d < 1) { swingStop(); return; }
        swingAmp = a; swingPeriod = d; swingState = 1; swingT = 0;
    }

    bool load(const std::string& name) {
        file = name.substr(0, 63);
        loaded = false;
        dropArt();
        Bytes d = g_fs.read(file);
        if (d.size() < 0x30 || memcmp(d.data(), "CSU", 4)) {
            if (!file.empty()) fprintf(stderr, "sprite: cannot load %s\n", file.c_str());
            return false;
        }
        const uint8_t* p = d.data();
        auto u = [&](size_t o) { return o + 4 <= d.size() ? rd32(p + o) : 0; };
        int anchor = u(4), bodyOff = u(8), eyeH = u(12), eyeT = u(16), mouthH = u(20), mouthT = u(24);
        ax = u(anchor); ay = u(anchor + 4);

        // Every tile is drawn in full, even past the image width, so keep the padded grid.
        const uint8_t* x = p + bodyOff;
        size_t xn = d.size() - bodyOff;
        if (xn < 0x30 || memcmp(x, "XGTL", 4)) return false;
        int tw = rd32(x + 16), th = rd32(x + 20), cols = rd32(x + 24), rows = rd32(x + 28);
        uint32_t first = rd32(x + 32), step = rd32(x + 36);
        Image im; im.w = tw * cols; im.h = th * rows; im.px.assign((size_t)im.w * im.h, 0);
        size_t off = first;
        for (int r = 0; r < rows; r++)
            for (int c = 0; c < cols; c++, off += step) {
                Image t;
                if (off + step > xn || !decodeTxg(x + off, step, t)) return false;
                for (int j = 0; j < th && j < t.h; j++)
                    memcpy(&im.px[(size_t)(r * th + j) * im.w + c * tw], &t.px[(size_t)j * t.w], std::min(tw, t.w) * 4);
            }
        body = makeNearest(im);
        bodyW = im.w; bodyH = im.h;

        auto overlay = [&](Overlay& o, int hdr, int tex) {
            o.count = u(hdr); o.x = u(hdr + 4); o.y = u(hdr + 8); o.w = u(hdr + 12); o.h = u(hdr + 16);
            o.texW = u(hdr + 20); o.texH = u(hdr + 24);
            if (o.count <= 0) { o.count = 0; return; }
            Image t;
            if ((size_t)tex >= d.size() || !decodeTxg(p + tex, d.size() - tex, t)) { o.count = 0; return; }
            o.tex = makeNearest(t);
        };
        overlay(eye, eyeH, eyeT);
        overlay(mouth, mouthH, mouthT);
        loaded = body != nullptr;
        return loaded;
    }

    bool speaking() const {
        return (lipMode == 1 || (lipMode == 0 && speaker == g_speaker)) && charVoicePlaying();
    }

    void update(bool skip) {
        if (aState == A_FADEIN) {
            fadeInT++; alpha = fadeInT * 255 / fadeInDur;
            if (skip || fadeInT >= fadeInDur) setAlpha(A_SHOWN);
        } else if (aState == A_FADEOUT) {
            fadeOutT++; alpha = 255 - fadeOutT * 255 / fadeOutDur;
            if (skip || fadeOutT >= fadeOutDur) setAlpha(A_HIDDEN);
        }
        if (moveState == 1) {
            moveT++;
            pos = lerp(moveStart, moveTarget, (float)moveT / (float)moveDur);
            if (skip || moveT >= moveDur) { pos = moveTarget; moveState = 0; }
        }
        if (bounceState == 1) {
            float f = (float)(bounceT + 1) / (float)bounceDur;
            bounceT++;
            bounce = bounceAmp * (-4 * f * f + 4 * f);
            if (skip || bounceT >= bounceDur) { bounceState = 0; bounce = {}; }
        }
        if (shakeState != 0) {
            Vec3 a = shakeState == 1 ? shakeAmp : shakeAmpInf;
            if (shakeState == 1) shakeT++;
            auto one = [&](float amp) { return (amp * 2 * (float)charRngNext(rng2)) / 32767.0f - amp; };
            shake.x = one(a.x); shake.y = one(a.y); shake.z = one(a.z);
            if (shakeState == 1 && (skip || shakeT >= shakeDur)) shakeStop();
        }
        if (swingState == 1) {
            swingT = (swingT + 1) % swingPeriod;
            swing = swingAmp * sinf(2 * (float)M_PI * (float)swingT / (float)swingPeriod);
        }
        if (!loaded) return;
        // blink
        if (blinkState == 0) {
            blinkC++;
            if (next() % 180 == 0) { blinkState = 1; blinkC = 0; blinkLen = next() % 6 + 3; }
        } else {
            blinkC++;
            if (blinkC >= blinkLen * 2) blinkState = 0;
        }
        // lip
        if (lipState == 0) {
            if (speaking()) { lipState = 1; lipC = 0; lipFrame = 0; }
        } else {
            if (mouth.count > 0) {
                lipC++;
                if ((int)(next() % 20) < lipC) {
                    if (lipFrame < 1) lipFrame++;
                    else if (lipFrame < mouth.count - 1) lipFrame += (next() & 1) == 0 ? 1 : -1;
                    else lipFrame--;
                    lipC = 0;
                }
            }
            bool stay = lipMode == 1 ? charVoicePlaying()
                      : lipMode == 0 ? (speaker == g_speaker && charVoicePlaying()) : false;
            if (!stay) lipState = 0;
        }
    }

    int eyeFrame() const {
        int N = eye.count;
        if (blinkState == 0 || N <= 0) return -1;
        int T = blinkLen, f = 0;
        if (blinkC < T) {
            for (int i = 1; i < N; i++) { if (blinkC < T * i / N) break; f = i; }
        } else {
            for (int i = 1; i < N; i++) { if (blinkC - T < T * i / N) break; f = i; }
            f = N - 1 - f;
        }
        return f;
    }
    int mouthFrame() const { return lipState != 0 && mouth.count > 0 ? std::min(lipFrame, mouth.count - 1) : -1; }

    void save(uint8_t* r) const {
        wrs(r, file, 0x40);
        wr32(r + 0x40, alpha); wrv(r + 0x44, pos); wrv(r + 0x50, bounce); wrv(r + 0x5c, shake); wrv(r + 0x68, swing);
        wr32(r + 0x74, aState);
        wr32(r + 0x78, fadeInDur); wr32(r + 0x7c, fadeInT); wr32(r + 0x80, fadeOutDur); wr32(r + 0x84, fadeOutT);
        wr32(r + 0x88, moveState);
        wrv(r + 0x8c, moveStart); wrv(r + 0x98, moveTarget); wr32(r + 0xa4, moveDur); wr32(r + 0xa8, moveT);
        wr32(r + 0xac, bounceState); wrv(r + 0xb0, bounceAmp); wr32(r + 0xbc, bounceDur); wr32(r + 0xc0, bounceT);
        wr32(r + 0xc4, shakeState); wrv(r + 200, shakeAmp); wr32(r + 0xd4, shakeDur); wr32(r + 0xd8, shakeT);
        wrv(r + 0xdc, shakeAmpInf);
        wr32(r + 0xe8, swingState); wrv(r + 0xec, swingAmp); wr32(r + 0xf8, swingPeriod); wr32(r + 0xfc, swingT);
        wrs(r + 0x100, speaker, 0x20);
        wr32(r + 0x120, lipMode);
    }
    void restore(const uint8_t* r) {
        std::string n = rds(r, 0x40);
        if (!n.empty()) load(n); else { file.clear(); loaded = false; dropArt(); }
        alpha = rd32(r + 0x40); pos = rdv(r + 0x44); bounce = rdv(r + 0x50); shake = rdv(r + 0x5c); swing = rdv(r + 0x68);
        aState = rd32(r + 0x74);
        fadeInDur = rd32(r + 0x78); fadeInT = rd32(r + 0x7c); fadeOutDur = rd32(r + 0x80); fadeOutT = rd32(r + 0x84);
        moveState = rd32(r + 0x88);
        moveStart = rdv(r + 0x8c); moveTarget = rdv(r + 0x98); moveDur = rd32(r + 0xa4); moveT = rd32(r + 0xa8);
        bounceState = rd32(r + 0xac); bounceAmp = rdv(r + 0xb0); bounceDur = rd32(r + 0xbc); bounceT = rd32(r + 0xc0);
        shakeState = rd32(r + 0xc4); shakeAmp = rdv(r + 200); shakeDur = rd32(r + 0xd4); shakeT = rd32(r + 0xd8);
        shakeAmpInf = rdv(r + 0xdc);
        swingState = rd32(r + 0xe8); swingAmp = rdv(r + 0xec); swingPeriod = rd32(r + 0xf8); swingT = rd32(r + 0xfc);
        speaker = rds(r + 0x100, 0x20);
        lipMode = rd32(r + 0x120);
    }
};

enum { SA_IDLE, SA_CROSS, SA_FADEIN, SA_FADEOUT };

struct Slot {
    Sprite spr[2];
    int front = 0;
    int aState = SA_IDLE, moveState = 0, bounceState = 0, shakeState = 0, swingState = 0;
    Sprite& frontS() { return spr[front]; }
    Sprite& backS() { return spr[1 - front]; }

    void fadeIn(int d) {
        if (d < 1) { frontS().setAlpha(A_SHOWN); backS().setAlpha(A_HIDDEN); aState = SA_IDLE; return; }
        backS().setAlpha(A_HIDDEN); frontS().fadeIn(d); aState = SA_FADEIN;
    }
    void fadeOut(int d) {
        if (d < 1) { frontS().setAlpha(A_HIDDEN); backS().setAlpha(A_HIDDEN); aState = SA_IDLE; return; }
        backS().setAlpha(A_HIDDEN); frontS().fadeOut(d); aState = SA_FADEOUT;
    }
    void crossfade(int d) {
        if (d < 1) { frontS().setAlpha(A_SHOWN); backS().setAlpha(A_HIDDEN); aState = SA_IDLE; return; }
        backS().fadeOut(d); frontS().setAlpha(A_SHOWN); aState = SA_CROSS;
    }
    void setPos(Vec3 p) { spr[0].setPos(p); spr[1].setPos(p); moveState = 0; }
    void moveTo(Vec3 p, int d) {
        if (d < 1) { setPos(p); return; }
        spr[0].moveTo(p, d); spr[1].moveTo(p, d); moveState = 1;
    }
    void hop(Vec3 a, int d) {
        if (d < 1) { bounceState = 0; return; }    // the slot idle enter is empty: a running hop finishes
        spr[0].hop(a, d); spr[1].hop(a, d); bounceState = 1;
    }
    void shakeStop() { shakeState = 0; spr[0].shakeStop(); spr[1].shakeStop(); }
    void shakeStart(Vec3 a, int d) {
        if (d == 0) { shakeStop(); return; }
        spr[0].shakeStart(a, d); spr[1].shakeStart(a, d);
        shakeState = d < 0 ? 2 : 1;
    }
    void swingStop() { swingState = 0; spr[0].swingStop(); spr[1].swingStop(); }
    void swingStart(Vec3 a, int d) {
        if (d < 1) { swingStop(); return; }
        spr[0].swingStart(a, d); spr[1].swingStart(a, d); swingState = 1;
    }

    void update(bool skip) {
        frontS().update(skip);
        backS().update(skip);
        if (aState == SA_CROSS) { if (frontS().aState == A_SHOWN && backS().aState == A_HIDDEN) aState = SA_IDLE; }
        else if (aState == SA_FADEIN) { if (frontS().aState == A_SHOWN) aState = SA_IDLE; }
        else if (aState == SA_FADEOUT) { if (frontS().aState == A_HIDDEN) aState = SA_IDLE; }
        if (moveState == 1 && !frontS().moveState && !backS().moveState) moveState = 0;
        if (bounceState == 1 && !frontS().bounceState && !backS().bounceState) bounceState = 0;
        if (shakeState == 1 && !frontS().shakeState && !backS().shakeState) shakeStop();
    }

    void save(uint8_t* r) const {
        memset(r, 0, kSlotRec);
        spr[0].save(r); spr[1].save(r + kSpriteRec);
        wr32(r + 0x248, front != 0);
        wr32(r + 0x24c, aState); wr32(r + 0x250, moveState); wr32(r + 0x254, bounceState);
        wr32(r + 0x258, shakeState); wr32(r + 0x25c, swingState);
    }
    void restore(const uint8_t* r) {
        spr[0].restore(r); spr[1].restore(r + kSpriteRec);
        front = rd32(r + 0x248) ? 1 : 0;
        aState = rd32(r + 0x24c); moveState = rd32(r + 0x250); bounceState = rd32(r + 0x254);
        shakeState = rd32(r + 0x258); swingState = rd32(r + 0x25c);
    }
};

}  // namespace

void charSetSpeaker(const std::string& n) { g_speaker = n; }
const std::string& charSpeaker() { return g_speaker; }
bool charVoicePlaying() { return g_forceVoice || g_audio.playing(CH_VOICE); }
void charForceVoice(bool on) { g_forceVoice = on; }
int64_t charClockMs() { return g_ticks * 1000 / 60; }

struct CharStage::Impl {
    Slot slots[2];
    L2DStage l2d;
    SDL_Texture* scratch = nullptr; int scratchW = 0, scratchH = 0;

    ~Impl() { freeScratch(); }
    void freeScratch() {
        if (scratch && g_gfx.renderer()) SDL_DestroyTexture(scratch);
        scratch = nullptr; scratchW = scratchH = 0;
    }
    bool ensureScratch(int w, int h) {
        if (scratch && w <= scratchW && h <= scratchH) return true;
        int nw = std::max(w, scratchW), nh = std::max(h, scratchH);
        freeScratch();
        scratch = g_gfx.makeTarget(nw, nh);
        if (!scratch) return false;
        SDL_SetTextureScaleMode(scratch, SDL_ScaleModeNearest);
        scratchW = nw; scratchH = nh;
        return true;
    }

    // The PSP draws the overlays first with depth write on, so the body is rejected under their
    // rectangles and a fading sprite blends each pixel once. Here the sprite is assembled in a
    // scratch target without blending, overlays replacing the body, and blended once as a whole.
    void drawSprite(Sprite& s) {
        if (!s.loaded || s.alpha <= 0) return;
        SDL_Renderer* r = g_gfx.renderer();
        int ef = s.eyeFrame(), mf = s.mouthFrame();
        int w = s.bodyW, h = s.bodyH;
        auto extend = [&](const Overlay& o) { w = std::max(w, o.x + o.w); h = std::max(h, o.y + o.h); };
        if (ef >= 0) extend(s.eye);
        if (mf >= 0) extend(s.mouth);
        if (!ensureScratch(w, h)) return;

        SDL_Texture* prev = SDL_GetRenderTarget(r);
        SDL_SetRenderTarget(r, scratch);
        SDL_Rect area{0, 0, w, h};
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 0);
        SDL_RenderFillRect(r, &area);
        SDL_SetTextureBlendMode(s.body->t, SDL_BLENDMODE_NONE);
        SDL_Rect bodyR{0, 0, s.bodyW, s.bodyH};
        SDL_RenderCopy(r, s.body->t, &bodyR, &bodyR);
        auto patch = [&](const Overlay& o, int f) {
            if (f < 0 || !o.tex) return;
            SDL_Rect src{0, o.h * f, o.w, o.h}, dst{o.x, o.y, o.w, o.h};
            SDL_SetTextureBlendMode(o.tex->t, SDL_BLENDMODE_NONE);
            SDL_RenderCopy(r, o.tex->t, &src, &dst);
        };
        patch(s.eye, ef);
        patch(s.mouth, mf);
        SDL_SetRenderTarget(r, prev);

        Vec3 p = s.pos + s.bounce + s.shake + s.swing;
        SDL_FRect d{Gfx::W * 0.5f + p.x - (float)s.ax, Gfx::H * 0.5f - p.y - (float)s.ay, (float)w, (float)h};
        SDL_SetTextureBlendMode(scratch, SDL_BLENDMODE_BLEND);
        SDL_SetTextureAlphaMod(scratch, (Uint8)std::min(s.alpha, 255));
        SDL_RenderCopyF(r, scratch, &area, &d);
    }
};

CharStage::CharStage() : p(new Impl) {}
CharStage::~CharStage() {}

void CharStage::update(bool skip) {
    g_ticks++;
    p->slots[0].update(skip);
    p->slots[1].update(skip);
    p->l2d.tick();
}

void CharStage::drawSlot(int i) {
    if (i < 0 || i > 1) return;
    Slot& s = p->slots[i];
    p->drawSprite(s.frontS());
    p->drawSprite(s.backS());
}

void CharStage::drawLive2D() { p->l2d.draw(); }

int CharStage::native(Vm& v, int id, int32_t& ret) {
    ret = 0;
    auto frames = [](int ms) { return ms >= 0 ? ms * 60 / 1000 : ms; };
    if (id <= 178) {
        Slot& s = p->slots[v.arg(0) & 1];
        Vec3 xy{(float)v.arg(1), (float)v.arg(2), 0};
        switch (id) {
        case 161: s.backS().speaker = std::string(v.argStr(2)).substr(0, 31); s.backS().load(v.argStr(1)); return 0;
        case 162: return 0;                       // loads are synchronous here
        case 163: s.front = 1 - s.front; return 0;
        case 164: s.fadeIn(frames(v.arg(1))); return 0;
        case 165: s.fadeOut(frames(v.arg(1))); return 0;
        case 166: s.crossfade(frames(v.arg(1))); return 0;
        case 167: return s.aState != SA_IDLE;
        case 168: s.setPos(xy); return 0;
        case 169: s.moveTo(xy, frames(v.arg(3))); return 0;
        case 170: return s.moveState != 0;
        case 171: s.hop(xy, frames(v.arg(3))); return 0;
        case 172: return s.bounceState != 0;
        case 173: s.shakeStart(xy, frames(v.arg(3))); return 0;
        case 174: s.shakeStop(); return 0;
        case 175: return s.shakeState != 0;
        case 176: s.swingStart(xy, frames(v.arg(3))); return 0;
        case 177: s.swingStop(); return 0;
        case 178: s.spr[0].lipMode = s.spr[1].lipMode = v.arg(1); return 0;
        }
        return 0;
    }
    L2DStage& l = p->l2d;
    switch (id) {
    case 179: l.setSpeaker(v.argStr(0)); l.startLoad(v.argStr(1)); return 0;
    case 180: return 0;                           // loads are synchronous here
    case 181: l.create(); return 0;
    case 182: l.setScale((float)v.arg(0) / 1000.0f); return 0;
    case 183: l.moveTo((float)v.arg(0), (float)v.arg(1), (float)v.arg(2), v.arg(3) * 60 / 1000); return 0;
    case 184: l.jumpTo((float)v.arg(0), (float)v.arg(1), (float)v.arg(2), (float)v.arg(3) / 1000.0f,
                       (float)v.arg(4), v.arg(5) * 60 / 1000); return 0;
    case 185: case 186: case 187: l.queueMotion(v.argStr(0), id - 185); return 0;
    case 188: l.queueExpression(v.arg(0), v.arg(1) * 60 / 1000); return 0;
    case 189: return l.motionQueueBusy();
    case 190: return l.expressionBusy();
    case 191: return l.moveBusy();
    case 192: return l.jumpBusy();
    case 193: l.clearMotionQueue(); return 0;
    case 194: l.clearExpressionQueue(); return 0;
    case 195: l.release(); return 0;
    case 196: l.setLipMode(v.arg(0)); return 0;
    }
    return 0;
}

void CharStage::saveState(Bytes& out) {
    size_t o = out.size();
    out.resize(o + kSlotRec * 2 + kL2dSaveSize);
    p->slots[0].save(&out[o]);
    p->slots[1].save(&out[o + kSlotRec]);
    p->l2d.save(&out[o + kSlotRec * 2]);
}

void CharStage::loadState(const uint8_t*& q) {
    p->slots[0].restore(q);
    p->slots[1].restore(q + kSlotRec);
    p->l2d.load(q + kSlotRec * 2);
    q += kSlotRec * 2 + kL2dSaveSize;
}
