#include "live2d_game.h"
#include "audio.h"
#include "characters_api.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

uint32_t rd32(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }
float rdf(const uint8_t* p) { float v; memcpy(&v, p, 4); return v; }
uint16_t rd16(const uint8_t* p) { uint16_t v; memcpy(&v, p, 2); return v; }

// GIM: nested blocks {u16 id; u16; u32 size; u32 nextRel; u32 dataRel}. Index8 swizzled + RGBA8888 palette.
bool decodeGim(const uint8_t* d, size_t n, Image& im) {
    if (n < 0x40 || memcmp(d, "MIG.00.1PSP", 11)) return false;
    size_t root = 0x10, pic = root + rd32(d + root + 12), blk = pic + rd32(d + pic + 12);
    if (pic + 16 > n || blk + 16 > n || rd16(d + blk) != 4) return false;
    size_t pal = blk + rd32(d + blk + 8);
    if (pal + 16 > n || rd16(d + pal) != 5) return false;
    size_t ih = blk + rd32(d + blk + 12), ph = pal + rd32(d + pal + 12);
    if (ih + 0x20 > n || ph + 0x20 > n) return false;
    int fmt = rd16(d + ih + 4), order = rd16(d + ih + 6), w = rd16(d + ih + 8), h = rd16(d + ih + 10);
    size_t pix = ih + rd32(d + ih + 0x1c), palData = ph + rd32(d + ph + 0x1c);
    if (fmt != 5 || rd16(d + ph + 4) != 3 || pix + (size_t)w * h > n || palData + 1024 > n) return false;
    const uint8_t* src = d + pix;
    std::vector<uint8_t> raw((size_t)w * h);
    if (order == 1) {
        for (int by = 0; by < h / 8; by++)
            for (int bx = 0; bx < w / 16; bx++)
                for (int l = 0; l < 8; l++, src += 16)
                    memcpy(&raw[(size_t)(by * 8 + l) * w + bx * 16], src, 16);
    } else memcpy(raw.data(), src, raw.size());
    uint32_t palette[256];
    memcpy(palette, d + palData, 1024);
    im.w = w; im.h = h; im.px.resize((size_t)w * h);
    for (size_t i = 0; i < im.px.size(); i++) im.px[i] = palette[raw[i]];
    return true;
}

float easing(float x) { return x < 0 ? 0 : x > 1 ? 1 : 0.5f - 0.5f * cosf(x * 3.1415f); }

void wr32(uint8_t* p, uint32_t v) { memcpy(p, &v, 4); }
void wrf(uint8_t* p, float v) { memcpy(p, &v, 4); }
void wrv(uint8_t* p, const float* v) { memcpy(p, v, 12); }
void rdv(const uint8_t* p, float* v) { memcpy(v, p, 12); }
void wrs(uint8_t* p, const std::string& s, size_t n) {
    memset(p, 0, n);
    memcpy(p, s.data(), std::min(s.size(), n - 1));
}
std::string rds(const uint8_t* p, size_t n) { return std::string((const char*)p, strnlen((const char*)p, n)); }

}  // namespace

L2DStage::~L2DStage() { freeTextures(); }

void L2DStage::freeTextures() {
    // Scenes can outlive the renderer, and destroying the renderer already freed the textures.
    if (!g_gfx.renderer()) for (auto& t : textures) if (t) t->t = nullptr;
    textures.clear();
}

