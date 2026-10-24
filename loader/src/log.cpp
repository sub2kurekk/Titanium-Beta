#include <cstdio>
#include <cstdarg>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <cstdlib>
#endif

namespace titanium {

static const char* getLogPath() {
    static char path[1024];
    static bool resolved = false;
    if (!resolved) {
#ifdef _WIN32
        char tmp[MAX_PATH];
        DWORD len = GetEnvironmentVariableA("TEMP", tmp, MAX_PATH);
        if (len == 0)
            len = GetEnvironmentVariableA("TMP", tmp, MAX_PATH);
        if (len > 0) {
            snprintf(path, sizeof(path), "%s\\titaniumloader.log", tmp);
        } else {
            strcpy(path, "C:\\titaniumloader.log");
        }
#else
        const char* tmp = getenv("TMPDIR");
        if (!tmp || !*tmp) tmp = getenv("TEMP");
        if (!tmp || !*tmp) tmp = "/tmp";
        snprintf(path, sizeof(path), "%s/titaniumloader.log", tmp);
#endif
        resolved = true;
    }
    return path;
}

void logMsg(const char* fmt, ...) {
    char buf[1024];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

#ifdef _WIN32
    HANDLE hFile = CreateFileA(getLogPath(), GENERIC_WRITE,
        FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        SetFilePointer(hFile, 0, nullptr, FILE_END);
        DWORD written;
        WriteFile(hFile, buf, (DWORD)strlen(buf), &written, nullptr);
        WriteFile(hFile, "\n", 1, &written, nullptr);
        CloseHandle(hFile);
    }
#else
    FILE* f = fopen(getLogPath(), "a");
    if (f) {
        fprintf(f, "%s\n", buf);
        fclose(f);
    }
#endif
}

} // namespace titanium

#include <Titanium/Log.hpp>

extern "C" {

#ifdef _WIN32
__declspec(dllexport)
#endif
__attribute__((visibility("default")))
void titanium_log(int level, const char* mod, const char* msg) {
    titanium::logMsg("[Titanium][%s][%s] %s", titanium::logLevelName(static_cast<titanium::LogLevel>(level)), mod, msg);
}

}
