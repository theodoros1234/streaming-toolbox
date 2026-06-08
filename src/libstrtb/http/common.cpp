#include "common.h"

#include <stdexcept>

namespace strtb::http {

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

static size_t parse_optional_whitespace(const char *str, size_t from, size_t to);
static parser_ret parse_required_whitespace(const char *str, size_t from, size_t to);

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

parser_ret parse_token(const std::string &str, size_t from, size_t to) {
    verify_range(str, from, to);
    return parse_token(str.data(), from, to);
}

parser_ret parse_token(const char *str, size_t from, size_t to) {
    size_t pos;

    for (pos = from; pos < to; pos++)
        if (!is_tchar(str[pos]))
            break;

    // token must have at least 1 character
    return std::make_pair(pos, pos > from);
}

// Set-Cookie fields are processed independently: https://www.rfc-editor.org/rfc/rfc9110.html#section-5.3-4.1

field_parser::field_parser(bool allow_obs_fold) : allow_obs_fold(allow_obs_fold) {}

bool field_parser::process_line(const std::string &line) {
    return process_line(line.data(), line.length());
}

/* WARNING: processing headers before reading all of them could be a security vulnerability
 *          due to combining multiple instances of a header into a list */

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
        if (pos >= length || line[pos] != ':')
            return false;

        // eat whitespace
        pos = parse_optional_whitespace(line, pos, length);
    } else if (allow_obs_fold) {
        // folded field (see obs-fold in RFC)
        std::tie(pos, folded) = parse_required_whitespace(line, 0, length);
        if (!folded)
            return false;
    } else return false;

    // parse field-value
    field_value_from = pos;
    for (size_t i = pos; i < length; i++) {
        // mark last non-whitespace character as end of value
        char c = line[i];
        if (is_vchar(c) || is_obs_text(c))
            field_value_to = i;
        else if (!is_whitespace(c))
            return false;
    }

    field_value.assign(line + field_value_from, field_value_to - field_value_from);

    // store processed field
    if (folded) {   // folded field line
        // reject if there wasn't a previous field
        if (_previous_field.empty())
            return false;
        // merge with previous line, separated by a space
        auto& field = fields.at(field_name);
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
                // TODO: deny combining some headers
                field->second.append(", ");
                field->second.append(field_value);
            }
        }
    }

    return true;
}

void field_parser::clear() {
    fields.clear();
    fields_set_cookie.clear();
    _previous_field.clear();
}

}