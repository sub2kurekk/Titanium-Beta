#pragma once
#include <cstdio>
#include <utility>

#ifndef TITANIUM_BUILD_LOADER
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#endif

namespace titanium {

enum class LogLevel {
    Debug,
    Info,
    Warn,
    Error,
};

inline const char* logLevelName(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
    }
    return "UNKNOWN";
}

#ifndef TITANIUM_BUILD_LOADER

inline void* getLogLoaderProc(const char* name) {
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
#endif

inline void log(LogLevel level, const char* mod, const char* fmt, auto... args) {
    char msg[1024];
    std::snprintf(msg, sizeof(msg), fmt, args...);

#ifndef TITANIUM_BUILD_LOADER
    typedef void (*LogFn)(int, const char*, const char*);
    static LogFn fn = (LogFn)getLogLoaderProc("titanium_log");
    if (fn) {
        fn(static_cast<int>(level), mod, msg);
        return;
    }
#endif

    std::fprintf(stderr, "[Titanium][%s][%s] %s\n", logLevelName(level), mod, msg);
}

} // namespace titanium

#define TITANIUM_LOG(level, fmt, ...) \
    titanium::log(titanium::LogLevel::level, "unknown", fmt, ##__VA_ARGS__)

#define TITANIUM_MOD_LOG(level, fmt, ...) \
    titanium::log(titanium::LogLevel::level, getInfo().id.data(), fmt, ##__VA_ARGS__)
