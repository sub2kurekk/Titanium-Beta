#include <Titanium/HookEngine.hpp>
#include <Log.hpp>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include "x64len.hpp"
#else
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#endif

namespace titanium {

HookEngine& HookEngine::get() {
    static HookEngine instance;
    return instance;
}

HookEngine::~HookEngine() {
    std::lock_guard lock(mutex);
    for (auto& [addr, hook] : hooks) {
        if (hook.active)
            restoreOriginal(hook);
    }
}

#ifdef _WIN32
static bool setMemoryProtection(void* addr, size_t size, DWORD prot) {
    DWORD oldProt;
    return VirtualProtect(addr, size, prot, &oldProt) != FALSE;
}

static void flushICache(void* addr, size_t size) {
    FlushInstructionCache(GetCurrentProcess(), addr, size);
}
#else
static bool setMemoryProtection(void* addr, size_t size, int prot) {
    return mprotect(addr, size, prot) == 0;
}

static void flushICache(void* addr, size_t size) {
    __builtin___clear_cache(reinterpret_cast<char*>(addr),
                            reinterpret_cast<char*>(addr) + size);
}
#endif

#ifdef _WIN32
// Allocates a small executable block within +-1.5 GB of `near`, so that
// RIP-relative operands and rel32 branches copied into it can still reach their
// targets. Returns nullptr if no free region is found.
static void* allocateNear(uintptr_t nearAddr, size_t size) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    const uintptr_t gran = si.dwAllocationGranularity;
    const uintptr_t range = 0x60000000;
    uintptr_t lo = nearAddr > range ? nearAddr - range : 0x10000;
    uintptr_t hi = nearAddr + range;
    if (lo < reinterpret_cast<uintptr_t>(si.lpMinimumApplicationAddress))
        lo = reinterpret_cast<uintptr_t>(si.lpMinimumApplicationAddress);

