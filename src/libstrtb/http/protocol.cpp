#include "protocol.h"
#include "common/strescape.h"

#include <cstdint>
#include <climits>  // IWYU pragma: keep
#include <cstring>
#include <stdexcept>
#include <map>

namespace strtb::http {

const char* get_status_code_phrase(int status_code) {
    switch (status_code) {
    case 100: return "Continue";
    case 101: return "Switching Protocols";
    case 200: return "OK";
    case 201: return "Created";
    case 202: return "Accepted";
    case 203: return "Non-Authoritative Information";
    case 204: return "No Content";
    case 205: return "Reset Content";
    case 206: return "Partial Content";
    case 300: return "Multiple Choices";
    case 301: return "Moved Permanently";
    case 302: return "Found";
    case 303: return "See Other";
    case 304: return "Not Modified";
    case 305: return "Use Proxy";
    case 307: return "Temporary Redirect";
    case 308: return "Permanent Redirect";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 402: return "Payment Required";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 406: return "Not Acceptable";
    case 407: return "Proxy Authentication Required";
    case 408: return "Request Timeout";
    case 409: return "Conflict";
    case 410: return "Gone";
    case 411: return "Length Required";
    case 412: return "Precondition Failed";
    case 413: return "Content Too Large";
    case 414: return "URI Too Long";
    case 415: return "Unsupported Media Type";
    case 416: return "Range Not Satisfiable";
    case 417: return "Expectation Failed";
    case 418: return "I'm a teapot";
    case 421: return "Misdirected Request";
    case 422: return "Unprocessable Content";
    case 426: return "Upgrade Required";
    case 428: return "Precondition Required";
    case 429: return "Too Many Requests";
    case 431: return "Request header Fields Too Large";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 502: return "Bad Gateway";
    case 503: return "Service Unavailable";
    case 504: return "Gateway Timeout";
    case 505: return "HTTP Version Not Supported";
    case 511: return "Network Authentication Required";
    default : return "";    // empty phrase for unknown codes
    }
};

typedef enum month_enum {
    MONTH_INVALID,
    MONTH_JAN,
    MONTH_FEB,
    MONTH_MAR,
    MONTH_APR,
    MONTH_MAY,
    MONTH_JUN,
    MONTH_JUL,
    MONTH_AUG,
    MONTH_SEP,
    MONTH_OCT,
    MONTH_NOV,
    MONTH_DEC
} month_enum;

static const char* month_names[] = {
    "Jan",
    "Feb",
    "Mar",
    "Apr",
    "May",
    "Jun",
    "Jul",
    "Aug",
    "Sep",
    "Oct",
    "Nov",
    "Dec"
};

static const char* day_names[] = {
    "Sun",
    "Mon",
    "Tue",
    "Wed",
    "Thu",
    "Fri",
    "Sat"
};

typedef struct date_parser_inner_ret {
    int year = 0, day = 0, hour = 0, minute = 0, second = 0;
    month_enum month = MONTH_INVALID;
    bool valid = false;
} date_parser_inner_ret;

static size_t parse_optional_whitespace(const char *str, size_t from, size_t to);
static parser_ret parse_required_whitespace(const char *str, size_t from, size_t to);
static parser_ret parse_word(const char *str, size_t from, size_t to, const char *word);
static size_t find_char(const char *str, size_t from, size_t to, char c);
static std::tuple<size_t, bool, unsigned int> parse_digits(const char *str, size_t from, size_t to, size_t digits);
static std::string parse_token_tolower(const char *str, size_t from, size_t to);
static parser_ret parse_token68(const char *str, size_t from, size_t to);
static constexpr size_t strlen_constexpr(const char *str);
static constexpr uint64_t date_hash_short(const char *str);
static constexpr uint64_t date_hash_short(const char *str, size_t from, size_t to);
static parser_ret parse_time_of_day(const char *str, size_t from, size_t to, date_parser_inner_ret &ret);
static date_parser_inner_ret parse_date_imf(const char *str, size_t from, size_t to);
static date_parser_inner_ret parse_date_rfc850(const char *str, size_t from, size_t to);
static date_parser_inner_ret parse_date_asctime(const char *str, size_t from, size_t to);

static inline bool is_digit(char c) {
    return '0' <= c && c <= '9';
}

static inline bool is_alpha(char c) {
    return ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z');
}

static inline bool is_tchar(char c) {
    return is_alpha(c) || is_digit(c) ||
           c == '!' || c == '#' || c == '$' || c == '%' || c == '&' || c == '\'' || c == '*' ||
           c == '+' || c == '-' || c == '.' || c == '^' || c == '_' || c == '`' || c == '|' || c == '~';
}

static inline bool is_whitespace(char c) {
    return c == ' ' || c == '\t';
}

static inline bool is_vchar(char c) {
    return 0x21 <= c && c <= 0x7E;
}

static inline bool is_obs_text(unsigned char c) {
    return 0x80 <= c;
}

static inline bool is_qdtext(unsigned char c) {
    return (0x20 <= c && c <= 0x7E && c != 0x22 && c != 0x5C) || c == '\t' || is_obs_text(c);
}

static inline char to_lower(char c) {
    return 'A' <= c && c <= 'Z' ? c + ('a' - 'A') : c;
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

static inline bool parse_char(const char *str, size_t from, size_t to, char c) {
    return to > from && str[from] == c;
}

static inline std::pair<int, bool> parse_digit(const char *str, size_t from, size_t to) {
    if (to > from && is_digit(str[from]))
        return std::make_pair(str[from] - '0', true);
    else
        return std::make_pair(0, false);
}

static size_t parse_optional_whitespace(const char *str, size_t from, size_t to) {
    size_t pos;

    for (pos = from; pos < to; pos++)
        if (!is_whitespace(str[pos]))
            break;

    return pos;
}

static parser_ret parse_required_whitespace(const char *str, size_t from, size_t to) {
    size_t pos;

    for (pos = from; pos < to; pos++)
        if (!is_whitespace(str[pos]))
            break;

    return std::make_pair(pos, pos > from);
}

// checks if a word (or any str) is present from this position
static parser_ret parse_word(const char *str, size_t from, size_t to, const char *word) {
    for (size_t i = from; i < to; i++) {
        if (*word == 0) // whole word matched
            return std::make_pair(i, true);
        else if (*word != str[i])   // chars here don't match
            return std::make_pair(i, false);
        word++;
    }

    // whole word matched right at the end
    if (*word == 0)
        return std::make_pair(to, true);

    // end of string before full match
    return std::make_pair(to, false);
}

static size_t find_char(const char *str, size_t from, size_t to, char c) {
    for (size_t i = from; i < to; i++)
        if (str[i] == c)
            return i;
    return to;
}

static std::tuple<size_t, bool, unsigned int> parse_digits(const char *str, size_t from, size_t to, size_t digits) {
    if (from + digits > to)     // reached end of string
        return {to, false, 0};

    unsigned int number = 0;
    for (size_t i = from; i < from + digits; i++) {
        char c = str[i];
        if (is_digit(c))
            number = number * 10 + (c - '0');
        else
            return {i, false, 0};
    }

    return {from + digits, true, number};
}

parser_ret parse_token(const std::string &str) {
    return parse_token(str.data(), 0, str.length());
}

parser_ret parse_token(const std::string &str, size_t from, size_t to) {
    verify_range(str, from, to);
    return parse_token(str.data(), from, to);
}

http_version_ret parse_http_version(const std::string &str) {
    return parse_http_version(str.data(), 0, str.length());
}

http_version_ret parse_http_version(const std::string &str, size_t from, size_t to) {
    verify_range(str, from, to);
    return parse_http_version(str.data(), from, to);
}

request_line_ret parse_request_line(const std::string &line) {
    return parse_request_line(line.data(), line.length());
}

status_line_ret parse_status_line(const std::string &line) {
    return parse_status_line(line.data(), line.length());
}

parser_ret parse_token(const char *str, size_t from, size_t to) {
    size_t pos;

    for (pos = from; pos < to; pos++)
        if (!is_tchar(str[pos]))
            break;

    // token must have at least 1 character
    return std::make_pair(pos, pos > from);
}

static std::string parse_token_tolower(const char *str, size_t from, size_t to) {
    std::string token;

    for (const char *c = str + from; c < str + to; c++) {
        if (is_tchar(*c))
            token.push_back(to_lower(*c));
        else
            return token;
    }

    return token;
}

static parser_ret parse_token68(const char *str, size_t from, size_t to) {
    size_t pos;

    for (pos = from; pos < to; pos++) {
        char c = str[pos];
        if (!(is_alpha(c) || is_digit(c) || c == '-' || c == '.' ||
                    c == '_' || c == '~' || c == '+' || c == '/'))
            break;
    }

    // must have at least 1 char of those above
    if (pos <= from)
        return {pos, false};

    // = must only appear in the end
    while (str[pos] == '=')
        pos++;

    return {pos, true};
}

// parse strings such as 'HTTP/1.1'
http_version_ret parse_http_version(const char *str, size_t from, size_t to) {
    size_t pos = from;
    int major = 0, minor = 0;
    bool valid = false;

    // 'HTTP/'
    std::tie(pos, valid) = parse_word(str, pos, to, "HTTP/");
    if (!valid)
        return {.to = pos, .valid = false};

    // major version
    std::tie(major, valid) = parse_digit(str, pos, to);
    if (!valid)
        return {.to = pos, .valid = false};
    pos++;

    // .
    if (!parse_char(str, pos, to, '.'))
        return {.to = pos, .valid = false};
    pos++;

    // minor version
    std::tie(minor, valid) = parse_digit(str, pos, to);
    if (!valid)
        return {.to = pos, .valid = false};
    pos++;

    // make sure there's no more stuff afterwards
    if (pos < to)
        return {.to = pos, .valid = false};

    return {pos, true, major, minor};
}

static inline request_target_form_enum parse_request_target(const std::string &target, uri::parser &uri_p) {
    // try to parse all valid forms of request-target
    // asterisk-form
    if (target.length() == 1 && target[0] == '*')
        return TARGET_FORM_ASTERISK;

    // origin form
    if (uri_p.parse_relative_ref(target).second) {
        // instantly reject #fragments cause # is only ever used for fragments
        if (uri_p.fragment_to)
            return TARGET_FORM_INVALID;

        if (uri_p.path_type == uri::PATH_ABSOLUTE)
            return TARGET_FORM_ORIGIN;
    }
    uri_p.clear_uri();

    // absolute-form
    if (uri_p.parse_uri(target).second) {
        // instantly reject #fragments cause # is only ever used for fragments
        if (uri_p.fragment_to)
            return TARGET_FORM_INVALID;

        std::string scheme = uri_p.scheme_str(target, true);
        if (uri_p.path_type == uri::PATH_ABEMPTY &&     // this path type only shows up for full URIs
            uri_p.userinfo_to == 0 &&                   // reject user login details in URI
            (scheme == "http" || scheme == "https"))    // make sure scheme is http(s)
            return TARGET_FORM_ABSOLUTE;
    }
    uri_p.clear_uri();

    // authority-form
    if (uri_p.parse_authority(target).second &&
        uri_p.userinfo_to == 0 &&
        uri_p.port_to > 0 && uri_p.port_to > uri_p.port_from)
        return TARGET_FORM_AUTHORITY;
    uri_p.clear_authority();

    // none of the forms matched
    return TARGET_FORM_INVALID;
}

request_line_ret parse_request_line(const char *line, size_t length) {
    size_t pos = 0;
    request_line_ret ret;

    // parse method
    auto [method_length, method_valid] = parse_token(line, 0, length);
    if (!method_valid)
        return {};
    ret.method.assign(line, method_length);
    pos = method_length;

    // single space
    if (!parse_char(line, pos, length, ' '))
        return {};
    pos++;

    // request target and single space
    // TODO: set max character limit
    size_t target_to = find_char(line, pos, length, ' ');
    if (target_to == length || target_to == pos)
        return {};
    ret.target.assign(line + pos, target_to - pos);
    pos = target_to + 1;
    ret.target_form = parse_request_target(ret.target, ret.target_segments);
    if (ret.target_form == TARGET_FORM_INVALID)
        return {};

    // http version
    auto http_version = parse_http_version(line, pos, length);
    if (!http_version.valid)
        return {};
    ret.http_version_major = http_version.v_major;
    ret.http_version_minor = http_version.v_minor;

    // line end is checked by http version parser
    ret.valid = true;
    return ret;
}

status_line_ret parse_status_line(const char *line, size_t length) {
    size_t pos = 0;
    status_line_ret ret;

    // http version and single space
    size_t http_version_to = find_char(line, pos, length, ' ');
    if (http_version_to == length || http_version_to == pos)
        return {};
    auto http_version = parse_http_version(line, pos, http_version_to);
    if (!http_version.valid)
        return {};
    ret.http_version_major = http_version.v_major;
    ret.http_version_minor = http_version.v_minor;
    pos = http_version_to + 1;

    // 3-digit status code
    bool status_code_valid;
    std::tie(pos, status_code_valid, ret.status_code) = parse_digits(line, pos, length, 3);
    if (!status_code_valid)
        return {};

    // single space
    if (!parse_char(line, pos, length, ' '))
        return {};
    pos++;

    // optional reason phrase
    for (size_t i = pos; i < length; i++) {
        char c = line[i];
        if (is_vchar(c) || is_whitespace(c) || is_obs_text(c))
            ret.reason_phrase.push_back(c);
        else
            return {};
    }

    ret.valid = true;
    return ret;
}

field_parser::field_parser(bool allow_obs_fold) : allow_obs_fold(allow_obs_fold) {}

bool field_parser::process_line(const std::string &line) {
    return process_line(line.data(), line.length());
}

/* WARNING: processing headers before reading all of them could be a security vulnerability
 *          due to combining multiple instances of a header into a list */

// Set-Cookie fields are processed independently: https://www.rfc-editor.org/rfc/rfc9110.html#section-5.3-4.1

// TODO: more specific error return codes
bool field_parser::process_line(const char *line, size_t length) {
    size_t pos = 0, field_value_from = 0, field_value_to = 0;
    bool folded = false;
    std::string field_name, field_value;

    // parse field-name
    field_name = parse_token_tolower(line, 0, length);
    if (!field_name.empty()) {
        pos += field_name.length();
        // check for semicolon
        if (!parse_char(line, pos, length, ':'))
            return false;
        pos++;

        // eat whitespace
        pos = parse_optional_whitespace(line, pos, length);
    } else if (allow_obs_fold) {
        // folded field (see obs-fold in RFC)
        std::tie(pos, folded) = parse_required_whitespace(line, 0, length);
        if (!folded)
            return false;

        // reject if there wasn't a previous field to fold from
        if (_previous_field.empty())
            return false;
    } else return false;

    // parse field-value
    field_value_from = pos;
    field_value_to = pos;
    for (size_t i = pos; i < length; i++) {
        // mark last non-whitespace character as end of value
        char c = line[i];
        if (is_vchar(c) || is_obs_text(c))
            field_value_to = i+1;
        else if (!is_whitespace(c))
            return false;
    }

    field_value.assign(line + field_value_from, field_value_to - field_value_from);

    // store processed field
    if (folded) {   // folded field line
        // merge with previous line, separated by a space
        std::string& field = _previous_field == "set-cookie" ? fields_set_cookie.back() : fields.at(_previous_field);
        field.push_back(' ');
        field.append(field_value);
    } else {        // regular field line
        if (field_name == "set-cookie") {
            // Set-Cookie headers are handled differently when appearing multiple times:
            // https://www.rfc-editor.org/rfc/rfc9110.html#section-5.3-4.1
            fields_set_cookie.push_back(field_value);
        } else {
            // other headers
            auto field = fields.find(field_name);
            if (field == fields.end()) {
                // first appearance of this header
                fields[field_name] = field_value;
            } else {
                // repeated appearance of this header
                // host header cannot appear multiple times
                if (field_name == "host")
                    return false;
                field->second.append(", ");
                field->second.append(field_value);
            }
        }

        _previous_field = std::move(field_name);
    }

    return true;
}

void field_parser::clear() {
    fields.clear();
    fields_set_cookie.clear();
    _previous_field.clear();
}

static constexpr size_t strlen_constexpr(const char *str) {
    const char *p = str;
    while (*p != 0)
        p++;
    return p - str;
}

static constexpr uint64_t date_hash_short(const char *str) {
    return date_hash_short(str, 0, strlen_constexpr(str));
}

// used for http dates, rejects unused stuff
static constexpr uint64_t date_hash_short(const char *str, size_t from, size_t to) {
    if (to - from != 3) // only 3-letter days/months
        return UINT64_MAX;
    uint64_t hash = UINT64_MAX;

    for (size_t i = from; i < to; i++) {
        hash *= 52;
        char c = str[i];
        if ('A' <= c && c <= 'Z')
            hash += c - 'A';
        else if ('a' <= c && c <= 'z')
            hash += c - 'a' + 26;
        else
            return UINT64_MAX;
    }

    return hash;
}

static month_enum parse_month(const char *str, size_t from, size_t to) {
    switch (date_hash_short(str, from, to)) {
    case date_hash_short("Jan"):
        return MONTH_JAN;
    case date_hash_short("Feb"):
        return MONTH_FEB;
    case date_hash_short("Mar"):
        return MONTH_MAR;
    case date_hash_short("Apr"):
        return MONTH_APR;
    case date_hash_short("May"):
        return MONTH_MAY;
    case date_hash_short("Jun"):
        return MONTH_JUN;
    case date_hash_short("Jul"):
        return MONTH_JUL;
    case date_hash_short("Aug"):
        return MONTH_AUG;
    case date_hash_short("Sep"):
        return MONTH_SEP;
    case date_hash_short("Oct"):
        return MONTH_OCT;
    case date_hash_short("Nov"):
        return MONTH_NOV;
    case date_hash_short("Dec"):
        return MONTH_DEC;
    default:
        return MONTH_INVALID;
    }
}

static parser_ret parse_time_of_day(const char *str, size_t from, size_t to, date_parser_inner_ret &ret) {
    size_t pos = from;

    // hour
    std::tie(pos, ret.valid, ret.hour) = parse_digits(str, pos, to, 2);
    if (!ret.valid)
        return {};

    // :
    if (!parse_char(str, pos, to, ':'))
        return {};
    pos++;

    // minute
    std::tie(pos, ret.valid, ret.minute) = parse_digits(str, pos, to, 2);
    if (!ret.valid)
        return {};

    // :
    if (!parse_char(str, pos, to, ':'))
        return {};
    pos++;

    // second
    std::tie(pos, ret.valid, ret.second) = parse_digits(str, pos, to, 2);
    if (!ret.valid)
        return {};

    return {pos, true};
}

static date_parser_inner_ret parse_date_imf(const char *str, size_t from, size_t to) {
    date_parser_inner_ret ret;
    size_t pos = from, pos_next = from;

    // day of week,
    pos_next = find_char(str, pos, to, ',');
    if (pos_next == to)     // no comma
        return {};
    pos = pos_next + 1;

    // space
    if (!parse_char(str, pos, to, ' '))
        return {};
    pos++;

    // day of month
    std::tie(pos, ret.valid, ret.day) = parse_digits(str, pos, to, 2);
    if (!ret.valid)
        return {};

    // space
    if (!parse_char(str, pos, to, ' '))
        return {};
    pos++;

    // month space
    pos_next = find_char(str, pos, to, ' ');
    if (pos_next == to) // no space after
        return {};
    ret.month = parse_month(str, pos, pos_next);
    if (ret.month == MONTH_INVALID)
        return {};
    pos = pos_next + 1;

    // year
    std::tie(pos, ret.valid, ret.year) = parse_digits(str, pos, to, 4);
    if (!ret.valid)
        return {};

    // space
    if (!parse_char(str, pos, to, ' '))
        return {};
    pos++;

    // time-of-day (hh:mm:ss)
    std::tie(pos, ret.valid) = parse_time_of_day(str, pos, to, ret);
    if (!ret.valid)
        return {};

    // space
    if (!parse_char(str, pos, to, ' '))
        return {};
    pos++;

    // GMT
    std::tie(pos, ret.valid) = parse_word(str, pos, to, "GMT");
    if (!ret.valid || pos != to)
        return {};

    ret.valid = true;
    return ret;
}

static date_parser_inner_ret parse_date_rfc850(const char *str, size_t from, size_t to) {
    size_t pos = from, pos_next = from;
    date_parser_inner_ret ret;

    // day of week,
    pos_next = find_char(str, pos, to, ',');
    if (pos_next == to)     // no comma
        return {};
    pos = pos_next + 1;

    // space
    if (!parse_char(str, pos, to, ' '))
        return {};
    pos++;

    // day of month
    std::tie(pos, ret.valid, ret.day) = parse_digits(str, pos, to, 2);
    if (!ret.valid)
        return {};

    // -
    if (!parse_char(str, pos, to, '-'))
        return {};
    pos++;

    // month-
    pos_next = find_char(str, pos, to, '-');
    if (pos_next == to) // no space after
        return {};
    ret.month = parse_month(str, pos, pos_next);
    if (ret.month == MONTH_INVALID)
        return {};
    pos = pos_next + 1;

    // year
    std::tie(pos, ret.valid, ret.year) = parse_digits(str, pos, to, 2);
    if (!ret.valid)
        return {};

    // space
    if (!parse_char(str, pos, to, ' '))
        return {};
    pos++;

    // time-of-day (hh:mm:ss)
    std::tie(pos, ret.valid) = parse_time_of_day(str, pos, to, ret);
    if (!ret.valid)
        return {};

    // space
    if (!parse_char(str, pos, to, ' '))
        return {};
    pos++;

    // GMT
    std::tie(pos, ret.valid) = parse_word(str, pos, to, "GMT");
    if (!ret.valid || pos != to)
        return {};

    // convert 2-digit year to 4-digit year
    time_t current_time = std::time(NULL);
    struct tm current_time_tm;
    if (!gmtime_r(&current_time, &current_time_tm))
        return {};
    // limit timestamp to no more than 50 years in the future (ignoring month/day differences)
    int year_in_50y = current_time_tm.tm_year + 1950;
    int YY_in_50y = year_in_50y % 100;
    int century_in_50y = year_in_50y - YY_in_50y;
    ret.year += century_in_50y - (ret.year > YY_in_50y) * 100;

    ret.valid = true;
    return ret;
}

static date_parser_inner_ret parse_date_asctime(const char *str, size_t from, size_t to) {
    date_parser_inner_ret ret;
    size_t pos = from, pos_next = from;

    // day of week space
    pos_next = find_char(str, pos, to, ' ');
    if (pos_next == to)     // no space
        return {};
    pos = pos_next + 1;

    // month space
    pos_next = find_char(str, pos, to, ' ');
    if (pos_next == to) // no space after
        return {};
    ret.month = parse_month(str, pos, pos_next);
    if (ret.month == MONTH_INVALID)
        return {};
    pos = pos_next + 1;

    // day of month
    if (parse_char(str, pos, to, ' ')) {
        // single digit
        std::tie(pos, ret.valid, ret.day) = parse_digits(str, pos + 1, to, 1);
    } else {
        // double digit
        std::tie(pos, ret.valid, ret.day) = parse_digits(str, pos, to, 2);
    }

    if (!ret.valid)
        return {};

    // space
    if (!parse_char(str, pos, to, ' '))
        return {};
    pos++;

    // time-of-day (hh:mm:ss)
    std::tie(pos, ret.valid) = parse_time_of_day(str, pos, to, ret);
    if (!ret.valid)
        return {};

    // space
    if (!parse_char(str, pos, to, ' '))
        return {};
    pos++;

    // year
    std::tie(pos, ret.valid, ret.year) = parse_digits(str, pos, to, 4);
    if (!ret.valid || pos != to)
        return {};

    ret.valid = true;
    return ret;
}

time_t parse_field_date(const std::string &str) {
    return parse_field_date(str.data(), 0, str.length());
}

time_t parse_field_date(const std::string &str, size_t from, size_t to) {
    verify_range(str, from, to);
    return parse_field_date(str.data(), from, to);
}

// used directly for fields such as: Date, If-(Un)modified-Since, Last-Modified, and indirectly for others (e.g. Retry-After)
time_t parse_field_date(const char *str, size_t from, size_t to) {
    date_parser_inner_ret inner_ret;

    // try parsing all 3 date formats
    inner_ret = parse_date_imf(str, from, to);
    if (!inner_ret.valid) {
        inner_ret = parse_date_rfc850(str, from, to);
        if (!inner_ret.valid) {
            inner_ret = parse_date_asctime(str, from, to);
            if (!inner_ret.valid)
                return -1L;     // all invalid
        }
    }

    if (inner_ret.second == 60) // leap second
        inner_ret.second--;
    if (inner_ret.day == 0)
        return -1L;
    if (inner_ret.year < 1970)  // limit the year to >= 1970 to simplify error handling,
        return -1L;             // and because _mkgmtime on Windows doesn't seem to support pre-epoch time

    // convert to seconds since epoch
    struct tm time_split, validate;
    time_split.tm_sec  = inner_ret.second;
    time_split.tm_min  = inner_ret.minute;
    time_split.tm_hour = inner_ret.hour;
    time_split.tm_mday = inner_ret.day;
    time_split.tm_mon  = inner_ret.month - MONTH_JAN;
    time_split.tm_year = inner_ret.year - 1900;
    time_split.tm_wday   = 0;
    time_split.tm_yday   = 0;
    time_split.tm_isdst  = 0;
    time_split.tm_gmtoff = 0;
    time_split.tm_zone   = NULL;
    validate = time_split;
    time_t timestamp;
#ifdef _WIN32
    // TODO: test this in Windows
    timestamp = _mkgmtime(&time_split);
#else
    timestamp = timegm(&time_split);
#endif
    if (timestamp < 0)  // error or <1970
        return -1L;

    // make sure the original date was valid
    if (time_split.tm_sec  != validate.tm_sec  ||
        time_split.tm_min  != validate.tm_min  ||
        time_split.tm_hour != validate.tm_hour ||
        time_split.tm_mday != validate.tm_mday ||
        time_split.tm_mon  != validate.tm_mon  ||
        time_split.tm_year != validate.tm_year)
        return -1L;

    return timestamp;
}

std::string timestamp_to_string(time_t timestamp) {
    // returns emtpy string on error
    // no dates pre-epoch
    if (timestamp < 0)
        return "";

    // convert to split-type
    struct tm t;
#ifdef _WIN32
    // TODO: test this on Windows
    if (gmtime_s(&t, &timestamp))
        return "";
#else
    if (!gmtime_r(&timestamp, &t))
        return "";
#endif

    // only 4-digit years
    if (t.tm_year > (9999 - 1900))
        return "";

    // sanity check, even though these checks will theoretically never fail
    if ((size_t)t.tm_wday >= sizeof(day_names) || (size_t)t.tm_mon > sizeof(month_names))
        return "";

    // print the formatted date into the internal buffer of a std::string
    const size_t date_buffer_size = 30;
    std::string date_buffer(30, 0);
    int date_len = std::snprintf(date_buffer.data(), date_buffer_size,
                                "%s, %02d %s %04d %02d:%02d:%02d GMT",
                                day_names[t.tm_wday], t.tm_mday, month_names[t.tm_mon], t.tm_year + 1900,
                                t.tm_hour, t.tm_min, t.tm_sec);
    if ((size_t) date_len >= date_buffer_size)  // sanity check snprintf errors, should never happen
        return "";
    // TODO: log error if sanity checks fail

    date_buffer.resize(date_len, ' ');
    return date_buffer;
}

quoted_ret parse_quoted_str(const std::string &str) {
    return parse_quoted_str(str.data(), 0, str.length());
}

quoted_ret parse_quoted_str(const std::string &str, size_t from, size_t to) {
    verify_range(str, from, to);
    return parse_quoted_str(str.data(), from, to);
}

quoted_ret parse_quoted_str(const char *str, size_t from, size_t to) {
    std::string parsed;

    // opening "
    if (!parse_char(str, from, to, '"'))
        return {0, false, ""};

    for (size_t i = from + 1; i < to; i++) {
        char c = str[i];
        if (is_qdtext(c)) {     // regular char
            parsed.push_back(c);
        } else if (c == '\\') { // escape sequence (quoted-pair)
            // get next char
            if (++i >= to)
                return {to, false, ""};
            char c2 = str[i];
            if (is_vchar(c2) || c2 == ' ' || c2 == '\t' || is_obs_text(c2))
                parsed.push_back(c2);
            else    // invalid char
                return {i, false, ""};
        } else if (c == '"') {  // closing "
            return {i+1, true, parsed};
        } else return {i, false, ""};   // invalid char
    }

    // no closing "
    return {to, false, ""};
}

parser_ret parse_comment(const std::string &str) {
    return parse_comment(str.data(), 0, str.length());
}

parser_ret parse_comment(const std::string &str, size_t from, size_t to) {
    verify_range(str, from, to);
    return parse_comment(str.data(), from, to);
}

parser_ret parse_comment(const char *str, size_t from, size_t to) {
    // opening (
    if (!parse_char(str, from, to, '('))
        return {};

    for (size_t i = from + 1; i < to;) {
        char c = str[i];
        if (c == '\\') {        // escape sequence (quoted-pair)
            // get next char
            if (++i >= to)
                return {to, false};
            char c2 = str[i];
            if (is_vchar(c2) || c2 == ' ' || c2 == '\t' || is_obs_text(c2))
                i++;
            else
                return {i, false};
        } else if (c == '(') {  // nested comment
            bool valid;
            std::tie(i, valid) = parse_comment(str, i, to);
            if (!valid)
                return {i, false};
        } else if (c == ')') {  // closing )
            return {i+1, true};
        } else if (!(is_vchar(c) || c == ' ' || c == '\t' || is_obs_text(c))) {  // invalid char
            return {i, false};
        } else i++;
    }

    // no closing )
    return {to, false};
}

parameters_ret parse_parameters(const std::string &str, bool allow_bad_whitespace) {
    return parse_parameters(str.data(), 0, str.length(), allow_bad_whitespace);
}

parameters_ret parse_parameters(const std::string &str, size_t from, size_t to, bool allow_bad_whitespace) {
    verify_range(str, from, to);
    return parse_parameters(str.data(), from, to, allow_bad_whitespace);
}

parameters_ret parse_parameters(const char *str, size_t from, size_t to, bool allow_bad_whitespace) {
    parameters_ret ret;
    ret.to = from;
    size_t pos = from;

    while (true) {
        // optional whitespace ; optional whitespace
        pos = parse_optional_whitespace(str, pos, to);
        if (!parse_char(str, pos, to, ';'))
            return ret;
        pos++;
        pos = parse_optional_whitespace(str, pos, to);
        ret.to = pos;

        // optional param specification
        // parameter-name
        std::string param_name = parse_token_tolower(str, pos, to);
        if (param_name.empty())
            continue;
        pos += param_name.length();

        if (allow_bad_whitespace)   // bad whitespace
            pos = parse_optional_whitespace(str, pos, to);

        // =
        if (!parse_char(str, pos, to, '='))
            return ret;
        pos++;

        if (allow_bad_whitespace)   // bad whitespace
            pos = parse_optional_whitespace(str, pos, to);

        // parameter-value (token or quoted-string)
        std::string param_value;
        auto [param_value_to, param_value_valid] = parse_token(str, pos, to);
        if (param_value_valid) {    // token
            param_value.assign(str + pos, param_value_to - pos);
            pos = param_value_to;
        } else {    // quoted-string
            std::tie(param_value_to, param_value_valid, param_value) = parse_quoted_str(str, pos, to);
            if (param_value_valid)
                pos = param_value_to;
            else    // invalid
                return ret;
        }

        /* NOTE: The charset param's value is supposed to be case-insensitive (e.g. "utf-8"="UTF-8").
         *       Make sure to use case-insensitive comparisons when checking it.
         */

        ret.params[std::move(param_name)] = std::move(param_value);
        ret.to = pos;
    }
}

parser_ret parse_list(const std::string &str,
                      const std::function<parser_ret(const char*, size_t, size_t)> &element_parser) {
    return parse_list(str.data(), 0, str.length(), element_parser);
}

parser_ret parse_list(const std::string &str, size_t from, size_t to,
                      const std::function<parser_ret(const char*, size_t, size_t)> &element_parser) {
    verify_range(str, from, to);
    return parse_list(str.data(), from, to, element_parser);
}

parser_ret parse_list(const char *str, size_t from, size_t to,
                      const std::function<parser_ret(const char*, size_t, size_t)> &element_parser) {
    /* NOTE: element_parser should try to parse until it finds the first invalid character
     *       (possibly a comma or space) and return its position, along with valid=true.
     *       When it returns invalid, it SHOULDN'T store/use its last element. The caller
     *       of parse_list should ignore any saved results if parse_list returns invalid,
     *       and confirm whether it reached end-of-line if that's required.
     */

    size_t pos = from, empty_count = 0, last_valid = from;

    while (true) {
        // list element
        auto [pos_next, valid] = element_parser(str, pos, to);
        if (valid) {    // if invalid, check if it's just an empty element and skip it
            pos = pos_next;
            last_valid = pos;
        }

        // [whitespace] , [whitespace]
        pos = parse_optional_whitespace(str, pos, to);
        size_t pos_comma = pos;
        if (!parse_char(str, pos, to, ',')) {
            if (valid)      // last element was valid => possible end of list
                return {pos_next, true};
            else if (pos >= to)     // ending with an empty element => valid
                return {to, true};
            else            // last element invalid => return upto last valid spot in list
                return {last_valid, true};
        }
        pos++;
        pos = parse_optional_whitespace(str, pos, to);

        // skip a reasonable amount of empty elements, as specified by the RFC
        if (!valid && (++empty_count >= STRTB_HTTP_PARSE_LIST_MAX_EMPTY_ELEMENTS))
            return {pos_comma, false};  // only returns valid=false when too many empty elements
    }
}

token_list_ret parse_field_token_list(const std::string &field_value, bool case_sensitive) {
    return parse_field_token_list(field_value.data(), 0, field_value.length(), case_sensitive);
}

token_list_ret parse_field_token_list(const std::string &field_value, size_t from, size_t to, bool case_sensitive) {
    verify_range(field_value, from, to);
    return parse_field_token_list(field_value.data(), from, to, case_sensitive);
}

// simple token list, used by headers such as: Connection, Content-Encoding, Content-Language, Allow, Trailer, Vary
token_list_ret parse_field_token_list(const char *field_value, size_t from, size_t to, bool case_sensitive) {
    std::vector<std::string> list;
    size_t list_to = 0;
    bool valid = false;

    if (!case_sensitive) {
        std::tie(list_to, valid) = parse_list(field_value, from, to,
            [&list](const char *str, size_t from, size_t to) -> parser_ret {
            std::string element = parse_token_tolower(str, from, to);
            parser_ret ret = {from + element.length(), !element.empty()};
            if (!element.empty())
                list.emplace_back(std::move(element));
            return ret;
        });
    } else {
        std::tie(list_to, valid) = parse_list(field_value, from, to,
            [&list](const char *str, size_t from, size_t to) -> parser_ret {
            auto r = parse_token(str, from, to);
            if (r.second)
                list.emplace_back(str + from, r.first - from);
            return r;
        });
    }

    if (valid && list_to == to)
        return {true, std::move(list)};
    else
        return {};
}

product_ret parse_product_or_protocol(const std::string &str) {
    return parse_product_or_protocol(str.data(), 0, str.length());
}

product_ret parse_product_or_protocol(const std::string &str, size_t from, size_t to) {
    verify_range(str, from, to);
    return parse_product_or_protocol(str.data(), from, to);
}

product_ret parse_product_or_protocol(const char *str, size_t from, size_t to) {
    size_t pos = from;
    std::string name, version;

    // name
    auto [pos_after_name, valid] = parse_token(str, pos, to);
    if (!valid)
        return {pos_after_name, false, {}};
    name.assign(str + pos, pos_after_name - pos);
    pos = pos_after_name;

    // /version (optional)
    if (!parse_char(str, pos, to, '/'))
        return {pos_after_name, true, {std::move(name), std::move(version)}};
    pos++;
    size_t pos_after_version;
    std::tie(pos_after_version, valid) = parse_token(str, pos, to);
    if (!valid)
        return {pos_after_name, true, {std::move(name), std::move(version)}};
    version.assign(str + pos, pos_after_version - pos);

    return {pos_after_version, true, {std::move(name), std::move(version)}};
}

product_list_ret parse_field_upgrade(const std::string &field_value) {
    return parse_field_upgrade(field_value.data(), 0, field_value.length());
}

product_list_ret parse_field_upgrade(const std::string &field_value, size_t from, size_t to) {
    verify_range(field_value, from, to);
    return parse_field_upgrade(field_value.data(), from, to);
}

product_list_ret parse_field_upgrade(const char* field_value, size_t from, size_t to) {
    std::vector<product> list;

    auto [list_to, valid] = parse_list(field_value, from, to,
        [&list](const char* str, size_t from, size_t to) -> parser_ret {
        auto ret = parse_product_or_protocol(str, from, to);
        // NOTE: The protocol name should be case insensitive, but protocols have a preferred case
        // TODO: think about how to handle this
        if (ret.valid)
            list.push_back(std::move(ret.pr));
        return {ret.to, ret.valid};
    });

    if (valid && list_to == to)
        return {true, std::move(list)};
    else
        return {};
}

media_type_ret parse_media_type(const std::string &str) {
    return parse_media_type(str.data(), 0, str.length());
}

media_type_ret parse_media_type(const std::string &str, size_t from, size_t to) {
    verify_range(str, from, to);
    return parse_media_type(str.data(), from, to);
}

media_type_ret parse_media_type(const char *str, size_t from, size_t to) {
    size_t pos = from;

    // type
    std::string type = parse_token_tolower(str, pos, to);
    if (type.empty())
        return {pos, false, {}};
    pos += type.length();

    // /
    if (!parse_char(str, pos, to, '/'))
        return {pos, false, {}};
    pos++;

    // subtype
    std::string subtype = parse_token_tolower(str, pos, to);
    if (subtype.empty())
        return {pos, false, {}};
    pos += subtype.length();

    // params
    auto [pos_next, params] = parse_parameters(str, pos, to);

    return {pos_next, true, {std::move(type), std::move(subtype), std::move(params)}};
}

content_type_ret parse_field_content_type(const std::string &field_value) {
    return parse_field_content_type(field_value.data(), 0, field_value.length());
}

content_type_ret parse_field_content_type(const std::string &field_value, size_t from, size_t to) {
    verify_range(field_value, from, to);
    return parse_field_content_type(field_value.data(), from, to);
}

content_type_ret parse_field_content_type(const char *field_value, size_t from, size_t to) {
    auto [mt_to, valid, media_type] = parse_media_type(field_value, from, to);
    if (mt_to != to)
        return {};

    return {true, std::move(media_type)};
}

integer_ret parse_integer(const std::string &str) {
    return parse_integer(str.data(), 0, str.length());
}

integer_ret parse_integer(const std::string &str, size_t from, size_t to) {
    verify_range(str, from, to);
    return parse_integer(str.data(), from, to);
}

integer_ret parse_integer(const char *str, size_t from, size_t to) {
    unsigned long long number = 0;
    size_t i;

    for (i = from; i < to; i++) {
        char c = str[i];
        if (!is_digit(c))
            break;
        unsigned int n = c - '0';

        // overflow check
        if (number > (ULONG_LONG_MAX - n) / 10)
            return {i, false, true, 0};

        number = number * 10 + n;
    }

    return {i, i > from, false, number};
}

integer_field_ret parse_field_integer(const std::string &field_value) {
    return parse_field_integer(field_value.data(), 0, field_value.length());
}

integer_field_ret parse_field_integer(const std::string &field_value, size_t from, size_t to) {
    verify_range(field_value, from, to);
    return parse_field_integer(field_value.data(), from, to);
}

// field that only contains a non-negative integer number, used by: Content-Length, Max-Forwards, possibly for Retry-After
integer_field_ret parse_field_integer(const char *field_value, size_t from, size_t to) {
    auto ret = parse_integer(field_value, from, to);
    if (!ret.valid || ret.to != to)     // invalid/overflown, or extra stuff after number
        return {false, ret.overflow, 0};
    return {true, false, ret.number};
}

abs_or_part_uri_field_ret parse_field_abs_or_part_uri(const std::string &field_value) {
    return parse_field_abs_or_part_uri(field_value.data(), 0, field_value.length());
}

abs_or_part_uri_field_ret parse_field_abs_or_part_uri(const std::string &field_value, size_t from, size_t to) {
    verify_range(field_value, from, to);
    return parse_field_abs_or_part_uri(field_value.data(), from, to);
}

// used for headers such as: Content-Location, Referer
abs_or_part_uri_field_ret parse_field_abs_or_part_uri(const char *field_value, size_t from, size_t to) {
    uri::parser uri;
    bool is_partial = false;

    if (!uri.parse_uri(field_value, from, to).second) { // try parsing as absolute
        if (!uri.parse_relative_ref(field_value, from, to).second)  // try parsing as partial
            return {};  // fully invalid
        is_partial = true;
    }

    // no fragment allowed
    if (uri.fragment_to)
        return {};

    return {true, is_partial, uri};
}

entity_tag_ret parse_etag(const std::string &str) {
    return parse_etag(str.data(), 0, str.length());
}

entity_tag_ret parse_etag(const std::string &str, size_t from, size_t to) {
    verify_range(str, from, to);
    return parse_etag(str.data(), from, to);
}

entity_tag_ret parse_etag(const char *str, size_t from, size_t to) {
    bool is_weak = false;
    size_t pos = from, pos_next = from;

    // optional weak indicator
    std::tie(pos_next, is_weak) = parse_word(str, pos, to, "W/");
    if (is_weak)
        pos = pos_next;

    // opening quote
    if (!parse_char(str, pos, to, '"'))
        return {pos, false, {}};    // invalid
    pos++;

    // check the rest of the characters
    for (size_t i = pos; i < to; i++) {
        char c = str[i];
        if (c == '"')   // closing quote
            return {i+1, true, {is_weak, std::string(str + pos, i - pos)}};
        else if (!(is_vchar(c) || is_obs_text(c)))
            return {i, false, {}};  // invalid
    }

    // closing quote not found
    return {to, false, {}};         // invalid
}

etag_field_ret parse_field_etag(const std::string &field_value) {
    return parse_field_etag(field_value.data(), 0, field_value.length());
}

etag_field_ret parse_field_etag(const std::string &field_value, size_t from, size_t to) {
    verify_range(field_value, from, to);
    return parse_field_etag(field_value.data(), from, to);
}

etag_field_ret parse_field_etag(const char *field_value, size_t from, size_t to) {
    entity_tag_ret ret = parse_etag(field_value, from, to);
    if (ret.valid && ret.to == to)
        return {true, std::move(ret.etag)};
    else
        return {};
}

bool etag_compare(const entity_tag &a, const entity_tag &b, bool strong) {
    // strong comparison requires both tags to not be weak
    if (strong && (a.is_weak || b.is_weak))
        return false;

    // contents must match
    if (a.tag.length() != b.tag.length())
        return false;
    for (size_t i = 0; i < a.tag.length(); i++)
        if (a.tag[i] != b.tag[i])
            return false;

    return true;
}

std::string etag_to_string(const entity_tag &etag) {
    using namespace std::string_literals;

    std::string str;
    if (etag.is_weak)
        str += "W/";

    str.push_back('"');     // opening quote
    for (char c : etag.tag) {   // check for invalid characters
        if ((is_vchar(c) && c != '"') || is_obs_text(c))
            str.push_back(c);
        else
            throw std::invalid_argument("found invalid character "s + strtb::common::char_escape(c) + " in tag");
    }
    str.push_back('"');     // closing quote

    return str;
}

expect_field_ret parse_field_expect(const std::string &field_value) {
    return parse_field_expect(field_value.data(), 0, field_value.length());
}

expect_field_ret parse_field_expect(const std::string &field_value, size_t from, size_t to) {
    verify_range(field_value, from, to);
    return parse_field_expect(field_value.data(), from, to);
}

expect_field_ret parse_field_expect(const char *field_value, size_t from, size_t to) {
    std::vector<expectation> list;

    auto [list_to, valid] = parse_list(field_value, from, to,
        [&list](const char *str, size_t from, size_t to) -> parser_ret {
        std::string name;
        size_t pos = from, pos_next = from;
        bool valid = false;

        // name
        name = parse_token_tolower(str, pos, to);
        if (name.empty())
            return {pos, false};
        pos += name.size();
        size_t pos_after_name = pos;

        // =
        if (!parse_char(str, pos, to, '=')) {
            list.push_back({std::move(name), std::string()});
            return {pos_after_name, true};
        }
        pos++;

        // value
        std::string value = parse_token_tolower(str, pos, to);  // try token
        if (value.empty()) {
            std::tie(pos_next, valid, value) = parse_quoted_str(str, pos, to);  // try quoted str
            if (!valid)
                return {pos, false};

            // to lowercase
            for (char &c : value)
                c = to_lower(c);
            pos = pos_next;
        } else {
            pos += value.size();
        }

        // parameters
        auto [pos_last, params] = parse_parameters(str, pos, to);

        list.push_back({std::move(name), std::move(value), std::move(params)});
        return {pos_last, true};
    });

    if (valid && list_to == to)
        return {true, std::move(list)};
    else
        return {};
}

token_params_list_ret parse_field_token_params_list(const std::string &field_value,
                                                    bool token_case_sensitive, bool allow_bad_whitespace) {
    return parse_field_token_params_list(field_value.data(), 0, field_value.length(), token_case_sensitive, allow_bad_whitespace);
}

token_params_list_ret parse_field_token_params_list(const std::string &field_value, size_t from, size_t to,
                                                    bool token_case_sensitive, bool allow_bad_whitespace) {
    verify_range(field_value, from, to);
    return parse_field_token_params_list(field_value.data(), from, to, token_case_sensitive, allow_bad_whitespace);
}

// used in fields such as: TE, Accept-Charset, Accept-Encoding, Accept-Language
token_params_list_ret parse_field_token_params_list(const char *field_value, size_t from, size_t to,
                                                    bool token_case_sensitive, bool allow_bad_whitespace) {
    std::vector<token_params> list;

    auto [list_to, valid] = parse_list(field_value, from, to,
        [&list, token_case_sensitive, allow_bad_whitespace](const char *str, size_t from, size_t to) -> parser_ret {
        size_t pos = from;
        std::string token;

        // token
        if (token_case_sensitive) {
            auto [pos_next, valid] = parse_token(str, pos, to);
            if (!valid)
                return {pos, false};
            token.assign(str + pos, pos_next - pos);
            pos = pos_next;
        } else {
            token = parse_token_tolower(str, pos, to);
            if (token.empty())
                return {pos, false};
            pos += token.size();
        }

        // parameters
        auto [pos_final, params] = parse_parameters(str, pos, to, allow_bad_whitespace);

        list.push_back({std::move(token), std::move(params)});
        return {pos_final, true};
    });

    if (valid && list_to == to)
        return {true, std::move(list)};
    else
        return {};
}

product_field_ret parse_field_product_info(const std::string &field_value) {
    return parse_field_product_info(field_value.data(), 0, field_value.length());
}

product_field_ret parse_field_product_info(const std::string &field_value, size_t from, size_t to) {
    verify_range(field_value, from, to);
    return parse_field_product_info(field_value.data(), from, to);
}

product_field_ret parse_field_product_info(const char *field_value, size_t from, size_t to) {
    std::vector< std::variant<product, std::string> > list;

    // first item must be a product
    auto [pos, valid, product] = parse_product_or_protocol(field_value, from, to);
    if (!valid)
        return {};
    list.push_back(std::move(product));

    // all other products and comments
    while (pos < to) {
        std::tie(pos, valid) = parse_required_whitespace(field_value, pos, to);
        if (!valid) // no space between parts
            return {};

        if (field_value[pos] == '(') {  // comment
            auto [pos_next, valid] = parse_comment(field_value, pos, to);
            if (!valid)
                return {};
            list.emplace_back(std::string(field_value + pos, pos_next - pos));
            pos = pos_next;
        } else {    // product info
            auto [pos_next, valid, product] = parse_product_or_protocol(field_value, pos, to);
            if (!valid)
                return {};
            list.emplace_back(std::move(product));
            pos = pos_next;
        }
    }

    return {true, std::move(list)};
}

auth_params_ret parse_auth_params(const std::string &str) {
    return parse_auth_params(str.data(), 0, str.length());
}

auth_params_ret parse_auth_params(const std::string &str, size_t from, size_t to) {
    verify_range(str, from, to);
    return parse_auth_params(str.data(), from, to);
}

auth_params_ret parse_auth_params(const char *str, size_t from, size_t to) {
    parameter_map params;
    bool params_duplicate = false;

    auto [list_to, valid] = parse_list(str, from, to,
        [&params, &params_duplicate](const char *str, size_t from, size_t to) -> parser_ret {
        size_t pos = from;
        if (params_duplicate)
            return {from, false};

        // key
        std::string key = parse_token_tolower(str, pos, to);
        if (key.empty())
            return {from, false};
        pos += key.length();

        // bad whitespace
        pos = parse_optional_whitespace(str, pos, to);

        // =
        if (!parse_char(str, pos, to, '='))
            return {pos, false};
        pos++;

        // bad whitespace
        pos = parse_optional_whitespace(str, pos, to);

        // value (token or quoted string)
        std::string value;
        auto [pos_next, valid] = parse_token(str, pos, to);     // try token
        if (valid) {
            value.assign(str + pos, pos_next - pos);
        } else {
            std::tie(pos_next, valid, value) = parse_quoted_str(str, pos, to);  // try quoted-string
            if (!valid)
                return {pos, false};
        }
        pos = pos_next;

        size_t params_size = params.size();
        params[std::move(key)] = std::move(value);

        // deny duplicates for security (checked here to prevent confusion with token68)
        if (params.size() == params_size) {
            params_duplicate = true;
            return {pos, false};
        }

        return {pos, true};
    });

    if (valid)
        return {list_to, true, params_duplicate, std::move(params)};
    else
        return {list_to, false, params_duplicate, {}};
}

credentials_ret parse_credentials_or_challenge(const std::string &str) {
    return parse_credentials_or_challenge(str.data(), 0, str.length());
}

credentials_ret parse_credentials_or_challenge(const std::string &str, size_t from, size_t to) {
    verify_range(str, from, to);
    return parse_credentials_or_challenge(str.data(), from, to);
}

credentials_ret parse_credentials_or_challenge(const char *str, size_t from, size_t to) {
    size_t pos = from;

    // auth-scheme
    std::string auth_scheme = parse_token_tolower(str, pos, to);
    if (auth_scheme.empty())
        return {};
    pos += auth_scheme.length();

    // optional parts
    size_t pos_pre_space = pos;
    // space
    if (!parse_char(str, pos, to, ' '))
        return {pos, true, {std::move(auth_scheme), false}};
    pos++;

    // try both token68 and #auth-param
    auto [token68_to, token68_valid] = parse_token68(str, pos, to);
    auto [params_to, params_valid, params_duplicate, params] = parse_auth_params(str, pos, to);

    if (params_duplicate)   // instantly reject duplicate params for security
        return {};
    else if (token68_valid && (!params_valid || token68_to > params_to))    // return token68
        return {token68_to, true, {std::move(auth_scheme), std::string(str + pos, token68_to - pos)}};
    else if (params_valid && !params.empty())   // return params
        return {params_to, true, {std::move(auth_scheme), std::move(params)}};
    else
        return {pos_pre_space, true, {std::move(auth_scheme), false}};
}

authenticate_field_ret parse_field_authenticate(const std::string &field_value) {
    return parse_field_authenticate(field_value.data(), 0, field_value.length());
}

authenticate_field_ret parse_field_authenticate(const std::string &field_value, size_t from, size_t to) {
    verify_range(field_value, from, to);
    return parse_field_authenticate(field_value.data(), from, to);
}

// Used by fields: WWW-Authenticate, Proxy-Authenticate
authenticate_field_ret parse_field_authenticate(const char *field_value, size_t from, size_t to) {
    std::vector<credentials> list;

    auto [list_to, valid] = parse_list(field_value, from, to,
        [&list](const char *str, size_t from, size_t to) -> parser_ret {
        auto ret = parse_credentials_or_challenge(str, from, to);

        if (!ret.valid)
            return {ret.to, false};

        list.push_back(std::move(ret.creds));
        return {ret.to, true};
    });

    if (valid && list_to == to)
        return {true, std::move(list)};
    else
        return {};
}

authorization_field_ret parse_field_authorization(const std::string &field_value) {
    return parse_field_authorization(field_value.data(), 0, field_value.length());
}

authorization_field_ret parse_field_authorization(const std::string &field_value, size_t from, size_t to) {
    verify_range(field_value, from, to);
    return parse_field_authorization(field_value.data(), from, to);
}

// Used by fields: Authorization, Proxy-Authorization
authorization_field_ret parse_field_authorization(const char *field_value, size_t from, size_t to) {
    auto ret = parse_credentials_or_challenge(field_value, from, to);
    if (ret.valid && ret.to == to)
        return {true, std::move(ret.creds)};
    else
        return {};
}

auth_params_field_ret parse_field_authentication_info(const std::string &field_value) {
    return parse_field_authentication_info(field_value.data(), 0, field_value.length());
}

auth_params_field_ret parse_field_authentication_info(const std::string &field_value, size_t from, size_t to) {
    verify_range(field_value, from, to);
    return parse_field_authentication_info(field_value.data(), from, to);
}

// Used by fields: Authentication-Info, Proxy-Authentication-Info
auth_params_field_ret parse_field_authentication_info(const char *field_value, size_t from, size_t to) {
    auto ret = parse_auth_params(field_value, from, to);
    if (ret.valid && !ret.duplicate && ret.to == to)
        return {ret.valid, std::move(ret.params)};
    else
        return {};
}

accept_field_ret parse_field_accept(const std::string &field_value) {
    return parse_field_accept(field_value.data(), 0, field_value.length());
}

accept_field_ret parse_field_accept(const std::string &field_value, size_t from, size_t to) {
    verify_range(field_value, from, to);
    return parse_field_accept(field_value.data(), from, to);
}

accept_field_ret parse_field_accept(const char *field_value, size_t from, size_t to) {
    std::vector<media_type> list;

    auto [list_to, valid] = parse_list(field_value, from, to,
        [&list](const char *str, size_t from, size_t to) -> parser_ret {
        auto ret = parse_media_type(str, from, to);
        if (!ret.valid)
            return {ret.to, false};

        list.push_back(std::move(ret.m));
        return {ret.to, true};
    });

    if (valid && list_to == to)
        return {true, std::move(list)};
    else
        return {};
}

}