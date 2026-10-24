#include <Titanium/ModLoader.hpp>
#include <Titanium/Mod.hpp>
#include <Log.hpp>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <dlfcn.h>
#include <sys/stat.h>
#endif

namespace titanium {

static std::string getModsDirectory() {
#ifdef _WIN32
    char path[MAX_PATH];
    DWORD len = GetEnvironmentVariableA("USERPROFILE", path, MAX_PATH);
    if (len == 0)
        GetEnvironmentVariableA("HOME", path, MAX_PATH);
    return std::string(path) + "\\.titaniumloader\\mods";
#else
    const char* home = getenv("HOME");
    if (!home) home = "/tmp";
    return std::string(home) + "/.titaniumloader/mods";
#endif
}

ModLoader& ModLoader::get() {
    static ModLoader instance;
    return instance;
}

bool ModLoader::parseMetadata(const std::string& path, std::string& id) {
    auto dirEnd = path.find_last_of("\\/");
    if (dirEnd == std::string::npos) return false;

    std::string dir = path.substr(0, dirEnd);
    std::string metaPath = dir + "/mod.json";
#ifdef _WIN32
    // Try both separators for the metadata file.
    if (FILE* f = fopen((dir + "\\mod.json").c_str(), "r")) {
        fclose(f);
        metaPath = dir + "\\mod.json";
    }
#endif

    FILE* f = fopen(metaPath.c_str(), "r");
    if (!f) return false;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char* pos = strstr(line, "\"id\"");
        if (pos) {
            char* colon = strchr(pos, ':');
            if (colon) {
                char* start = strchr(colon + 1, '"');
                if (start) {
                    char* end = strchr(start + 1, '"');
                    if (end) {
                        *end = '\0';
                        id = start + 1;
                        fclose(f);
                        return true;
                    }
                }
            }
        }
    }

    fclose(f);
    return false;
}

#ifdef _WIN32
static std::string fileExt(const std::string& path) {
    auto dot = path.find_last_of('.');
    if (dot == std::string::npos) return "";
    std::string ext = path.substr(dot);
    for (auto& c : ext) c = (char)tolower((unsigned char)c);
    return ext;
}
#else
static void* loadSharedLib(const std::string& path) {
    return dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
}
static void unloadSharedLib(void* h) {
    if (h) dlclose((void*)h);
}
static void* getSym(void* h, const char* name) {
    return dlsym(h, name);
}
static std::string dlError() { return dlerror(); }
#endif