// ---- LBD ----
bool L2DStage::parseLbd() {
    const uint8_t* d = file.data();
    size_t n = file.size();
    std::vector<const uint8_t*> mocs;
    const uint8_t* str = nullptr; size_t strN = 0;
    auto name = [&](uint32_t o) { return o < strN ? std::string((const char*)str + o, strnlen((const char*)str + o, strN - o)) : std::string(); };
    motions.clear(); motionNames_.clear(); eyes.clear(); exprNames.clear(); exprTable.clear();
    noBlink.clear(); groups.clear(); overrides.clear(); freeTextures();
    mdl.reset();
    std::vector<std::pair<const uint8_t*, uint32_t>> exprChunks;
    for (size_t o = 0; o + 8 <= n;) {
        uint32_t type = rd32(d + o), size = rd32(d + o + 4);
        if (size < 8 || o + size > n) break;
        const uint8_t* c = d + o;
        switch (type) {
        case 9: str = c + 8; strN = size - 8; break;
        case 0: {
            auto m = std::make_unique<l2d::Model>();
            if (!m->parse(c + 0x10, rd32(c + 8))) return false;
            mdl = std::move(m);
            break;
        }
        case 1: {
            Image im;
            if (!decodeGim(c + 0x10, rd32(c + 8), im)) { fprintf(stderr, "L2D: bad GIM in %s\n", filename.c_str()); return false; }
            TexP t = g_gfx.fromImage(im);
            if (t) SDL_SetTextureScaleMode(t->t, SDL_ScaleModeLinear);
            textures.push_back(t);
            break;
        }
        case 2: {
            Motion m;
            m.name = name(rd32(c + 8)); m.loop = rd32(c + 0x10) != 0;
            parseMotion(m, (const char*)c + 0x14, std::min<size_t>(rd32(c + 0xc), size - 0x14));
            if (motions.size() < 20) { motions.push_back(std::move(m)); motionNames_.push_back(motions.back().name); }
            break;
        }
        case 3: {
            Eye e; e.name = name(rd32(c + 8));
            for (int i = 0; i < 4; i++) e.t[i] = (int)rd32(c + 0xc + i * 4);
            if (eyes.size() < 20) eyes.push_back(std::move(e));
            break;
        }
        case 4: {
            uint32_t cnt = rd32(c + 8);
            for (uint32_t i = 0; i < cnt && i < 30; i++) exprNames.push_back(name(rd32(c + 0xc + i * 4)));
            break;
        }
        case 5: exprChunks.push_back({c, size}); break;
        case 6: {
            uint32_t cnt = rd32(c + 8);
            for (uint32_t i = 0; i < cnt; i++) noBlink.push_back((int)rd32(c + 0xc + i * 4));
            break;
        }
        case 7: if (overrides.size() < 10) overrides.push_back({name(rd32(c + 8)), rdf(c + 0xc)}); break;
        case 8: {
            std::vector<std::string> g;
            uint32_t cnt = rd32(c + 8);
            for (uint32_t i = 0; i < cnt; i++) g.push_back(name(rd32(c + 0xc + i * 4)));
            if (groups.size() < 4) groups.push_back(std::move(g));
            break;
        }
        }
        o += size;
    }
    for (auto& [c, size] : exprChunks) {
        if (exprTable.size() >= 30) break;
        Expr e; e.eyeWeight = rdf(c + 8);
        for (size_t i = 0; i < exprNames.size() && 0xc + i * 4 + 4 <= size; i++) e.value.push_back(rdf(c + 0xc + i * 4));
        e.value.resize(exprNames.size());
        exprTable.push_back(std::move(e));
    }
    if (!mdl) return false;
    for (auto& m : motions) for (auto& t : m.tracks) t.param = mdl->paramIndex(t.name);
    return true;
}

void L2DStage::parseMotion(Motion& m, const char* p, size_t n) {
    const char* e = p + n;
    m.fps = 30; m.maxFrames = 0; m.tracks.clear();
    while (p < e) {
        const char* eol = (const char*)memchr(p, '\n', e - p);
        if (!eol) eol = e;
        const char* q = p;
        p = eol < e ? eol + 1 : e;
        while (q < eol && (*q == ' ' || *q == '\t')) q++;
        if (q >= eol || *q == '#') continue;
        const char* eq = (const char*)memchr(q, '=', eol - q);
        if (!eq) continue;
        std::string key(q, eq);
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
        if (key[0] == '$') {
            if (key == "$fps") { float f = (float)atof(std::string(eq + 1, eol).c_str()); if (f > 5 && f < 121) m.fps = f; }
            continue;
        }
        if (!isalpha((unsigned char)key[0]) && key[0] != '_') continue;
        std::vector<float> v;
        for (const char* c = eq + 1; c < eol;) {
            if (*c == ',' || *c == ' ' || *c == '\t') { c++; continue; }
            bool neg = *c == '-'; if (neg) c++;
            if (c >= eol || !isdigit((unsigned char)*c)) { if (c < eol && !neg) c++; continue; }
            double x = 0;
            while (c < eol && isdigit((unsigned char)*c)) x = x * 10 + (*c++ - '0');
            if (c < eol && *c == '.') {
                c++;
                double sc = 0.1;
                while (c < eol && isdigit((unsigned char)*c)) { x += (*c++ - '0') * sc; sc *= 0.1; }
            }
            v.push_back((float)(neg ? -x : x));
        }
        m.maxFrames = std::max(m.maxFrames, (int)v.size());
        if (key.rfind("LAYOUT:", 0) == 0) continue;     // parsed and ignored at runtime
        Track t; t.type = key.rfind("VISIBLE:", 0) == 0 ? 1 : 0; t.name = key; t.v = std::move(v);
        if (!t.v.empty()) m.tracks.push_back(std::move(t));
    }
    m.durationMs = (int)((float)m.maxFrames * 1000.f / m.fps);
}

