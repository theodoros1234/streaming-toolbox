#ifndef VERSION_H
#define VERSION_H

#define STRTB_SRC_VERSION_MAJOR 0
#define STRTB_SRC_VERSION_MINOR 2
#define STRTB_SRC_VERSION_PATCH 2
#define STRTB_SRC_VERSION_PHASE "alpha"

namespace strtb::common {

struct version {
    int major, minor, patch;
    const char* phase;
};

version get_libstrtb_version();
const char* get_libstrtb_version_string();

}

#endif // VERSION_H
