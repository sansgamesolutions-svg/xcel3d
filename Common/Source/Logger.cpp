#include "Common/Logger.h"
#include <iostream>

namespace xcel {

Logger& Logger::Instance()
{
    static Logger s_instance;
    return s_instance;
}

Logger::Logger()
    : m_minSeverity{static_cast<uint8_t>(LogSeverity::Debug)}
{
    for (auto& flag : m_catEnabled)
        flag.store(true, std::memory_order_relaxed);

    m_sink = [](LogSeverity sev, LogCategory cat, std::string_view msg)
    {
        std::cerr << "[" << Logger::SeverityName(sev) << "]["
                  << Logger::CategoryName(cat) << "] " << msg << "\n";
    };
}

void Logger::SetMinSeverity(LogSeverity level) noexcept
{
    m_minSeverity.store(static_cast<uint8_t>(level), std::memory_order_relaxed);
}

void Logger::SetCategoryEnabled(LogCategory cat, bool enabled) noexcept
{
    m_catEnabled[static_cast<size_t>(cat)].store(enabled, std::memory_order_relaxed);
}

void Logger::SetSink(Sink sink)
{
    std::scoped_lock lock(m_sinkMutex);
    m_sink = std::move(sink);
}

bool Logger::IsEnabled(LogSeverity sev, LogCategory cat) const noexcept
{
    if (static_cast<uint8_t>(sev) < m_minSeverity.load(std::memory_order_relaxed))
        return false;
    return m_catEnabled[static_cast<size_t>(cat)].load(std::memory_order_relaxed);
}

void Logger::LogImpl(LogSeverity sev, LogCategory cat, std::string msg)
{
    std::scoped_lock lock(m_sinkMutex);
    if (m_sink)
        m_sink(sev, cat, msg);
}

std::string_view Logger::SeverityName(LogSeverity s) noexcept
{
    switch (s)
    {
    case LogSeverity::Trace: return "TRACE";
    case LogSeverity::Debug: return "DEBUG";
    case LogSeverity::Info:  return "INFO ";
    case LogSeverity::Warn:  return "WARN ";
    case LogSeverity::Error: return "ERROR";
    }
    return "?????";
}

std::string_view Logger::CategoryName(LogCategory c) noexcept
{
    switch (c)
    {
    case LogCategory::Vulkan:    return "Vulkan   ";
    case LogCategory::Batching:  return "Batching ";
    case LogCategory::Rendering: return "Rendering";
    case LogCategory::IO:        return "IO       ";
    case LogCategory::App:       return "App      ";
    case LogCategory::_Count:    break;
    }
    return "Unknown  ";
}

} // namespace xcel
