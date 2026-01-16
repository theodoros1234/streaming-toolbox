#include "uri.h"
#include <stdexcept>
#include <cassert>
#include <cstdint>
#include <vector>
#include "common/strescape.h"
#include "logging/logging.h"

using namespace strtb::uri;

static strtb::logging::source log("URI");

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

// makes sure the range params of parser functions is in string's bounds
static inline void verify_range(const std::string& str, size_t from, size_t to) {
    if (from > str.size())
        throw std::out_of_range("'from' is out of range");
    if (to > str.size())
        throw std::out_of_range("'to' is out of range");
    if (from > to)
        throw std::out_of_range("'from' is bigger than 'to'");
}

// NOTE: percent encode/decode doesn't check for invalid UTF-8 sequences

std::string strtb::uri::percent_encode(const std::string& str) {
    return percent_encode(str, 0, str.length());
}

std::string strtb::uri::percent_encode_limited(const std::string& str) {
    return percent_encode_limited(str, 0, str.length());
}

std::pair<std::string, ssize_t> strtb::uri::percent_decode(const std::string& str) {
    return percent_decode(str, 0, str.length());
}

std::string strtb::uri::percent_encode(const std::string& str, size_t from, size_t to) {
    verify_range(str, from, to);
    std::string encoded;
    encoded.reserve(str.size() + str.size()/2);
    for (auto i = str.begin() + from; i < str.begin() + to; i++) {
        char c = *i;
        // unreserved characters: https://datatracker.ietf.org/doc/html/rfc3986#section-2.3
        if (is_unreserved(c)) {
            encoded.push_back(c);
        } else {
            // percent-encode
            encoded.push_back('%');
            encoded.push_back(to_hex[((unsigned char) c) / 16]);
            encoded.push_back(to_hex[((unsigned char) c) % 16]);
        }
    }
    return encoded;
}

std::string strtb::uri::percent_encode_limited(const std::string& str, size_t from, size_t to) {
    verify_range(str, from, to);
    // useful for encoding user-input URIs that may have special characters, without breaking the rest of the URI
    std::string encoded;
    encoded.reserve(str.size() + str.size()/2);
    for (auto i = str.begin() + from; i < str.begin() + to; i++) {
        char c = *i;
        // reserved characters: https://datatracker.ietf.org/doc/html/rfc3986#section-2.3
        // unreserved characters: https://datatracker.ietf.org/doc/html/rfc3986#section-2.3
        if (is_unreserved(c) || is_gen_delim(c) || is_sub_delim(c) || c == '%') {
            encoded.push_back(c);
        } else {
            // percent-encode
            encoded.push_back('%');
            encoded.push_back(to_hex[((unsigned char) c) / 16]);
            encoded.push_back(to_hex[((unsigned char) c) % 16]);
        }
    }
    return encoded;
}

std::pair<std::string, ssize_t> strtb::uri::percent_decode(const std::string& str, size_t from, size_t to) {
    verify_range(str, from, to);
    std::string decoded;
    decoded.reserve(to - from);
    for (size_t i=from; i<to; i++) {
        char c = str[i];
        if (c == '%') {
            // decode percentage encoded char
            if (i+2 >= to)
                return {"incomplete escape code", str.size()};
            unsigned char h1 = from_hex(str[i+1]);
            unsigned char h2 = from_hex(str[i+2]);
            if (h1 == 255)
                return {"invalid hex digit", i+1};
            if (h2 == 255)
                return {"invalid hex digit", i+2};
            decoded.push_back(16 * h1 + h2);
            i += 2;
        } else {
            // no decoding needed
            decoded.push_back(c);
        }
    }
    if (decoded.size() <= to - from)
        decoded.shrink_to_fit();
    // .second == -1 means no error, != -1 means error at that pos
    return {std::move(decoded), -1};
}

void parser::clear_uri() {
    scheme_from = 0, scheme_to = 0;
    clear_authority();
    path_from = 0, path_to = 0;
    query_from = 0, query_to = 0;
    fragment_from = 0, fragment_to = 0;
}

void parser::clear_authority() {
    authority_from = 0, authority_to = 0;
    userinfo_from = 0, userinfo_to = 0;
    clear_host();
    port_from = 0, port_to = 0;
}

void parser::clear_host() {
    host_from = 0, host_to = 0;
    host_type = HOST_EMPTY;
}

