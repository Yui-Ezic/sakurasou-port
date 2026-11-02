// Save buffer and snapshots (spec_flow_menus section 8).
#include "game.h"
#include <SDL.h>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>

static std::vector<uint8_t> s_buf(kSaveSize, 0);
static std::array<uint8_t, 0x3c78> s_pristine;
int g_lastSlot = 0; int g_mode = 0;
uint8_t* saveBuffer() { return s_buf.data(); }
void clearSaveBuffer() { std::fill(s_buf.begin(), s_buf.end(), 0); }
static uint8_t* snap(int i) { return s_buf.data() + 0x10 + i * kSnapSize; }

// 64x64 swizzled thumbnail: averaged 8x8 blocks of the canvas
static void makeThumb(uint8_t* out) {
    Image c; g_gfx.readCanvas(c);
    static uint32_t px[64 * 64]; memset(px, 0, sizeof px);
    for (int y = 0; y < 34; y++) for (int x = 0; x < 59; x++) {
        unsigned r = 0, g = 0, b = 0;
        for (int j = 0; j < 8; j += 2) for (int i = 0; i < 8; i += 2) {
            uint32_t v = c.px[(y * 8 + j) * 480 + x * 8 + i];
            r += v & 255; g += v >> 8 & 255; b += v >> 16 & 255;
        }
        px[y * 64 + x] = (r / 16) | (g / 16) << 8 | (b / 16) << 16 | 0xff000000u;
    }
    // swizzle 16 bytes x 8 lines
    const uint8_t* src = (const uint8_t*)px;
    for (int by = 0; by < 8; by++) for (int bx = 0; bx < 16; bx++) for (int l = 0; l < 8; l++) {
        memcpy(out, src + (by * 8 + l) * 256 + bx * 16, 16); out += 16;
    }
}

void takeSnapshot() {
    uint8_t* s = snap(0);
    memset(s, 0, kSnapSize);
    *(uint32_t*)s = 1;
    time_t t = time(nullptr); tm* lt = localtime(&t);
    uint16_t* d = (uint16_t*)(s + 4);
    d[0] = lt->tm_year + 1900; d[1] = lt->tm_mon + 1; d[2] = lt->tm_mday; d[3] = lt->tm_hour; d[4] = lt->tm_min; d[5] = lt->tm_sec;
    makeThumb(s + 0x10);
    memcpy(s + 0x4010, g_game.raw.data(), g_game.raw.size());
    Bytes blob;
    if (Job* h = progressHost())
        for (auto& k : h->children()) {
            Bytes b; k->saveState(b);
            int32_t hd[2] = {k->id, (int32_t)b.size()};
            blob.insert(blob.end(), (uint8_t*)hd, (uint8_t*)(hd + 2)); blob.insert(blob.end(), b.begin(), b.end());
        }
    int32_t e[2] = {-1, 0}; blob.insert(blob.end(), (uint8_t*)e, (uint8_t*)(e + 2));
    size_t room = kSnapSize - 0x7c88;
    if (blob.size() > room) fprintf(stderr, "save blob too large: %zu > %zu\n", blob.size(), room);
    memcpy(s + 0x7c88, blob.data(), std::min(room, blob.size()));
}
void shiftHistory() {
    for (int i = 9; i >= 1; i--) memcpy(snap(i + 1), snap(i), kSnapSize);
    memcpy(snap(1), snap(0), kSnapSize);
}
void selectHistory(int i) { memmove(snap(0), snap(i + 1), kSnapSize); }
void carryOverSnapshot() {
    uint8_t* gd = snap(0) + 0x4010;
    Bytes keep(g_game.raw.begin(), g_game.raw.end());
    GameData fresh; fresh.reset();
    memcpy(gd, fresh.raw.data(), fresh.raw.size());
    memcpy(gd + 0x3870, keep.data() + 0x3870, 8);
    // planFinished: field +0x14 of the 500 entries at dword 0x8a
    for (int i = 0; i < 500; i++) memcpy(gd + 0x228 + i * 24 + 0x14, keep.data() + 0x228 + i * 24 + 0x14, 4);
    int32_t e[2] = {-1, 0}; memcpy(snap(0) + 0x7c88, e, 8);
}
bool writeSaveFile(const std::string& f) {
    std::string p = savePath(f), t = p + ".tmp";
    FILE* o = fopen(t.c_str(), "wb"); if (!o) return false;
    bool ok = fwrite(s_buf.data(), 1, kSaveSize, o) == kSaveSize;
    ok = fclose(o) == 0 && ok;
    if (ok) std::filesystem::rename(t, p);
    return ok;
}
bool readSaveFile(const std::string& f, Bytes& out) {
    FILE* i = fopen(savePath(f).c_str(), "rb"); if (!i) return false;
    out.resize(kSaveSize);
    bool ok = fread(out.data(), 1, kSaveSize, i) == kSaveSize; fclose(i);
    return ok;
}
void useSaveFile(const Bytes& b) { memcpy(s_buf.data(), b.data(), kSaveSize); }

void restoreFromSnapshot(Job& host) {
    uint8_t* s = snap(0);
    if (*(uint32_t*)s != 1) return;
    memcpy(g_game.raw.data(), s + 0x4010, g_game.raw.size());
    const uint8_t* p = s + 0x7c88;
    for (;;) {
        int32_t id, size; memcpy(&id, p, 4); memcpy(&size, p + 4, 4); p += 8;
        if (id < 0) break;
        Job* j = host.pushNow(id);
        if (j) j->loadState(p, size);
        p += size;
    }
}
