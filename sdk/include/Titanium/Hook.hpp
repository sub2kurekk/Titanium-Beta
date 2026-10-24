#pragma once
#include <cstdint>
#include <functional>
#include <optional>

#ifndef TITANIUM_BUILD_LOADER
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#endif

namespace titanium {

enum class HookResult {
    Ok,
    AlreadyHooked,
    AddressNotFound,
    PermissionError,
};

struct HookHandle {
    void* detour;
    void* trampoline;
    void* target;
};

using HookCallback = std::function<void()>;

#ifndef TITANIUM_BUILD_LOADER

inline void* getLoaderProc(const char* name) {
#ifdef _WIN32
    HMODULE h = GetModuleHandleA("pthreadVCE3.dll");
    if (!h) h = GetModuleHandleA("pthreadVCE3_o.dll");
    if (!h) h = GetModuleHandleA("titaniumproxy.dll");
    if (!h) return nullptr;
    return (void*)GetProcAddress(h, name);
#else
    return dlsym(RTLD_DEFAULT, name);
#endif
}

inline bool hook(void* target, void* detour, HookHandle* out = nullptr) {
    typedef bool (*HookFn)(void*, void*, HookHandle*);
    static HookFn fn = (HookFn)getLoaderProc("titanium_hook");
    if (fn) return fn(target, detour, out);
    return false;
}

inline bool unhook(HookHandle* handle) {
    typedef bool (*UnhookFn)(HookHandle*);
    static UnhookFn fn = (UnhookFn)getLoaderProc("titanium_unhook");
    if (fn) return fn(handle);
    return false;
}

inline std::optional<uintptr_t> resolveAddress(const char* symbol) {
    typedef void* (*ResolveFn)(const char*);
    static ResolveFn fn = (ResolveFn)getLoaderProc("titanium_resolve_address");
    if (fn) {
        void* addr = fn(symbol);
        if (addr) return reinterpret_cast<uintptr_t>(addr);
    }
    return std::nullopt;
}
#else
bool hook(void* target, void* detour, HookHandle* out = nullptr);
bool unhook(HookHandle* handle);
std::optional<uintptr_t> resolveAddress(const char* symbol);
#endif

} // namespace titanium

#define TITANIUM_HOOK(ret, name, args...) \
    ret name##_Detour(args); \
    using name##_Fn = ret(*)(args); \
    name##_Fn name = nullptr; \
    \
    __attribute__((constructor)) void name##_HookInit() { \
        auto addr = titanium::resolveAddress(#name); \
        if (addr) { \
            titanium::HookHandle h; \
            titanium::hook((void*)addr, (void*)name##_Detour, &h); \
            name = (name##_Fn)h.trampoline; \
        } \
    } \
    ret name##_Detour(args)
