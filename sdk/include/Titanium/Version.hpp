#pragma once

namespace titanium {

inline constexpr unsigned VersionMajor = 0;
inline constexpr unsigned VersionMinor = 1;
inline constexpr unsigned VersionPatch = 0;

inline constexpr const char* VersionString = "0.1.0";

struct Version {
    unsigned major;
    unsigned minor;
    unsigned patch;

    constexpr bool operator>=(const Version& other) const {
        if (major != other.major) return major > other.major;
        if (minor != other.minor) return minor > other.minor;
        return patch >= other.patch;
    }
};

inline constexpr Version LoaderVersion = { VersionMajor, VersionMinor, VersionPatch };

} // namespace titanium