// ---- motion evaluation and queue manager (7.4) ----
void L2DStage::evaluate(Entry& e, float w, int64_t t) {
    Motion& m = motions[e.motion];
    float elapsed = (float)std::max<int64_t>(0, t - e.start);
    float ft = elapsed * m.fps / 1000.f;
    int f = (int)ft;
    for (auto& tr : m.tracks) {
        int n = (int)tr.v.size();
        if (tr.type == 1) { mdl->setParam(tr.param, tr.v[std::min(f, n - 1)]); continue; }
        float cur_ = mdl->value(tr.param);
        float limit = (mdl->paramMax(tr.param) - mdl->paramMin(tr.param)) * 0.4f;
        float a = tr.v[std::min(f, n - 1)], b = tr.v[std::min(f + 1, n - 1)];
        float v = fabsf(b - a) > limit ? a : a + (b - a) * (ft - (float)f);
        mdl->setParam(tr.param, cur_ + (v - cur_) * w);
    }
    if (f >= m.maxFrames) {
        if (!m.loop) e.finished = true;
        else { e.start = t; e.fadeStart = t; }
    }
}

void L2DStage::mgrStart(int motion) {
    int64_t now = charClockMs();
    for (auto& e : entries) {
        int64_t end = now + motions[e.motion].fadeOut;
        e.end = e.end < 0 ? end : std::min(e.end, end);
    }
    entries.push_back({motion});
}

void L2DStage::mgrUpdate() {
    int64_t t = charClockMs();
    for (auto& e : entries) {
        if (!e.available || e.finished) continue;
        Motion& m = motions[e.motion];
        if (!e.started) {
            e.started = true; e.start = t; e.fadeStart = t;
            if (e.end < 0) {
                int dur = m.loop ? -1 : m.durationMs;
                e.end = dur > 0 ? e.start + dur : -1;
            }
        }
        float wIn = m.fadeIn ? easing((float)(t - e.fadeStart) / (float)m.fadeIn) : 1;
        float wOut = (m.fadeOut && e.end >= 0) ? easing((float)(e.end - t) / (float)m.fadeOut) : 1;
        evaluate(e, wIn * wOut, t);
        if (e.end >= 0 && e.end < t) e.finished = true;
    }
    entries.erase(std::remove_if(entries.begin(), entries.end(), [](const Entry& e) { return e.finished; }), entries.end());
}

bool L2DStage::mgrFinished() const {
    for (auto& e : entries) if (!e.finished) return false;
    return true;
}

const L2DStage::Eye* L2DStage::eyeWindow(const std::string& name) const {
    for (auto& e : eyes) if (e.name == name) return &e;
    return nullptr;
}

void L2DStage::next() {
    Queued& h_ = queue.front();
    if (blinkState != 0 && eyeWindow(h_.name)) return;   // wait
    auto setCur = [&] {
        cur = h_.motion; curName = h_.name; curKind = h_.kind;
        curFrames = motions[cur].durationMs / 16;
    };
    switch (h_.kind) {
    case 0: setCur(); motions[cur].fadeIn = 1000; motions[cur].fadeOut = 100; break;
    case 1: if (cur >= 0) motions[cur].fadeOut = 0;
            setCur(); motions[cur].fadeIn = 0; motions[cur].fadeOut = 0; break;
    default: setCur(); motions[cur].fadeIn = 1000; motions[cur].fadeOut = 0; break;
    }
    mgrStart(cur);
    frameCounter = 0;
    queue.pop_front();
}

