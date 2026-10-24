#include <Titanium/PatternScan.hpp>
#include <Log.hpp>
#include <cstdio>
#include <cstring>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <link.h>
#include <dlfcn.h>
#endif

namespace titanium {

#ifdef _WIN32
static FILE* logFile() {
    FILE* f = nullptr;
    fopen_s(&f, "C:\\titaniumloader.log", "a");
    return f;
}
#endif

PatternScanner& PatternScanner::get() {
    static PatternScanner instance;
    return instance;
}

bool PatternScanner::load() {
    if (baseAddr != 0) return true;

    if (!findGdBinary())
        return false;

#ifdef _WIN32
    auto log = logFile();
    if (log) {
        fprintf(log, "[Titanium] GD base: 0x%llx, code end: 0x%llx\n",
                (unsigned long long)baseAddr, (unsigned long long)codeEnd);
        fclose(log);
    }
#endif
    logMsg("[Titanium] GD base: 0x%llx, code end: 0x%llx",
           (unsigned long long)baseAddr, (unsigned long long)codeEnd);

    return true;
}

void PatternScanner::unload() {
    regions.clear();
    baseAddr = 0;
    codeEnd = 0;
}

#ifdef _WIN32
bool PatternScanner::findGdBinary() {
    HANDLE process = GetCurrentProcess();
    HMODULE modules[256];
    DWORD needed;

    if (!EnumProcessModules(process, modules, sizeof(modules), &needed))
        return false;

    DWORD count = needed / sizeof(HMODULE);

    for (DWORD i = 0; i < count; ++i) {
        char name[MAX_PATH];
        if (!GetModuleBaseNameA(process, modules[i], name, MAX_PATH))
            continue;

        char lower[260];
        for (int j = 0; name[j]; ++j)
            lower[j] = (char)tolower(name[j]);
        lower[strlen(name)] = '\0';

        if (strstr(lower, "geometrydash") || strstr(lower, "libcocos2d")) {
            MODULEINFO info;
            if (GetModuleInformation(process, modules[i], &info, sizeof(info))) {
                baseAddr = reinterpret_cast<uintptr_t>(info.lpBaseOfDll);
                codeEnd = baseAddr + info.SizeOfImage;
                return true;
            }
        }
    }

    for (DWORD i = 0; i < count; ++i) {
        MODULEINFO info;
        if (GetModuleInformation(process, modules[i], &info, sizeof(info))) {
            char name[MAX_PATH];
            if (GetModuleBaseNameA(process, modules[i], name, MAX_PATH)) {
                if (strstr(name, ".exe")) {
                    baseAddr = reinterpret_cast<uintptr_t>(info.lpBaseOfDll);
                    codeEnd = baseAddr + info.SizeOfImage;
                    return true;
                }
            }
        }
    }

    return false;
}
#else
// Linux: parse /proc/self/maps to find the executable and shared libs.
bool PatternScanner::findGdBinary() {
    std::ifstream maps("/proc/self/maps");
    if (!maps.is_open())
        return false;

    std::string line;
    while (std::getline(maps, line)) {
        // Format: addr-addr perms offset dev inode pathname
        // We just want the executable region of the main binary (or libGD/cocos2d).
        uintptr_t start = 0, end = 0;
        char perms[8] = {0};
        char path[512] = {0};
        if (sscanf(line.c_str(), "%lx-%lx %7s %*x %*s %*u %511s",
                   &start, &end, perms, path) < 3) {
            continue;
        }

        if (strstr(path, "GeometryDash") || strstr(path, "libcocos2d") ||
            strstr(path, "geometrydash")) {
            // For the .exe, we want the executable region only.
            if (perms[2] == 'x') {
                baseAddr = start;
                codeEnd = end;
                return true;
            }
        }
    }

    // Fallback: use the main program's first executable region.
    maps.clear();
    maps.seekg(0);
    while (std::getline(maps, line)) {
        uintptr_t start = 0, end = 0;
        char perms[8] = {0};
        if (sscanf(line.c_str(), "%lx-%lx %7s", &start, &end, perms) == 3) {
            if (perms[2] == 'x') {
                baseAddr = start;
                codeEnd = end;
                return true;
            }
        }
    }

    return false;
}
#endif

bool PatternScanner::isIdSym(const char* name) {
    return name && (
        strstr(name, "cocos2d") ||
        strstr(name, "GeometryDash") ||
        strstr(name, "gd")
    );
}

#ifdef _WIN32
std::optional<uintptr_t> PatternScanner::resolveSymbol(const char* symbol) {
    if (!load()) return std::nullopt;

    HANDLE process = GetCurrentProcess();
    HMODULE modules[256];
    DWORD needed;

    if (!EnumProcessModules(process, modules, sizeof(modules), &needed))
        return std::nullopt;

    DWORD count = needed / sizeof(HMODULE);

    for (DWORD i = 0; i < count; ++i) {
        FARPROC addr = GetProcAddress(modules[i], symbol);
        if (addr)
            return reinterpret_cast<uintptr_t>(addr);
    }

    return std::nullopt;
}
#else
std::optional<uintptr_t> PatternScanner::resolveSymbol(const char* symbol) {
    if (!load()) return std::nullopt;

    // Use dl_iterate_phdr to look through loaded objects, then dlsym.
    // For mangled C++ names, we fall back to a linear scan of the symbol
    // table of each loaded object.
    void* h = dlopen(nullptr, RTLD_NOW);
    if (!h) return std::nullopt;

    void* addr = dlsym(h, symbol);
    if (addr) {
        dlclose(h);
        return reinterpret_cast<uintptr_t>(addr);
    }

    // Try each loaded object explicitly.
    std::ifstream maps("/proc/self/maps");
    std::string line;
    while (std::getline(maps, line)) {
        char path[512] = {0};
        uintptr_t s = 0, e = 0;
        if (sscanf(line.c_str(), "%lx-%lx %*s %*x %*s %*u %511s",
                   &s, &e, path) < 3) continue;
        if (!*path || path[0] == '[') continue;

        void* oh = dlopen(path, RTLD_NOW | RTLD_NOLOAD);
        if (!oh) continue;

        void* a = dlsym(oh, symbol);
        dlclose(oh);
        if (a) {
            dlclose(h);
            return reinterpret_cast<uintptr_t>(a);
        }
    }

    dlclose(h);
    return std::nullopt;
}
#endif

std::optional<uintptr_t> PatternScanner::findPattern(
    std::string_view pattern,
    uintptr_t start,
    uintptr_t end)
{
    if (!load()) return std::nullopt;

    if (start == 0) start = baseAddr;
    if (end == 0)   end = codeEnd;

    size_t patternLen = pattern.length();
    if (patternLen == 0) return std::nullopt;

    const unsigned char* code = reinterpret_cast<const unsigned char*>(start);
    size_t size = end - start;

    for (size_t i = 0; i < size - patternLen + 1; ++i) {
        bool found = true;
        for (size_t j = 0; j < patternLen; ++j) {
            if (pattern[j] != '?' && code[i + j] != static_cast<unsigned char>(pattern[j])) {
                found = false;
                break;
            }
        }
        if (found)
            return start + i;
    }

    return std::nullopt;
}

} // namespace titanium
