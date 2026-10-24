#pragma once
#include <cstdint>
#include <string_view>
#include <string>
#include <vector>
#include <memory>

namespace titanium {

struct LoadedMod {
    std::string_view id;
    std::string path;
    void* handle;
    std::unique_ptr<class Mod> instance;
    bool enabled;
};

struct TitaniumApi {
    bool (*hookFn)(void* target, void* detour, void* trampoline);
    bool (*unhookFn)(void* target);
    void* (*resolveAddr)(const char* symbol);
    void (*logFn)(int level, const char* mod, const char* msg);
};

class Loader {
public:
    static Loader& get();

    bool init();
    void loadMods();
    void unloadMods();

    const std::vector<LoadedMod>& getMods() const { return mods; }
    TitaniumApi getApi() const;

private:
    Loader() = default;
    ~Loader();

    std::vector<LoadedMod> mods;
    std::string modsDir;
};

} // namespace titanium
