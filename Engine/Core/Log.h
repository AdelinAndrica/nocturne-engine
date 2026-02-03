#pragma once
#include "BuildConfig.h"
#include <cstdint>

namespace noc {

    enum class LogLevel : uint8_t
    {
        Trace, Debug, Info, Warn, Error, Fatal
    };

    struct LogMessage
    {
        LogLevel level;
        const char* channel;
        const char* text; // formatted final text (Phase 1: transient)
    };

    class Logger
    {
    public:
        void Init();
        void Shutdown();

        void Write(LogLevel level, const char* channel, const char* fmt, ...);
    };

    Logger& GetLogger();

} // namespace noc

#if NOC_ENABLE_LOGGING

#define NOC_LOG_TRACE(ch, fmt, ...) ::noc::GetLogger().Write(::noc::LogLevel::Trace, (ch), (fmt), __VA_ARGS__)
#define NOC_LOG_DEBUG(ch, fmt, ...) ::noc::GetLogger().Write(::noc::LogLevel::Debug, (ch), (fmt), __VA_ARGS__)
#define NOC_LOG_INFO(ch, fmt, ...)  ::noc::GetLogger().Write(::noc::LogLevel::Info,  (ch), (fmt), __VA_ARGS__)
#define NOC_LOG_WARN(ch, fmt, ...)  ::noc::GetLogger().Write(::noc::LogLevel::Warn,  (ch), (fmt), __VA_ARGS__)
#define NOC_LOG_ERROR(ch, fmt, ...) ::noc::GetLogger().Write(::noc::LogLevel::Error, (ch), (fmt), __VA_ARGS__)
#define NOC_LOG_FATAL(ch, fmt, ...) ::noc::GetLogger().Write(::noc::LogLevel::Fatal, (ch), (fmt), __VA_ARGS__)

#else

#define NOC_LOG_TRACE(...) do{}while(0)
#define NOC_LOG_DEBUG(...) do{}while(0)
#define NOC_LOG_INFO(...)  do{}while(0)
#define NOC_LOG_WARN(...)  do{}while(0)
#define NOC_LOG_ERROR(...) do{}while(0)
#define NOC_LOG_FATAL(...) do{}while(0)

#endif
