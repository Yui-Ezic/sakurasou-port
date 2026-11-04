#include "tr.h"
#include "fs.h"
#include "gfx.h"
#include <algorithm>
#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <set>
#include <unordered_map>

static std::string s_dir;
static std::unordered_map<uint64_t, std::string> s_tab;
static bool s_loaded = false;     // a pack was loaded (--tl)
static bool s_ru = true;          // Settings: Russian or the original Japanese
static bool s_on = false;         // loaded && s_ru: translation is applied
static uint64_t s_curKey = 0;
static int s_curId = -1;
static std::string s_curText;
static std::set<uint64_t> s_flagged;
static std::string s_toast;
static int s_toastT = 0;
static void toast(const std::string& t) { s_toast = t; s_toastT = 150; }

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
    if (d.rfind("#SAKURA-TL 1", 0) != 0) { fprintf(stderr, "translation: bad pack header\n"); return false; }
    s_tab.clear();
    size_t i = d.find('\n') + 1;
    while (i < d.size()) {
        size_t e = d.find('\n', i); if (e == std::string::npos) e = d.size();
        size_t eEnd = e; if (eEnd > i && d[eEnd - 1] == '\r') eEnd--;   // tolerate a pack that was checked out with CRLF
        size_t t = d.find('\t', i);
        if (t != std::string::npos && t < e) {
            uint64_t k = strtoull(d.substr(i, t - i).c_str(), nullptr, 16);
            std::string v; v.reserve(eEnd - t);
            for (size_t j = t + 1; j < eEnd; j++) {
                if (d[j] == '\\' && j + 1 < eEnd) { j++; v += d[j] == 'n' ? '\n' : d[j]; }
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
    if (FILE* f = fopen((dir + "/feedback.tsv").c_str(), "rb")) {
        unsigned long long k; int id;
        while (fscanf(f, "%llx %d", &k, &id) == 2) s_flagged.insert(k);
        fclose(f);
    }
    s_loaded = true;
    if (FILE* f = fopen((dir + "/lang.txt").c_str(), "rb")) { char b[8] = {}; if (fread(b, 1, 7, f) > 0) s_ru = strncmp(b, "ja", 2) != 0; fclose(f); }
    g_fs.setOverrideDir(s_ru ? dir + "/assets" : "");
    s_on = s_ru;
    return true;
}
bool trAvailable() { return s_loaded; }
bool trRussian() { return s_ru; }
void trSetRussian(bool ru) {
    if (!s_loaded || ru == s_ru) return;
    s_ru = s_on = ru;
    g_fs.setOverrideDir(ru ? s_dir + "/assets" : "");
    if (FILE* f = fopen((s_dir + "/lang.txt").c_str(), "wb")) { fputs(ru ? "ru\n" : "ja\n", f); fclose(f); }
}
void trReload() {
    if (!s_loaded) return;
    toast(loadPack() ? "Перевод обновлён, со следующей реплики"
                     : "Не удалось загрузить перевод");
}
bool trOn() { return s_on; }

static bool validUtf8(const std::string& s) {
    for (size_t i = 0; i < s.size();) {
        unsigned char c = s[i]; int n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
        if (!n || i + n > s.size()) return false;
        for (int j = 1; j < n; j++) if ((s[i + j] & 0xc0) != 0x80) return false;
        i += n;
    }
    return true;
}
static bool hasJapanese(const std::string& s) {
    if (!validUtf8(s)) return false;    // uninitialised buffers (dev --scene runs) are not text
    for (const char* p = s.c_str(); *p;) { uint32_t cp = utf8Next(p); if (cp == 0x3000 || (cp >= 0xff10 && cp <= 0xff19)) continue;    // padding and digits are numbers, not text
        if (cp >= 0x3000 && cp < 0xa000) return true; if (cp >= 0xff00 && cp < 0xfff0) return true; }
    return false;
}
// Phase 1 check: every Japanese string that reached the screen without a translation is appended once to
// <dir>/untranslated.tsv ("kind<TAB>text"), so a playthrough shows what the pack is missing.
static std::set<uint64_t> s_missLogged;
static void logMiss(const char* kind, const std::string& jp, uint64_t k) {
    if (!hasJapanese(jp) || !s_missLogged.insert(k).second) return;
    FILE* f = fopen((s_dir + "/untranslated.tsv").c_str(), "ab");
    if (!f) return;
    std::string e;
    for (char c : jp) { if (c == '\n') e += "\\n"; else e += c; }
    fprintf(f, "%s\t%s\n", kind, e.c_str());
    fclose(f);
}

const std::string& tr(const std::string& jp) {
    if (!s_on || jp.empty()) return jp;
    uint64_t k = keyOf("", jp);
    auto i = s_tab.find(k);
    if (i == s_tab.end()) { logMiss("ui", jp, k); return jp; }
    return i->second;
}

std::string trFmt(const char* jpFormat, ...) {
    const std::string& f = tr(jpFormat);
    va_list ap; va_start(ap, jpFormat);
    char b[512]; vsnprintf(b, sizeof b, f.c_str(), ap);
    va_end(ap);
    return b;
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
    s_curKey = k; s_curId = id; s_curText = jp;
    auto i = s_tab.find(k);
    if (i == s_tab.end()) { logMiss("msg", jp, k); return false; }
    out = wrapMessage(i->second);
    s_curText = i->second;
    return true;
}

void trFlagCurrent() {
    if (!s_on) return;
    if (!s_curKey) { toast("Сейчас нет реплики для отметки"); return; }
    // first characters of the line, so the player can see which one was flagged
    std::string head; int n = 0;
    for (const char* p = s_curText.c_str(); *p && n < 28;) {
        const char* q = p; uint32_t cp = utf8Next(q);
        if (cp == '\n') head += ' ';
        else if (cp != 0x25bc) { head.append(p, q - p); n++; }
        p = q;
    }
    if (!s_flagged.insert(s_curKey).second) { toast("Уже отмечено: " + head); return; }
    std::string p = s_dir + "/feedback.tsv";
    FILE* f = fopen(p.c_str(), "ab");
    if (!f) { s_flagged.erase(s_curKey); toast("Не удалось сохранить отметку"); return; }
    fprintf(f, "%016llx\t%d\n", (unsigned long long)s_curKey, s_curId);
    fclose(f);
    toast("Отмечено: " + head);
}

void trDrawToast() {
    if (s_toastT <= 0) return;
    s_toastT--;
    float a = std::min(1.f, s_toastT / 20.f), w = 0;
    bool prop = isCyrillic(s_toast);
    for (const char* p = s_toast.c_str(); *p;) { uint32_t cp = utf8Next(p); w += prop ? g_fontM.propAdv(cp) : 14; }
    float x = 240 - w / 2;
    g_gfx.fillRect(x - 8, 6, w + 16, 20, {20, 24, 32, (uint8_t)(220 * a)});
    g_fontM.drawText(s_toast, x, 9, {255, 255, 255, (uint8_t)(255 * a)});
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