std::string parser::scheme_str(const std::string& str, bool fix_case) const {
    if (fix_case) {
        std::string new_str = str.substr(scheme_from, scheme_to - scheme_from);
        for (auto& c : new_str) // to lowercase, as the RFC recommends
            if ('A' <= c && c <= 'Z')
                c += 32;
        return new_str;
    } else {
        return str.substr(scheme_from, scheme_to - scheme_from);
    }
}

std::string parser::authority_str(const std::string& str) const {
    return str.substr(authority_from, authority_to - authority_from);
}

std::string parser::userinfo_str(const std::string& str) const {
    return str.substr(userinfo_from, userinfo_to - userinfo_from);
}

std::string parser::host_str(const std::string& str, bool fix_case) const {
    if (fix_case) {
        std::string new_str = str.substr(host_from, host_to - host_from);

        // to lowercase, except for percent-escaped characters, as the RFC recommends
        unsigned int pct_encode_remaining = 0;
        for (auto& c : new_str) {
            if (c == '%') {
                pct_encode_remaining = 2;
            } else if (pct_encode_remaining) {
                // pct encoded to uppercase
                if ('a' <= c && c <= 'z')
                    c -= 32;
                pct_encode_remaining--;
            } else {
                // regular chars to lowercase
                if ('A' <= c && c <= 'Z')
                    c += 32;
            }
        }

        return new_str;
    } else {
        return str.substr(host_from, host_to - host_from);
    }
}

std::string parser::port_str(const std::string& str) const {
    return str.substr(port_from, port_to - port_from);
}

int parser::port_uint16(const std::string& str) const {
    if (port_to == port_from)
        return -1;  // no port specified

    int x;
    try {
        x = std::stoi(port_str(str));
    } catch (std::out_of_range&) {
        return -1;  // overflows massively
    } catch (std::invalid_argument&) {
        return -1;  // something else went wrong
    }

    if (port_to - port_from > 5 || x >= 65536)
        return -1;  // overflows
    else
        return (uint16_t) x;    // good
}

std::string parser::path_str(const std::string& str) const {
    return str.substr(path_from, path_to - path_from);
}

std::string parser::query_str(const std::string& str) const {
    return str.substr(query_from, query_to - query_from);
}

std::string parser::fragment_str(const std::string& str) const {
    return str.substr(fragment_from, fragment_to - fragment_from);
}

// NOTE: parser functions will accept invalid percent-encoded parts, percent_decode will catch these errors

static parser_ret parse_scheme(const std::string& str, size_t from, size_t to);
static parser_ret parse_userinfo(const std::string& str, size_t from, size_t to);
static parser_ret parse_port(const std::string& str, size_t from, size_t to);
static parser_ret parse_path_abempty(const std::string& str, size_t from, size_t to);
static parser_ret parse_path_absolute(const std::string& str, size_t from, size_t to);
static parser_ret parse_path_noscheme(const std::string& str, size_t from, size_t to);
static parser_ret parse_path_rootless(const std::string& str, size_t from, size_t to);
static parser_ret parse_path_segment(const std::string& str, size_t from, size_t to, bool nz = false, bool nc = false);
static parser_ret parse_query(const std::string& str, size_t from, size_t to);
static parser_ret parse_fragment(const std::string& str, size_t from, size_t to);
static parser_ret parse_host_ipv6(const std::string& str, size_t from, size_t to);
static parser_ret parse_h16_multi(const std::string& str, size_t from, size_t to, size_t min, size_t max);
static parser_ret parse_h16(const std::string& str, size_t from, size_t to);
static parser_ret parse_ls32(const std::string& str, size_t from, size_t to);
static parser_ret parse_host_ipvfuture(const std::string& str, size_t from, size_t to);
static parser_ret parse_host_ipv4(const std::string& str, size_t from, size_t to);
static parser_ret parse_dec_octet(const std::string& str, size_t from, size_t to);
static parser_ret parse_host_regname(const std::string& str, size_t from, size_t to);

parser_ret parser::parse_uri(const std::string& str) {
    return parse_uri(str, 0, str.length());
}

parser_ret parser::parse_uri_suffix(const std::string& str) {
    return parse_uri_suffix(str, 0, str.length());
}

parser_ret parser::parse_relative_ref(const std::string& str) {
    return parse_relative_ref(str, 0, str.length());
}

parser_ret parser::parse_authority(const std::string& str) {
    return parse_authority(str, 0, str.length());
}

parser_ret parser::parse_host(const std::string& str) {
    return parse_host(str, 0, str.length());
}

