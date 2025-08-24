#ifndef STRTB_URI_H
#define STRTB_URI_H

#include <string>
#include <utility>

// RFC Specification: https://datatracker.ietf.org/doc/html/rfc3986

namespace strtb::uri {

class parser {
public:
    size_t scheme_from = 0, scheme_to = 0,
           authority_from = 0, authority_to = 0,
           userinfo_from = 0, userinfo_to = 0,
           host_from = 0, host_to = 0,
           port_from = 0, port_to = 0,
           path_from = 0, path_to = 0,
           query_from = 0, query_to = 0,
           fragment_from = 0, fragment_to = 0;

    // host_type enum    
    typedef std::pair<size_t, bool> parse_ret;  // .first: ends at, .second: is valid

    parse_ret parse_scheme(const std::string& str, size_t from, size_t to);
    ssize_t parse(const std::string& uri_str);

    void clear_scheme();
    void clear();
};

std::string percent_encode(const std::string& from, bool plus_space = false);
std::pair<std::string, ssize_t> percent_decode(const std::string& from, bool plus_space = false);

}

#endif // STRTB_URI_H
