#include "parser.h"
#include "../logging/logging.h"
#include "../unicode/unicode.h"

#include <sstream>
#include <locale>
#include <limits>
#include <fstream>

using namespace json::parser;

invalid_json::invalid_json(const size_t pos) : _what("Error at position " + std::to_string(pos)) {}

const char* invalid_json::what() const noexcept {return _what.c_str();}

logging::source log("JSON Parser");

// Helper functions (declarations only)
static void eat_whitespace(std::istream &stream);
static void parse_wanted_word(std::istream &stream, const char* wanted_word);
static json::value* parse_null(std::istream &stream);
static json::value* parse_false(std::istream &stream);
static json::value* parse_true(std::istream &stream);
static json::value* parse_number(std::istream &stream);
static json::value* parse_string(std::istream &stream);
static json::value* parse_array(std::istream &stream);
static json::value* parse_object(std::istream &stream);
static json::value* parse_value(std::istream &stream);

// Public functions (definitions)
json::value* json::parser::from_stream(std::istream &stream) {
    return parse_value(stream);
}

json::value* json::parser::from_string(const std::string &str) {
    std::stringstream str_stream(str, std::ios_base::in);
    return from_stream(str_stream);
}

json::value* json::parser::from_file(const char* path) {
    std::ifstream file(path, std::ifstream::in);
    return from_stream(file);
}

// Helper functions (definitions)
static void eat_whitespace(std::istream &stream) {
    int c = stream.peek();
    while (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
        stream.get();
        c = stream.peek();
    }
}

static void parse_wanted_word(std::istream &stream, const char* wanted_word) {
    while (stream.get() == *wanted_word) {
        wanted_word++;
        if (*wanted_word == 0)
            return;
    }
    throw invalid_json((size_t)stream.tellg());
}

static json::value* parse_null(std::istream &stream) {
    parse_wanted_word(stream, "null");
    return new json::value_null();
}


static json::value* parse_false(std::istream &stream) {
    parse_wanted_word(stream, "false");
    return new json::value_bool(false);
}

static json::value* parse_true(std::istream &stream) {
    parse_wanted_word(stream, "true");
    return new json::value_bool(true);
}

static json::value* parse_number(std::istream &stream) {
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
                    throw invalid_json((size_t)stream.tellg());
            } else if (!sci_notation_got_digits) {          // get at least one digit
                if ('0' <= c && c <= '9')                   // get first digit
                    sci_notation_got_digits = true;
                else                                        // ERROR: must get at least one digit
                    throw invalid_json((size_t)stream.tellg());
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
                throw invalid_json((size_t)stream.tellg());
        } else {                                    // INTEGER PART
            if (number_str.empty() && c == '-');            // optional minus sign
            else if (!is_zero && !is_not_zero) {            // get at least one digit
                if (c == '0')                               // starts with zero (can only be 0 or 0.something)
                    is_zero = true;
                else if ('1' <= c && c <= '9')              // starts with other digit (can be any other number)
                    is_not_zero = true;
                else                                        // ERROR: must get at least one digit
                    throw invalid_json((size_t)stream.tellg());
            } else if (is_not_zero && '0' <= c && c <= '9');// get more digits if NOT starting with 0
            else if (c == '.')                              // switch to fraction part
                is_fraction = true;
            else if (c == 'E' || c == 'e') {                // switch to sci notation part
                is_sci_notation = true;
                is_fraction = true;
            } else                                          // number is only an integer
                keep_reading = false;
        }

        if (keep_reading)
            number_str.push_back(stream.get());
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

static json::value* parse_string(std::istream &stream) {
    std::string str;
    bool escape = false;
    int hex_pos = -1;
    bool keep_reading = true;
    uint32_t hex_codepoint = 0;

    // Get first quotation mark
    if (stream.get() != '"')
        throw invalid_json((size_t)stream.tellg()-1);

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
                throw invalid_json((size_t)stream.tellg()-1);

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
                throw invalid_json((size_t)stream.tellg()-1);
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
                if (EOF <= c && c <= 0x1f)  // control characters (not allowed) or premature EOF
                    throw invalid_json((size_t)stream.tellg()-1);
                else                        // regular character
                    str.push_back(c);
            }
        }
    }

    return new json::value_string(str);
}

static json::value* parse_array(std::istream &stream) {
    json::value_array* arr = new json::value_array();
    bool keep_reading = true;

    try {
        // Read opening [
        if (stream.get() != '[')
            throw invalid_json((size_t)stream.tellg()-1);
        eat_whitespace(stream);

        // Check for closing ] (which means the array is empty)
        if (stream.peek() == ']') {
            keep_reading = false;
            stream.get();
        }

        // Read all values and closing ]
        while (keep_reading) {
            // Get value
            arr->push_back_move(parse_value(stream));

            switch (stream.get()) {
            case ',':   // comma => more values to get after this
                break;
            case ']':   // square bracket => end of array
                keep_reading = false;
                break;
            default:    // invalid character
                throw invalid_json((size_t)stream.tellg()-1);
            }
        }
    } catch (...) {
        // Clear used memory before passing on the exception
        delete arr;
        throw;
    }

    return arr;
}

static json::value* parse_object(std::istream &stream) {
    json::value_object* obj = new json::value_object();
    bool keep_reading = true;

    try {
        // Read opening {
        if (stream.get() != '{')
            throw invalid_json((size_t)stream.tellg()-1);
        eat_whitespace(stream);

        // Check for closing } (which means the object is empty)
        if (stream.peek() == '}') {
            keep_reading = false;
            stream.get();
        }

        // Read all key-value pairs and closing }
        while (keep_reading) {
            // Key
            eat_whitespace(stream);
            json::value_string* key_obj = (json::value_string*) parse_string(stream);
            std::string key = key_obj->value();
            delete key_obj;
            if (obj->exists(key))   // Make sure it doesn't already exist
                throw invalid_json((size_t)stream.tellg()-1);
            eat_whitespace(stream);

            // Colon separator
            if (stream.get() != ':')
                throw invalid_json((size_t)stream.tellg()-1);

            // Value
            obj->set_move(key, parse_value(stream));

            // Comma or closing }
            switch (stream.get()) {
            case ',':   // comma => more key/value pairs after this
                break;
            case '}':   // curly bracket => end of object
                keep_reading = false;
                break;
            default:    // invalid character
                throw invalid_json((size_t)stream.tellg()-1);
            }
        }
    } catch (...) {
        // Clear used memory before passing on the exception
        delete obj;
        throw;
    }

    return obj;
}

static json::value* parse_value(std::istream &stream) {
    json::value* val;
    eat_whitespace(stream);

    // Decide what kind of value it is
    int c = stream.peek();
    switch (c) {
    case 'n':       // null
        val = parse_null(stream);
        break;
    case 'f':       // false
        val = parse_false(stream);
        break;
    case 't':       // true
        val = parse_true(stream);
        break;
    case '[':       // array
        val = parse_array(stream);
        break;
    case '{':       // object
        val = parse_object(stream);
        break;
    case '"':       // string
        val = parse_string(stream);
        break;
    default:
        if (c == '-' || ('0' <= c && c <= '9'))     // number
            val = parse_number(stream);
        else        // invalid character
            throw invalid_json((size_t)stream.tellg());
    }

    try {
        eat_whitespace(stream);
    } catch (...) {
        // On exception, delete the object we created before rethrowing it
        delete val;
        throw;
    }

    return val;
}