parser_ret parser::parse_uri(const std::string& str, size_t from, size_t to) {
    verify_range(str, from, to);
    clear_uri();

    // scheme
    parser_ret ret = parse_scheme(str, from, to);
    if (!ret.second)
        return ret;
    if (ret.first >= to || str[ret.first] != ':')   // colon after scheme
        return {ret.first, false};
    scheme_from = from;
    scheme_to = ret.first;

    // hier-part (all variations and things after it)
    size_t pos_pre_hier_part = ret.first + 1;
    size_t best_progress = pos_pre_hier_part;
    for (unsigned int var=0; var<4; var++) {
        size_t pos = pos_pre_hier_part;

        // hier-part
        switch (var) {
        // "//" authority path-abempty
        case 0:
            // "//"
            if (pos >= to || str[pos] != '/') {
                if (pos > best_progress)
                    best_progress = pos;
                continue;
            }
            pos++;
            if (pos >= to || str[pos] != '/') {
                if (pos > best_progress)
                    best_progress = pos;
                continue;
            }
            pos++;

            // authority
            ret = parse_authority(str, pos, to);
            if (!ret.second) {
                if (ret.first > best_progress)
                    best_progress = ret.first;
                continue;
            }
            pos = ret.first;

            // path-abempty (either starting with / or empty)
            path_from = pos;
            pos = parse_path_abempty(str, pos, to).first;
            path_to = pos;
            break;

        // path-absolute
        case 1:
            ret = parse_path_absolute(str, pos, to);
            if (!ret.second) {
                if (ret.first > best_progress)
                    best_progress = ret.first;
                continue;
            }
            path_from = pos;
            pos = ret.first;
            path_to = pos;
            break;

        // path-rootless
        case 2:
            ret = parse_path_rootless(str, pos, to);
            if (!ret.second) {
                if (ret.first > best_progress)
                    best_progress = ret.first;
                continue;
            }
            path_from = pos;
            pos = ret.first;
            path_to = pos;
            break;

        // path-empty
        case 3:
            path_from = path_to = pos;
            break;
        }

        // query (optional)
        ret = parse_query(str, pos, to);
        if (ret.second) {
            query_from = pos + 1;   // skip ?
            pos = ret.first;
            query_to = pos;
        }

        // fragment (optional)
        ret = parse_fragment(str, pos, to);
        if (ret.second) {
            fragment_from = pos + 1;    // skip #
            pos = ret.first;
            fragment_to = pos;
        }

        // must have reached the end, otherwise there's some kind of error
        if (pos < to) {
            if (pos > best_progress)
                best_progress = pos;
            clear_authority();
            path_from = 0, path_to = 0;
            query_from = 0, query_to = 0;
            fragment_from = 0, fragment_to = 0;
            continue;
        }

        return {to, true};
    }

    // no full matches
    clear_uri();
    return {best_progress, false};
}

/* authority path-abempty [?query] [#fragment]
 * e.g. www.theonicolaou.net/path/to/resource?q=something
 *      theonicolaou.net/page.html
 *
 * This is an invalid URI syntax, but is often used for human-written URLs.
 * This can ONLY be used when a user-written ABSOLUTE path is entered,
 * and NEVER in situations where the input could be a relative reference,
 * as their syntax is the same. Also, there can be false positives.
 * Use is_web_url() instead, which checks for both full and suffix URLs
 * and further analyzes the URL to reduce false positives and security issues.
 *
 * Read more: https://datatracker.ietf.org/doc/html/rfc3986#section-4.5
 */
parser_ret parser::parse_uri_suffix(const std::string& str, size_t from, size_t to) {
    verify_range(str, from, to);
    clear_uri();

    parser_ret ret;
    size_t pos = from;

    // authority
    ret = parse_authority(str, pos, to);
    if (!ret.second)
        return {ret.first, false};
    pos = ret.first;

    // path-abempty (either starting with / or empty)
    path_from = pos;
    pos = parse_path_abempty(str, pos, to).first;
    path_to = pos;

    // query (optional)
    ret = parse_query(str, pos, to);
    if (ret.second) {
        query_from = pos + 1;   // skip ?
        pos = ret.first;
        query_to = pos;
    }

    // fragment (optional)
    ret = parse_fragment(str, pos, to);
    if (ret.second) {
        fragment_from = pos + 1;    // skip #
        pos = ret.first;
        fragment_to = pos;
    }

    // must have reached the end, otherwise there's some kind of error
    if (pos < to) {
        clear_uri();
        return {pos, false};
    }

    return {pos, true};
}

