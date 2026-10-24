// Minimal x86-64 instruction length decoder and trampoline builder.
//
// The hook engine overwrites the first bytes of a function with a jump. To still
// be able to call the original function it must copy whole instructions (never
// half of one) into a trampoline, and fix up any instruction whose meaning
// depends on its own address (RIP-relative operands, relative call/jmp).
//
// This header only needs to know instruction LENGTHS and which instructions are
// position dependent, not what they do. It is intentionally conservative: for
// anything it does not understand it reports failure, and the caller must then
// refuse to hook instead of guessing.
//
// Header-only and platform neutral so it can be unit-tested on any x86-64 host.
// It is only used by the Windows build of the hook engine.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace titanium::x64 {

struct Insn {
    uint8_t length = 0;          // total length in bytes
    int8_t  ripDispOffset = -1;  // offset of a RIP-relative disp32, or -1
    int8_t  rel32Offset = -1;    // offset of the rel32 of call/jmp/jcc, or -1
    bool    rel8 = false;        // short relative branch (jcc/jmp/loop/jrcxz rel8)
    bool    terminates = false;  // ret / unconditional jmp / int3 / ud2
};

// Decodes the length of one instruction at p (at most `avail` readable bytes).
// Returns false for truncated, invalid or unsupported encodings.
inline bool decode(const uint8_t* p, size_t avail, Insn& out) {
    out = Insn{};
    size_t i = 0;
    bool opsize16 = false, addr32 = false, rexW = false;

    // Legacy prefixes.
    for (;; ++i) {
        if (i >= avail || i > 14) return false;
        const uint8_t b = p[i];
        if (b == 0x66) opsize16 = true;
        else if (b == 0x67) addr32 = true;
        else if (b == 0xF0 || b == 0xF2 || b == 0xF3 || b == 0x2E || b == 0x36 ||
                 b == 0x3E || b == 0x26 || b == 0x64 || b == 0x65) {
        } else break;
    }

    // REX.
    if ((p[i] & 0xF0) == 0x40) {
        rexW = (p[i] & 8) != 0;
        if (++i >= avail) return false;
    }

    const uint8_t op = p[i++];
    const size_t immZ = opsize16 ? 2 : 4;
    uint8_t reg = 0;

    auto skip = [&](size_t n) -> bool {
        if (i + n > avail) return false;
        i += n;
        return true;
    };

    auto modrm = [&]() -> bool {
        if (i >= avail) return false;
        const uint8_t m = p[i++];
        const uint8_t mod = m >> 6, rm = m & 7;
        reg = (m >> 3) & 7;
        if (mod == 3) return true;
        uint8_t sibBase = 0;
        const bool sib = (rm == 4);
        if (sib) {
            if (i >= avail) return false;
            sibBase = p[i++] & 7;
        }
        if (mod == 0) {
            if (rm == 5) {                       // [rip + disp32]
                if (addr32) return false;        // 32-bit RIP form: unsupported
                if (i + 4 > avail) return false;
                out.ripDispOffset = static_cast<int8_t>(i);
                i += 4;
            } else if (sib && sibBase == 5) {    // [index*scale + disp32]
                if (i + 4 > avail) return false;
                i += 4;
            }
        } else if (mod == 1) {
            if (i + 1 > avail) return false;
            i += 1;
        } else {
            if (i + 4 > avail) return false;
            i += 4;
        }
        return true;
    };

    auto finish = [&]() -> bool {
        if (i > 15) return false;
        out.length = static_cast<uint8_t>(i);
        return true;
    };

    // ---- VEX (C4 / C5 are always VEX in 64-bit mode) ----------------------
    if (op == 0xC5 || op == 0xC4) {
        uint8_t map = 1;
        if (op == 0xC5) {
            if (!skip(1)) return false;
        } else {
            if (i + 2 > avail) return false;
            map = p[i] & 0x1F;
            i += 2;
        }
        if (i >= avail) return false;
        const uint8_t vop = p[i++];
        if (map == 1 && vop == 0x77) return finish();   // vzeroupper / vzeroall: no ModRM
        if (!modrm()) return false;
        const bool imm8 = (map == 3) ||
            (map == 1 && (vop == 0x70 || vop == 0x71 || vop == 0x72 || vop == 0x73 ||
                          vop == 0xC2 || vop == 0xC4 || vop == 0xC5 || vop == 0xC6));
        if (imm8 && !skip(1)) return false;
        return finish();
    }

    // ---- Two-byte opcodes (0F xx) -----------------------------------------
    if (op == 0x0F) {
        if (i >= avail) return false;
        const uint8_t op2 = p[i++];

        if (op2 == 0x38) {                       // 0F 38 xx: always ModRM
            if (!skip(1) || !modrm()) return false;
            return finish();
        }
        if (op2 == 0x3A) {                       // 0F 3A xx: ModRM + imm8
            if (!skip(1) || !modrm() || !skip(1)) return false;
            return finish();
        }
        if (op2 >= 0x80 && op2 <= 0x8F) {        // jcc rel32
            out.rel32Offset = static_cast<int8_t>(i);
            if (!skip(4)) return false;
            return finish();
        }

        switch (op2) {
            // No operands.
            case 0x05: case 0x06: case 0x07: case 0x08: case 0x09: case 0x0E:
            case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35:
            case 0x37: case 0x77: case 0xA0: case 0xA1: case 0xA2: case 0xA8:
            case 0xA9: case 0xAA:
            case 0xC8: case 0xC9: case 0xCA: case 0xCB:
            case 0xCC: case 0xCD: case 0xCE: case 0xCF:
                return finish();
            case 0x0B:                           // ud2
                out.terminates = true;
                return finish();
            // ModRM + imm8.
            case 0x70: case 0x71: case 0x72: case 0x73: case 0xA4: case 0xAC:
            case 0xBA: case 0xC2: case 0xC4: case 0xC5: case 0xC6: case 0x0F:
                if (!modrm() || !skip(1)) return false;
                return finish();
            // ModRM only.
            case 0x00: case 0x01: case 0x02: case 0x03: case 0x0D:
            case 0xA3: case 0xA5: case 0xAB: case 0xAD: case 0xAE: case 0xAF:
            case 0xB0: case 0xB1: case 0xB3: case 0xB6: case 0xB7: case 0xB8:
            case 0xB9: case 0xBB: case 0xBC: case 0xBD: case 0xBE: case 0xBF:
            case 0xC0: case 0xC1: case 0xC3: case 0xC7:
                if (!modrm()) return false;
                return finish();
            default:
                if ((op2 >= 0x10 && op2 <= 0x23) || (op2 >= 0x28 && op2 <= 0x2F) ||
                    (op2 >= 0x40 && op2 <= 0x6F) || (op2 >= 0x74 && op2 <= 0x76) ||
                    (op2 >= 0x78 && op2 <= 0x7F) || (op2 >= 0x90 && op2 <= 0x9F) ||
                    (op2 >= 0xD0)) {
                    if (!modrm()) return false;
                    return finish();
                }
                return false;
        }
    }

    // ---- One-byte opcodes -------------------------------------------------
    if (op < 0x40) {
        const uint8_t lo = op & 7;
        if (lo < 4)       { if (!modrm()) return false; }
        else if (lo == 4) { if (!skip(1)) return false; }
        else if (lo == 5) { if (!skip(immZ)) return false; }
        else return false;                       // invalid in 64-bit mode
        return finish();
    }
    if (op >= 0x50 && op <= 0x5F) return finish();       // push/pop reg
    if (op >= 0x40 && op <= 0x4F) return false;          // stray REX
    if (op >= 0x91 && op <= 0x99) return finish();       // xchg / cdqe / cdq
    if (op >= 0x9B && op <= 0x9F) return finish();
    if (op >= 0xA4 && op <= 0xA7) return finish();
    if (op >= 0xAA && op <= 0xAF) return finish();
    if (op >= 0xB0 && op <= 0xB7) { if (!skip(1)) return false; return finish(); }
    if (op >= 0xB8 && op <= 0xBF) { if (!skip(rexW ? 8 : immZ)) return false; return finish(); }
    if (op >= 0xD8 && op <= 0xDF) { if (!modrm()) return false; return finish(); }
    if (op >= 0xD0 && op <= 0xD3) { if (!modrm()) return false; return finish(); }
    if (op >= 0xE4 && op <= 0xE7) { if (!skip(1)) return false; return finish(); }
    if (op >= 0xEC && op <= 0xEF) return finish();
    if (op >= 0xF8 && op <= 0xFD) return finish();
    if (op >= 0x70 && op <= 0x7F) {                      // jcc rel8
        if (!skip(1)) return false;
        out.rel8 = true;
        return finish();
    }
    if (op >= 0xE0 && op <= 0xE3) {                      // loop / jrcxz rel8
        if (!skip(1)) return false;
        out.rel8 = true;
        return finish();
    }
    if (op >= 0x84 && op <= 0x8F) { if (!modrm()) return false; return finish(); }

    switch (op) {
        case 0x63: if (!modrm()) return false; return finish();
        case 0x68: if (!skip(immZ)) return false; return finish();
        case 0x69: if (!modrm() || !skip(immZ)) return false; return finish();
        case 0x6A: if (!skip(1)) return false; return finish();
        case 0x6B: if (!modrm() || !skip(1)) return false; return finish();
        case 0x6C: case 0x6D: case 0x6E: case 0x6F: return finish();
        case 0x80: case 0x83: if (!modrm() || !skip(1)) return false; return finish();
        case 0x81: if (!modrm() || !skip(immZ)) return false; return finish();
        case 0x90: return finish();
        case 0xA0: case 0xA1: case 0xA2: case 0xA3:
            if (!skip(addr32 ? 4 : 8)) return false; return finish();
        case 0xA8: if (!skip(1)) return false; return finish();
        case 0xA9: if (!skip(immZ)) return false; return finish();
        case 0xC0: case 0xC1: case 0xC6:
            if (!modrm() || !skip(1)) return false; return finish();
        case 0xC7: if (!modrm() || !skip(immZ)) return false; return finish();
        case 0xC2: case 0xCA:
            if (!skip(2)) return false; out.terminates = true; return finish();
        case 0xC3: case 0xCB:
            out.terminates = true; return finish();
        case 0xC8: if (!skip(3)) return false; return finish();
        case 0xC9: return finish();
        case 0xCC: out.terminates = true; return finish();
        case 0xCD: if (!skip(1)) return false; return finish();
        case 0xCF: return finish();
        case 0xD7: return finish();
        case 0xE8:
            out.rel32Offset = static_cast<int8_t>(i);
            if (!skip(4)) return false;
            return finish();
        case 0xE9:
            out.rel32Offset = static_cast<int8_t>(i);
            out.terminates = true;
            if (!skip(4)) return false;
            return finish();
        case 0xEB:
            if (!skip(1)) return false;
            out.rel8 = true;
            out.terminates = true;
            return finish();
        case 0xF1: case 0xF4: case 0xF5: return finish();
        case 0xF6:
            if (!modrm()) return false;
            if (reg < 2 && !skip(1)) return false;
            return finish();
        case 0xF7:
            if (!modrm()) return false;
            if (reg < 2 && !skip(immZ)) return false;
            return finish();
        case 0xFE: if (!modrm()) return false; return finish();
        case 0xFF:
            if (!modrm()) return false;
            if (reg == 4 || reg == 5) out.terminates = true;   // jmp r/m
            return finish();
        default:
            return false;
    }
}

