// Mixer with channels matching the game's sound manager: BGM, ambient, SE, voice etc.
#pragma once
#include "fs.h"
#include <memory>
#include <string>

struct Pcm { int rate = 0, ch = 0; std::vector<int16_t> s; int64_t loopStart = -1, loopEnd = -1; };
bool decodeAdx(const Bytes& d, Pcm& out);
bool decodeHca(const Bytes& d, Pcm& out);
std::shared_ptr<Pcm> loadSound(const std::string& name);

enum { CH_BGM = 0, CH_BGM2, CH_AMB, CH_SE, CH_SE2, CH_VOICE, CH_SYS, CH_COUNT };

class Audio {
public:
    bool init(bool enable);
    void shutdown();
    void play(int ch, const std::string& name, bool loop, int fadeInFrames = 0);
    void playPcm(int ch, std::shared_ptr<Pcm> pcm);
    void stop(int ch, int fadeOutFrames = 0);
    void setVolume(int ch, float vol, int fadeFrames = 0);  // per-channel script volume 0..1
    bool playing(int ch);
    bool fading(int ch);
    void setGroupVolume(int group, float v);               // config: 0 bgm 1 se 2 voice
    void loadCues(const std::string& awb);                 // AFS2 archive of SE cues
    void playCue(int id, int ch = CH_SYS);
    float voiceLevel();                                    // for lip sync
};
extern Audio g_audio;
