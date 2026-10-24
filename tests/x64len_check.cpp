// Reads lines "<expected-length> <hex bytes>" from stdin (the bytes of one
// instruction followed by some of the bytes after it), runs the decoder on
// them and reports every disagreement. Used by check_x64len.sh.
#include "../loader/src/x64len.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

int main() {
    char line[512];
    unsigned long total = 0, same = 0, undecodable = 0, wrongLength = 0;
    int shown = 0;
    while (std::fgets(line, sizeof(line), stdin)) {
        unsigned expected = 0;
        int consumed = 0;
        if (std::sscanf(line, "%u %n", &expected, &consumed) < 1) continue;
        std::vector<uint8_t> bytes;
        for (const char* s = line + consumed; s[0] && s[1] && s[0] != '\n'; s += 2)
            bytes.push_back(static_cast<uint8_t>(std::strtoul(std::string(s, 2).c_str(), nullptr, 16)));

        titanium::x64::Insn in;
        ++total;
        if (!titanium::x64::decode(bytes.data(), bytes.size(), in)) {
            ++undecodable;
            if (shown++ < 15) { std::printf("UNDECODABLE: %s", line); }
        } else if (bytes[0] == 0x9B && in.length == 1) {
            // objdump fuses `fwait` with the x87 instruction that follows it; as a
            // hook boundary a lone 1-byte WAIT is equally correct.
            ++same;
        } else if (in.length != expected) {
            ++wrongLength;
            if (shown++ < 15) std::printf("WRONG LENGTH (got %u): %s", in.length, line);
        } else {
            ++same;
        }
    }
    std::printf("instructions: %lu | same boundary: %lu | undecodable: %lu | wrong length: %lu\n",
                total, same, undecodable, wrongLength);
    return wrongLength == 0 ? 0 : 1;
}
