#ifndef STRTB_COMMON_VERSION_H
#define STRTB_COMMON_VERSION_H

/* NOTE: In semantic versioning, a major version increment indicates incompatible API changes.
 * However, I'm making an exception for STRTB while it's in alpha, due to continuous changes.
 * The major version will stay at 0 during the alpha phase, despite API incompatibilities.
 * When it leaves the alpha phase, the usual rules for semantic versioning should be followed.
 */

namespace strtb::common {

struct version {
    int major, minor, patch;
    const char* phase;
};

version get_libstrtb_version();
const char* get_libstrtb_version_string();

bool versions_equal(version a, version b);

}

#endif // STRTB_COMMON_VERSION_H
