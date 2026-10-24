#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

static std::string findGdBinary() {
    const char* paths[] = {
        "/usr/bin/GeometryDash",
        "/usr/local/bin/GeometryDash",
        "/opt/GeometryDash/GeometryDash",
        "/home/",  // Steam: ~/.steam/steam/steamapps/common/GeometryDash/
        nullptr
    };

    for (int i = 0; paths[i]; ++i) {
        if (access(paths[i], F_OK) == 0)
            return paths[i];
    }

    const char* home = getenv("HOME");
    if (home) {
        std::string steamPath = std::string(home)
            + "/.steam/steam/steamapps/common/Geometry Dash/GeometryDash";
        if (access(steamPath.c_str(), F_OK) == 0)
            return steamPath;

        steamPath = std::string(home)
            + "/.local/share/Steam/steamapps/common/Geometry Dash/GeometryDash";
        if (access(steamPath.c_str(), F_OK) == 0)
            return steamPath;
    }

    return "";
}

static std::string findLoaderLib() {
    const char* paths[] = {
        "/usr/lib/titaniumloader/libtitaniumloader.so",
        "/usr/local/lib/titaniumloader/libtitaniumloader.so",
        "/opt/titaniumloader/libtitaniumloader.so",
        "./loader/libtitaniumloader.so",
        nullptr
    };

    for (int i = 0; paths[i]; ++i) {
        if (access(paths[i], F_OK) == 0)
            return paths[i];
    }

    return "";
}

static void printUsage(const char* prog) {
    fprintf(stderr, "Usage: %s [options]\n", prog);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --gd-path <path>  Path to Geometry Dash binary\n");
    fprintf(stderr, "  --loader <path>   Path to libtitaniumloader.so\n");
    fprintf(stderr, "  --help            Show this help\n");
}

int main(int argc, char* argv[]) {
    std::string gdPath = findGdBinary();
    std::string loaderPath = findLoaderLib();

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--gd-path") == 0 && i + 1 < argc) {
            gdPath = argv[++i];
        } else if (strcmp(argv[i], "--loader") == 0 && i + 1 < argc) {
            loaderPath = argv[++i];
        }
    }

    if (gdPath.empty()) {
        fprintf(stderr, "Error: Could not find Geometry Dash binary.\n");
        fprintf(stderr, "Specify with --gd-path or install GD.\n");
        return 1;
    }

    if (loaderPath.empty()) {
        fprintf(stderr, "Error: Could not find libtitaniumloader.so.\n");
        fprintf(stderr, "Specify with --loader or install titaniumloader.\n");
        return 1;
    }

    fprintf(stderr, "[Titanium] Launching: %s\n", gdPath.c_str());
    fprintf(stderr, "[Titanium] Loader: %s\n", loaderPath.c_str());

    setenv("LD_PRELOAD", loaderPath.c_str(), 1);
    setenv("TITANIUM_LOADER", "1", 1);

    execvp(gdPath.c_str(), &argv[1]);

    fprintf(stderr, "Error: Failed to launch GD: %s\n", strerror(errno));
    return 1;
}
