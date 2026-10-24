#include <windows.h>
#include <cstdio>

extern bool titaniumInit();
extern void titaniumShutdown();

static HMODULE g_originalPthread = nullptr;
static bool g_initialized = false;

static const char* getLogPath() {
    static char path[MAX_PATH];
    static bool resolved = false;
    if (!resolved) {
        char tmp[MAX_PATH];
        DWORD len = GetEnvironmentVariableA("TEMP", tmp, MAX_PATH);
        if (len == 0)
            len = GetEnvironmentVariableA("TMP", tmp, MAX_PATH);
        if (len > 0) {
            snprintf(path, MAX_PATH, "%s\\titaniumloader.log", tmp);
        } else {
            strcpy(path, "C:\\titaniumloader.log");
        }
        resolved = true;
    }
    return path;
}

static void logMsg(const char* msg) {
    HANDLE hFile = CreateFileA(getLogPath(), GENERIC_WRITE,
        FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        SetFilePointer(hFile, 0, nullptr, FILE_END);
        DWORD written;
        WriteFile(hFile, msg, (DWORD)strlen(msg), &written, nullptr);
        WriteFile(hFile, "\n", 1, &written, nullptr);
        CloseHandle(hFile);
    }
}

static DWORD WINAPI titaniumInitThread(LPVOID) {
    Sleep(500);

    logMsg("[Titanium] Proxy DLL loaded, initializing...");

    if (!titaniumInit()) {
        logMsg("[Titanium] titaniumInit() FAILED");
        return 1;
    }

    g_initialized = true;
    logMsg("[Titanium] titaniumInit() OK");
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);

        char path[MAX_PATH];
        GetModuleFileNameA(hModule, path, MAX_PATH);

        const char* lastSlash = strrchr(path, '\\');
        size_t dirLen = lastSlash ? (lastSlash - path) : 0;

        char origPath[MAX_PATH];
        memcpy(origPath, path, dirLen);
        origPath[dirLen] = '\0';
        strcat(origPath, "\\pthreadVCE3_o.dll");

        g_originalPthread = LoadLibraryA(origPath);
        if (!g_originalPthread) {
            g_originalPthread = LoadLibraryA("pthreadVCE3_o.dll");
        }

        if (!g_originalPthread) {
            char buf[512];
            snprintf(buf, sizeof(buf), "[Titanium] Warning: Could not load pthreadVCE3_o.dll (error %lu), continuing anyway", GetLastError());
            logMsg(buf);
        }

        HANDLE hThread = CreateThread(nullptr, 0, titaniumInitThread, nullptr, 0, nullptr);
        if (hThread) CloseHandle(hThread);
    }
    else if (reason == DLL_PROCESS_DETACH) {
        if (g_initialized)
            titaniumShutdown();
        if (g_originalPthread)
            FreeLibrary(g_originalPthread);
    }

    return TRUE;
}
