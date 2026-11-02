// PMF (PSMF) movie playback: own MPEG-PS demux, H.264 + ATRAC3+ through libavcodec.
#pragma once
#include "fs.h"
#include "gfx.h"
#include <memory>
#include <string>

class Movie {
public:
    Movie(); ~Movie();
    bool open(const std::string& asset);
    // Advances to the frame due now; false once the movie has finished.
    bool update();
    void draw();
    void stop();
private:
    struct Impl; std::unique_ptr<Impl> p;
};
