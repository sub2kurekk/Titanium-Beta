// Executes real hooked functions to prove the trampoline builder in x64len.hpp
// preserves behaviour. For every test function: record what the original
// returns, install a detour (exactly like the hook engine: an absolute jump
// over the first 14 bytes) that calls the original through the trampoline and
// adds 1000, and check that results and call counts are right.
#include "../loader/src/x64len.hpp"
#include <cstdio>
#include <vector>
#include <sys/mman.h>
#include <unistd.h>

extern "C" {
long f_pushes(long, long);   long f_straddle(long, long); long f_riprel(long, long);
long f_call(long, long);     long f_endbr(long, long);    long f_sse(long, long);
long f_prefixes(long, long); long f_rel8(long, long);     long f_thunk(long, long);
long f_short(long, long);
}

using Fn = long (*)(long, long);
static Fn g_original = nullptr;
static int g_detourCalls = 0;
static long detour(long a, long b) { ++g_detourCalls; return g_original(a, b) + 1000; }

struct Case { const char* name; Fn fn; bool expectOk; bool farFails; };

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; std::printf("  FAIL: " __VA_ARGS__); std::printf("\n"); } } while (0)

static void* allocRwx(uintptr_t hint) {
    void* p = mmap(reinterpret_cast<void*>(hint), 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
                   MAP_PRIVATE | MAP_ANONYMOUS | (hint ? MAP_FIXED_NOREPLACE : 0), -1, 0);
    return p == MAP_FAILED ? nullptr : p;
}

int main() {
    const Case cases[] = {
        {"f_pushes   (lea straddles byte 14)", f_pushes,   true,  false},
        {"f_straddle (sub straddles byte 14)", f_straddle, true,  false},
        {"f_riprel   (RIP-relative operands)", f_riprel,   true,  true },
        {"f_call     (call rel32 first)",      f_call,     true,  true },
        {"f_endbr    (endbr64 + frame)",       f_endbr,    true,  false},
        {"f_sse      (SSE + movabs)",          f_sse,      true,  false},
        {"f_prefixes (66/REX prefixes)",       f_prefixes, true,  false},
        {"f_rel8     (short jcc in prefix)",   f_rel8,     false, false},
        {"f_thunk    (starts with jmp)",       f_thunk,    false, false},
        {"f_short    (ret inside window)",     f_short,    false, false},
    };
    const long args[][2] = {{0, 0}, {1, 2}, {-5, 9}, {123456, -7}, {0x7fffffff, 3}};

    for (const Case& c : cases) {
        std::printf("%s\n", c.name);
        auto* code = reinterpret_cast<uint8_t*>(c.fn);
        const uintptr_t addr = reinterpret_cast<uintptr_t>(c.fn);

        long expected[5];
        for (int k = 0; k < 5; ++k) expected[k] = c.fn(args[k][0], args[k][1]);

        // Trampoline within +-2GB of the function (like the Windows engine does).
        static uintptr_t nextOffset = 0x2000000;
        void* tramp = allocRwx((addr & ~0xFFFul) + nextOffset);
        nextOffset += 0x10000;
        CHECK(tramp, "could not allocate a nearby trampoline");
        if (!tramp) continue;

        auto r = titanium::x64::buildTrampoline(code, 64, addr,
                     static_cast<uint8_t*>(tramp), 4096, reinterpret_cast<uintptr_t>(tramp));
        if (!c.expectOk) {
            CHECK(!r.ok, "should have been refused but was accepted");
            std::printf("  refused as expected: %s\n", r.error ? r.error : "?");
            continue;
        }
        CHECK(r.ok, "builder failed: %s", r.error ? r.error : "?");
        if (!r.ok) continue;
        std::printf("  prefix length copied: %zu bytes\n", r.prefixLength);
        CHECK(r.prefixLength >= 14, "prefix shorter than the detour jump");

        // A trampoline too far away must be reported, not silently miscompiled.
        if (c.farFails) {
            void* far = allocRwx(0);
            std::vector<uint8_t> buf(64);
            uintptr_t farAddr = addr + 0x100000000ull;   // pretend it lives 4 GB away
            auto rf = titanium::x64::buildTrampoline(code, 64, addr, buf.data(), buf.size(), farAddr);
            CHECK(!rf.ok && rf.needsNear, "far trampoline should be refused with needsNear");
            (void)far;
        }

        // Install the detour like the engine does, then run the function.
        mprotect(reinterpret_cast<void*>(addr & ~0xFFFul), 0x2000, PROT_READ | PROT_WRITE | PROT_EXEC);
        titanium::x64::writeAbsJump(code, reinterpret_cast<uintptr_t>(&detour));
        g_original = reinterpret_cast<Fn>(tramp);
        g_detourCalls = 0;
        bool ok = true;
        for (int k = 0; k < 5; ++k) {
            long got = c.fn(args[k][0], args[k][1]);
            if (got != expected[k] + 1000) {
                ok = false;
                std::printf("  args(%ld,%ld): got %ld, expected %ld\n", args[k][0], args[k][1], got, expected[k] + 1000);
            }
        }
        CHECK(ok, "hooked results differ from the original");
        CHECK(g_detourCalls == 5, "detour called %d times, expected 5", g_detourCalls);
        if (ok && g_detourCalls == 5) std::printf("  hooked function behaves identically (5/5 calls)\n");
    }

    std::printf(failures ? "\n%d FAILURE(S)\n" : "\nALL TESTS PASSED\n", failures);
    return failures ? 1 : 0;
}
