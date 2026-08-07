#ifndef STRTB_VERSION_H
#define STRTB_VERSION_H

#include <string>

/* NOTE: In semantic versioning, a major version_t increment indicates incompatible API changes.
 * However, I'm making an exception for STRTB while it's in alpha, due to continuous changes.
 * The major version_t will stay at 0 during the alpha phase, despite API incompatibilities.
 * When it leaves the alpha phase, the usual rules for semantic versioning should be followed.
 */

namespace strtb {

struct version_t {
    int major, minor, patch;
    const char* phase;
};

version_t get_libstrtb_version();
const std::string& get_libstrtb_version_string();

bool versions_equal(version_t a, version_t b);

}

#endif // STRTB_VERSION_H
