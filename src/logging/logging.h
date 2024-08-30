#ifndef STRTB_LOGGING_LOGGING_H
#define STRTB_LOGGING_LOGGING_H

#include <cstdint>
#include <filesystem>
#include <ostream>
#include <vector>

namespace strtb::logging {

enum level {DEBUG, INFO, WARNING, ERROR, CRITICAL};
enum endline_type {WINDOWS, MAC, LINUX};
enum formatting_method {NONE, ANSI_ESCAPE_CODES};

void add_output_stream(std::ostream *ostream, level log_level, endline_type endline_type, bool force_flush, formatting_method formatting);
void add_output_file(std::string path, level log_level, endline_type endline_type, bool force_flush, formatting_method formatting);

class message_part {
private:
    std::string v_str;
    std::filesystem::path v_path;
    union {
        const char *c_str;
        char c;
        int32_t int32;
        uint32_t uint32;
        int64_t int64;
        uint64_t uint64;
        float fl;
        double db;
        void *ptr;
    } v;
    enum {STR, C_STR, CHAR, INT32, UINT32, INT64, UINT64, FLOAT, DOUBLE, PTR, PATH} type;
public:
    message_part(std::string value);
    message_part(const char *value);
    message_part(char value);
    message_part(int32_t value);
    message_part(uint32_t value);
    message_part(int64_t value);
    message_part(uint64_t value);
    message_part(float value);
    message_part(double value);
    message_part(void *value);
    message_part(std::filesystem::path value);
protected:
    friend class source;
    void put_into_stream(std::ostream *stream);
};

class source {
private:
    std::string name;
public:
    source(std::string name);
    void put(level type, std::vector<message_part> message);
};

}

#endif // STRTB_LOGGING_LOGGING_H
