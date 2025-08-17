#ifndef STRTB_NETWORKING_SIGPIPE_SUPPRESSOR_H
#define STRTB_NETWORKING_SIGPIPE_SUPPRESSOR_H

#include <csignal>      // IWYU pragma: keep

/* Used to suppress SIGPIPE in cases where we can't set the MSG_NOSIGNAL flag (e.g. with SSL/TLS sockets).
 * Solution done using help from this stackoverflow answer:
 * https://stackoverflow.com/a/77688810/4983207 */

namespace strtb::networking {

class sigpipe_suppressor {
    // Only do this for non-windows systems
#ifndef _WIN32
private:
    bool needs_block;
    sigset_t sigpipe_set;
public:
    sigpipe_suppressor();
    sigpipe_suppressor(const sigpipe_suppressor&) = delete;
    sigpipe_suppressor(sigpipe_suppressor&&) = delete;
    ~sigpipe_suppressor();
#endif
};

}

#endif // STRTB_NETWORKING_SIGPIPE_SUPPRESSOR_H
