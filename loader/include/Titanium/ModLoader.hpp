#pragma once
#include <string_view>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <Titanium/Mod.hpp>

namespace titanium {

class ModLoader {
public:
    using ModFactory = std::unique_ptr<Mod>(*)();

    struct ModEntry {
        std::string id;
        std::string path;
        void* handle;
        std::unique_ptr<Mod> instance;
        bool enabled;
    };

    static ModLoader& get();

    bool loadMod(const std::string& path);
    bool unloadMod(const std::string& id);
    std::vector<std::string> discoverMods(const std::string& directory);

    void enableMod(const std::string& id);
    void disableMod(const std::string& id);

    const std::vector<ModEntry>& getMods() const { return mods; }

private:
    ModLoader() = default;

    std::vector<ModEntry> mods;

    bool parseMetadata(const std::string& path, std::string& id);
};

} // namespace titanium
