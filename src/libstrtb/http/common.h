#ifndef STRTB_HTTP_COMMON_H
#define STRTB_HTTP_COMMON_H

#include <string>
#include <vector>
#include <map>

namespace strtb::http {

typedef std::pair<size_t, bool> parser_ret;  // .first: ends at, .second: is valid

parser_ret parse_token(const std::string &str, size_t from, size_t to);

parser_ret parse_token(const char *str, size_t from, size_t to);

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

#endif // STRTB_HTTP_COMMON_H