parser_ret parser::parse_relative_ref(const std::string& str, size_t from, size_t to) {
    verify_range(str, from, to);
    clear_uri();

    // relative-part (all variations and things after it)
    size_t best_progress = from;
    for (unsigned int var=0; var<4; var++) {
        size_t pos = from;
        parser_ret ret;

        // relative-part
        switch (var) {
        // "//" authority path-abempty
        case 0:
            // "//"
            if (pos >= to || str[pos] != '/') {
                if (pos > best_progress)
                    best_progress = pos;
                continue;
            }
            pos++;
            if (pos >= to || str[pos] != '/') {
                if (pos > best_progress)
                    best_progress = pos;
                continue;
            }
            pos++;

            // authority
            ret = parse_authority(str, pos, to);
            if (!ret.second) {
                if (ret.first > best_progress)
                    best_progress = ret.first;
                continue;
            }
            pos = ret.first;

            // path-abempty (either starting with / or empty)
            path_from = pos;
            pos = parse_path_abempty(str, pos, to).first;
            path_to = pos;
            break;

        // path-absolute
        case 1:
            ret = parse_path_absolute(str, pos, to);
            if (!ret.second) {
                if (ret.first > best_progress)
                    best_progress = ret.first;
                continue;
            }
            path_from = pos;
            pos = ret.first;
            path_to = pos;
            break;

        // path-noscheme
        case 2:
            ret = parse_path_noscheme(str, pos, to);
            if (!ret.second) {
                if (ret.first > best_progress)
                    best_progress = ret.first;
                continue;
            }
            path_from = pos;
            pos = ret.first;
            path_to = pos;
            break;

        // path-empty
        case 3:
            path_from = path_to = pos;
            break;
        }

        // query (optional)
        ret = parse_query(str, pos, to);
        if (ret.second) {
            query_from = pos + 1;   // skip ?
            pos = ret.first;
            query_to = pos;
        }

        // fragment (optional)
        ret = parse_fragment(str, pos, to);
        if (ret.second) {
            fragment_from = pos + 1;    // skip #
            pos = ret.first;
            fragment_to = pos;
        }

        // must have reached the end, otherwise there's some kind of error
        if (pos < to) {
            if (pos > best_progress)
                best_progress = pos;
            clear_uri();
            continue;
        }

        return {to, true};
    }

    // no full matches
    clear_uri();
    return {best_progress, false};
}

web_url_ret parser::is_web_url(const std::string& str) {
    return is_web_url(str, 0, str.length());
}

web_url_ret parser::is_web_url(const std::string& str, size_t from, size_t to) {
    verify_range(str, from, to);
    bool type = true;

    // try to parse a full URL first
    if (parse_uri(str, from, to).second) {
        // make sure the scheme is http(s)
        std::string scheme = scheme_str(str);
        for (size_t i = 0; i < scheme.length(); i++)
            if ('A' <= scheme[i] && scheme[i] <= 'Z')
                scheme[i] += 'a' - 'A';
        if (scheme == "http" || scheme == "https")
            type = false;
    }

    // if that fails, try to parse a suffix
    if (type)
        if (!parse_uri_suffix(str, from, to).second)
            return {WEB_URL_ERROR, false};

    // determine how likely this is an intentional URI, specifically a web URL
    // empty authority or host
    if (authority_to == authority_from || host_type == HOST_EMPTY)
        return {WEB_URL_UNLIKELY, type};

    // if a port was specified, it must be within the valid port range (also port 0 and 1 are unsafe)
    if (host_to != authority_to) {
        size_t port_len = port_to - port_from;
        if (!(!type && port_len == 0)) {    // ignore empty port for full URLs
            if (port_len < 1 || port_len > 5)
                return {WEB_URL_UNLIKELY, type};
            int n = std::stoi(port_str(str));
            if (n <= 1 || n >= 65536)
                return {WEB_URL_UNLIKELY, type};
        }
    }

    // IP addresses only recognized as possible, so they don't get picked up from chat messages
    if (host_type != HOST_REGNAME)
        return {WEB_URL_POSSIBLE, type};

    // scan host pieces
    size_t piece_start = host_from;
    size_t piece_count = 1;
    bool more_than_numbers = false;
    for (size_t i = host_from; i < host_to; i++) {
        char c = str[i];

        if (c == '.') {
            if (i == piece_start)   // empty segment in host (e.g. "example..com")
                return {WEB_URL_UNLIKELY, type};

            piece_start = i+1;
            piece_count++;
            more_than_numbers = false;
        } else if (!(is_alpha(c) || is_digit(c) || c == '-')) {
            // invalid character
            return {WEB_URL_UNLIKELY, type};
        } else if (!is_digit(c)) {
            // current segment isn't only numbers
            more_than_numbers = true;
        }
    }

    // check if host ends with empty segment
    // NOTE: extra dots at the end must be removed by the caller
    if (piece_start == host_to)
        return {WEB_URL_UNLIKELY, type};
    // TLD cannot be only numbers
    if (!more_than_numbers)
        return {WEB_URL_UNLIKELY, type};
    // must have multiple pieces (e.g. example.com) to be recognized in chat messages
    if (piece_count == 1)
        return {WEB_URL_POSSIBLE, type};
    // check TLD
    if (!is_known_tld(str, piece_start, host_to))
        return {WEB_URL_POSSIBLE, type};
    // unknown TLDs are still accepted as "possible" so they'll get recognized in an address input field
    // but NOT in chat messages. remember that TLDs such as "local" are NOT in the IANA list

    // check if userinfo was specified, return at most SUFFIX_POSSIBLE_WEBSITE for security
    if (host_from > authority_from)
        return {WEB_URL_POSSIBLE, type};

    return {WEB_URL_LIKELY, type};
}

