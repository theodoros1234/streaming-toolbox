#include "logging.h"

#include <vector>
#include <ostream>
#include <fstream>
#include <mutex>
#include <time.h>

namespace strtb::logging {

struct LogStreamInstance {
    std::ostream *stream;
    level log_level;
    std::string endline;
    bool force_flush;
    formatting_method formatting;
};

std::mutex lock;
std::vector<LogStreamInstance> output_streams;
std::vector<std::fstream> output_files;
std::string endline_type_symbol[] = {"\r\n", "\r", "\n"};
std::string level_name[] = {"DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"};
std::string level_ansi_color[] = {"\e[32m", "\e[34m", "\e[33m", "\e[31m", "\e[35m"};
int min_level = CRITICAL+1;

void add_output_stream(std::ostream *stream, level log_level, endline_type endline_type, bool force_flush, formatting_method formatting) {
    std::lock_guard<std::mutex> guard(lock);
    output_streams.push_back({
        .stream = stream,
        .log_level = log_level,
        .endline = endline_type_symbol[endline_type],
        .force_flush = force_flush,
        .formatting = formatting
    });
    if (log_level < min_level)
        min_level = log_level;
}

void add_output_file(std::string path, level log_level, endline_type endline_type, bool force_flush, formatting_method formatting) {
    std::ostream *stream;
    {
        // Open file
        std::lock_guard<std::mutex> guard(lock);
        output_files.emplace_back(path, std::fstream::out);
        stream = &output_files.back();
    }
    // Add file into output streams
    add_output_stream(stream, log_level, endline_type, force_flush, formatting);
}

message_part::message_part(std::string value) {
    v_str = value;
    type = STR;
}

message_part::message_part(const char *value) {
    v.c_str = value;
    type = C_STR;
}

message_part::message_part(char value) {
    v.c = value;
    type = CHAR;
}

message_part::message_part(int32_t value) {
    v.int32 = value;
    type = INT32;
}

message_part::message_part(uint32_t value) {
    v.uint32 = value;
    type = UINT32;
}

message_part::message_part(int64_t value) {
    v.int64 = value;
    type = INT64;
}

message_part::message_part(uint64_t value) {
    v.uint64 = value;
    type = UINT64;
}

message_part::message_part(float value) {
    v.fl = value;
    type = FLOAT;
}

message_part::message_part(double value) {
    v.db = value;
    type = DOUBLE;
}

message_part::message_part(void *value) {
    v.ptr = value;
    type = PTR;
}

message_part::message_part(std::filesystem::path value) {
    v_path = value;
    type = PATH;
}

void message_part::put_into_stream(std::ostream *stream) {
    switch (type) {
    case STR:
        *stream << v_str;
        break;
    case C_STR:
        *stream << v.c_str;
        break;
    case CHAR:
        *stream << v.c;
        break;
    case INT32:
        *stream << v.int32;
        break;
    case UINT32:
        *stream << v.uint32;
        break;
    case INT64:
        *stream << v.int64;
        break;
    case UINT64:
        *stream << v.uint64;
        break;
    case FLOAT:
        *stream << v.fl;
        break;
    case DOUBLE:
        *stream << v.db;
        break;
    case PTR:
        *stream << v.ptr;
        break;
    case PATH:
        *stream << v_path;
        break;
    }
}

source::source(std::string name) {
    this->name = name;
}

void source::put(level type, std::vector<message_part> message) {
    // Skip if no outputs accept this level
    if (type < min_level)
        return;

    std::lock_guard<std::mutex> guard(lock);

    // Get timestamp
    time_t current_time = time(NULL);
    tm current_time_split;
    localtime_r(&current_time, &current_time_split);
    char timestamp_buffer[32];
    strftime(timestamp_buffer, 32, "%Y-%m-%d %H:%M:%S", &current_time_split);

    // Find output streams with appropriate log level and no errors
    for (auto &output : output_streams) {
        if (output.log_level <= type && output.stream->good()) {
            // Color and format message appropriately: print timestamp, log level and object name
            switch (output.formatting) {
            case NONE:
                *output.stream << "[" << timestamp_buffer << "] [" << level_name[type] << "] [" << this->name << "] ";
                break;
            case ANSI_ESCAPE_CODES:
                *output.stream << "\e[90m[\e[1m" << timestamp_buffer << "\e[0m\e[90m] [\e[0m\e[1m" << level_ansi_color[type] << level_name[type] << "\e[0m\e[90m] [\e[0m\e[1m" << this->name << "\e[0m\e[90m]\e[0m ";
                break;
            }
            // Print actual message
            for (auto &part : message)
                part.put_into_stream(output.stream);
            // Endline and force flush if needed
            *output.stream << output.endline;
            if (output.force_flush)
                output.stream->flush();
        }
    }
}

}
