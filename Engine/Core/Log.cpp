#include "Log.h"
#include <cstdarg>
#include <cstdio>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace noc {

    static Logger g_logger;

    Logger& GetLogger()
    {
        return g_logger;
    }

    void Logger::Init()
    {
        // Phase 1: nothing heavy
    }

    void Logger::Shutdown()
    {
        // Phase 1: nothing heavy
    }

    static const char* ToString(LogLevel lvl)
    {
        switch (lvl)
        {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Fatal: return "FATAL";
        default:              return "LOG";
        }
    }

    void Logger::Write(LogLevel level, const char* channel, const char* fmt, ...)
    {
        char msg[1024];

        va_list args;
        va_start(args, fmt);
        std::vsnprintf(msg, sizeof(msg), fmt, args);
        va_end(args);

        char line[1200];
        std::snprintf(line, sizeof(line), "[%s][%s] %s\n", ToString(level), channel ? channel : "General", msg);

#if defined(_WIN32)
        OutputDebugStringA(line);
#endif

        std::fputs(line, stdout);
        std::fflush(stdout);
    }

} // namespace noc