parser_ret parse_scheme(const std::string& str, size_t from, size_t to) {
    if (from >= to || !is_alpha(str[from]))     // first char alpha
        return {from, false};

    size_t pos;
    for (pos=from+1; pos<to; pos++) {
        char c = str[pos];
        if (!(is_alpha(c) || is_digit(c) || c == '+' || c == '-' || c == '.'))  // stop on first invalid char
            break;
    }

    return {pos, true};
}

parser_ret parser::parse_authority(const std::string& str, size_t from, size_t to) {
    verify_range(str, from, to);
    clear_authority();
    size_t pos = from;

    // try to get userinfo if it exists
    parser_ret ret = parse_userinfo(str, from, to);
    if (ret.first < to && str[ret.first] == '@') {  // userinfo exists
        userinfo_from = pos;
        userinfo_to = ret.first;
        pos = ret.first + 1;
    }

    // get host
    ret = parse_host(str, pos, to);
    if (!ret.second) {  // invalid host
        clear_authority();
        return ret;
    } else {
        pos = ret.first;
    }

    // get port if specified
    if (pos < to && str[pos] == ':') {
        port_from = pos + 1;
        pos = parse_port(str, pos + 1, to).first;
        port_to = pos;
    }

    authority_from = from;
    authority_to = pos;
    return {pos, true};
}

parser_ret parse_userinfo(const std::string& str, size_t from, size_t to) {
    size_t pos;
    for (pos=from; pos<to; pos++) {
        char c = str[pos];
        if (!(is_unreserved(c) || c == '%' || is_sub_delim(c) || c == ':')) // stop on first invalid char
            break;
    }

    return {pos, true};
}

parser_ret parser::parse_host(const std::string& str, size_t from, size_t to) {
    verify_range(str, from, to);
    clear_host();

    // array with function ptrs for all the possible host types
    parser_ret (*ptrs[4])(const std::string&, size_t, size_t) = {
        &parse_host_ipvfuture, &parse_host_ipv6, &parse_host_ipv4, &parse_host_regname
    };
    host_type_enum types[4] = {HOST_IPVFUTURE, HOST_IPV6, HOST_IPV4, HOST_REGNAME};

    size_t best_progress = from;
    // check all host types until one matches
    for (unsigned int i=0; i<4; i++) {
        parser_ret ret = ptrs[i](str, from, to);
        if (ret.first > best_progress)
            best_progress = ret.first;
        if (ret.second) {   // matched
            char c;
            if (ret.first < to)
                c = str[ret.first];
            // make sure the host part ends here (end of uri, or delim for port, path, query, fragment)
            if (ret.first >= to || c == ':' || c == '/' || c == '?' || c == '#') {
                host_from = from;
                host_to = ret.first;
                if (host_to == host_from)
                    host_type = HOST_EMPTY;
                else
                    host_type = types[i];
                return ret;
            }
        }
    }

    // none matched fully, or host part didn't end after the match
    return {best_progress, false};
}

