#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <format>
#include <functional>
#include <mutex>
#include <string_view>

namespace xcel {

enum class LogSeverity : uint8_t { Trace = 0, Debug, Info, Warn, Error };

enum class LogCategory : uint8_t
{
    Vulkan    = 0,
    Batching,
    Rendering,
    IO,
    App,
    _Count
};

// Thread-safe logger with per-category and per-severity filtering.
// Default sink writes to stderr. Replace with SetSink() for file output, GUI overlay, etc.
// IsEnabled() is lock-free; the sink call is mutex-guarded.
class Logger
{
public:
    using Sink = std::function<void(LogSeverity, LogCategory, std::string_view)>;

    static Logger& Instance();

    void SetMinSeverity(LogSeverity level) noexcept;
    void SetCategoryEnabled(LogCategory cat, bool enabled) noexcept;
    void SetSink(Sink sink);

    [[nodiscard]] bool IsEnabled(LogSeverity sev, LogCategory cat) const noexcept;

    template<typename... Args>
    void Log(LogSeverity sev, LogCategory cat,
             std::format_string<Args...> fmt, Args&&... args)
    {
        if (!IsEnabled(sev, cat)) return;
        LogImpl(sev, cat, std::format(fmt, std::forward<Args>(args)...));
    }

    static std::string_view SeverityName(LogSeverity s) noexcept;
    static std::string_view CategoryName(LogCategory c) noexcept;

private:
    Logger();
    void LogImpl(LogSeverity sev, LogCategory cat, std::string msg);

    static constexpr size_t kCatCount = static_cast<size_t>(LogCategory::_Count);

    std::atomic<uint8_t>                     m_minSeverity;
    std::array<std::atomic<bool>, kCatCount> m_catEnabled{};
    mutable std::mutex                       m_sinkMutex;
    Sink                                     m_sink;
};

} // namespace xcel

// ── Compile-time enable/disable ────────────────────────────────────────────
// Debug builds default to enabled; Release to disabled.
// Override by passing -DXCEL_LOGGING=1 (or 0) to CMake.
#ifndef XCEL_LOGGING
#  ifdef NDEBUG
#    define XCEL_LOGGING 0
#  else
#    define XCEL_LOGGING 1
#  endif
#endif

#if XCEL_LOGGING
#  define XCEL_LOG(sev, cat, ...) \
       ::xcel::Logger::Instance().Log( \
           ::xcel::LogSeverity::sev, ::xcel::LogCategory::cat, __VA_ARGS__)
#  define XCEL_LOG_TRACE(cat, ...) XCEL_LOG(Trace, cat, __VA_ARGS__)
#  define XCEL_LOG_DEBUG(cat, ...) XCEL_LOG(Debug, cat, __VA_ARGS__)
#  define XCEL_LOG_INFO(cat, ...)  XCEL_LOG(Info,  cat, __VA_ARGS__)
#  define XCEL_LOG_WARN(cat, ...)  XCEL_LOG(Warn,  cat, __VA_ARGS__)
#  define XCEL_LOG_ERROR(cat, ...) XCEL_LOG(Error, cat, __VA_ARGS__)
#else
#  define XCEL_LOG(sev, cat, ...)  do {} while(0)
#  define XCEL_LOG_TRACE(cat, ...) do {} while(0)
#  define XCEL_LOG_DEBUG(cat, ...) do {} while(0)
#  define XCEL_LOG_INFO(cat, ...)  do {} while(0)
#  define XCEL_LOG_WARN(cat, ...)  do {} while(0)
#  define XCEL_LOG_ERROR(cat, ...) do {} while(0)
#endif
