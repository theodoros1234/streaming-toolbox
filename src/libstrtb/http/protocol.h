#ifndef STRTB_HTTP_PROTOCOL_H
#define STRTB_HTTP_PROTOCOL_H

#include <string>
#include <vector>
#include <map>
#include <ctime>
#include <limits>   // IWYU pragma: keep
#include <functional>
#include "../uri.h"

#define STRTB_HTTP_PARSE_LIST_MAX_EMPTY_ELEMENTS 16

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

typedef struct product_ret {
    size_t to = 0;
    bool valid = false;
    std::string name, version;  // version may be empty if not specified
} product_ret;

typedef struct integer_ret {
    size_t to = 0;
    bool valid = false, overflow = false;
    unsigned long long number = 0;
} integer_ret;

typedef struct token_list_ret {
    bool valid = false;
    std::vector<std::string> list;
} token_list_ret;

typedef struct product_list_ret {
    bool valid = false;
    std::vector< std::pair<std::string, std::string> > list;    // .first=name, .second=version (may be empty)
} product_list_ret;

typedef struct content_type_ret {
    bool valid = false;
    std::string type, subtype;
    std::map<std::string, std::string> params;
} content_type_ret;

typedef struct integer_field_ret {
    bool valid = false, overflow = false;
    unsigned long long number = 0;
} integer_field_ret;

typedef struct abs_or_part_uri_field_ret {
    bool valid = false, is_partial = false;
    uri::parser segments;
} abs_or_part_uri_field_ret;

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
product_ret parse_product_or_protocol(const std::string &str);
product_ret parse_product_or_protocol(const std::string &str, size_t from, size_t to);
integer_ret parse_integer(const std::string &str);
integer_ret parse_integer(const std::string &str, size_t from, size_t to);
parser_ret parse_list(const std::string &str,
                      const std::function<parser_ret(const char*, size_t, size_t)> &element_parser);
parser_ret parse_list(const std::string &str, size_t from, size_t to,
                      const std::function<parser_ret(const char*, size_t, size_t)> &element_parser);
token_list_ret parse_field_token_list(const std::string &field_value, bool tolower=false);
token_list_ret parse_field_token_list(const std::string &field_value, size_t from, size_t to, bool tolower=false);
product_list_ret parse_field_upgrade(const std::string &field_value);
product_list_ret parse_field_upgrade(const std::string &field_value, size_t from, size_t to);
content_type_ret parse_field_content_type(const std::string &field_value);
content_type_ret parse_field_content_type(const std::string &field_value, size_t from, size_t to);
integer_field_ret parse_field_integer(const std::string &field_value);
integer_field_ret parse_field_integer(const std::string &field_value, size_t from, size_t to);
abs_or_part_uri_field_ret parse_field_abs_or_part_uri(const std::string &field_value);
abs_or_part_uri_field_ret parse_field_abs_or_part_uri(const std::string &field_value, size_t from, size_t to);

parser_ret parse_token(const char *str, size_t from, size_t to);
quoted_ret parse_quoted_str(const char *str, size_t from, size_t to);
parser_ret parse_comment(const char *str, size_t from, size_t to);
http_version_ret parse_http_version(const char *str, size_t from, size_t to);
request_line_ret parse_request_line(const char *line, size_t length);
status_line_ret parse_status_line(const char *line, size_t length);
time_t parse_date(const char *str, size_t from, size_t to);
parameters_ret parse_parameters(const char *str, size_t from, size_t to);
product_ret parse_product_or_protocol(const char *str, size_t from, size_t to);
integer_ret parse_integer(const char *str, size_t from, size_t to);
parser_ret parse_list(const char *str, size_t from, size_t to,
                      const std::function<parser_ret(const char*, size_t, size_t)> &element_parser);
token_list_ret parse_field_token_list(const char *field_value, size_t from, size_t to, bool tolower=false);
product_list_ret parse_field_upgrade(const char *field_value, size_t from, size_t to);
content_type_ret parse_field_content_type(const char *field_value, size_t from, size_t to);
integer_field_ret parse_field_integer(const char *field_value, size_t from, size_t to);
abs_or_part_uri_field_ret parse_field_abs_or_part_uri(const char *field_value, size_t from, size_t to);

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