static parser_ret parse_host_ipv6(const std::string& str, size_t from, size_t to) {
    // starts with [
    if (from >= to || str[from] != '[')
        return {from, false};

    size_t best_progress = from + 1;
    // test for all the variations of IPv6 addresses, as specified in the RFC
    for (unsigned int variant=0; variant<9; variant++) {
        size_t pos = from + 1;

        // pre :: (optional piece)
        if (variant >= 2) {
            parser_ret ret = parse_h16_multi(str, pos, to, 0, variant-2);
            if (ret.second) {
                ret = parse_h16(str, ret.first, to);
                if (ret.second)
                    pos = ret.first;
            }
        }

        // ::
        if (variant >= 1) {
            if (pos >= to || str[pos] != ':') {
                if (pos > best_progress)
                    best_progress = pos;
                continue;
            }
            if (pos+1 >= to || str[pos+1] != ':') {
                if (pos+1 > best_progress)
                    best_progress = pos+1;
                continue;
            }
            pos += 2;
        }

        // post :: or no ::
        if (variant <= 6) {
            parser_ret ret = parse_h16_multi(str, pos, to, 6-variant, 6-variant);
            if (!ret.second) {
                if (ret.first > best_progress)
                    best_progress = ret.first;
                continue;
            }
            pos = ret.first;

            ret = parse_ls32(str, pos, to);
            if (!ret.second) {
                if (ret.first > best_progress)
                    best_progress = ret.first;
                continue;
            }
            pos = ret.first;
        }

        if (variant == 7) {
            parser_ret ret = parse_h16(str, pos, to);
            if (!ret.second) {
                if (ret.first > best_progress)
                    best_progress = ret.first;
                continue;
            }
            pos = ret.first;
        }

        // ends with ]
        if (pos >= to || str[pos] != ']') {
            if (pos > best_progress)
                best_progress = pos;
            continue;
        }
        pos++;

        // full address read, success
        return {pos, true};
    }

    // no variants matched, failure
    return {best_progress, false};
}

// get multiple pieces of h16: (with the colon) for IPv6
static parser_ret parse_h16_multi(const std::string& str, size_t from, size_t to, size_t min, size_t max) {
    assert(min <= max);
    size_t pos = from;
    for (size_t i=0; i<max; i++) {
        parser_ret ret = parse_h16(str, pos, to);
        if (!ret.second ||
            ret.first >= to || str[ret.first] != ':' ||     // piece must end with :
            (ret.first + 1 < to && str[ret.first + 1] == ':')) {   // but not ::
            // error in this piece
            if (i >= min)           // already got enough pieces, success
                return {pos, true};
            else if (!ret.second)   // not enough pieces, error in h16
                return ret;
            else                    // not enough pieces, error in colon char
                return {ret.first, false};
        }
        pos = ret.first + 1;
    }
    // got max amount of pieces, success
    return {pos, true};
}

// get 4 hex chars (16 bits of data) for IPv6
static parser_ret parse_h16(const std::string& str, size_t from, size_t to) {
    // 1 to 4 chars allowed
    if (to - from < 1)
        return {to, false};
    if (to - from > 4)
        to = from + 4;

    for (size_t i=from; i<to; i++) {
        if (from_hex(str[i]) == 255) {  // invalid hex char marks end of h16 part
            if (i == from)  // must be at least 1 char
                return {i, false};
            else
                return {i, true};
        }
    }

    // 4 valid hex chars
    return {to, true};
}

// get 32 bits of data for IPv6 ending (either 8 hex chars and a : or an IPv4 address)
static parser_ret parse_ls32(const std::string& str, size_t from, size_t to) {
    // IPv6 can end with an IPv4 piece in some cases
    parser_ret ret = parse_host_ipv4(str, from, to);
    if (ret.second)
        return ret;

    // h16:h16
    ret = parse_h16(str, from, to);
    if (!ret.second)    // first h16 invalid
        return ret;
    if (ret.first >= to || str[ret.first] != ':') // missing colon separator
        return {ret.first, false};
    return parse_h16(str, ret.first+1, to);     // second h16
}

