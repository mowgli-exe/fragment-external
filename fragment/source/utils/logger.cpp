#include "utils/logger.h"

#include <format>
#include <print>
#include <string>
#include <windows.h>

namespace {
    std::FILE* open_log_file()
    {
        static std::FILE* log_file = []() -> std::FILE* {
            std::FILE* f = nullptr;
            fopen_s(&f, "fragment.log", "a");
            return f;
        }();
        return log_file;
    }

    HANDLE resolve_handle(std::FILE* stream)
    {
        return stream == stderr ? GetStdHandle(STD_ERROR_HANDLE) : GetStdHandle(STD_OUTPUT_HANDLE);
    }

    WORD read_default_attributes(HANDLE handle)
    {
        CONSOLE_SCREEN_BUFFER_INFO info{};
        if (GetConsoleScreenBufferInfo(handle, &info))
        {
            return info.wAttributes;
        }
        return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    }

    WORD default_stdout_attributes = 0;
    WORD default_stderr_attributes = 0;
    bool stdout_attributes_ready = false;
    bool stderr_attributes_ready = false;

    WORD fetch_default_attributes(bool is_error, HANDLE handle)
    {
        WORD& attributes = is_error ? default_stderr_attributes : default_stdout_attributes;
        bool& ready = is_error ? stderr_attributes_ready : stdout_attributes_ready;
        if (!ready)
        {
            attributes = read_default_attributes(handle);
            ready = true;
        }
        return attributes;
    }

    WORD select_color(std::string_view prefix, WORD fallback)
    {
        if (prefix == "[error]")
        {
            return FOREGROUND_RED | FOREGROUND_INTENSITY;
        }
        if (prefix == "[warning]")
        {
            return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        }
        if (prefix == "[success]")
        {
            return FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        }
        if (prefix == "[info]")
        {
            return FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        }
        return fallback;
    }

    std::string make_timestamp()
    {
        SYSTEMTIME system_time{};
        GetLocalTime(&system_time);
        return std::format("{:02}:{:02}:{:02}", system_time.wHour, system_time.wMinute, system_time.wSecond);
    }
}

namespace logger_core
{
    void log_prompt(std::string_view prefix, std::string_view message)
    {
        const HANDLE handle = resolve_handle(stdout);
        const WORD base_attributes = fetch_default_attributes(false, handle);
        const WORD color = select_color(prefix, base_attributes);
        const WORD background = base_attributes & 0xF0;
        constexpr WORD timestamp_color = FOREGROUND_INTENSITY;

        const auto stamp = make_timestamp();

        if (handle != INVALID_HANDLE_VALUE)
        {
            SetConsoleTextAttribute(handle, timestamp_color | background);
        }
        std::print(stdout, "[{}] ", stamp);

        if (handle != INVALID_HANDLE_VALUE)
        {
            SetConsoleTextAttribute(handle, color | background);
        }
        std::print(stdout, "{} ", prefix);

        if (handle != INVALID_HANDLE_VALUE)
        {
            SetConsoleTextAttribute(handle, base_attributes);
        }
        std::print(stdout, "{}", message);
        std::fflush(stdout);
    }

    void log_message(std::FILE* stream, std::string_view prefix, std::string_view message)
    {
        const HANDLE handle = resolve_handle(stream);
        const bool is_error = stream == stderr;
        const WORD base_attributes = fetch_default_attributes(is_error, handle);
        const WORD color = select_color(prefix, base_attributes);
        constexpr WORD timestamp_color = FOREGROUND_INTENSITY;
        const WORD background = base_attributes & 0xF0;

        const auto stamp = make_timestamp();

        if (handle != INVALID_HANDLE_VALUE)
        {
            SetConsoleTextAttribute(handle, timestamp_color | background);
        }
        std::print(stream, "[{}] ", stamp);

        if (handle != INVALID_HANDLE_VALUE)
        {
            SetConsoleTextAttribute(handle, color | background);
        }
        std::print(stream, "{} ", prefix);

        if (handle != INVALID_HANDLE_VALUE)
        {
            SetConsoleTextAttribute(handle, base_attributes);
        }
std::print(stream, "{}\n", message);

        if (auto* log_file = open_log_file())
        {
            const auto line = std::format("{} {} {}\n", stamp, prefix, message);
            std::fputs(line.c_str(), log_file);
            std::fflush(log_file);
        }
    }
}
