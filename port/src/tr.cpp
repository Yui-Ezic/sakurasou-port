#include "tr.h"
#include "fs.h"
#include "gfx.h"
#include <cstdio>
#include <unordered_map>

static std::string s_dir;
static std::unordered_map<uint64_t, std::string> s_tab;
static bool s_on = false;
static uint64_t s_curKey = 0;
static int s_curId = -1;

// FNV-1a 64 over context, NUL, text. Must match tools/tl_common.py.
static uint64_t keyOf(const std::string& ctx, const std::string& s) {
    uint64_t h = 0xcbf29ce484222325ull;
    auto mix = [&](unsigned char c) { h = (h ^ c) * 0x100000001b3ull; };
    for (unsigned char c : ctx) mix(c);
    mix(0);
    for (unsigned char c : s) mix(c);
    return h;
}

static bool loadPack() {
    std::string p = s_dir + "/ru.pack";
    FILE* f = fopen(p.c_str(), "rb");
    if (!f) { fprintf(stderr, "translation: cannot open %s\n", p.c_str()); return false; }
    std::string d; char buf[65536]; size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) d.append(buf, n);
    fclose(f);
    if (d.rfind("#SAKURA-TL 1\n", 0) != 0) { fprintf(stderr, "translation: bad pack header\n"); return false; }
    s_tab.clear();
    size_t i = d.find('\n') + 1;
    while (i < d.size()) {
        size_t e = d.find('\n', i); if (e == std::string::npos) e = d.size();
        size_t t = d.find('\t', i);
        if (t != std::string::npos && t < e) {
            uint64_t k = strtoull(d.substr(i, t - i).c_str(), nullptr, 16);
            std::string v; v.reserve(e - t);
            for (size_t j = t + 1; j < e; j++) {
                if (d[j] == '\\' && j + 1 < e) { j++; v += d[j] == 'n' ? '\n' : d[j]; }
                else v += d[j];
            }
            s_tab[k] = std::move(v);
        }
        i = e + 1;
    }
    fprintf(stderr, "translation: %zu strings\n", s_tab.size());
    return true;
}

bool trInit(const std::string& dir) {
    s_dir = dir;
    if (!loadPack()) return false;
    g_fs.setOverrideDir(dir + "/assets");
    return s_on = true;
}
bool trReload() { return s_on && loadPack(); }
bool trOn() { return s_on; }

const std::string& tr(const std::string& jp) {
    if (!s_on || jp.empty()) return jp;
    auto i = s_tab.find(keyOf("", jp));
    return i == s_tab.end() ? jp : i->second;
}

// Greedy word wrap. tools/tl_common.py wrap() is the same algorithm; change them together.
static std::string wrapMessage(const std::string& s) {
    std::string out; int x = 0, afterSpace = 0; size_t lastSp = std::string::npos;
    const char* p = s.c_str(); const char* end = p + s.size();
    while (p < end) {
        if (!strncmp(p, "▼", 3) || !strncmp(p, "‡", 3)) { out.append(p, 3); p += 3; continue; }
        if (!strncmp(p, "†", 3)) {                       // †digits‡ opens a TIPS link
            const char* q = strstr(p + 3, "‡");
            if (!q) { out.append(p); break; }
            out.append(p, q + 3 - p); p = q + 3; continue;
        }
        if (*p == '\n') { out += '\n'; p++; x = afterSpace = 0; lastSp = std::string::npos; continue; }
        const char* q = p; uint32_t cp = utf8Next(q);
        int a = g_fontM.propAdv(cp);
        if (cp == ' ') { lastSp = out.size(); afterSpace = 0; }
        else {
            if (x + a > kTrMsgWidth && lastSp != std::string::npos) { out[lastSp] = '\n'; x = afterSpace; lastSp = std::string::npos; }
            afterSpace += a;
        }
        out.append(p, q - p); x += a; p = q;
    }
    return out;
}

bool trMessage(int id, const std::string& jp, std::string& out) {
    if (!s_on) return false;
    uint64_t k = keyOf(std::to_string(id), jp);
    s_curKey = k; s_curId = id;
    auto i = s_tab.find(k);
    if (i == s_tab.end()) return false;
    out = wrapMessage(i->second);
    return true;
}

void trFlagCurrent() {
    if (!s_on || !s_curKey) return;
    std::string p = s_dir + "/feedback.tsv";
    if (FILE* f = fopen(p.c_str(), "ab")) { fprintf(f, "%016llx\t%d\n", (unsigned long long)s_curKey, s_curId); fclose(f); }
    fprintf(stderr, "translation: flagged line %d\n", s_curId);
}

void trDump(const std::string& path) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return;
    for (auto& [k, v] : s_tab) {
        std::string w = wrapMessage(v), e;
        for (char c : w) { if (c == '\n') e += "\\n"; else if (c == '\\') e += "\\\\"; else e += c; }
        fprintf(f, "%016llx\t%s\n", (unsigned long long)k, e.c_str());
    }
    fclose(f);
}