void L2DStage::motionUpdate(float dt) {
    frameCounter = (int)((float)frameCounter + dt);
    if (queue.empty()) {
        if (!mgrFinished()) mgrUpdate();
        else if (cur >= 0) { mgrStart(cur); frameCounter = 0; }
    } else if (cur < 0 || motions[cur].loop || mgrFinished()) next();
    else mgrUpdate();
}

void L2DStage::queueMotion(const std::string& name, int kind) {
    if (!mdl) return;
    if (queue.size() >= 8) { fprintf(stderr, "L2D: motion queue full, dropping %s\n", name.c_str()); return; }
    for (size_t i = 0; i < motions.size(); i++)
        if (motions[i].name == name) { queue.push_back({kind, (int)i, name}); return; }
}

// ---- expressions (7.6) ----
void L2DStage::queueExpression(int id, int frames) {
    if (exprQueue.size() >= 4) { fprintf(stderr, "L2D: expression queue full\n"); return; }
    exprQueue.push_back({id, frames});
}

void L2DStage::expressionUpdate(float dt, bool canSwitch) {
    exprT += dt * exprRate;
    if (exprT >= 1 && !exprQueue.empty() && canSwitch) {
        ExprQ q = exprQueue.front();
        if (q.id != exprId) {
            for (size_t i = 0; i < exprNames.size() && i < 30; i++) snapshot[i] = mdl->value(exprNames[i]);
            exprT = 0; exprId = q.id;
            if (q.frames < 1) { exprT = 1; exprRate = 0; } else exprRate = 1.f / (float)q.frames;
        }
        exprQueue.pop_front();
    }
    if (exprT >= 1) exprT = 1;
    if (exprId < 0 || exprId >= (int)exprTable.size()) return;
    const Expr& e = exprTable[exprId];
    for (size_t i = 0; i < exprNames.size(); i++) {
        const std::string& n = exprNames[i];
        bool ball = n == "PARAM_EYE_BALL_X" || n == "PARAM_EYE_BALL_Y";
        mdl->addWeighted(n, e.value[i], ball ? e.eyeWeight : 1.f);
        if (exprT < 1) mdl->addWeighted(n, snapshot[i], 1 - exprT);
    }
}

float L2DStage::exprTarget(const std::string& name) const {
    if (exprId >= 0 && exprId < (int)exprTable.size())
        for (size_t i = 0; i < exprNames.size(); i++)
            if (exprNames[i] == name) return exprTable[exprId].value[i];
    return 90.0f;
}

// ---- blink (7.7) ----
void L2DStage::blinkUpdate(float dt) {
    if (exprT < 1) return;
    if (std::find(noBlink.begin(), noBlink.end(), exprId) != noBlink.end()) return;
    float tl = exprTarget("PARAM_EYE_L_OPEN"), tr = exprTarget("PARAM_EYE_R_OPEN");
    bool windowed = false;
    if (const Eye* w = eyeWindow(curName); w && curFrames > 0) {
        int pos_ = frameCounter % curFrames;
        if (pos_ < w->t[0]) armed = true;
        if (pos_ < w->t[0] || armed) {
            auto window = [&](float v) {
                if (pos_ < w->t[0]) return v;
                if (pos_ < w->t[1]) return v - v * (float)(pos_ - w->t[0]) / (float)(w->t[1] - w->t[0]);
                if (pos_ < w->t[2]) return 0.f;
                if (pos_ < w->t[3]) return v * (float)(pos_ - w->t[2]) / (float)(w->t[3] - w->t[2]);
                return v;
            };
            eyeL = window(tl); eyeR = window(tr);
            windowed = true;
        }
    }
    if (!windowed) {
        switch (blinkState) {
        case 0:
            blinkCounter = (int)((float)blinkCounter - dt);
            if (blinkCounter <= 0) blinkState = 1;
            eyeL = tl; eyeR = tr;
            break;
        case 1:
            for (float* v : {&eyeL, &eyeR}) { *v -= dt * 0.1f; if (*v <= 0) { *v = 0; blinkState = 2; } }
            break;
        case 2: {
            float* eye[2] = {&eyeL, &eyeR}; float tg[2] = {tl, tr};
            for (int i = 0; i < 2; i++) {
                *eye[i] += dt * 0.1f;
                if (*eye[i] >= tg[i]) { *eye[i] = tg[i]; blinkState = 0; blinkCounter = (int)(charRngNext(blinkRng) % 900); }
            }
            break;
        }
        }
    }
    mdl->setParam("PARAM_EYE_L_OPEN", eyeL);
    mdl->setParam("PARAM_EYE_R_OPEN", eyeR);
}

