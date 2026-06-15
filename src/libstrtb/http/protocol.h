#ifndef STRTB_HTTP_PROTOCOL_H
#define STRTB_HTTP_PROTOCOL_H

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include "../uri.h"

// NOTE: currently targetting HTTP/1.1 compatibility

namespace strtb::http {

typedef std::pair<size_t, bool> parser_ret;  // .first: ends at, .second: is valid

typedef struct http_version_ret {
    size_t to = 0;
    bool valid = false; // make sure to check this
    int v_major = 0, v_minor = 0;
} http_version_ret;

typedef enum request_target_form_enum {
    TARGET_FORM_INVALID,
    TARGET_FORM_ORIGIN,
    TARGET_FORM_ABSOLUTE,
    TARGET_FORM_AUTHORITY,
    TARGET_FORM_ASTERISK
} request_target_form_enum;

typedef enum day_enum {
    DAY_INVALID,
    DAY_MON,
    DAY_TUE,
    DAY_WED,
    DAY_THU,
    DAY_FRI,
    DAY_SAT,
    DAY_SUN
} day_enum;

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

typedef struct request_line_ret {
    bool valid = false;
    std::string method, target;
    request_target_form_enum target_form = TARGET_FORM_INVALID;
    uri::parser target_segments;
    int http_version_major = 0, http_version_minor = 0;
} request_line_ret;

typedef struct status_line_ret {
    bool valid = false;
    int http_version_major = 0, http_version_minor = 0,
        status_code = 0;
    std::string reason_phrase;
} status_line_ret;

// all line parsers need CRLF pre-stripped from the end of the string

parser_ret parse_token(const std::string &str);
parser_ret parse_token(const std::string &str, size_t from, size_t to);
http_version_ret parse_http_version(const std::string &str);
http_version_ret parse_http_version(const std::string &str, size_t from, size_t to);
request_line_ret parse_request_line(const std::string &line);
status_line_ret parse_status_line(const std::string &line);
day_enum parse_day_short(const std::string &str, size_t from, size_t to);
day_enum parse_day_short(const std::string &str);
day_enum parse_day_long(const std::string &str, size_t from, size_t to);
day_enum parse_day_long(const std::string &str);
month_enum parse_month(const std::string &str, size_t from, size_t to);
month_enum parse_month(const std::string &str);
// TODO: std::string variants for full date and to_XXXXX

parser_ret parse_token(const char *str, size_t from, size_t to);
http_version_ret parse_http_version(const char *str, size_t from, size_t to);
request_line_ret parse_request_line(const char *line, size_t length);
status_line_ret parse_status_line(const char *line, size_t length);
const char* get_status_code_phrase(int status_code);
day_enum parse_day_short(const char *str, size_t from, size_t to);
day_enum parse_day_long(const char *str, size_t from, size_t to);
month_enum parse_month(const char *str, size_t from, size_t to);
const char* day_to_str(day_enum day);
const char* month_to_str(month_enum month);
uint64_t parse_date(const char *str, size_t from, size_t to);

// WARNING: MUST run clear() before processing another HTTP message
class field_parser {
private:
    std::string _previous_field;

public:
    bool allow_obs_fold = true;
    std::map<std::string, std::string> fields;
    std::vector<std::string> fields_set_cookie;

    field_parser() = default;
    field_parser(bool allow_obs_fold);
    bool process_line(const char *line, size_t length);
    void clear();

    bool process_line(const std::string &line);
};

}

#endif // STRTB_HTTP_PROTOCOL_H
