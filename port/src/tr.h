// Runtime translation: replaces strings at display time, so the scripts and the ISO stay untouched.
// Without a pack (no --tl) every function here is a no-op and the game behaves exactly as the original.
#pragma once
#include <cstdint>
#include <string>

bool trInit(const std::string& dir);       // reads <dir>/ru.pack, mounts <dir>/assets over the ISO
void trReload();                           // F5: re-reads the pack; applies from the next message
bool trOn();
// UI, names, choices, clouds, data tables: looked up by the Japanese text. Returns the input on a miss.
const std::string& tr(const std::string& jp);
// Dialogue: looked up by message id + text, because the same Japanese line can need different Russian
// (speaker gender). Returns the text wrapped to the message window; false on a miss.
bool trMessage(int id, const std::string& jp, std::string& out);
void trDump(const std::string& path);      // dev: wrap of every entry, compared by tl_build.py --verify
void trFlagCurrent();                      // F8: records the line on screen in <dir>/feedback.tsv
void trDrawToast();                        // on-screen confirmation for F5 and F8; call once per frame
inline bool isCyrillic(const std::string& s) {
    for (unsigned char c : s) if (c == 0xd0 || c == 0xd1) return true;
    return false;
}
constexpr int kTrMsgWidth = 396;           // px, from the text origin (x=38) to clear of the marker (x=444)