// ---- mouth (7.8) ----
bool L2DStage::mouthActive() const {
    return (lipMode == 1 || (lipMode == 0 && speaker == charSpeaker())) && charVoicePlaying();
}

void L2DStage::mouthUpdate(float dt) {
    bool act = mouthActive();
    if (mouthState == 0) { if (act) mouthState = 1; }
    else if (mouthState == 2) {
        if (act) mouthState = 1;
        else if (phase < 90 && phase + dt * 7.2f >= 90) mouthState = 0;
    } else if (!act) mouthState = 2;
    if (mouthState == 0) phase = 90;
    else { phase += dt * 7.2f; if (phase >= 180) phase = 0; }
    mdl->setParam("PARAM_MOUTH_OPEN_Y", 1 - sinf(phase / 180.f * (float)M_PI));
}

// ---- move and jump (7.9) ----
void L2DStage::moveTo(float x, float y, float z, int frames) {
    memcpy(moveStart, pos, sizeof pos);
    moveTarget[0] = x; moveTarget[1] = y; moveTarget[2] = z;
    moveDur = frames; moveT = 0;
    if (frames < 1) memcpy(pos, moveTarget, sizeof pos);
}
void L2DStage::jumpTo(float x, float y, float z, float scale, float height, int frames) {
    memcpy(jumpStart, pos, sizeof pos);
    jumpTarget[0] = x; jumpTarget[1] = y; jumpTarget[2] = z;
    s0 = s; s1 = scale; jumpHeight = height; jumpDur = frames; jumpT = 0;
    if (frames < 1) { memcpy(pos, jumpTarget, sizeof pos); s = s1; h = 0; }
}
void L2DStage::moveUpdate(float dt) {
    if (moveT >= moveDur) return;
    moveT = (int)((float)moveT + dt);
    if (moveT >= moveDur) { moveT = moveDur; memcpy(pos, moveTarget, sizeof pos); return; }
    float f = (float)moveT / (float)moveDur;
    for (int i = 0; i < 3; i++) pos[i] = moveStart[i] + (moveTarget[i] - moveStart[i]) * f;
}
void L2DStage::jumpUpdate(float dt) {
    if (jumpT >= jumpDur) return;
    jumpT = (int)((float)jumpT + dt);
    if (jumpT < jumpDur) {
        float f = (float)jumpT / (float)jumpDur;
        for (int i = 0; i < 3; i++) pos[i] = jumpStart[i] + (jumpTarget[i] - jumpStart[i]) * f;
        s = s0 * (1 - f) + s1 * f;
        h = jumpHeight * (-4 * f * f + 4 * f);
    } else {
        jumpT = jumpDur; memcpy(pos, jumpTarget, sizeof pos); h = 0; s = s1;
    }
}

// ---- lifecycle ----
void L2DStage::startLoad(const std::string& name) {
    filename = name.substr(0, 31);
    file = g_fs.read(filename);
    if (file.empty()) fprintf(stderr, "L2D: cannot read %s\n", filename.c_str());
}

void L2DStage::release() {
    if (!isCreated) return;
    entries.clear(); queue.clear(); cur = -1; curName.clear();
    filename.clear();
    mdl.reset(); freeTextures();
    isCreated = false;
}

