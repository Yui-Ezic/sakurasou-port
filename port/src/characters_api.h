// Hooks the character module needs from the scenario layer.
#pragma once
#include <cstdint>
#include <string>

// ADV must call this wherever Game+0x59f984 is written (natives 119/121/128/129/134, spec_adv).
// It decides which sprite or Live2D model lip syncs while the voice channel plays.
void charSetSpeaker(const std::string& name);
const std::string& charSpeaker();

// g_audio.playing(CH_VOICE), or the test override.
bool charVoicePlaying();
void charForceVoice(bool on);   // test only

inline uint32_t charRngNext(uint32_t& x) { x = x * 0x41C64E6Du + 0x3039u; return (x >> 16) & 0x7FFF; }
