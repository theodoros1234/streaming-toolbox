#include "protocol.h"

#include <cstdint>
#include <stdexcept>
#include <map>
#include <ctime>
#include "../logging/logging.h"

namespace strtb::http {

static std::map<unsigned int, const char*> status_code_phrases = {
    {100, "Continue"},
    {101, "Switching Protocols"},
    {200, "OK"},
    {201, "Created"},
    {202, "Accepted"},
    {203, "Non-Authoritative Information"},
    {204, "No Content"},
    {205, "Reset Content"},
    {206, "Partial Content"},
    {300, "Multiple Choices"},
    {301, "Moved Permamently"},
    {302, "Found"},
    {303, "See Other"},
    {304, "Not Modified"},
    {305, "Use Proxy"},
    {307, "Temporary Redirect"},
    {308, "Permanent Redirect"},
    {400, "Bad Request"},
    {401, "Unauthorized"},
    {402, "Payment Required"},
    {403, "Forbidden"},
    {404, "Not Found"},
    {405, "Method Not Allowed"},
    {406, "Not Acceptable"},
    {407, "Proxy Authentication Required"},
    {408, "Request Timeout"},
    {409, "Conflict"},
    {410, "Gone"},
    {411, "Length Required"},
    {412, "Precondition Failed"},
    {413, "Content Too Large"},
    {414, "URI Too Long"},
    {415, "Unsupported Media Type"},
    {416, "Range Not Satisfiable"},
    {417, "Expectation Failed"},
    {418, "I'm a teapot"},
    {421, "Misdirected Request"},
    {422, "Unprocessable Content"},
    {426, "Upgrade Required"},
    {428, "Precondition Required"},
    {429, "Too Many Requests"},
    {431, "Request header Fields Too Large"},
    {500, "Internal Server Error"},
    {501, "Not Implemented"},
    {502, "Bad Gateway"},
    {503, "Service Unavailable"},
    {504, "Gateway Timeout"},
    {505, "HTTP Version Not Supported"},
    {511, "Network Authentication Required"}
};

typedef struct date_parser_inner_ret {
    uint64_t year = 0, day = 0, hour = 0, minute = 0, second = 0;
    month_enum month = MONTH_INVALID;
    day_enum day_of_week = DAY_INVALID;
    bool valid = false;
} date_parser_inner_ret;

static size_t parse_optional_whitespace(const char *str, size_t from, size_t to);
static parser_ret parse_required_whitespace(const char *str, size_t from, size_t to);
static parser_ret parse_word(const char *str, size_t from, size_t to, const char *word);
static size_t find_char(const char *str, size_t from, size_t to, char c);
static std::tuple<size_t, bool, unsigned int> parse_digits(const char *str, size_t from, size_t to, size_t digits);
static constexpr size_t strlen_constexpr(const char *str);
static constexpr uint64_t date_hash_short(const char *str);
static constexpr uint64_t date_hash_short(const char *str, size_t from, size_t to);
static constexpr uint64_t date_hash_long(const char *str);
static constexpr uint64_t date_hash_long(const char *str, size_t from, size_t to);
parser_ret parse_time_of_day(const char *str, size_t from, size_t to, date_parser_inner_ret &ret);
date_parser_inner_ret parse_date_imf(const char *str, size_t from, size_t to);
date_parser_inner_ret parse_date_rfc850(const char *str, size_t from, size_t to);
date_parser_inner_ret parse_date_asctime(const char *str, size_t from, size_t to);

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
    auto [field_name_size, field_name_valid] = parse_token(line, 0, length);
    if (field_name_valid) {
        field_name.reserve(field_name_size);
        // copy field-name in lowercase
        for (const char *c = line; c < line + field_name_size; c++)
            field_name.push_back('A' <= *c && *c <= 'Z' ? *c + ('a' - 'A') : *c);

        pos = field_name_size;
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

const char* get_status_code_phrase(int status_code) {
    auto phrase = status_code_phrases.find(status_code);
    if (phrase == status_code_phrases.end())
        return "";  // empty phrase for unknown codes
    else
        return phrase->second;
}

day_enum parse_day_short(const std::string &str, size_t from, size_t to) {
    return parse_day_short(str.data(), from, to);
}

day_enum parse_day_short(const std::string &str) {
    return parse_day_short(str.data(), 0, str.length());
}

day_enum parse_day_long(const std::string &str, size_t from, size_t to) {
    return parse_day_long(str.data(), from, to);
}

day_enum parse_day_long(const std::string &str) {
    return parse_day_long(str.data(), 0, str.length());
}

month_enum parse_month(const std::string &str, size_t from, size_t to) {
    return parse_month(str.data(), from, to);
}

month_enum parse_month(const std::string &str) {
    return parse_month(str.data(), 0, str.length());
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

static constexpr uint64_t date_hash_long(const char *str) {
    return date_hash_long(str, 0, strlen_constexpr(str));
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

// used for http dates, rejects unused stuff
static constexpr uint64_t date_hash_long(const char *str, size_t from, size_t to) {
    if (to - from > 9 || to == from)    // biggest valid day or month we'll encounter or empty
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

day_enum parse_day_short(const char *str, size_t from, size_t to) {
    switch (date_hash_short(str, from, to)) {
    case date_hash_short("Mon"):
        return DAY_MON;
    case date_hash_short("Tue"):
        return DAY_TUE;
    case date_hash_short("Wed"):
        return DAY_WED;
    case date_hash_short("Thu"):
        return DAY_THU;
    case date_hash_short("Fri"):
        return DAY_FRI;
    case date_hash_short("Sat"):
        return DAY_SAT;
    case date_hash_short("Sun"):
        return DAY_SUN;
    default:
        return DAY_INVALID;
    }
}

day_enum parse_day_long(const char *str, size_t from, size_t to) {
    switch (date_hash_long(str, from, to)) {
    case date_hash_long("Monday"):
        return DAY_MON;
    case date_hash_long("Tuesday"):
        return DAY_TUE;
    case date_hash_long("Wednesday"):
        return DAY_WED;
    case date_hash_long("Thursday"):
        return DAY_THU;
    case date_hash_long("Friday"):
        return DAY_FRI;
    case date_hash_long("Saturday"):
        return DAY_SAT;
    case date_hash_long("Sunday"):
        return DAY_SUN;
    default:
        return DAY_INVALID;
    }
}

month_enum parse_month(const char *str, size_t from, size_t to) {
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

const char* day_to_str(day_enum day) {
    switch (day) {
    case DAY_MON:
        return "Mon";
    case DAY_TUE:
        return "Tue";
    case DAY_WED:
        return "Wed";
    case DAY_THU:
        return "Thu";
    case DAY_FRI:
        return "Fri";
    case DAY_SAT:
        return "Sat";
    case DAY_SUN:
        return "Sun";
    case DAY_INVALID:
    default:
        return "Invalid";
    }
}

const char* month_to_str(month_enum month) {
    switch (month) {
    case MONTH_JAN:
        return "Jan";
    case MONTH_FEB:
        return "Feb";
    case MONTH_MAR:
        return "Mar";
    case MONTH_APR:
        return "Apr";
    case MONTH_MAY:
        return "May";
    case MONTH_JUN:
        return "Jun";
    case MONTH_JUL:
        return "Jul";
    case MONTH_AUG:
        return "Aug";
    case MONTH_SEP:
        return "Sep";
    case MONTH_OCT:
        return "Oct";
    case MONTH_NOV:
        return "Nov";
    case MONTH_DEC:
        return "Dec";
    case MONTH_INVALID:
    default:
        return "Invalid";
    }
}

parser_ret parse_time_of_day(const char *str, size_t from, size_t to, date_parser_inner_ret &ret) {
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

date_parser_inner_ret parse_date_imf(const char *str, size_t from, size_t to) {
    date_parser_inner_ret ret;
    size_t pos = from, pos_next = from;

    // day of week,
    pos_next = find_char(str, pos, to, ',');
    if (pos_next == to) // no comma
        return {};
    ret.day_of_week = parse_day_short(str, pos, pos_next);
    if (ret.day_of_week == DAY_INVALID)
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

date_parser_inner_ret parse_date_rfc850(const char *str, size_t from, size_t to) {
    size_t pos = from, pos_next = from;
    date_parser_inner_ret ret;

    // day of week,
    pos_next = find_char(str, pos, to, ',');
    if (pos_next == to) // no comma
        return {};
    ret.day_of_week = parse_day_long(str, pos, pos_next);
    if (ret.day_of_week == DAY_INVALID)
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
    uint64_t year_in_50y = current_time_tm.tm_year + 1950;
    uint64_t YY_in_50y = year_in_50y % 100;
    uint64_t century_in_50y = year_in_50y - YY_in_50y;
    ret.year += century_in_50y - (ret.year > YY_in_50y) * 100;

    ret.valid = true;
    return ret;
}

date_parser_inner_ret parse_date_asctime(const char *str, size_t from, size_t to) {
    date_parser_inner_ret ret;
    size_t pos = from, pos_next = from;

    // day of week space
    pos_next = find_char(str, pos, to, ' ');
    if (pos_next == to) // no comma
        return {};
    ret.day_of_week = parse_day_short(str, pos, pos_next);
    if (ret.day_of_week == DAY_INVALID)
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

uint64_t parse_date(const char *str, size_t from, size_t to) {
    date_parser_inner_ret inner_ret;

    // try parsing all 3 date formats
    inner_ret = parse_date_imf(str, from, to);
    if (!inner_ret.valid) {
        inner_ret = parse_date_rfc850(str, from, to);
        if (!inner_ret.valid) {
            inner_ret = parse_date_asctime(str, from, to);
            if (!inner_ret.valid)
                return UINT64_MAX;  // all invalid
        }
    }

    logging::source l("HTTP DATE TEST");
    l.critical({'(', day_to_str(inner_ret.day_of_week), ") ", inner_ret.year, '-', month_to_str(inner_ret.month), '-', inner_ret.day, ' ',
                inner_ret.hour, ':', inner_ret.minute, ':', inner_ret.second});

    // TODO: verify that the date is actually valid

    // TODO: convert to seconds since epoch
    return 0;
}

}