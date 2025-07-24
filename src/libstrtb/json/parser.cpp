#include "parser.h"
#include "../logging/logging.h"
#include "../unicode/unicode.h"

#include <sstream>
#include <locale>
#include <limits>
#include <fstream>

using namespace strtb;
using namespace strtb::json::parser;

invalid_json::invalid_json(size_t line, size_t col) : _what("invalid JSON at line " + std::to_string(line) + ", col " + std::to_string(col)) {}

const char* invalid_json::what() const noexcept {return _what.c_str();}

static logging::source log("JSON Parser");

// Helper functions (declarations only)
static void eat_whitespace(std::istream &stream, size_t &line, size_t &col);
static void parse_wanted_word(std::istream &stream, const char* wanted_word, size_t &line, size_t &col);
static json::value* parse_null(std::istream &stream, size_t &line, size_t &col);
static json::value* parse_false(std::istream &stream, size_t &line, size_t &col);
static json::value* parse_true(std::istream &stream, size_t &line, size_t &col);
static json::value* parse_number(std::istream &stream, size_t &line, size_t &col);
static json::value* parse_string(std::istream &stream, size_t &line, size_t &col);
static json::value* parse_array(std::istream &stream, size_t &line, size_t &col);
static json::value* parse_object(std::istream &stream, size_t &line, size_t &col);
static json::value* parse_value(std::istream &stream, size_t &line, size_t &col);

// Public functions (definitions)
json::value* json::parser::from_stream(std::istream &stream) {
    size_t line=1, col=1;
    try {
        value* val = parse_value(stream, line, col);
        if (!stream.eof())
            // There's extra stuff after what was parsed, meaning the file has invalid JSON
            throw invalid_json(line, col);
        return val;
    } catch (std::istream::failure &e) {
        if (stream.eof())
            // Failure exeptions can be thrown by getting a character on EOF, in which case the JSON file ended prematurely and is invalid
            throw invalid_json(line, col);
        else
            // Otherwise, it's just a general I/O error.
            throw;
    }
}

json::value* json::parser::from_string(const std::string &str) {
    std::stringstream str_stream(str, std::ios_base::in);
    return from_stream(str_stream);
}

json::value* json::parser::from_file(const char* path) {
    std::ifstream file(path, std::ifstream::in);
    file.exceptions(std::istream::failbit | std::ifstream::badbit);
    return from_stream(file);
}

// Helper functions (definitions)
static void eat_whitespace(std::istream &stream, size_t &line, size_t &col) {
    int c_prev = 0;
    int c = stream.peek();
    while (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
        stream.get();
        if (c == '\r' || (c == '\n' && c_prev != '\r')) {
            line++;
            col = 1;
        } else if (c != '\n') {
            col++;
        }
        c_prev = c;
        c = stream.peek();
    }
}

static void parse_wanted_word(std::istream &stream, const char* wanted_word, size_t &line, size_t &col) {
    while (stream.get() == *wanted_word) {
        // Assuming no newlines for the wanted word for line/col counting
        col++;
        wanted_word++;
        if (*wanted_word == 0)
            return;
    }
    throw invalid_json(line, col);
}

static json::value* parse_null(std::istream &stream, size_t &line, size_t &col) {
    parse_wanted_word(stream, "null", line, col);
    return new json::value_null();
}


static json::value* parse_false(std::istream &stream, size_t &line, size_t &col) {
    parse_wanted_word(stream, "false", line, col);
    return new json::value_bool(false);
}

static json::value* parse_true(std::istream &stream, size_t &line, size_t &col) {
    parse_wanted_word(stream, "true", line, col);
    return new json::value_bool(true);
}