    // Walk outwards from the target: try addresses below and above alternately.
    for (uintptr_t step = 0; step < range; step += gran * 16) {
        for (int dir = 0; dir < 2; ++dir) {
            uintptr_t a = dir ? nearAddr + step : (nearAddr > step ? nearAddr - step : 0);
            if (a < lo || a > hi) continue;
            a &= ~(gran - 1);
            MEMORY_BASIC_INFORMATION mbi;
            if (!VirtualQuery(reinterpret_cast<void*>(a), &mbi, sizeof(mbi))) continue;
            if (mbi.State != MEM_FREE) continue;
            uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
            uintptr_t end = base + mbi.RegionSize;
            uintptr_t cand = (base + gran - 1) & ~(gran - 1);
            if (cand < lo) cand = (lo + gran - 1) & ~(gran - 1);
            if (cand + size > end || cand + size > hi) continue;
            void* p = VirtualAlloc(reinterpret_cast<void*>(cand), size,
                                   MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
            if (p) return p;
        }
    }
    return nullptr;
}

// Builds a trampoline that copies whole instructions from the start of `target`
// (relocating RIP-relative operands) and jumps back. Returns nullptr and logs the
// reason if the function cannot be hooked safely.
static void* createSafeTrampoline(void* target) {
    constexpr size_t kBlock = 128;

    // Do not read past the end of the mapped, readable memory.
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(target, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT) {
        logMsg("[Titanium] Hook target %p is not committed memory", target);
        return nullptr;
    }
    size_t avail = (reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize) -
                   reinterpret_cast<uintptr_t>(target);
    if (avail > 64) avail = 64;

    uintptr_t addr = reinterpret_cast<uintptr_t>(target);
    void* block = allocateNear(addr, kBlock);
    if (!block) {
        logMsg("[Titanium] No executable memory available near %p for the trampoline", target);
        return nullptr;
    }

    auto res = x64::buildTrampoline(static_cast<const uint8_t*>(target), avail, addr,
                                    static_cast<uint8_t*>(block), kBlock,
                                    reinterpret_cast<uintptr_t>(block));
    if (!res.ok) {
        logMsg("[Titanium] Refusing to hook %p: %s%s", target,
               res.error ? res.error : "unknown error",
               res.needsNear ? " (relocated operand out of range)" : "");
        VirtualFree(block, 0, MEM_RELEASE);
        return nullptr;
    }

    logMsg("[Titanium] Trampoline for %p built at %p (%zu bytes of prologue copied)",
           target, block, res.prefixLength);
    FlushInstructionCache(GetCurrentProcess(), block, kBlock);
    DWORD old;
    VirtualProtect(block, kBlock, PAGE_EXECUTE_READ, &old);
    return block;
}
#endif

bool HookEngine::installHook(void* target, void* detour, Hook* out) {
    std::lock_guard lock(mutex);

    if (hooks.count(target)) {
        logMsg("[Titanium] Hook already exists at %p", target);
        return false;
    }

    Hook hook;
    hook.target = target;
    hook.detour = detour;
    hook.active = false;

    hook.originalBytes.resize(14);
    memcpy(hook.originalBytes.data(), target, 14);

#ifdef _WIN32
    hook.trampoline = createSafeTrampoline(target);
#else
    hook.trampoline = createTrampoline(target, 14);
#endif
    if (!hook.trampoline) {
        logMsg("[Titanium] Failed to create trampoline for %p", target);
        return false;
    }

    if (!applyDetour(hook)) {
#ifdef _WIN32
        VirtualFree(hook.trampoline, 0, MEM_RELEASE);
#else
        munmap(hook.trampoline, 64);
#endif
        return false;
    }

    hooks[target] = hook;

    if (out)
        *out = hook;

    return true;
}

bool HookEngine::removeHook(void* target) {
    std::lock_guard lock(mutex);
    auto it = hooks.find(target);
    if (it == hooks.end())
        return false;

    bool result = restoreOriginal(it->second);
    it->second.active = false;
    hooks.erase(it);
    return result;
}

bool HookEngine::removeHook(Hook* hook) {
    if (!hook) return false;
    return removeHook(hook->target);
}

void* HookEngine::createTrampoline(void* target, int numPrefixBytes) {
#ifdef _WIN32
    void* trampoline = VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!trampoline)
        return nullptr;
#else
    void* trampoline = mmap(nullptr, 64, PROT_READ | PROT_WRITE | PROT_EXEC,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (trampoline == MAP_FAILED)
        return nullptr;
#endif

    auto* code = static_cast<uint8_t*>(trampoline);

    memcpy(code, target, numPrefixBytes);
    code += numPrefixBytes;

    code[0] = 0xFF;
    code[1] = 0x25;
    code[2] = 0x00;
    code[3] = 0x00;
    code[4] = 0x00;
    code[5] = 0x00;

    void* jumpTarget = static_cast<uint8_t*>(target) + numPrefixBytes;
    memcpy(code + 6, &jumpTarget, sizeof(void*));

#ifdef _WIN32
    DWORD old;
    VirtualProtect(trampoline, 64, PAGE_EXECUTE_READ, &old);
#endif

    return trampoline;
}

bool HookEngine::applyDetour(Hook& hook) {
    if (hook.active) return true;

#ifdef _WIN32
    constexpr int protRW = PAGE_READWRITE;
    constexpr int protRX = PAGE_EXECUTE_READ;
#else
    constexpr int protRW = PROT_READ | PROT_WRITE;
    constexpr int protRX = PROT_READ | PROT_EXEC;
#endif

    if (!setMemoryProtection(hook.target, 14, protRW)) {
        logMsg("[Titanium] setMemoryProtection(RW) failed");
        return false;
    }

    auto* code = static_cast<uint8_t*>(hook.target);

    code[0] = 0xFF;
    code[1] = 0x25;
    code[2] = 0x00;
    code[3] = 0x00;
    code[4] = 0x00;
    code[5] = 0x00;
    memcpy(code + 6, &hook.detour, sizeof(void*));

    flushICache(code, 14);

    setMemoryProtection(hook.target, 14, protRX);

    hook.active = true;
    return true;
}

bool HookEngine::restoreOriginal(Hook& hook) {
    if (!hook.active) return true;

#ifdef _WIN32
    constexpr int protRW = PAGE_READWRITE;
    constexpr int protRX = PAGE_EXECUTE_READ;
#else
    constexpr int protRW = PROT_READ | PROT_WRITE;
    constexpr int protRX = PROT_READ | PROT_EXEC;
#endif

    if (!setMemoryProtection(hook.target, 14, protRW))
        return false;

    memcpy(hook.target, hook.originalBytes.data(), 14);
    flushICache(hook.target, 14);

    setMemoryProtection(hook.target, 14, protRX);
    hook.active = false;

    return true;
}

} // namespace titanium

#include <Titanium/Hook.hpp>
#include <Titanium/PatternScan.hpp>

extern "C" {

__attribute__((visibility("default")))
bool titanium_hook(void* target, void* detour, titanium::HookHandle* out) {
    titanium::Hook hook;
    bool success = titanium::HookEngine::get().installHook(target, detour, &hook);
    if (success && out) {
        out->target = hook.target;
        out->detour = hook.detour;
        out->trampoline = hook.trampoline;
    }
    return success;
}

__attribute__((visibility("default")))
bool titanium_unhook(titanium::HookHandle* handle) {
    if (!handle) return false;
    return titanium::HookEngine::get().removeHook(handle->target);
}

__attribute__((visibility("default")))
void* titanium_resolve_address(const char* symbol) {
    auto addr = titanium::PatternScanner::get().resolveSymbol(symbol);
    return addr ? reinterpret_cast<void*>(*addr) : nullptr;
}

}
