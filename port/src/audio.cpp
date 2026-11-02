#include "audio.h"
#include <algorithm>
#include <SDL.h>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <mutex>
#include <map>
extern "C" {
#include "clhca.h"
}
Audio g_audio;

bool decodeAdx(const Bytes& d, Pcm& out) {
    if (d.size() < 0x20 || d[0] != 0x80) return false;
    auto be32 = [&](size_t o) { return (uint32_t)d[o] << 24 | d[o+1] << 16 | d[o+2] << 8 | d[o+3]; };
    auto be16 = [&](size_t o) { return (d[o] << 8) | d[o+1]; };
    int dataOff = be16(2) + 4, block = d[5], ch = d[7];
    uint32_t rate = be32(8), total = be32(12); int hp = be16(16); int ver = d[18];
    out.rate = rate; out.ch = ch;
    size_t loopHdr = ver == 4 ? 0x18 + (ch > 1 ? 4 * ch : 8) : 0x14;
    if ((size_t)dataOff > loopHdr + 0x18 && be32(loopHdr + 4)) { out.loopStart = be32(loopHdr + 8); out.loopEnd = be32(loopHdr + 16); }
    const double a = sqrt(2.0) - cos(2 * M_PI * hp / rate);
    const double b = sqrt(2.0) - 1;
    const double c = (a - sqrt((a + b) * (a - b))) / b;
    const int co1 = (int)floor(c * 2 * 4096 + 0.5), co2 = (int)floor(-c * c * 4096 + 0.5);
    int spf = (block - 2) * 2;
    out.s.assign((size_t)total * ch, 0);
    int h1[8] = {}, h2[8] = {};
    size_t p = dataOff;
    for (uint32_t pos = 0; pos < total; pos += spf) {
        for (int c2 = 0; c2 < ch; c2++, p += block) {
            if (p + block > d.size()) return true;
            int sc = ((d[p] << 8) | d[p+1]) + 1;
            if (d[p] & 0x80) return true; // end marker
            for (int i = 0; i < spf; i++) {
                int nib = d[p + 2 + i / 2]; nib = (i & 1) ? nib & 15 : nib >> 4;
                if (nib & 8) nib -= 16;
                int pred = (co1 * h1[c2] + co2 * h2[c2]) >> 12;
                int v = pred + nib * sc;
                v = std::clamp(v, -32768, 32767);
                h2[c2] = h1[c2]; h1[c2] = v;
                if (pos + i < total) out.s[(size_t)(pos + i) * ch + c2] = (int16_t)v;
            }
        }
    }
    return true;
}

bool decodeHca(const Bytes& d, Pcm& out) {
    clHCA* h = clHCA_new();
    if (!h) return false;
    bool ok = false;
    int hs = clHCA_isOurFile(d.data(), (unsigned)std::min<size_t>(d.size(), 0x1000));
    if (hs > 0 && clHCA_DecodeHeader(h, d.data(), hs) >= 0) {
        clHCA_stInfo in; clHCA_getInfo(h, &in);
        out.rate = in.samplingRate; out.ch = in.channelCount;
        std::vector<short> buf((size_t)in.samplesPerBlock * in.channelCount);
        size_t p = in.headerSize;
        std::vector<uint8_t> blk(in.blockSize);
        for (unsigned b = 0; b < in.blockCount && p + in.blockSize <= d.size(); b++, p += in.blockSize) {
            memcpy(blk.data(), &d[p], in.blockSize);
            if (clHCA_DecodeBlock(h, blk.data(), in.blockSize) < 0) continue;
            clHCA_ReadSamples16(h, buf.data());
            out.s.insert(out.s.end(), buf.begin(), buf.end());
        }
        ok = true;
    }
    clHCA_delete(h);
    return ok;
}

std::shared_ptr<Pcm> loadSound(const std::string& n) {
    Bytes d = g_fs.read(n);
    if (d.empty()) { fprintf(stderr, "missing sound %s\n", n.c_str()); return nullptr; }
    auto p = std::make_shared<Pcm>();
    bool ok = n.size() > 4 && n.substr(n.size() - 4) == ".hca" ? decodeHca(d, *p) : decodeAdx(d, *p);
    return ok ? p : nullptr;
}

