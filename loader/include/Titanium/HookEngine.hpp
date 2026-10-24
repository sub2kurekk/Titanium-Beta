#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace titanium {

struct Hook {
    void* target;
    void* detour;
    void* trampoline;
    std::vector<uint8_t> originalBytes;
    bool active;
};

class HookEngine {
public:
    static HookEngine& get();

    bool installHook(void* target, void* detour, Hook* out = nullptr);
    bool removeHook(void* target);
    bool removeHook(Hook* hook);

    void* createTrampoline(void* target, int numPrefixBytes = 0);

private:
    HookEngine() = default;
    ~HookEngine();

    bool applyDetour(Hook& hook);
    bool restoreOriginal(Hook& hook);

    std::unordered_map<void*, Hook> hooks;
    std::mutex mutex;
};

} // namespace titanium
