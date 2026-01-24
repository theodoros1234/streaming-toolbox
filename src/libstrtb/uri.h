#ifndef STRTB_URI_H
#define STRTB_URI_H

#include <string>
#include <utility>
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
typedef enum {
    WEB_URL_ERROR,      // failed to parse
    WEB_URL_UNLIKELY,   // successfully parsed, but is almost certainly by mistake
    WEB_URL_POSSIBLE,   // can be accepted on dedicated and trusted URL input fields
    WEB_URL_LIKELY      // can be accepted in chat messages and other viewer-generated text
} web_url_confidence;
typedef std::pair<size_t, bool> parser_ret;  // .first: ends at, .second: is valid
typedef std::pair<web_url_confidence, bool> web_url_ret;    // .second = true if suffix, false if full URL

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
    parser_ret parse_uri_suffix(const std::string& str);
    parser_ret parse_relative_ref(const std::string& str);
    parser_ret parse_authority(const std::string& str);
    parser_ret parse_host(const std::string& str);

    parser_ret parse_uri(const std::string& str, size_t from, size_t to);
    parser_ret parse_uri_suffix(const std::string& str, size_t from, size_t to);
    parser_ret parse_relative_ref(const std::string& str, size_t from, size_t to);
    parser_ret parse_authority(const std::string& str, size_t from, size_t to);
    parser_ret parse_host(const std::string& str, size_t from, size_t to);

    parser_ret parse_uri(const char* str, size_t from, size_t to);
    parser_ret parse_uri_suffix(const char* str, size_t from, size_t to);
    parser_ret parse_relative_ref(const char* str, size_t from, size_t to);
    parser_ret parse_authority(const char* str, size_t from, size_t to);
    parser_ret parse_host(const char* str, size_t from, size_t to);

    web_url_ret is_web_url(const std::string& str);
    web_url_ret is_web_url(const std::string& str, size_t from, size_t to);
    web_url_ret is_web_url(const char* str, size_t from, size_t to);

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

    std::string scheme_str(const char* str, bool fix_case = false) const;
    std::string authority_str(const char* str) const;
    std::string userinfo_str(const char* str) const;
    std::string host_str(const char* str, bool fix_case = false) const;
    std::string port_str(const char* str) const;
    int port_uint16(const char* str) const;  // returns -1 when out of range or not specified
    std::string path_str(const char* str) const;
    std::string query_str(const char* str) const;
    std::string fragment_str(const char* str) const;
};

std::string percent_encode(const std::string& str);     // like encodeURIComponent()
std::string percent_encode_limited(const std::string& str);     // like encodeURI()
std::pair<std::string, ssize_t> percent_decode(const std::string& str);     // like decodeURIComponent()

std::string percent_encode(const std::string& str, size_t from, size_t to);
std::string percent_encode_limited(const std::string& str, size_t from, size_t to);
std::pair<std::string, ssize_t> percent_decode(const std::string& str, size_t from, size_t to);

extern const char known_tlds_default[];
extern const size_t known_tlds_default_length;
void known_tlds_load_str(const char* str, size_t len);
bool is_known_tld(const char* str, size_t from, size_t to);     // case-insensitive
bool is_known_tld(const std::string& str, size_t from, size_t to);
bool is_known_tld(const std::string& str);

}

#endif // STRTB_URI_H
