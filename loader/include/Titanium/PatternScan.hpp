#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace titanium {

struct MemoryRegion {
    uintptr_t start;
    uintptr_t end;
    bool executable;
    bool readable;
    bool writable;
};

class PatternScanner {
public:
    static PatternScanner& get();

    bool load();
    void unload();

    std::optional<uintptr_t> findPattern(
        std::string_view pattern,
        uintptr_t start = 0,
        uintptr_t end = 0
    );

    std::optional<uintptr_t> resolveSymbol(const char* symbol);

    uintptr_t getBaseAddress() const { return baseAddr; }
    uintptr_t getCodeEnd() const { return codeEnd; }

private:
    PatternScanner() = default;

    std::vector<MemoryRegion> regions;
    uintptr_t baseAddr = 0;
    uintptr_t codeEnd = 0;
    std::string gdPath;

    bool findGdBinary();
    bool parseMaps();
    bool isIdSym(const char* name);
};

} // namespace titanium
