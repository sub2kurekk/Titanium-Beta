#include <Titanium/Loader.hpp>
#include <Titanium/ModLoader.hpp>
#include <Titanium/HookEngine.hpp>
#include <Titanium/PatternScan.hpp>
#include <Titanium/ModMenu.hpp>
#include <Log.hpp>

#ifdef _WIN32
#include <windows.h>
#include <string>
#else
#include <cstdlib>
#include <sys/stat.h>
#include <sys/types.h>
#include <cerrno>
#include <cstring>
#include <string>
#endif

namespace titanium {

#ifdef _WIN32
extern bool detectGdInit();
#endif

Loader& Loader::get() {
    static Loader instance;
    return instance;
}

Loader::~Loader() {
    unloadMods();
}

bool Loader::init() {
#ifdef _WIN32
    char profile[MAX_PATH];
    DWORD len = GetEnvironmentVariableA("USERPROFILE", profile, MAX_PATH);
    if (len == 0)
        GetEnvironmentVariableA("HOME", profile, MAX_PATH);
    modsDir = std::string(profile) + "\\.titaniumloader\\mods";
    CreateDirectoryA(modsDir.c_str(), nullptr);
#else
    const char* profile = getenv("HOME");
    if (!profile) profile = "/tmp";
    modsDir = std::string(profile) + "/.titaniumloader/mods";

    // Best-effort mkdir -p for ~/.titaniumloader/mods.
    std::string titaniumDir = std::string(profile) + "/.titaniumloader";
    mkdir(titaniumDir.c_str(), 0755);
    mkdir(modsDir.c_str(), 0755);
#endif

    if (!PatternScanner::get().load()) {
        logMsg("[Titanium] PatternScanner failed");
        return false;
    }

#ifdef _WIN32
    detectGdInit();
    installModMenuHook();
#endif

    logMsg("[Titanium] Loader ready. Mods dir: %s", modsDir.c_str());

    return true;
}

void Loader::loadMods() {
    auto found = ModLoader::get().discoverMods(modsDir);
    for (auto& path : found) {
        ModLoader::get().loadMod(path);
    }

    logMsg("[Titanium] Loaded %zu mod(s)", found.size());
}

void Loader::unloadMods() {
    auto& loader = ModLoader::get();
    const auto& entries = loader.getMods();
    for (const auto& m : entries) {
        loader.unloadMod(m.id);
    }
}

TitaniumApi Loader::getApi() const {
    TitaniumApi api{};
    return api;
}

} // namespace titanium

bool titaniumInit() {
    return titanium::Loader::get().init();
}

void titaniumShutdown() {
    titanium::Loader::get().unloadMods();
}
