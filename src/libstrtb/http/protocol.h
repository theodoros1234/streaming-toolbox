#ifndef STRTB_HTTP_PROTOCOL_H
#define STRTB_HTTP_PROTOCOL_H

#include <string>
#include <vector>
#include <map>
#include <ctime>
#include <limits>   // IWYU pragma: keep
#include "../uri.h"

// NOTE: currently targetting HTTP/1.1 compatibility

namespace strtb::http {

typedef std::pair<size_t, bool> parser_ret;  // .first: ends at, .second: is valid
typedef std::tuple<size_t, bool, std::string> quoted_ret;   // ends at, is valid, unescaped string

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

typedef struct parameters_ret {
    size_t to = 0;
    std::map<std::string, std::string> params;
} parameters_ret;

// all line parsers need CRLF pre-stripped from the end of the string

parser_ret parse_token(const std::string &str);
parser_ret parse_token(const std::string &str, size_t from, size_t to);
quoted_ret parse_quoted_str(const std::string &str);
quoted_ret parse_quoted_str(const std::string &str, size_t from, size_t to);
parser_ret parse_comment(const std::string &str);
parser_ret parse_comment(const std::string &str, size_t from, size_t to);
http_version_ret parse_http_version(const std::string &str);
http_version_ret parse_http_version(const std::string &str, size_t from, size_t to);
request_line_ret parse_request_line(const std::string &line);
status_line_ret parse_status_line(const std::string &line);
time_t parse_date(const std::string &str);
time_t parse_date(const std::string &str, size_t from, size_t to);
parameters_ret parse_parameters(const std::string &str);
parameters_ret parse_parameters(const std::string &str, size_t from, size_t to);

parser_ret parse_token(const char *str, size_t from, size_t to);
quoted_ret parse_quoted_str(const char *str, size_t from, size_t to);
parser_ret parse_comment(const char *str, size_t from, size_t to);
http_version_ret parse_http_version(const char *str, size_t from, size_t to);
request_line_ret parse_request_line(const char *line, size_t length);
status_line_ret parse_status_line(const char *line, size_t length);
time_t parse_date(const char *str, size_t from, size_t to);
parameters_ret parse_parameters(const char *str, size_t from, size_t to);

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

const char* get_status_code_phrase(int status_code);
std::string timestamp_to_string(time_t timestamp);

}

#endif // STRTB_HTTP_PROTOCOL_H
