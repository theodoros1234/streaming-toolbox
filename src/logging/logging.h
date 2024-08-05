#ifndef LOGGING_LOGGING_H
#define LOGGING_LOGGING_H

#include <cstdint>
#include <filesystem>
#include <ostream>
#include <vector>

namespace logging {

enum level {DEBUG, INFO, WARNING, ERROR, CRITICAL};
enum endline_type {WINDOWS, MAC, LINUX};
enum formatting_method {NONE, ANSI_ESCAPE_CODES};

void addOutputStream(std::ostream *ostream, level log_level, endline_type endline_type, bool force_flush, formatting_method formatting);
void addOutputFile(std::string path, level log_level, endline_type endline_type, bool force_flush, formatting_method formatting);

class LogMessagePart {
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
    LogMessagePart(std::string value);
    LogMessagePart(const char *value);
    LogMessagePart(char value);
    LogMessagePart(int32_t value);
    LogMessagePart(uint32_t value);
    LogMessagePart(int64_t value);
    LogMessagePart(uint64_t value);
    LogMessagePart(float value);
    LogMessagePart(double value);
    LogMessagePart(void *value);
    LogMessagePart(std::filesystem::path value);
    void putIntoStream(std::ostream *stream);
};

class LogSource {
private:
    std::string name;
public:
    LogSource(std::string name);
    void put(level type, std::vector<LogMessagePart> message);
};

}

#endif // LOGGING_LOGGING_H
