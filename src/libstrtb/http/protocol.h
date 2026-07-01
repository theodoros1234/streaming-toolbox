#ifndef STRTB_HTTP_PROTOCOL_H
#define STRTB_HTTP_PROTOCOL_H

#include <string>
#include <vector>
#include <map>
#include <ctime>
#include <limits>   // IWYU pragma: keep
#include <functional>
#include <variant>
#include "../uri.h"

#define STRTB_HTTP_PARSE_LIST_MAX_EMPTY_ELEMENTS 16

// NOTE: currently targetting HTTP/1.1 compatibility

namespace strtb::http {

typedef std::pair<size_t, bool> parser_ret;  // .first: ends at, .second: is valid
typedef std::tuple<size_t, bool, std::string> quoted_ret;   // ends at, is valid, unescaped string
typedef std::map<std::string, std::string> parameter_map;

struct http_version_ret {
    size_t to = 0;
    bool valid = false; // make sure to check this
    int v_major = 0, v_minor = 0;
};

enum request_target_form_enum {
    TARGET_FORM_INVALID,
    TARGET_FORM_ORIGIN,
    TARGET_FORM_ABSOLUTE,
    TARGET_FORM_AUTHORITY,
    TARGET_FORM_ASTERISK
};

struct request_line_ret {
    bool valid = false;
    std::string method, target;
    request_target_form_enum target_form = TARGET_FORM_INVALID;
    uri::parser target_segments;
    int http_version_major = 0, http_version_minor = 0;
};

struct status_line_ret {
    bool valid = false;
    int http_version_major = 0, http_version_minor = 0,
        status_code = 0;
    std::string reason_phrase;
};

struct parameters_ret {
    size_t to = 0;
    parameter_map params;
};

struct product {
    std::string name, version;  // version may be empty if not specified
};

struct product_ret {
    size_t to = 0;
    bool valid = false;
    product pr;
};

struct integer_ret {
    size_t to = 0;
    bool valid = false, overflow = false;
    unsigned long long number = 0;
};

struct entity_tag {
    bool is_weak = false;
    std::string tag;
};

struct entity_tag_ret {
    size_t to = 0;
    bool valid = false;
    entity_tag etag;
};

struct auth_params_ret {
    size_t to = 0;
    bool valid = false, duplicate = false;  // should reject the entire filed if duplicate=true for security
    parameter_map params;
};

struct credentials {    // or challenge
    std::string auth_scheme;
    std::variant<bool, std::string, parameter_map> value = false;   // no value, token68, #auth_param
};

struct credentials_ret {
    size_t to = 0;
    bool valid = false;
    credentials creds;
};

struct media_type {
    std::string type, subtype;
    parameter_map params;
};

struct media_type_ret {
    size_t to = 0;
    bool valid = false;
    media_type m;
};

struct token_list_ret {
    bool valid = false;
    std::vector<std::string> list;
};

struct product_list_ret {
    bool valid = false;
    std::vector<product> list;
};

struct content_type_ret {
    bool valid = false;
    media_type m;
};

struct integer_field_ret {
    bool valid = false, overflow = false;
    unsigned long long number = 0;
};

struct abs_or_part_uri_field_ret {
    bool valid = false, is_partial = false;
    uri::parser segments;
};

struct etag_field_ret {
    bool valid = false;
    entity_tag etag;
};

struct expectation {
    std::string name, value;
    parameter_map params = {};
};

struct expect_field_ret {
    bool valid = false;
    std::vector<expectation> list;
};

struct token_params {
    std::string token;
    parameter_map params;
};

struct token_params_list_ret {
    bool valid = false;
    std::vector<token_params> list;
};

struct product_field_ret {  // User-Agent, Server
    bool valid = false;
    std::vector< std::variant<product, std::string> > list; // product or comment, use std::variant::index()
};

struct authorization_field_ret {
    bool valid = false;
    credentials creds;
};

struct authenticate_field_ret {
    bool valid = false;
    std::vector<credentials> challenges;
};

struct auth_params_field_ret {
    bool valid = false;
    parameter_map params;
};

struct accept_field_ret {
    bool valid = false;
    std::vector<media_type> list;
};

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
parameters_ret parse_parameters(const std::string &str, bool allow_bad_whitespace=false);
parameters_ret parse_parameters(const std::string &str, size_t from, size_t to, bool allow_bad_whitespace=false);
product_ret parse_product_or_protocol(const std::string &str);
product_ret parse_product_or_protocol(const std::string &str, size_t from, size_t to);
integer_ret parse_integer(const std::string &str);
integer_ret parse_integer(const std::string &str, size_t from, size_t to);
entity_tag_ret parse_etag(const std::string &str);
entity_tag_ret parse_etag(const std::string &str, size_t from, size_t to);
parser_ret parse_list(const std::string &str,
                      const std::function<parser_ret(const char*, size_t, size_t)> &element_parser);
parser_ret parse_list(const std::string &str, size_t from, size_t to,
                      const std::function<parser_ret(const char*, size_t, size_t)> &element_parser);
credentials_ret parse_credentials_or_challenge(const std::string &str);
credentials_ret parse_credentials_or_challenge(const std::string &str, size_t from, size_t to);
auth_params_ret parse_auth_params(const std::string &str);
auth_params_ret parse_auth_params(const std::string &str, size_t from, size_t to);
media_type_ret parse_media_type(const std::string &str);
media_type_ret parse_media_type(const std::string &str, size_t from, size_t to);

time_t parse_field_date(const std::string &str);
time_t parse_field_date(const std::string &str, size_t from, size_t to);
token_list_ret parse_field_token_list(const std::string &field_value, bool case_sensitive);
token_list_ret parse_field_token_list(const std::string &field_value, size_t from, size_t to, bool case_sensitive);
product_list_ret parse_field_upgrade(const std::string &field_value);
product_list_ret parse_field_upgrade(const std::string &field_value, size_t from, size_t to);
content_type_ret parse_field_content_type(const std::string &field_value);
content_type_ret parse_field_content_type(const std::string &field_value, size_t from, size_t to);
integer_field_ret parse_field_integer(const std::string &field_value);
integer_field_ret parse_field_integer(const std::string &field_value, size_t from, size_t to);
abs_or_part_uri_field_ret parse_field_abs_or_part_uri(const std::string &field_value);
abs_or_part_uri_field_ret parse_field_abs_or_part_uri(const std::string &field_value, size_t from, size_t to);
etag_field_ret parse_field_etag(const std::string &field_value);
etag_field_ret parse_field_etag(const std::string &field_value, size_t from, size_t to);
expect_field_ret parse_field_expect(const std::string &field_value);
expect_field_ret parse_field_expect(const std::string &field_value, size_t from, size_t to);
token_params_list_ret parse_field_token_params_list(const std::string &field_value,
                                                    bool token_case_sensitive, bool allow_bad_whitespace);
token_params_list_ret parse_field_token_params_list(const std::string &field_value, size_t from, size_t to,
                                                    bool token_case_sensitive, bool allow_bad_whitespace);
product_field_ret parse_field_product_info(const std::string &field_value);
product_field_ret parse_field_product_info(const std::string &field_value, size_t from, size_t to);
authenticate_field_ret parse_field_authenticate(const std::string &field_value);
authenticate_field_ret parse_field_authenticate(const std::string &field_value, size_t from, size_t to);
authorization_field_ret parse_field_authorization(const std::string &field_value);
authorization_field_ret parse_field_authorization(const std::string &field_value, size_t from, size_t to);
auth_params_field_ret parse_field_authentication_info(const std::string &field_value);
auth_params_field_ret parse_field_authentication_info(const std::string &field_value, size_t from, size_t to);
accept_field_ret parse_field_accept(const std::string &field_value);
accept_field_ret parse_field_accept(const std::string &field_value, size_t from, size_t to);


parser_ret parse_token(const char *str, size_t from, size_t to);
quoted_ret parse_quoted_str(const char *str, size_t from, size_t to);
parser_ret parse_comment(const char *str, size_t from, size_t to);
http_version_ret parse_http_version(const char *str, size_t from, size_t to);
request_line_ret parse_request_line(const char *line, size_t length);
status_line_ret parse_status_line(const char *line, size_t length);
parameters_ret parse_parameters(const char *str, size_t from, size_t to, bool allow_bad_whitespace=false);
product_ret parse_product_or_protocol(const char *str, size_t from, size_t to);
integer_ret parse_integer(const char *str, size_t from, size_t to);
entity_tag_ret parse_etag(const char *str, size_t from, size_t to);
parser_ret parse_list(const char *str, size_t from, size_t to,
                      const std::function<parser_ret(const char*, size_t, size_t)> &element_parser);
credentials_ret parse_credentials_or_challenge(const char *str, size_t from, size_t to);
auth_params_ret parse_auth_params(const char *str, size_t from, size_t to);
media_type_ret parse_media_type(const char *str, size_t from, size_t to);

time_t parse_field_date(const char *str, size_t from, size_t to);
token_list_ret parse_field_token_list(const char *field_value, size_t from, size_t to, bool case_sensitive);
product_list_ret parse_field_upgrade(const char *field_value, size_t from, size_t to);
content_type_ret parse_field_content_type(const char *field_value, size_t from, size_t to);
integer_field_ret parse_field_integer(const char *field_value, size_t from, size_t to);
abs_or_part_uri_field_ret parse_field_abs_or_part_uri(const char *field_value, size_t from, size_t to);
etag_field_ret parse_field_etag(const char *field_value, size_t from, size_t to);
expect_field_ret parse_field_expect(const char *field_value, size_t from, size_t to);
token_params_list_ret parse_field_token_params_list(const char *field_value, size_t from, size_t to,
                                                    bool token_case_sensitive, bool allow_bad_whitespace);
product_field_ret parse_field_product_info(const char *field_value, size_t from, size_t to);
authenticate_field_ret parse_field_authenticate(const char *field_value, size_t from, size_t to);
authorization_field_ret parse_field_authorization(const char *field_value, size_t from, size_t to);
auth_params_field_ret parse_field_authentication_info(const char *field_value, size_t from, size_t to);
accept_field_ret parse_field_accept(const char *field_value, size_t from, size_t to);

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
bool etag_compare(const entity_tag &a, const entity_tag &b, bool strong);
std::string etag_to_string(const entity_tag &etag);

}

#endif // STRTB_HTTP_PROTOCOL_H
