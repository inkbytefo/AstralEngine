#pragma once

#include <format>
#include <functional>
#include <string>
#include <string_view>

namespace Astral {

enum class LogLevel { Trace, Info, Warn, Error };

// Minimal logging. Messages go to stderr by default; SetLogSink redirects them
// (the editor routes them into its console panel, tests can silence them).
namespace Log {

using Sink = std::function<void(LogLevel, std::string_view category, std::string_view message)>;

void SetSink(Sink sink);          // nullptr restores the default stderr sink
void SetMinLevel(LogLevel level);
LogLevel GetMinLevel();
void Write(LogLevel level, std::string_view category, std::string_view message);

template <typename... Args>
void Trace(std::string_view category, std::format_string<Args...> fmt, Args&&... args) {
    if (GetMinLevel() <= LogLevel::Trace) Write(LogLevel::Trace, category, std::format(fmt, std::forward<Args>(args)...));
}
template <typename... Args>
void Info(std::string_view category, std::format_string<Args...> fmt, Args&&... args) {
    if (GetMinLevel() <= LogLevel::Info) Write(LogLevel::Info, category, std::format(fmt, std::forward<Args>(args)...));
}
template <typename... Args>
void Warn(std::string_view category, std::format_string<Args...> fmt, Args&&... args) {
    if (GetMinLevel() <= LogLevel::Warn) Write(LogLevel::Warn, category, std::format(fmt, std::forward<Args>(args)...));
}
template <typename... Args>
void Error(std::string_view category, std::format_string<Args...> fmt, Args&&... args) {
    Write(LogLevel::Error, category, std::format(fmt, std::forward<Args>(args)...));
}

} // namespace Log
} // namespace Astral