void L2DStage::create() {
    std::string name = filename;
    release();
    filename = name;
    k = 0.27f; memset(pos, 0, sizeof pos); s = 1; h = 0;
    moveDur = moveT = 0; memset(moveStart, 0, sizeof moveStart); memset(moveTarget, 0, sizeof moveTarget);
    jumpDur = jumpT = 0; memset(jumpStart, 0, sizeof jumpStart); memset(jumpTarget, 0, sizeof jumpTarget);
    s0 = s1 = 1; jumpHeight = 0; tickCounter = 0;
    if (!parseLbd()) { fprintf(stderr, "L2D: cannot build %s\n", filename.c_str()); return; }
    isCreated = true;
    frameCounter = 0; curFrames = 0; curKind = 0;
    exprQueue.clear(); exprT = 1; exprRate = 0; exprId = 0; memset(snapshot, 0, sizeof snapshot);
    queueExpression(0, 1);
    blinkState = 0; armed = false; eyeL = eyeR = 1; blinkCounter = (int)(charRngNext(blinkRng) % 900);
    mouthState = 0; phase = 90;
    for (auto& [n, v] : overrides) { int i = mdl->paramIndex(n); if (i > 0) mdl->setParam(i, v); }
    mdl->saveParam();
    blinkRng = (uint32_t)SDL_GetPerformanceCounter();
}

// ---- tick (7.2) ----
void L2DStage::tick() {
    if (!isCreated) return;
    if (tickCounter == 0) {
        const float dt = 3.0f;
        bool blinkWasIdle = blinkState == 0;
        mdl->loadParam();
        motionUpdate(dt);
        mdl->saveParam();
        expressionUpdate(dt, blinkWasIdle);
        blinkUpdate(dt);
        mouthUpdate(dt);
        moveUpdate(dt); jumpUpdate(dt);
        mdl->update();
    }
    tickCounter = (tickCounter + 1) % 3;
}

// ---- draw (7.5) ----
void L2DStage::updateGroup(float dt, const std::vector<std::string>& names) {
    auto part = [](const std::string& n) { return n.size() > 8 ? n.substr(8) : std::string(); };
    if (dt == 0) {
        for (auto& n : names) mdl->setPartOpacity(part(n), mdl->value(n) != 0 ? 1.f : 0.f);
        return;
    }
    int vis = -1; float o = 1;
    for (size_t i = 0; i < names.size(); i++)
        if (mdl->value(names[i]) != 0) {
            if (vis >= 0) break;
            o = std::min(1.f, mdl->partOpacity(part(names[i])) + dt / 0.5f);
            vis = (int)i;
        }
    if (vis < 0) { vis = 1; o = 1; }
    float hide = 1 - o;
    for (size_t i = 0; i < names.size(); i++) {
        std::string pn = part(names[i]);
        if ((int)i == vis) { mdl->setPartOpacity(pn, o); continue; }
        float c = mdl->partOpacity(pn);
        float t = o < 0.5f ? (o * -0.5f) / 0.5f + 1 : (hide * 0.5f) / 0.5f;
        if (0.15f < (1 - t) * hide) t = 1 - 0.15f / hide;
        mdl->setPartOpacity(pn, std::min(c, t));
    }
}

void L2DStage::draw() {
    if (!mdl || (queue.empty() && cur < 0)) return;
    int64_t now = charClockMs();
    if (!groups.empty() && (lastTime == 0 || now != lastTime)) {   // same tick drawn again: nothing to advance
        float dt = lastTime == 0 ? 0.f : (float)(now - lastTime) / 1000.f;
        lastTime = now;
        for (auto& g : groups) updateGroup(dt, g);
    }
    const float cw = mdl->canvasW * 0.5f, ch = mdl->canvasH * 0.5f;
    for (int i : mdl->drawOrder()) {
        l2d::Mesh& m = mdl->meshes[i];
        if (!m.available || m.outside || m.final_.empty()) continue;
        float alpha = m.opacity * mdl->parts[m.part].opacity;
        int a = std::clamp((int)(alpha * 255.0f), 0, 255);
        if (a == 0 || m.texNo < 0 || m.texNo >= (int)textures.size() || !textures[m.texNo]) continue;
        int n = std::min({m.pointCount, (int)m.final_.size() / 2, (int)m.uv.size() / 2});
        verts.resize(n);
        for (int v = 0; v < n; v++) {
            float wx = pos[0] + s * (k * (m.final_[v * 2] - cw));
            float wy = pos[1] + s * (h - k * (m.final_[v * 2 + 1] - ch));
            verts[v].position = {Gfx::W * 0.5f + wx, Gfx::H * 0.5f - wy};
            verts[v].color = {255, 255, 255, (Uint8)a};
            verts[v].tex_coord = {m.uv[v * 2], m.uv[v * 2 + 1]};
        }
        SDL_BlendMode bm = m.blend == 1 ? SDL_BLENDMODE_ADD : m.blend == 2 ? SDL_BLENDMODE_MOD : SDL_BLENDMODE_BLEND;
        g_gfx.geometry(textures[m.texNo].get(), verts.data(), n, m.indices.data(), (int)m.indices.size(), bm);
    }
}

