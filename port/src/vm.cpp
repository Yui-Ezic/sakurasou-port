#include "vm.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

static const uint8_t kLen[70] = {
    1,1,2,2,2,2,1,0,2,2,3,3,0,3,3,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};

void Vm::load(Bytes c, const std::string& n) { code = std::move(c); nm = n; reset(); }

void Vm::reset() {
    memset(stk, 0, sizeof stk);
    pc = 0; csp = 0; sp = kStackWords; cs = 0; fp = kStackWords; halt = false;
}

const char* Vm::str(int32_t w) const {
    size_t o = (size_t)w * 4;
    return o < code.size() ? (const char*)code.data() + o : "";
}

void Vm::pushFrame(int argc) {
    int oldCs = cs, oldFp = fp, nfp = sp + argc;
    int rec = csp;
    cs = rec; fp = nfp;
    stk[rec + 0] = oldCs; stk[rec + 1] = oldFp; stk[rec + 2] = rec; stk[rec + 3] = nfp; stk[rec + 4] = pc;
    csp = rec + 5;
}

void Vm::popFrame() {
    int r = cs;
    int a = stk[r], b = stk[r + 1], c = stk[r + 2], d = stk[r + 3], e = stk[r + 4];
    cs = a; fp = b; csp = c; sp = d; pc = e;
}

void Vm::popFrameKeepArgs(int argc) { popFrame(); sp -= argc; }

Vm::State Vm::save() const {
    State s{cs, fp, pc, csp, sp, halt};
    memcpy(s.stack, stk, sizeof stk);
    return s;
}
void Vm::restore(const State& s) {
    cs = s.cs; fp = s.fp; pc = s.pc; csp = s.csp; sp = s.sp; halt = s.halt;
    memcpy(stk, s.stack, sizeof stk);
}

void Vm::run() {
    if (halt || code.empty()) return;
    const int codeWords = (int)(code.size() / 4);
    auto adv = [&] { pc += kLen[word(0)]; };
    for (;;) {
        if (pc < 0 || pc >= codeWords) { fprintf(stderr, "VM %s: pc out of range %x\n", nm.c_str(), pc); halt = true; return; }
        uint32_t op = word(0);
        if (op >= 70) { fprintf(stderr, "VM %s: bad op %u at %x\n", nm.c_str(), op, pc); halt = true; return; }
        int32_t a0 = kLen[op] > 1 ? (int32_t)word(1) : 0;
        int32_t a1 = kLen[op] > 2 ? (int32_t)word(2) : 0;
        int32_t x, y;
        switch (op) {
        case 0: halt = true; return;
        case 1: adv(); return;
        case 2: case 3: push(a0); adv(); break;
        case 4: push(lref(a0)); adv(); break;
        case 5: push(gref(a0)); adv(); break;
        case 6: pop(); adv(); break;
        case 7: case 12: break;
        case 8: pc = a0; break;
        case 9: if (pop() == 0) pc = a0; else adv(); break;
        case 10: case 11:
            for (int i = 0; i < a0; i++) { sp = stk[--csp]; }
            pc = a1; break;
        case 13: pushFrame(a0); pc = a1; break;
        case 14: {
            pushFrame(a0);
            int32_t ret = 0;
            int r = handler ? handler->native(*this, a1, ret) : 0;
            if (r == 1) { popFrameKeepArgs(a0); return; }
            if (r == 0) { popFrame(); push(ret); adv(); }
            break;
        }
        case 15: x = pop(); popFrame(); push(x); adv(); break;
        case 16: stk[csp++] = sp; adv(); break;
        case 17: sp = stk[--csp]; adv(); break;
#define ST(O, R) case O: R = pop(); push(R); adv(); break;
        ST(18, lref(a0)) ST(19, gref(a0))
#undef ST
#define RMW(O, EXPR) \
        case O: { x = pop(); int32_t& r = lref(a0); r = EXPR; push(r); adv(); break; } \
        case O + 1: { x = pop(); int32_t& r = gref(a0); r = EXPR; push(r); adv(); break; }
        RMW(20, r + x) RMW(22, r - x) RMW(24, r * x)
        RMW(26, x ? r / x : 0) RMW(28, x ? r % x : 0)
        RMW(30, (int32_t)((uint32_t)r << (x & 31))) RMW(32, r >> (x & 31))
        RMW(34, r | x) RMW(36, r & x) RMW(38, r ^ x)
#undef RMW
        case 40: { int32_t& r = lref(a0); r++; push(r); adv(); break; }
        case 41: { int32_t& r = gref(a0); r++; push(r); adv(); break; }
        case 42: { int32_t& r = lref(a0); r--; push(r); adv(); break; }
        case 43: { int32_t& r = gref(a0); r--; push(r); adv(); break; }
        case 44: case 45: case 46: case 47: { // post-inc/dec: pushes the OLD value (confirmed in MIPS at 0xe7b80)
            int32_t& r = (op & 1) ? gref(a0) : lref(a0);
            x = r; r += op < 46 ? 1 : -1; push(x); adv(); break; }
#define BIN(O, E) case O: x = pop(); y = pop(); push(E); adv(); break;
        BIN(48, y + x) BIN(49, y - x) BIN(50, y * x) BIN(51, x ? y / x : 0) BIN(52, x ? y % x : 0)
        BIN(53, y | x) BIN(54, y & x) BIN(55, y ^ x)
        BIN(57, (int32_t)((uint32_t)y << (x & 31))) BIN(58, y >> (x & 31))
        BIN(61, y != 0 || x != 0) BIN(62, y != 0 && x != 0)
        BIN(64, y == x) BIN(65, y != x)
        // decompiler: 66 -> x<y (pops top into x first), 67 -> y<x, 68 -> x<=y, 69 -> y<=x
        BIN(66, x < y) BIN(67, y < x) BIN(68, x <= y) BIN(69, y <= x)
#undef BIN
        case 56: x = pop(); push(~x); adv(); break;
        case 59: x = pop(); push(x); adv(); break;
        case 60: x = pop(); push(-x); adv(); break;
        case 63: x = pop(); push(x == 0); adv(); break;
        }
    }
}