static json::value* parse_number(std::istream &stream, size_t &line, size_t &col) {
    bool is_fraction = false;
    bool is_sci_notation = false;
    bool is_zero = false; // Excluding anything after the decimal point
    bool is_not_zero = false; // Excluding anything after the decimal point
    bool fraction_got_digits = false;
    bool sci_notation_has_sign = false;
    bool sci_notation_doesnt_have_sign = false;
    bool sci_notation_got_digits = false;
    bool keep_reading = true;
    std::string number_str;

    // Isolate number from input stream: Read until first invalid char or EOF
    while (keep_reading) {
        int c = stream.peek();
        /* We intentionally DON'T check for read errors or EOF;
         * the following logic needs to run first, so we can know whether
         * the number is valid or not.
         */

        if (is_sci_notation) {                      // SCIENTIFIC NOTATION PART
            if (!sci_notation_has_sign && !sci_notation_doesnt_have_sign) { // get at least one digit, or start with sign
                if (c == '+' || c == '-')                   // optionally get a sign
                    sci_notation_has_sign = true;
                else if ('0' <= c && c <= '9') {            // start with digits, so we don't have a sign
                    sci_notation_doesnt_have_sign = true;
                    sci_notation_got_digits = true;
                } else                                      // ERROR: must get at least one digit
                    throw invalid_json(line, col);
            } else if (!sci_notation_got_digits) {          // get at least one digit
                if ('0' <= c && c <= '9')                   // get first digit
                    sci_notation_got_digits = true;
                else                                        // ERROR: must get at least one digit
                    throw invalid_json(line, col);
            } else if ('0' <= c && c <= '9');               // keep getting more digits
            else                                            // number is in scientific notation
                keep_reading = false;
        } else if (is_fraction) {                   // FRACTION PART
            if (fraction_got_digits) {
                if ('0' <= c && c <= '9');                  // keep getting digits
                else if (c == 'E' || c == 'e')              // switch to sci notation part
                    is_sci_notation = true;
                else                                        // number is a fraction (no sci notation)
                    keep_reading = false;
            } else if ('0' <= c && c <= '9')                // get at least one digit
                fraction_got_digits = true;
            else                                            // ERROR: must get at least one digit
                throw invalid_json(line, col);
        } else {                                    // INTEGER PART
            if (number_str.empty() && c == '-');            // optional minus sign
            else if (!is_zero && !is_not_zero) {            // get at least one digit
                if (c == '0')                               // starts with zero (can only be 0 or 0.something)
                    is_zero = true;
                else if ('1' <= c && c <= '9')              // starts with other digit (can be any other number)
                    is_not_zero = true;
                else                                        // ERROR: must get at least one digit
                    throw invalid_json(line, col);
            } else if (is_not_zero && '0' <= c && c <= '9');// get more digits if NOT starting with 0
            else if (c == '.')                              // switch to fraction part
                is_fraction = true;
            else if (c == 'E' || c == 'e') {                // switch to sci notation part
                is_sci_notation = true;
                is_fraction = true;
            } else                                          // number is only an integer
                keep_reading = false;
        }

        if (keep_reading) {
            number_str.push_back(stream.get());
            col++;
        }
    }

    if (is_fraction) {
        /* JSON fractions always use . as the decimal point, so we need to work around
         * the system's locale, which could use , as the decimal point.
         */
        double n;
        std::stringstream str(number_str, std::ios_base::in);
        str.imbue(std::locale("C"));
        str >> n;
        return new json::value_float(n);
    } else {
        // Try to return as an integer (long long), or if it's too big return it as a double
        try {
            return new json::value_int(std::stoll(number_str));
        } catch (std::out_of_range &e) {
            try {
                return new json::value_float(std::stod(number_str));
            } catch (std::out_of_range &e) {
                // If this is also too big, just return the maximum or minimum number, based on sign
                if (number_str[0] == '-')
                    return new json::value_float(std::numeric_limits<double>::max());
                else
                    return new json::value_float(std::numeric_limits<double>::min());
            }
        }
    }
}

static json::value* parse_string(std::istream &stream, size_t &line, size_t &col) {
    std::string str;
    bool escape = false;
    int hex_pos = -1;
    bool keep_reading = true;
    uint32_t hex_codepoint = 0;

    // Get first quotation mark
    if (stream.get() != '"')
        throw invalid_json(line, col);
    col++;

    // Get rest of string
    while (keep_reading) {
        int c = stream.get();

        if (hex_pos != -1) {            // HEX ESCAPE CODE
            uint32_t hex_digit;             // convert hex digit char to number
            if ('0' <= c && c <= '9')
                hex_digit = c - '0';
            else if ('A' <= c && c <= 'F')
                hex_digit = c - 'A' + 10;
            else if ('a' <= c && c <= 'f')
                hex_digit = c - 'a' + 10;
            else
                throw invalid_json(line, col);

            hex_codepoint = hex_codepoint | (hex_digit << hex_pos);   // set appropriate bits for this hex digit
            if (hex_pos) {
                // Get next char
                hex_pos -= 4;
            } else {
                // Hex segment finished, put unicode character into string
                str.append(unicode::codepoint_to_utf8(hex_codepoint));
                hex_codepoint = 0;
                hex_pos = -1;
            }
        } else if (escape) {            // SIMPLE ESCAPE CODE
            escape = false;
            switch (c) {
            case '"':                       // quote
            case '\\':                      // backslash
            case '/':                       // forward slash
                str.push_back(c);
                break;
            case 'b':                       // backspace
                str.push_back('\b');
                break;
            case 'f':                       // formfeed
                str.push_back('\f');
                break;
            case 'n':                       // newline
                str.push_back('\n');
                break;
            case 'r':                       // carriage return
                str.push_back('\r');
                break;
            case 't':                       // horizontal tab
                str.push_back('\t');
                break;
            case 'u':                       // hex escape code
                hex_pos = 12;
                break;
            default:                        // invalid escape code
                throw invalid_json(line, col);
            }
        } else {                        // REGULAR CHARACTER
            switch (c) {
            case '"':                       // end quote
                keep_reading = false;
                break;
            case '\\':                      // escape code
                escape = true;
                break;
            default:
                if ((EOF <= c && c <= 0x1f) || c == 0x7f)   // control characters (not allowed) or premature EOF
                    throw invalid_json(line, col);
                else                        // regular character
                    str.push_back(c);
            }
        }

        col++;
    }

    return new json::value_string(str);
}

