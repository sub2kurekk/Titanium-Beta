#include <Titanium/Mod.hpp>
#include <Titanium/Log.hpp>
#include <cstdio>

class ExampleMod : public titanium::Mod {
public:
    titanium::ModInfo getInfo() const override {
        return {
            .id = "example",
            .name = "Example Mod",
            .version = "1.0.0",
            .author = "TitaniumLoader",
            .description = "An example mod for TitaniumLoader",
        };
    }

    void onLoad() override {
        TITANIUM_MOD_LOG(Info, "Example mod loaded! Hello from TitaniumLoader!");
    }

    void onUnload() override {
        TITANIUM_MOD_LOG(Info, "Example mod unloaded!");
    }
};

TITANIUM_MOD(ExampleMod)