// ---- save record (FUN_000666f0 / FUN_00066968) ----
void L2DStage::save(uint8_t* r) const {
    memset(r, 0, kL2dSaveSize);
    wrs(r + 0x88, filename, 0x20);
    if (filename.empty()) return;
    r[0] = isCreated;
    wrf(r + 4, k); wrv(r + 8, pos); wrf(r + 0x14, s); wrf(r + 0x18, h);
    wr32(r + 0x1c, moveDur); wrv(r + 0x20, moveStart); wrv(r + 0x2c, moveTarget); wr32(r + 0x38, moveT);
    wr32(r + 0x3c, jumpDur); wrv(r + 0x40, jumpStart); wrv(r + 0x4c, jumpTarget);
    wrf(r + 0x58, s0); wrf(r + 0x5c, s1); wrf(r + 0x60, jumpHeight); wr32(r + 0x64, jumpT);
    wrs(r + 0x68, speaker, 0x20);
    wr32(r + 0xa8, curKind); wrs(r + 0xac, curName, 0x20);
    wr32(r + 0xcc, (uint32_t)queue.size());
    for (size_t i = 0; i < queue.size() && i < 8; i++) {
        wr32(r + 0xd0 + i * 4, queue[i].kind);
        wrs(r + 0xf0 + i * 0x20, queue[i].name, 0x20);
    }
    wr32(r + 0x1f0, exprId);
    wr32(r + 0x1f4, (uint32_t)exprQueue.size());
    for (size_t i = 0; i < exprQueue.size() && i < 4; i++) {
        wr32(r + 0x1f8 + i * 4, exprQueue[i].id);
        wr32(r + 0x208 + i * 4, exprQueue[i].frames);
    }
}

void L2DStage::load(const uint8_t* r) {
    release();
    file.clear(); filename.clear();
    std::string name = rds(r + 0x88, 0x20);
    if (name.empty()) return;
    startLoad(name);
    if (!r[0]) return;
    create();
    if (!isCreated) return;
    k = rdf(r + 4); rdv(r + 8, pos); s = rdf(r + 0x14); h = rdf(r + 0x18);
    moveDur = (int)rd32(r + 0x1c); rdv(r + 0x20, moveStart); rdv(r + 0x2c, moveTarget); moveT = (int)rd32(r + 0x38);
    jumpDur = (int)rd32(r + 0x3c); rdv(r + 0x40, jumpStart); rdv(r + 0x4c, jumpTarget);
    s0 = rdf(r + 0x58); s1 = rdf(r + 0x5c); jumpHeight = rdf(r + 0x60); jumpT = (int)rd32(r + 0x64);
    speaker = rds(r + 0x68, 0x20);
    std::string c = rds(r + 0xac, 0x20);
    if (!c.empty()) queueMotion(c, (int)rd32(r + 0xa8));
    queueExpression((int)rd32(r + 0x1f0), 0);
    int nm = std::min((int)rd32(r + 0xcc), 8);
    for (int i = 0; i < nm; i++) queueMotion(rds(r + 0xf0 + i * 0x20, 0x20), (int)rd32(r + 0xd0 + i * 4));
    int ne = std::min((int)rd32(r + 0x1f4), 4);
    for (int i = 0; i < ne; i++) queueExpression((int)rd32(r + 0x1f8 + i * 4), (int)rd32(r + 0x208 + i * 4));
}