static parser_ret parse_host_ipvfuture(const std::string& str, size_t from, size_t to) {
    // starts with [
    if (from >= to || str[from] != '[')
        return {from, false};
    size_t pos = from + 1;

    // version flag
    if (pos >= to || str[pos] != 'v')
        return {pos, false};
    pos++;
    for (; pos<to; pos++)
        if (from_hex(str[pos]) == 255)  // stop when hex digits end
            break;
    if (pos <= from + 2)        // must have at least one hex digit
        return {pos, false};

    // dot separator
    size_t dot_pos = pos;
    if (dot_pos >= to || str[dot_pos] != '.')   // missing dot
        return {dot_pos, false};
    pos++;

    // address
    for (; pos<to; pos++) {
        char c = str[pos];
        if (!(is_unreserved(c) || is_sub_delim(c) || c == ':')) // stop on invalid char
            break;
    }
    if (pos - dot_pos < 2)  // must have at least one char after dot
        return {pos, false};

    // ends with ]
    if (pos >= to || str[pos] != ']')
        return {pos, false};
    pos++;

    // full address read, success
    return {pos, true};
}

static parser_ret parse_host_ipv4(const std::string& str, size_t from, size_t to) {
    // get four octets
    size_t pos = from;
    for (unsigned int i=0; i<4; i++) {
        if (i != 0) {
            if (pos >= to || str[pos] != '.')   // dot separator
                return {pos, false};
            pos++;
        }

        // octet
        parser_ret ret = parse_dec_octet(str, pos, to);
        if (!ret.second)    // error in octet
            return ret;
        pos = ret.first;
    }

    // success
    return {pos, true};
}

static parser_ret parse_dec_octet(const std::string& str, size_t from, size_t to) {
    // 3 digits max
    if (to > from + 3)
        to = from + 3;

    // find where digits stop
    size_t pos;
    for (pos = from; pos < to; pos++)
        if (!is_digit(str[pos]))
            break;

    // at least 1 digit
    if (pos - from < 1)
        return {from, false};

    // max number is 255
    if (std::stoi(str.substr(from, pos - from)) < 256)
        return {pos, true};
    else
        return {from, false};
}

static parser_ret parse_host_regname(const std::string& str, size_t from, size_t to) {
    for (size_t i=from; i<to; i++) {    // stop on first invalid character
        char c = str[i];
        if (!(is_unreserved(c) || c == '%' || is_sub_delim(c)))
            return {i, true};
    }

    return {to, true};
}

parser_ret parse_port(const std::string& str, size_t from, size_t to) {
    // just keep reading as long as there are digits
    size_t pos;
    for (pos=from; pos<to; pos++)
        if (!is_digit(str[pos]))
            break;

    return {pos, true};
}

static parser_ret parse_path_abempty(const std::string &str, size_t from, size_t to) {
    // abempty = absolute or empty (kinda)
    size_t pos = from;
    while (pos < to) {
        if (str[pos] != '/')
            break;
        pos = parse_path_segment(str, pos + 1, to).first;
    }
    return {pos, true};
}

static parser_ret parse_path_absolute(const std::string &str, size_t from, size_t to) {
    // must start with /
    if (from >= to || str[from] != '/')
        return {from, false};

    // optional segment-nz
    parser_ret ret = parse_path_segment(str, from + 1, to, true);
    if (!ret.second)
        return {from + 1, true};

    // optional other segments after segment-nz
    return parse_path_abempty(str, ret.first, to);
}

static parser_ret parse_path_noscheme(const std::string &str, size_t from, size_t to) {
    // first segment has no : and starts without / (segment-nz-nc)
    parser_ret ret = parse_path_segment(str, from, to, true, true);
    if (!ret.second)
        return ret;

    // optional other segments
    return parse_path_abempty(str, ret.first, to);
}

static parser_ret parse_path_rootless(const std::string &str, size_t from, size_t to) {
    // starts with segment-nz, without /
    parser_ret ret = parse_path_segment(str, from, to, true);
    if (!ret.second)
        return ret;

    // optional other segments
    return parse_path_abempty(str, ret.first, to);
}

static parser_ret parse_path_segment(const std::string &str, size_t from, size_t to, bool nz, bool nc) {
    // nz = non-zero size, nc = no colon (more info in RFC: segment, segment-nz, segment-nz-nc)
    size_t pos;
    for (pos = from; pos < to; pos++)
        if (!is_pchar(str[pos], nc))
            break;
    if (nz && pos == from)  // check if non-zero requirement failed
        return {pos, false};
    return {pos, true};
}

static parser_ret parse_query(const std::string &str, size_t from, size_t to) {
    // starts with ?
    if (from >= to || str[from] != '?')
        return {from, false};

    size_t pos;
    for (pos = from + 1; pos < to; pos++) {
        char c = str[pos];
        if (!(is_pchar(c) || c == '/' || c == '?'))     // stop at first invalid char
            break;
    }

    return {pos, true};
}