bool ModLoader::loadMod(const std::string& path) {
    std::string id;
    if (!parseMetadata(path, id)) {
        // Fall back to filename without extension. If there's no extension, use
        // the full filename. A missing lastDot would underflow the substr length
        // (e.g. "my.titanium" -> "m"), so guard it.
        auto lastSlash = path.find_last_of("\\/");
        auto lastDot   = path.find_last_of('.');
        const auto stemEnd = (lastSlash == std::string::npos) ? 0 : lastSlash + 1;
        const auto extStart = (lastDot == std::string::npos || lastDot < stemEnd)
                                  ? std::string::npos
                                  : lastDot;
        id = path.substr(stemEnd, extStart - stemEnd);
    }
    logMsg("[Titanium] Resolved mod id='%s' for path='%s'", id.c_str(), path.c_str());

    for (auto& m : mods) {
        if (m.id == id) {
            logMsg("[Titanium] Mod '%s' already loaded", id.c_str());
            return false;
        }
    }

#ifdef _WIN32
    HMODULE handle = LoadLibraryA(path.c_str());
    if (!handle) {
        logMsg("[Titanium] Failed to load mod '%s': %lu", path.c_str(), GetLastError());
        return false;
    }

    auto initFn = reinterpret_cast<Mod* (*)()>(GetProcAddress(handle, "titaniumModInit"));
    if (!initFn) {
        logMsg("[Titanium] Mod '%s' has no titaniumModInit", path.c_str());
        FreeLibrary(handle);
        return false;
    }
#else
    void* handle = loadSharedLib(path);
    if (!handle) {
        logMsg("[Titanium] Failed to load mod '%s': %s", path.c_str(), dlError().c_str());
        return false;
    }

    using InitFn = Mod* (*)();
    auto initFn = reinterpret_cast<InitFn>(getSym(handle, "titaniumModInit"));
    if (!initFn) {
        logMsg("[Titanium] Mod '%s' has no titaniumModInit: %s", path.c_str(), dlError().c_str());
        unloadSharedLib(handle);
        return false;
    }
#endif

    Mod* mod = initFn();
    if (!mod) {
#ifdef _WIN32
        FreeLibrary(handle);
#else
        unloadSharedLib(handle);
#endif
        return false;
    }

    ModEntry entry;
    entry.id = id;
    entry.path = path;
    entry.handle = handle;
    entry.instance.reset(mod);
    entry.enabled = true;

    // Fire the mod's onLoad() callback now that it's registered.
    if (entry.instance) {
        entry.instance->onLoad();
    }

    mods.push_back(std::move(entry));

    logMsg("[Titanium] Loaded mod: %s (%s)", id.c_str(), path.c_str());

    return true;
}
bool ModLoader::unloadMod(const std::string& id) {
    for (auto it = mods.begin(); it != mods.end(); ++it) {
        if (it->id == id) {
            it->instance->onUnload();
            it->instance.reset();

#ifdef _WIN32
            auto exitFn = reinterpret_cast<void(*)(Mod*)>(GetProcAddress(
                (HMODULE)it->handle, "titaniumModExit"));
            if (exitFn) exitFn(nullptr);
            FreeLibrary((HMODULE)it->handle);
#else
            using ExitFn = void(*)(Mod*);
            auto exitFn = reinterpret_cast<ExitFn>(getSym(it->handle, "titaniumModExit"));
            if (exitFn) exitFn(nullptr);
            unloadSharedLib(it->handle);
#endif

            logMsg("[Titanium] Unloaded mod: %s", id.c_str());

            mods.erase(it);
            return true;
        }
    }
    return false;
}

std::vector<std::string> ModLoader::discoverMods(const std::string& directory) {
    std::vector<std::string> found;

#ifdef _WIN32
    std::string search = directory + "\\*.dll";
    logMsg("[Titanium] Scanning mods dir: %s", search.c_str());
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(search.c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
        logMsg("[Titanium] FindFirstFileA failed (err=%lu)", GetLastError());
        return found;
    }

    do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            std::string fullPath = directory + "\\" + findData.cFileName;
            logMsg("[Titanium] Found mod file: %s", fullPath.c_str());
            found.push_back(fullPath);
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
#else
    DIR* d = opendir(directory.c_str());
    if (!d) return found;

    struct dirent* ent;
    while ((ent = readdir(d)) != nullptr) {
        if (ent->d_name[0] == '.') continue;
        std::string name = ent->d_name;
        // Accept .so and .titanium files as mods on Linux.
        if (name.size() < 4) continue;
        std::string ext = name.substr(name.size() - 3);
        for (auto& c : ext) c = (char)tolower((unsigned char)c);
        if (ext != ".so" && (name.size() < 8 || name.substr(name.size() - 6) != ".titanium")) {
            continue;
        }
        found.push_back(directory + "/" + name);
    }
    closedir(d);
#endif
    return found;
}

void ModLoader::enableMod(const std::string& id) {
    for (auto& m : mods) {
        if (m.id == id) {
            m.enabled = true;
            m.instance->onLoad();
            break;
        }
    }
}

void ModLoader::disableMod(const std::string& id) {
    for (auto& m : mods) {
        if (m.id == id) {
            m.instance->onUnload();
            m.enabled = false;
            break;
        }
    }
}

} // namespace titanium
