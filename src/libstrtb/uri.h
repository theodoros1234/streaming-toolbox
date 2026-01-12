#ifndef STRTB_URI_H
#define STRTB_URI_H

#include <string>
#include <utility>
#include <set>
#include <string>

// RFC Specification: https://datatracker.ietf.org/doc/html/rfc3986

namespace strtb::uri {

inline bool is_alpha(char c) {
    return ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z');
}

inline bool is_digit(char c) {
    return '0' <= c && c <= '9';
}

inline bool is_unreserved(char c) {
    return is_alpha(c) || is_digit(c) ||
           c == '-' || c == '.' || c == '_' || c == '~';
}

inline bool is_gen_delim(char c) {
    return c == ':' || c == '/' || c == '?' || c == '#' || c == '[' || c == ']' || c == '@';
}

inline bool is_sub_delim(char c) {
    return c == '!' || c == '$' || c == '&' || c == '\''|| c == '(' || c == ')' ||
           c == '*' || c == '+' || c == ',' || c == ';' || c == '=';
}

inline bool is_pchar(char c, bool nc = false) {
    return is_unreserved(c) || c == '%' || is_sub_delim(c) || c == '@' || (!nc && c == ':');
}

typedef enum {HOST_EMPTY, HOST_REGNAME, HOST_IPV4, HOST_IPV6, HOST_IPVFUTURE} host_type_enum;
typedef enum {SUFFIX_ERROR, SUFFIX_UNLIKELY, SUFFIX_POSSIBLE_FILE,
              SUFFIX_POSSIBLE_WEBSITE, SUFFIX_LIKELY_WEBSITE} suffix_confidence;
typedef std::pair<size_t, bool> parser_ret;  // .first: ends at, .second: is valid
typedef std::pair<size_t, suffix_confidence> parser_ret_suffix;

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
    host_type_enum host_type = HOST_EMPTY;

    parser_ret parse_uri(const std::string& str);
    parser_ret_suffix parse_uri_suffix(const std::string& str);
    parser_ret parse_relative_ref(const std::string& str);
    parser_ret parse_authority(const std::string& str);
    parser_ret parse_host(const std::string& str);

    parser_ret parse_uri(const std::string& str, size_t from, size_t to);
    parser_ret_suffix parse_uri_suffix(const std::string& str, size_t from, size_t to);
    parser_ret parse_relative_ref(const std::string& str, size_t from, size_t to);
    parser_ret parse_authority(const std::string& str, size_t from, size_t to);
    parser_ret parse_host(const std::string& str, size_t from, size_t to);

    void clear_uri();
    void clear_authority();
    void clear_host();

    std::string scheme_str(const std::string& str, bool fix_case = false) const;
    std::string authority_str(const std::string& str) const;
    std::string userinfo_str(const std::string& str) const;
    std::string host_str(const std::string& str, bool fix_case = false) const;
    std::string port_str(const std::string& str) const;
    int port_uint16(const std::string& str) const;  // returns -1 when out of range or not specified
    std::string path_str(const std::string& str) const;
    std::string query_str(const std::string& str) const;
    std::string fragment_str(const std::string& str) const;
};

std::string percent_encode(const std::string& str);     // like encodeURIComponent()
std::string percent_encode_limited(const std::string& str);     // like encodeURI()
std::pair<std::string, ssize_t> percent_decode(const std::string& str);     // like decodeURIComponent()

std::string percent_encode(const std::string& str, size_t from, size_t to);
std::string percent_encode_limited(const std::string& str, size_t from, size_t to);
std::pair<std::string, ssize_t> percent_decode(const std::string& str, size_t from, size_t to);

extern const std::set<std::string> known_tlds;
bool is_known_tld(const std::string& str);

}

#endif // STRTB_URI_H