namespace {
struct Chan {
    std::shared_ptr<Pcm> pcm; double pos = 0; bool loop = false, on = false;
    float vol = 1, target = 1, step = 0;   // script volume
    float fade = 1, fadeStep = 0;          // fade envelope, stop when reaching 0 on fade out
    float level = 0;
};
Chan ch[CH_COUNT];
float groupVol[3] = {1, 1, 1};
SDL_AudioDeviceID dev = 0;
int outRate = 48000;
int groupOf(int c) { return c <= CH_BGM2 ? 0 : c == CH_VOICE ? 2 : 1; }

void mix(void*, Uint8* stream, int len) {
    int16_t* o = (int16_t*)stream; int frames = len / 4;
    memset(stream, 0, len);
    for (int c = 0; c < CH_COUNT; c++) {
        Chan& k = ch[c];
        if (!k.on || !k.pcm) { k.level = 0; continue; }
        const Pcm& p = *k.pcm;
        int64_t n = (int64_t)p.s.size() / p.ch;
        int64_t end = (k.loop && p.loopEnd > 0) ? std::min(p.loopEnd, n) : n;
        double inc = (double)p.rate / outRate;
        float gv = groupVol[groupOf(c)];
        float peak = 0;
        for (int i = 0; i < frames; i++) {
            int64_t a = (int64_t)k.pos; double f = k.pos - a;
            if (a >= end) {
                if (k.loop) { k.pos -= (double)(end - std::max<int64_t>(p.loopStart, 0)); a = (int64_t)k.pos; f = k.pos - a; }
                else { k.on = false; break; }
            }
            int64_t b = a + 1 < n ? a + 1 : a;
            float l = p.s[a * p.ch] * (float)(1 - f) + p.s[b * p.ch] * (float)f;
            float r = p.ch > 1 ? p.s[a * p.ch + 1] * (float)(1 - f) + p.s[b * p.ch + 1] * (float)f : l;
            float g = k.vol * k.fade * gv;
            int vl = (int)o[i * 2] + (int)(l * g), vr = (int)o[i * 2 + 1] + (int)(r * g);
            o[i * 2] = (int16_t)std::clamp(vl, -32768, 32767);
            o[i * 2 + 1] = (int16_t)std::clamp(vr, -32768, 32767);
            peak = std::max(peak, std::fabs(l) / 32768.f);
            k.pos += inc;
            if (k.step != 0) { k.vol += k.step; if ((k.step > 0) == (k.vol >= k.target)) { k.vol = k.target; k.step = 0; } }
            if (k.fadeStep != 0) {
                k.fade += k.fadeStep;
                if (k.fade >= 1) { k.fade = 1; k.fadeStep = 0; }
                else if (k.fade <= 0) { k.on = false; break; }
            }
        }
        k.level = peak;
    }
}
}

bool Audio::init(bool enable) {
    if (!enable) return true;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) { fprintf(stderr, "audio: %s\n", SDL_GetError()); return false; }
    SDL_AudioSpec w{}, h{}; w.freq = 48000; w.format = AUDIO_S16SYS; w.channels = 2; w.samples = 1024; w.callback = mix;
    dev = SDL_OpenAudioDevice(nullptr, 0, &w, &h, 0);
    if (!dev) { fprintf(stderr, "audio open: %s\n", SDL_GetError()); return false; }
    outRate = h.freq;
    SDL_PauseAudioDevice(dev, 0);
    return true;
}
void Audio::shutdown() { if (dev) SDL_CloseAudioDevice(dev); dev = 0; }
void Audio::play(int c, const std::string& n, bool loop, int fi) {
    if (n.empty()) { stop(c); return; }
    auto p = loadSound(n);
    if (dev) SDL_LockAudioDevice(dev);
    Chan& k = ch[c];
    k.pcm = p; k.pos = 0; k.loop = loop && p && p->loopEnd > 0; k.on = p != nullptr;
    k.fade = fi > 0 ? 0 : 1; k.fadeStep = fi > 0 ? 60.f / (fi * outRate) : 0;
    if (dev) SDL_UnlockAudioDevice(dev);
}
void Audio::playPcm(int c, std::shared_ptr<Pcm> p) {
    if (dev) SDL_LockAudioDevice(dev);
    Chan& k = ch[c]; k.pcm = std::move(p); k.pos = 0; k.loop = false; k.on = true; k.fade = 1; k.fadeStep = 0;
    if (dev) SDL_UnlockAudioDevice(dev);
}
void Audio::stop(int c, int fo) {
    if (dev) SDL_LockAudioDevice(dev);
    if (fo > 0 && ch[c].on) ch[c].fadeStep = -60.f / (fo * outRate); else ch[c].on = false;
    if (dev) SDL_UnlockAudioDevice(dev);
}
void Audio::setVolume(int c, float v, int ff) {
    if (dev) SDL_LockAudioDevice(dev);
    Chan& k = ch[c]; k.target = v;
    if (ff > 0 && v != k.vol) k.step = (v - k.vol) * 60.f / (ff * outRate); else { k.vol = v; k.step = 0; }
    if (dev) SDL_UnlockAudioDevice(dev);
}
bool Audio::playing(int c) { return dev && ch[c].on; }
bool Audio::fading(int c) { return dev && ch[c].on && ch[c].fadeStep != 0; }
void Audio::setGroupVolume(int g, float v) { groupVol[g] = v; }
float Audio::voiceLevel() { return ch[CH_VOICE].on ? ch[CH_VOICE].level : 0; }

static std::map<int, std::shared_ptr<Pcm>> g_cues;
void Audio::loadCues(const std::string& awb) {
    Bytes d = g_fs.read(awb);
    if (d.size() < 16 || memcmp(d.data(), "AFS2", 4)) return;
    int osz = d[5], idsz = d[6]; uint32_t n; memcpy(&n, &d[8], 4);
    uint16_t align; memcpy(&align, &d[12], 2);
    size_t p = 16 + (size_t)n * idsz;
    auto off = [&](uint32_t i) { uint32_t v = 0; memcpy(&v, &d[p + i * osz], osz); return v; };
    for (uint32_t i = 0; i < n; i++) {
        uint16_t id; memcpy(&id, &d[16 + i * idsz], 2);
        size_t a = (off(i) + align - 1) / align * align, b = off(i + 1);
        Bytes one(d.begin() + a, d.begin() + b);
        auto pcm = std::make_shared<Pcm>();
        if (decodeAdx(one, *pcm)) g_cues[id] = pcm;
    }
}
void Audio::playCue(int id, int c) {
    auto i = g_cues.find(id);
    if (i != g_cues.end()) playPcm(c, i->second);
}
