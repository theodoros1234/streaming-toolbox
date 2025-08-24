#include "uri.h"

using namespace strtb::uri;

static const char to_hex[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

static inline unsigned char from_hex(char c) {
    if ('0' <= c && c <= '9')
        return c - '0';
    else if ('A' <= c && c <= 'F')
        return c - 'A' + 10;
    else if ('a' <= c && c <= 'f')
        return c - 'a' + 10;
    else
        return 255; // invalid
}

static inline bool is_alpha(char c) {
    return ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z');
}

static inline bool is_digit(char c) {
    return '0' <= c && c <= '9';
}

static inline bool is_unreserved(char c) {
    return is_alpha(c) || is_digit(c) ||
           c == '-' || c == '.' || c == '_' || c == '~';
}

static inline bool is_gen_delim(char c) {
    return c == ':' || c == '/' || c == '?' || c == '#' || c == '[' || c == ']' || c == '@';
}

static inline bool is_sub_delim(char c) {
    return c == '!' || c == '$' || c == '&' || c == '\''|| c == '(' || c == ')' ||
           c == '*' || c == '+' || c == ',' || c == ';' || c == '=';
}

// NOTE: percent encode/decode doesn't check for invalid UTF-8 sequences

std::string strtb::uri::percent_encode(const std::string& from, bool plus_space) {
    std::string to;
    to.reserve(from.size() + from.size()/2);
    for (auto i : from) {
        // unreserved characters: https://datatracker.ietf.org/doc/html/rfc3986#section-2.3
        if (is_unreserved(i)) {
            to.push_back(i);
        } else if (plus_space && i == ' ') {
            // HTTP sometimes encodes spaces as a plus character
            to.push_back('+');
        } else {
            // percent-encode
            to.push_back('%');
            to.push_back(to_hex[((unsigned char) i) / 16]);
            to.push_back(to_hex[((unsigned char) i) % 16]);
        }
    }
    return to;
}

std::pair<std::string, ssize_t> strtb::uri::percent_decode(const std::string& from, bool plus_space) {
    std::string to;
    to.reserve(from.size());
    for (size_t i=0; i<from.size(); i++) {
        char c = from[i];
        if (c == '%') {
            // decode percentage encoded char
            if (i+2 >= from.size())
                return {"incomplete escape code", from.size()};
            unsigned char h1 = from_hex(from[i+1]);
            unsigned char h2 = from_hex(from[i+2]);
            if (h1 == 255)
                return {"invalid hex digit", i+1};
            if (h2 == 255)
                return {"invalid hex digit", i+2};
            to.push_back(16 * h1 + h2);
            i += 2;
        } else if (plus_space && c == '+') {
            // HTTP sometimes encodes spaces as a plus character
            to.push_back(' ');
        } else {
            // no decoding needed
            to.push_back(c);
        }
    }
    if (to.size() <= from.size()/2)
        to.shrink_to_fit();
    // .second == -1 means no error, != -1 means error at that pos
    return {std::move(to), -1};
}

void parser::clear() {
    scheme_from = 0, scheme_to = 0;
    authority_from = 0, authority_to = 0;
    userinfo_from = 0, userinfo_to = 0;
    host_from = 0, host_to = 0;
    port_from = 0, port_to = 0;
    path_from = 0, path_to = 0;
    query_from = 0, query_to = 0;
    fragment_from = 0, fragment_to = 0;
}

void parser::clear_scheme() {
    scheme_from = 0, scheme_to = 0;
}

parser::parse_ret parse_scheme(const std::string& str, size_t from, size_t to) {

}

ssize_t parser::parse(const std::string& uri_str) {
    /* WARNING: even though user info is supported, it is considered depcecated by the RFC
     *          and should be rejected by the caller of this function if it's present   */
    clear();

    enum {
        SCHEME,
        AUTHORITY,
        PATH,
        QUERY,
        FRAGMENT
    } state = SCHEME;

    size_t size = uri_str.size();
    ssize_t userinfo_delim_pos = -1;
    for (size_t i=0; i<size; i++) {
        char c = uri_str[i];

        switch (state) {
        case SCHEME:
            if (i == 0) {
                // first char must be alpha
                if (!is_alpha(c))
                    return i;
            } else {
                if (c == ':') {
                    // scheme ends with ://
                    scheme_to = i;

                    // check if URI ends too soon
                    if (i + 2 >= size)
                        return size;
                    // check if separator is wrong
                    if (uri_str[i+1] != '/')
                        return i+1;
                    if (uri_str[i+2] != '/')
                        return i+2;

                    state = AUTHORITY;
                    authority_from = i+3;
                    i+=2;
                } else if (!(is_alpha(c) || is_digit(c) || c == '+' || c == '-' || c == '.')) {
                    // not allowed chars in scheme
                    return i;
                }
            }
            break;

        case AUTHORITY:
            if (c == '@') {     // @ delim separates userinfo from host and port
                if (userinfo_delim_pos != -1)   // more than one @ delim
                    return i;

                userinfo_delim_pos = i;
                userinfo_from = authority_from;
                userinfo_to = i;
                host_from = i+1;
            } else if (c == '/' || c == '?' || c == '#') {
                authority_to = i;
                if (userinfo_delim_pos == -1)   // no userinfo => host starts where authority starts
                    host_from = authority_from;
                host_to = i;
                // NOTE: host details and port are ignored, they will be scanned for later

                // select next part based on character
                switch (c) {
                case '/':
                    state = PATH;
                    path_from = i;
                    break;

                case '?':
                    state = QUERY;
                    query_from = i;
                    break;

                case '#':
                    state = FRAGMENT;
                    fragment_from = i;
                    break;
                }
            } else if (!(is_unreserved(c) || c == '%' || is_sub_delim(c) || c == ':' ||
                         c == '[' || c == ']')) {   // if not an allowed character
                // NOTE: there are additional constraints for some characters, they will be scanned for later
                return i;
            }
            break;

        case PATH:

            break;
        }
    }

    // Check state when URI ends
    switch (state) {
    case SCHEME:    // incomplete URI
        return size;

    case AUTHORITY:
        authority_to = size;
        if (userinfo_delim_pos == -1)   // no userinfo => host starts where authority starts
            host_from = authority_from;
        host_to = size;
    }

    // Analyze authority components in more detail

    return -1;
}
