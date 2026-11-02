// Rendering: a 480x272 logical canvas (the PSP framebuffer), scaled with preserved aspect.
#pragma once
#include "fs.h"
#include <SDL.h>
#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct Image { int w = 0, h = 0; std::vector<uint32_t> px; };  // RGBA8888 in memory order R,G,B,A

// Decoders for the game's texture containers.
bool decodeTxg(const uint8_t* d, size_t n, Image& out);
bool decodeXgtl(const uint8_t* d, size_t n, Image& out);
bool decodeAny(const Bytes& d, Image& out, int* cx = nullptr, int* cy = nullptr);

struct Tex {
    SDL_Texture* t = nullptr; int w = 0, h = 0; int cx = 0, cy = 0;
    ~Tex() { if (t) SDL_DestroyTexture(t); }
};
using TexP = std::shared_ptr<Tex>;

struct Color { uint8_t r = 255, g = 255, b = 255, a = 255; };

class Gfx {
public:
    static constexpr int W = 480, H = 272;
    bool init(bool headless, int scale, bool fullscreen);
    void shutdown();
    TexP load(const std::string& name);           // cached by asset name
    TexP fromImage(const Image&);
    void purge();

    void beginFrame();
    void endFrame();                              // presents, letterboxed
    void clear(Color c);
    void draw(const Tex& t, float x, float y, float alpha = 1, float sx = 1, float sy = 1,
              Color tint = {}, SDL_BlendMode bm = SDL_BLENDMODE_BLEND);
    void drawSub(const Tex& t, SDL_Rect src, SDL_FRect dst, float alpha = 1, Color tint = {},
                 SDL_BlendMode bm = SDL_BLENDMODE_BLEND);
    void fillRect(float x, float y, float w, float h, Color c);
    void geometry(const Tex* t, const SDL_Vertex* v, int nv, const int* idx, int ni,
                  SDL_BlendMode bm = SDL_BLENDMODE_BLEND);
    bool screenshot(const std::string& path);     // saves current canvas as BMP
    bool readCanvas(Image& out);                  // RGBA copy of the 480x272 canvas
    void toggleFullscreen();
    SDL_Renderer* renderer() { return r; }
    SDL_Window* window() { return win; }
    // Off-screen layers (transitions)
    SDL_Texture* makeTarget(int w = W, int h = H);
    void setTarget(SDL_Texture* t);
    SDL_Texture* canvasTexture() { return canvas; }
    // window->canvas mouse mapping
    void toCanvas(int wx, int wy, int& cx, int& cy);
private:
    SDL_Window* win = nullptr;
    SDL_Renderer* r = nullptr;
    SDL_Texture* canvas = nullptr;
    std::unordered_map<std::string, std::weak_ptr<Tex>> cache;
    SDL_Rect dstRect() const;
    bool fs = false;
};
extern Gfx g_gfx;

class Font {
public:
    bool load(const std::string& asset);
    int cell() const { return sz; }
    const uint8_t* glyph(uint32_t cp) const;      // 4bpp packed glyph or null
    // draws a glyph into an RGBA8 buffer position via a cached atlas
    void drawGlyph(uint32_t cp, float x, float y, Color c, float scale = 1);
    void drawText(const std::string& utf8, float x, float y, Color c, int pitch = 0);
private:
    Bytes data; int sz = 0;
    std::vector<uint8_t> atlas; TexP tex; int atlasCols = 256;
};
extern Font g_fontM, g_fontS;
uint32_t utf8Next(const char*& p);