static parser_ret parse_fragment(const std::string &str, size_t from, size_t to) {
    // starts with #
    if (from >= to || str[from] != '#')
        return {from, false};

    size_t pos;
    for (pos = from + 1; pos < to; pos++) {
        char c = str[pos];
        if (!(is_pchar(c) || c == '/' || c == '?'))     // stop at first invalid char
            break;
    }

    return {pos, true};
}

#define TLD_TRIE_CHAR_COUNT 10 + 1 + 26
#define TLD_TRIE_NUM_POS 0
#define TLD_TRIE_DASH_POS 10
#define TLD_TRIE_ALPHA_POS 11
struct tld_trie_node {
    int next[TLD_TRIE_CHAR_COUNT];
    bool match = false;
    // next[0 to 9]: numbers
    // next[10]: dash
    // next[11 to 36]: upper&lowercase chars combined
    // next[i] == -1 means no next node for that char
    // points to the next node for each character
    // if the string ends on a node with match == true it means it's in the trie

    tld_trie_node() {
        for (size_t i = 0; i < TLD_TRIE_CHAR_COUNT; i++)
            next[i] = -1;
    }
};

static std::vector<struct tld_trie_node> known_tlds_trie;

static void tld_trie_reset() {
    known_tlds_trie.clear();
    known_tlds_trie.emplace_back();
}

static inline int tld_trie_charcode(char c) {
    if ('A' <= c && c <= 'Z')
        return c - 'A' + TLD_TRIE_ALPHA_POS;
    else if ('a' <= c && c <= 'z')
        return c - 'a' + TLD_TRIE_ALPHA_POS;
    else if (is_digit(c))
        return c - '0' + TLD_TRIE_NUM_POS;
    else if (c == '-')
        return TLD_TRIE_DASH_POS;
    else
        return -1;
}

static void tld_trie_add_word(const char* word, size_t from, size_t to) {
    int current_node = 0;
    for (size_t i = from; i < to; i++) {
        int code = tld_trie_charcode(word[i]);
        if (code == -1) {
            log.warning({"Failed to add ", strtb::common::string_escape(std::string(word).substr(from, to-from)),
                         " to the set of known TLDs due to an unsupported character."});
            return;
        }

        int next_node = known_tlds_trie[current_node].next[code];
        if (next_node == -1) {
            // next node doesn't exist, create it
            next_node = known_tlds_trie.size();
            known_tlds_trie[current_node].next[code] = next_node;
            known_tlds_trie.emplace_back();
        }
        current_node = next_node;
    }

    known_tlds_trie[current_node].match = true;
}

static inline bool is_whitespace_not_crlf(char c) {
    return c == '\0' || c == '\t' || c == '\v' || c == '\f' || c == ' ';
}

static inline bool is_crlf(char c) {
    return c == '\n' || c == '\r';
}

static inline bool is_whitespace(char c) {
    return is_whitespace_not_crlf(c) || is_crlf(c);
}

static void known_tlds_load_line(const char* str, size_t from, size_t to) {
    size_t i;

    // trim starting whitespace
    for (i = from; i < to && is_whitespace_not_crlf(str[i]); i++);
    from = i;

    // trim ending whitespace
    for (i = to - 1; i >= from && is_whitespace(str[i]); i--);
    to = i + 1;

    // ignore empty lines or comments
    if (to <= from || str[from] == '#')
        return;

    tld_trie_add_word(str, from, to);
}

void strtb::uri::known_tlds_load_str(const char* str, size_t len) {
    size_t line_start = 0, line_end = 0;
    tld_trie_reset();

    for (size_t i = 0; i < len; i++) {
        if (str[i] == '\n') {
            line_start = line_end;
            line_end = i + 1;
            known_tlds_load_line(str, line_start, line_end);
        }
    }

    line_start = line_end;
    line_end = len;
    known_tlds_load_line(str, line_start, line_end);
}

bool strtb::uri::is_known_tld(const char* str, size_t from, size_t to) {
    int current_node = 0;

    for (size_t i = from; i < to; i++) {
        int code = tld_trie_charcode(str[i]);
        if (code == -1)
            return false;

        current_node = known_tlds_trie[current_node].next[code];
        if (current_node == -1)
            return false;
    }

    return known_tlds_trie[current_node].match;
}

bool strtb::uri::is_known_tld(const std::string& str, size_t from, size_t to) {
    return is_known_tld(str.data(), from, to);
}

bool strtb::uri::is_known_tld(const std::string& str) {
    return is_known_tld(str.data(), 0, str.length());
}