static json::value* parse_array(std::istream &stream, size_t &line, size_t &col) {
    json::value_array* arr = new json::value_array();
    bool keep_reading = true;

    try {
        // Read opening [
        if (stream.get() != '[')
            throw invalid_json(line, col);
        col++;
        eat_whitespace(stream, line, col);

        // Check for closing ] (which means the array is empty)
        if (stream.peek() == ']') {
            keep_reading = false;
            stream.get();
            col++;
        }

        // Read all values and closing ]
        while (keep_reading) {
            // Get value
            arr->push_back_move(parse_value(stream, line, col));

            switch (stream.get()) {
            case ',':   // comma => more values to get after this
                break;
            case ']':   // square bracket => end of array
                keep_reading = false;
                break;
            default:    // invalid character
                throw invalid_json(line, col);
            }
            col++;
        }
    } catch (...) {
        // Clear used memory before passing on the exception
        delete arr;
        throw;
    }

    return arr;
}

static json::value* parse_object(std::istream &stream, size_t &line, size_t &col) {
    json::value_object* obj = new json::value_object();
    bool keep_reading = true;

    try {
        // Read opening {
        if (stream.get() != '{')
            throw invalid_json(line, col);
        col++;
        eat_whitespace(stream, line, col);

        // Check for closing } (which means the object is empty)
        if (stream.peek() == '}') {
            keep_reading = false;
            stream.get();
            col++;
        }

        // Read all key-value pairs and closing }
        while (keep_reading) {
            // Key
            eat_whitespace(stream, line, col);
            size_t line_pre_key = line, col_pre_key = col;
            json::value_string* key_obj = (json::value_string*) parse_string(stream, line, col);
            std::string key = key_obj->value();
            delete key_obj;
            if (obj->exists(key))   // Make sure it doesn't already exist
                throw invalid_json(line_pre_key, col_pre_key);
            eat_whitespace(stream, line, col);

            // Colon separator
            if (stream.get() != ':')
                throw invalid_json(line, col);
            col++;

            // Value
            obj->set_move(key, parse_value(stream, line, col));

            // Comma or closing }
            switch (stream.get()) {
            case ',':   // comma => more key/value pairs after this
                break;
            case '}':   // curly bracket => end of object
                keep_reading = false;
                break;
            default:    // invalid character
                throw invalid_json(line, col);
            }
            col++;
        }
    } catch (...) {
        // Clear used memory before passing on the exception
        delete obj;
        throw;
    }

    return obj;
}

static json::value* parse_value(std::istream &stream, size_t &line, size_t &col) {
    json::value* val;
    eat_whitespace(stream, line, col);

    // Decide what kind of value it is
    int c = stream.peek();
    switch (c) {
    case 'n':       // null
        val = parse_null(stream, line, col);
        break;
    case 'f':       // false
        val = parse_false(stream, line, col);
        break;
    case 't':       // true
        val = parse_true(stream, line, col);
        break;
    case '[':       // array
        val = parse_array(stream, line, col);
        break;
    case '{':       // object
        val = parse_object(stream, line, col);
        break;
    case '"':       // string
        val = parse_string(stream, line, col);
        break;
    default:
        if (c == '-' || ('0' <= c && c <= '9'))     // number
            val = parse_number(stream, line, col);
        else        // invalid character
            throw invalid_json(line, col);
    }

    try {
        eat_whitespace(stream, line, col);
    } catch (...) {
        // On exception, delete the object we created before rethrowing
        delete val;
        throw;
    }

    return val;
}
