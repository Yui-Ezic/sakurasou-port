// Live2D game layer: LBD container, motions, expressions, blink, mouth, move and jump, drawing.
// docs/spec_characters.md sections 4 and 7.
#pragma once
#include "gfx.h"
#include "live2d.h"
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

constexpr size_t kL2dSaveSize = 0x218;

class L2DStage {
public:
    ~L2DStage();
    void setSpeaker(const std::string& s) { speaker = s.substr(0, 31); }
    void startLoad(const std::string& file);    // N179
    void create();                              // N181
    void release();                             // N195
    void setScale(float v) { k = v; }           // N182
    void moveTo(float x, float y, float z, int frames);
    void jumpTo(float x, float y, float z, float scale, float height, int frames);
    void queueMotion(const std::string& name, int kind);   // ignored when unknown
    void queueExpression(int id, int frames);
    void setLipMode(int m) { lipMode = m; }
    void clearMotionQueue() { queue.clear(); }
    void clearExpressionQueue() { exprQueue.clear(); }
    bool motionQueueBusy() const { return !queue.empty(); }
    bool expressionBusy() const { return !(exprT >= 1 && exprQueue.empty()); }
    bool moveBusy() const { return moveT < moveDur; }
    bool jumpBusy() const { return jumpT < jumpDur; }

    void tick();             // every tick, acts on every third one
    void draw();
    void save(uint8_t* rec) const;
    void load(const uint8_t* rec);

    // for tests
    l2d::Model* model() { return mdl.get(); }
    const std::vector<std::string>& motionNames() const { return motionNames_; }
    int expressionCount() const { return (int)exprTable.size(); }
    bool created() const { return isCreated; }
private:
    struct Track { int type = 0; std::string name; int param = -1; std::vector<float> v; };
    struct Motion {
        std::string name; bool loop = false; std::vector<Track> tracks;
        float fps = 30; int maxFrames = 0, durationMs = 0, fadeIn = 1000, fadeOut = 100;
    };
    struct Entry { int motion; bool available = true, finished = false, started = false; int64_t start = -1, end = -1, fadeStart = -1; };
    struct Queued { int kind, motion; std::string name; };
    struct Eye { std::string name; int t[4]; };
    struct Expr { float eyeWeight = 0; std::vector<float> value; };
    struct ExprQ { int id, frames; };

    bool parseLbd();
    void parseMotion(Motion& m, const char* text, size_t n);
    void evaluate(Entry& e, float w, int64_t now);
    void mgrStart(int motion);
    void mgrUpdate();
    bool mgrFinished() const;
    void next();
    void motionUpdate(float dt);
    void expressionUpdate(float dt, bool canSwitch);
    float exprTarget(const std::string& name) const;
    void blinkUpdate(float dt);
    void mouthUpdate(float dt);
    void moveUpdate(float dt);
    void jumpUpdate(float dt);
    void updateGroup(float dt, const std::vector<std::string>& names);
    bool mouthActive() const;
    const Eye* eyeWindow(const std::string& name) const;
    void freeTextures();

    Bytes file;
    std::string filename, speaker;
    std::unique_ptr<l2d::Model> mdl;
    std::vector<TexP> textures;
    std::vector<Motion> motions;
    std::vector<std::string> motionNames_;
    std::vector<Eye> eyes;
    std::vector<std::string> exprNames;
    std::vector<Expr> exprTable;
    std::vector<int> noBlink;
    std::vector<std::vector<std::string>> groups;
    std::vector<std::pair<std::string, float>> overrides;

    float k = 0.27f, s = 1, h = 0;
    float pos[3] = {};
    int moveDur = 0, moveT = 0; float moveStart[3] = {}, moveTarget[3] = {};
    int jumpDur = 0, jumpT = 0; float jumpStart[3] = {}, jumpTarget[3] = {}, s0 = 1, s1 = 1, jumpHeight = 0;
    int lipMode = 0, tickCounter = 0;
    bool isCreated = false;

    int cur = -1, curKind = 0, curFrames = 0, frameCounter = 0;
    std::string curName;
    std::deque<Queued> queue;
    std::vector<Entry> entries;

    float exprT = 1, exprRate = 0; int exprId = 0;
    std::deque<ExprQ> exprQueue;
    float snapshot[30] = {};

    float eyeL = 1, eyeR = 1; int blinkCounter = 0, blinkState = 0; bool armed = false;
    uint32_t blinkRng = 0;

    int mouthState = 0; float phase = 90;

    int64_t lastTime = 0;       // static in the original: survives across models
    std::vector<SDL_Vertex> verts;
};

int64_t charClockMs();          // ticks * 1000 / 60