// Size of the absolute jump used both to enter the detour and to return from a
// trampoline: FF 25 00 00 00 00 <8-byte address>.
constexpr size_t kAbsJumpSize = 14;

inline void writeAbsJump(uint8_t* at, uintptr_t destination) {
    at[0] = 0xFF; at[1] = 0x25; at[2] = at[3] = at[4] = at[5] = 0;
    std::memcpy(at + 6, &destination, sizeof(destination));
}

struct TrampolineResult {
    bool ok = false;
    size_t prefixLength = 0;     // bytes of the original function that were copied
    const char* error = nullptr; // reason when !ok (static string)
    bool needsNear = false;      // failed only because the trampoline is too far away
};

// Builds a trampoline for the function at `fn` (readable at that address and at
// least `fnAvail` bytes long).
//
// The trampoline executes the original function's first instructions, relocated
// so they behave the same at their new address, then jumps back into the
// function right after them. At least `minPrefix` bytes are copied (the number
// of bytes the detour will overwrite), always ending on an instruction boundary.
//
// `out` is where the trampoline bytes are written, `outAddr` is the address the
// trampoline will EXECUTE at (these differ when building before copying).
inline TrampolineResult buildTrampoline(const uint8_t* fn, size_t fnAvail, uintptr_t fnAddr,
                                        uint8_t* out, size_t outCap, uintptr_t outAddr,
                                        size_t minPrefix = kAbsJumpSize) {
    TrampolineResult r;
    size_t pos = 0;

    while (pos < minPrefix) {
        Insn in;
        if (!decode(fn + pos, fnAvail - pos, in)) {
            r.error = "unsupported or unreadable instruction in the function prologue";
            return r;
        }
        if (in.terminates) {
            r.error = "the function is too short to hook, or already starts with a jump (hooked by something else?)";
            return r;
        }
        if (in.rel8) {
            r.error = "short relative branch in the function prologue cannot be relocated";
            return r;
        }
        if (pos + in.length + kAbsJumpSize > outCap) {
            r.error = "trampoline buffer too small";
            return r;
        }

        std::memcpy(out + pos, fn + pos, in.length);

        const int fixOff = in.ripDispOffset >= 0 ? in.ripDispOffset : in.rel32Offset;
        if (fixOff >= 0) {
            int32_t oldDisp;
            std::memcpy(&oldDisp, fn + pos + fixOff, sizeof(oldDisp));
            const int64_t absTarget = static_cast<int64_t>(fnAddr + pos + in.length) + oldDisp;
            const int64_t newDisp = absTarget - static_cast<int64_t>(outAddr + pos + in.length);
            if (newDisp < INT32_MIN || newDisp > INT32_MAX) {
                r.error = "the trampoline is too far from the function to relocate its prologue";
                r.needsNear = true;
                return r;
            }
            const int32_t fixed = static_cast<int32_t>(newDisp);
            std::memcpy(out + pos + fixOff, &fixed, sizeof(fixed));
        }
        pos += in.length;
    }

    writeAbsJump(out + pos, fnAddr + pos);
    r.ok = true;
    r.prefixLength = pos;
    return r;
}

} // namespace titanium::x64
