#ifdef _WIN32

#include <Titanium/PatternScan.hpp>
#include <Titanium/HookEngine.hpp>
#include <Titanium/Loader.hpp>
#include <Log.hpp>
#include <cstdio>
#include <cstring>
#include <atomic>
#include <windows.h>

namespace titanium {

static std::atomic<bool> g_gdInitialized{false};

static void* gdSwapBuffersAddr = nullptr;
static void* gdApplicationRunAddr = nullptr;

void onGdInit() {
    if (g_gdInitialized.exchange(true))
        return;

    logMsg("[Titanium] GD initialized! Loading mods...");
    Loader::get().loadMods();
}

using SwapBuffersFn = void(*)(void*);
using ApplicationRunFn = void(*)(void*);

static SwapBuffersFn originalSwapBuffers = nullptr;
static ApplicationRunFn originalApplicationRun = nullptr;

void swapBuffersDetour(void* self) {
    onGdInit();
    if (originalSwapBuffers)
        originalSwapBuffers(self);
}

void applicationRunDetour(void* self) {
    onGdInit();
    if (originalApplicationRun)
        originalApplicationRun(self);
}

bool detectGdInit() {
    auto& scanner = PatternScanner::get();

    auto swapAddr = scanner.resolveSymbol("?swapBuffers@CCEGLView@cocos2d@@QEAAXXZ");
    auto runAddr  = scanner.resolveSymbol("?run@CCApplication@cocos2d@@QEAAHXZ");

    if (swapAddr)
        gdSwapBuffersAddr = reinterpret_cast<void*>(*swapAddr);
    if (runAddr)
        gdApplicationRunAddr = reinterpret_cast<void*>(*runAddr);

    if (!gdSwapBuffersAddr && !gdApplicationRunAddr) {
        swapAddr = scanner.resolveSymbol("_ZN7cocos2d11CCEGLView12swapBuffersEv");
        runAddr  = scanner.resolveSymbol("_ZN7cocos2d13CCApplication3runEv");
        if (swapAddr)
            gdSwapBuffersAddr = reinterpret_cast<void*>(*swapAddr);
        if (runAddr)
            gdApplicationRunAddr = reinterpret_cast<void*>(*runAddr);
    }

    if (!gdSwapBuffersAddr && !gdApplicationRunAddr) {
        logMsg("[Titanium] Could not find GD init functions");
        onGdInit();
        return false;
    }

    auto& engine = HookEngine::get();

    if (gdSwapBuffersAddr) {
        Hook hook;
        if (engine.installHook(gdSwapBuffersAddr, (void*)swapBuffersDetour, &hook)) {
            originalSwapBuffers = (SwapBuffersFn)hook.trampoline;
            logMsg("[Titanium] Hooked CCEGLView::swapBuffers");
        }
    }

    if (gdApplicationRunAddr && !originalSwapBuffers) {
        Hook hook;
        if (engine.installHook(gdApplicationRunAddr, (void*)applicationRunDetour, &hook)) {
            originalApplicationRun = (ApplicationRunFn)hook.trampoline;
            logMsg("[Titanium] Hooked CCApplication::run");
        }
    }

    return true;
}

} // namespace titanium

#endif // _WIN32
