#include <Titanium/Loader.hpp>
#include <Titanium/Log.hpp>
#include <cstdio>
#include <dlfcn.h>
#include <unistd.h>

static bool g_titaniumInitialized = false;

__attribute__((constructor(101))) void titaniumEntry() {
    auto* env = getenv("TITANIUM_LOADER");
    if (!env || env[0] != '1') {
        return;
    }

    FILE* log = fopen("/tmp/titaniumloader.log", "w");
    if (log) {
        fprintf(log, "[Titanium] Loader v0.1.0\n");
        fprintf(log, "[Titanium] PID: %d\n", getpid());
        fprintf(log, "[Titanium] LD_PRELOAD detected, initializing...\n");
        fclose(log);
    }

    if (!titanium::Loader::get().init()) {
        if (log) {
            log = fopen("/tmp/titaniumloader.log", "a");
            fprintf(log, "[Titanium] FAILED to initialize loader!\n");
            fclose(log);
        }
        return;
    }

    g_titaniumInitialized = true;

    log = fopen("/tmp/titaniumloader.log", "a");
    if (log) {
        fprintf(log, "[Titanium] Loader initialized successfully\n");
        fclose(log);
    }
}

__attribute__((destructor)) void titaniumExit() {
    if (g_titaniumInitialized) {
        titanium::Loader::get().unloadMods();
    }
}
