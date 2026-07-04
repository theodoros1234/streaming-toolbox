#include "version.h"
#include <cstdio>
#include <cstring>

static char version_string[32];

namespace strtb {

version_t get_libstrtb_version() {
    return {
        .major = STRTB_SRC_VERSION_MAJOR,
        .minor = STRTB_SRC_VERSION_MINOR,
        .patch = STRTB_SRC_VERSION_PATCH,
        .phase = STRTB_SRC_VERSION_PHASE
    };
}

const char* get_libstrtb_version_string() {
    if (!version_string[0])
        std::snprintf(version_string, 32, "v%d.%d.%d-%s", STRTB_SRC_VERSION_MAJOR, STRTB_SRC_VERSION_MINOR, STRTB_SRC_VERSION_PATCH, STRTB_SRC_VERSION_PHASE);

    return version_string;
}

bool versions_equal(version_t a, version_t b) {
    return a.major == b.major && a.minor == b.minor && a.patch == b.patch && !std::strcmp(a.phase, b.phase);
}

}