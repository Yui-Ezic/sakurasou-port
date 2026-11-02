// SSB script VM: exact behaviour of the game's interpreter (EBOOT 0xe835c).
#pragma once
#include "fs.h"
#include <cstdint>
#include <string>
#include <vector>

class Vm;
struct NativeHandler {
    virtual ~NativeHandler() = default;
    // returns 0 = finished (ret is pushed), 1 = suspend and re-execute this call next frame
    virtual int native(Vm& vm, int id, int32_t& ret) = 0;
};

class Vm {
public:
    static constexpr int kStackWords = 512; // 0x800 bytes, as on PSP
    void load(Bytes code, const std::string& name);
    void reset();
    void run();                      // runs until yield/suspend/halt, as the game does once per frame
    bool halted() const { return halt; }
    bool loaded() const { return !code.empty(); }
    int32_t arg(int i) const { return stk[fp - i - 1]; }
    int32_t global(int i) const { return stk[kStackWords - i - 1]; }
    void setGlobal(int i, int32_t v) { stk[kStackWords - i - 1] = v; }
    const char* str(int32_t wordIndex) const;
    const char* argStr(int i) const { return str(arg(i)); }
    const std::string& name() const { return nm; }
    NativeHandler* handler = nullptr;
    int traceBudget = 0;

    // complete VM state, for save games
    struct State { int32_t cs, fp, pc, csp, sp, halt; int32_t stack[kStackWords]; };
    State save() const;
    void restore(const State&);
private:
    void pushFrame(int argc);
    void popFrame();
    void popFrameKeepArgs(int argc);
    void push(int32_t v) { stk[--sp] = v; }
    int32_t pop() { return stk[sp++]; }
    uint32_t word(int i) const { return ((const uint32_t*)code.data())[pc + i]; }
    int32_t& gref(int n) { return stk[kStackWords - n - 1]; }
    int32_t& lref(int n) { return stk[fp - n - 1]; }

    Bytes code;
    std::string nm;
    int32_t stk[kStackWords] = {};
    int cs = 0;    // base of the current frame record (grows up)
    int fp = kStackWords;
    int pc = 0;
    int csp = 0;   // top of frame/mark stack (grows up)
    int sp = kStackWords;
    bool halt = true;
};
