#include "movie.h"
#include "audio.h"
#include <SDL.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

namespace {
constexpr int kAtracFrame = 752;     // 8 byte header + payload

struct Demuxed { std::vector<uint8_t> video, audio; };

Demuxed demux(const Bytes& d) {
    Demuxed o;
    size_t p = 0x800;
    while (p + 6 < d.size()) {
        if (d[p] || d[p+1] || d[p+2] != 1) { p++; continue; }
        int sid = d[p+3];
        if (sid == 0xba) { p += 14 + (d[p+13] & 7); continue; }
        size_t len = (d[p+4] << 8) | d[p+5];
        if (sid == 0xe0 || sid == 0xbd) {
            size_t hl = d[p+8], b = p + 9 + hl, e = p + 6 + len;
            e = std::min(e, d.size());
            if (sid == 0xe0) o.video.insert(o.video.end(), d.begin() + b, d.begin() + e);
            else if (b + 4 <= e) o.audio.insert(o.audio.end(), d.begin() + b + 4, d.begin() + e);
        }
        p += 6 + len;
    }
    return o;
}

// returns interleaved s16 stereo
bool decodeAtrac(const std::vector<uint8_t>& a, Pcm& out) {
    if (a.size() < 8) return false;
    int block = (((a[2] << 8) | a[3]) & 0x3ff) * 8 + 8;
    const AVCodec* c = avcodec_find_decoder(AV_CODEC_ID_ATRAC3P);
    if (!c) return false;
    AVCodecContext* x = avcodec_alloc_context3(c);
    x->sample_rate = 44100; x->block_align = block;
    av_channel_layout_default(&x->ch_layout, 2);
    if (avcodec_open2(x, c, nullptr) < 0) return false;
    AVFrame* f = av_frame_alloc(); AVPacket* k = av_packet_alloc();
    out.rate = 44100; out.ch = 2;
    for (size_t p = 0; p + 8 + block <= a.size(); p += 8 + block) {
        if (a[p] != 0x0f || a[p+1] != 0xd0) { fprintf(stderr, "ATRAC sync lost at %zu\n", p); break; }
        av_new_packet(k, block); memcpy(k->data, &a[p + 8], block);
        if (avcodec_send_packet(x, k) >= 0)
            while (avcodec_receive_frame(x, f) >= 0)
                for (int i = 0; i < f->nb_samples; i++)
                    for (int ch = 0; ch < 2; ch++) {
                        float v = ((float*)f->extended_data[ch])[i];
                        out.s.push_back((int16_t)std::clamp(v * 32767.f, -32768.f, 32767.f));
                    }
        av_packet_unref(k);
    }
    av_frame_free(&f); av_packet_free(&k); avcodec_free_context(&x);
    return true;
}
}

struct Movie::Impl {
    Demuxed dx;
    AVCodecContext* v = nullptr; AVCodecParserContext* parser = nullptr;
    AVFrame* frame = nullptr; AVPacket* pkt = nullptr; SwsContext* sws = nullptr;
    size_t vpos = 0; bool flushed = false, done = false;
    SDL_Texture* tex = nullptr; std::vector<uint8_t> rgba;
    uint64_t startMs = 0; int shown = -1; int frames = 0;
    bool hasFrame = false;
    std::shared_ptr<Pcm> pcm;

    bool decodeOne() {
        for (;;) {
            int r = avcodec_receive_frame(v, frame);
            if (r >= 0) return true;
            if (r == AVERROR_EOF) return false;
            if (vpos >= dx.video.size()) {
                if (!flushed) { avcodec_send_packet(v, nullptr); flushed = true; continue; }
                return false;
            }
            int chunk = (int)std::min<size_t>(4096, dx.video.size() - vpos);
            uint8_t* od; int os;
            int used = av_parser_parse2(parser, v, &od, &os, dx.video.data() + vpos, chunk, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
            vpos += used;
            if (os > 0) {
                av_new_packet(pkt, os); memcpy(pkt->data, od, os);
                avcodec_send_packet(v, pkt); av_packet_unref(pkt);
            }
        }
    }
    ~Impl() {
        if (tex) SDL_DestroyTexture(tex);
        if (sws) sws_freeContext(sws);
        if (parser) av_parser_close(parser);
        if (v) avcodec_free_context(&v);
        if (frame) av_frame_free(&frame);
        if (pkt) av_packet_free(&pkt);
    }
};

Movie::Movie() : p(new Impl) {}
Movie::~Movie() { stop(); }

bool Movie::open(const std::string& name) {
    p.reset(new Impl);
    Bytes d = g_fs.read(name);
    if (d.empty()) return false;
    p->dx = demux(d);
    const AVCodec* c = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!c) { fprintf(stderr, "no H.264 decoder\n"); return false; }
    p->v = avcodec_alloc_context3(c);
    p->parser = av_parser_init(AV_CODEC_ID_H264);
    if (avcodec_open2(p->v, c, nullptr) < 0) return false;
    p->frame = av_frame_alloc(); p->pkt = av_packet_alloc();
    p->tex = SDL_CreateTexture(g_gfx.renderer(), SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, 480, 272);
    p->rgba.resize(480 * 272 * 4);
    p->pcm = std::make_shared<Pcm>();
    if (decodeAtrac(p->dx.audio, *p->pcm)) {
        if (getenv("SAKURA_TRACE")) {
            double sum = 0; int peak = 0, clip = 0;
            for (int16_t v : p->pcm->s) { sum += (double)v * v; peak = std::max(peak, abs((int)v)); clip += abs((int)v) >= 32767; }
            fprintf(stderr, "movie audio: %.2f s, rms %.0f, peak %d, clipped %d, es %zu bytes\n",
                    p->pcm->s.size() / 2.0 / p->pcm->rate, sqrt(sum / std::max<size_t>(1, p->pcm->s.size())), peak, clip, p->dx.audio.size());
        }
        g_audio.playPcm(CH_BGM, p->pcm);
    } else fprintf(stderr, "movie: no audio\n");
    p->startMs = SDL_GetTicks64();
    return true;
}

bool Movie::update() {
    if (!p->v || p->done) return false;
    // PSMF video is 29.97 fps; the clock is the wall clock started with the audio
    int due = (int)((SDL_GetTicks64() - p->startMs) * 29.97 / 1000.0);
    while (p->shown < due) {
        if (!p->decodeOne()) { p->done = true; return false; }
        p->shown++; p->hasFrame = true;
    }
    if (p->hasFrame) {
        AVFrame* f = p->frame;
        p->sws = sws_getCachedContext(p->sws, f->width, f->height, (AVPixelFormat)f->format, 480, 272, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);
        uint8_t* dst[1] = {p->rgba.data()}; int ls[1] = {480 * 4};
        sws_scale(p->sws, f->data, f->linesize, 0, f->height, dst, ls);
        SDL_UpdateTexture(p->tex, nullptr, p->rgba.data(), 480 * 4);
    }
    return true;
}

void Movie::draw() {
    if (p->tex && p->hasFrame) SDL_RenderCopy(g_gfx.renderer(), p->tex, nullptr, nullptr);
}
void Movie::stop() { if (p && p->pcm) { g_audio.stop(CH_BGM); p->pcm.reset(); } }
