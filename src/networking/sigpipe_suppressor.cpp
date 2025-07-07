#include "sigpipe_suppressor.h"

/* Used to suppress SIGPIPE in cases where we can't set the MSG_NOSIGNAL flag (e.g. with SSL/TLS sockets).
 * Solution done using help from this stackoverflow answer:
 * https://stackoverflow.com/a/77688810/4983207 */

using namespace strtb::networking;

// Only do this for non-windows systems
#ifndef _WIN32

sigpipe_suppressor::sigpipe_suppressor() {
    // Check if SIGPIPE is already blocked, which means we don't need to change it
    sigset_t blocked;
    sigemptyset(&blocked);
    pthread_sigmask(SIG_BLOCK, NULL, &blocked);
    needs_block = !sigismember(&blocked, SIGPIPE);

    if (needs_block) {
        // Prepare SIGPIPE set
        sigemptyset(&sigpipe_set);
        sigaddset(&sigpipe_set, SIGPIPE);
        // Then, block it for this thread
        pthread_sigmask(SIG_BLOCK, &sigpipe_set, NULL);
    }
}

sigpipe_suppressor::~sigpipe_suppressor() {
    if (needs_block) {
        // Consume any SIGPIPE signal that's pending
        sigset_t pending;
        sigemptyset(&pending);
        sigpending(&pending);
        if (sigismember(&pending, SIGPIPE)) {
            int returned_signal;
            sigwait(&sigpipe_set, &returned_signal);
        }

        // Unblock SIGPIPE
        pthread_sigmask(SIG_UNBLOCK, &sigpipe_set, NULL);
    }
}

#endif
