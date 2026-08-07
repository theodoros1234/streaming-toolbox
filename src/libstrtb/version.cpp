#include "version.h"
#include <string>
#include <cstring>

namespace strtb {

std::string make_libstrtb_version_string() {
    // TODO: possible issues with certain locales for std::to_string
    return 'v' + std::to_string(STRTB_SRC_VERSION_MAJOR) + '.'
               + std::to_string(STRTB_SRC_VERSION_MINOR) + '.'
               + std::to_string(STRTB_SRC_VERSION_PATCH) + '-'
               + STRTB_SRC_VERSION_PHASE;
}

static std::string version_string = make_libstrtb_version_string();

version_t get_libstrtb_version() {
    return {
        .major = STRTB_SRC_VERSION_MAJOR,
        .minor = STRTB_SRC_VERSION_MINOR,
        .patch = STRTB_SRC_VERSION_PATCH,
        .phase = STRTB_SRC_VERSION_PHASE
    };
}

const std::string& get_libstrtb_version_string() {
    return version_string;
}

bool versions_equal(version_t a, version_t b) {
    return a.major == b.major && a.minor == b.minor && a.patch == b.patch && !std::strcmp(a.phase, b.phase);
}

}